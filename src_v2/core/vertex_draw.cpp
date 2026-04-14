#include <amal/trigonometric.hpp>
#include <auik/v2/detail/vertex_draw.hpp>
#include <algorithm>
#include <cmath>

namespace auik::v2::detail
{
    namespace
    {
        struct PerimeterPoint
        {
            amal::vec2 position;
            amal::vec4 color;
        };

        inline u32 estimate_corner_segments(f32 radius)
        {
            if (radius < 0.5f) return 1u;

            constexpr f32 max_error = 0.06f;
            const f32 safe_radius = amal::max(radius, 1e-5f);
            const f32 error = amal::min(max_error, safe_radius);
            const f32 arg = amal::clamp(1.0f - error / safe_radius, -1.0f, 1.0f);
            const f32 step = std::acos(arg);
            if (step <= 1e-5f) return 24u;

            const f32 quarter_segment_count_f = amal::ceil((amal::pi<f32>() * 0.25f) / step);
            const u32 quarter_segment_count = static_cast<u32>(quarter_segment_count_f);
            return amal::clamp(quarter_segment_count, 4u, 24u);
        }

        inline void push_gradient_vertex(GradientRectVertexData &vertex_data, const amal::vec2 &position,
                                         const amal::vec4 &color, f32 z_order, u16 clip_id)
        {
            VertexStreamVertex vertex{};
            vertex.position = position;
            vertex.z_order = z_order;
            vertex.color = detail::pack_rgba8(color);
            vertex.clip_id = clip_id;
            vertex_data.vertices.push_back(vertex);
        }

        inline f32 rounded_rect_top_y(f32 x, f32 left, f32 top, f32 right, f32 rtl, f32 rtr)
        {
            auto corner_top = [](f32 px, f32 cx, f32 cy, f32 radius) {
                const f32 dx = px - cx;
                const f32 inside = amal::max(radius * radius - dx * dx, 0.0f);
                return cy - std::sqrt(inside);
            };

            if (rtl > 0.0f && x < left + rtl) return corner_top(x, left + rtl, top + rtl, rtl);
            if (rtr > 0.0f && x > right - rtr) return corner_top(x, right - rtr, top + rtr, rtr);
            return top;
        }

        inline f32 rounded_rect_bottom_y(f32 x, f32 left, f32 bottom, f32 right, f32 rbl, f32 rbr)
        {
            auto corner_bottom = [](f32 px, f32 cx, f32 cy, f32 radius) {
                const f32 dx = px - cx;
                const f32 inside = amal::max(radius * radius - dx * dx, 0.0f);
                return cy + std::sqrt(inside);
            };

            if (rbl > 0.0f && x < left + rbl) return corner_bottom(x, left + rbl, bottom - rbl, rbl);
            if (rbr > 0.0f && x > right - rbr) return corner_bottom(x, right - rbr, bottom - rbr, rbr);
            return bottom;
        }

        inline void append_left_corner_x_samples(acul::vector<f32> &x_samples, f32 left, f32 radius, u32 segments)
        {
            if (radius < 0.5f || segments == 0u) return;

            constexpr f32 pi = 3.14159265358979323846f;
            constexpr f32 half_pi = pi * 0.5f;
            const f32 center_x = left + radius;
            for (u32 i = 0; i <= segments; ++i)
            {
                const f32 t = static_cast<f32>(i) / static_cast<f32>(segments);
                const f32 angle = pi - half_pi * t;
                x_samples.push_back(center_x + std::cos(angle) * radius);
            }
        }

        inline void append_right_corner_x_samples(acul::vector<f32> &x_samples, f32 right, f32 radius, u32 segments)
        {
            if (radius < 0.5f || segments == 0u) return;

            constexpr f32 pi = 3.14159265358979323846f;
            constexpr f32 half_pi = pi * 0.5f;
            const f32 center_x = right - radius;
            for (u32 i = 0; i <= segments; ++i)
            {
                const f32 t = static_cast<f32>(i) / static_cast<f32>(segments);
                const f32 angle = pi * 1.5f + half_pi * t;
                x_samples.push_back(center_x + std::cos(angle) * radius);
            }
        }

        inline void append_gradient_strip_vertices(GradientRectVertexData &vertex_data, const acul::vector<f32> &x_samples,
                                                   const amal::rect &rect, f32 z_order, u16 clip_id,
                                                   const amal::vec4 *colors, u32 color_count, f32 left, f32 top,
                                                   f32 right, f32 bottom, f32 rtl, f32 rtr, f32 rbl, f32 rbr)
        {
            const f32 width = amal::max(rect.size.x, 1e-5f);
            for (f32 x : x_samples)
            {
                const f32 t = (x - rect.offset.x) / width;
                const amal::vec4 color = sample_gradient_color(colors, color_count, t);
                const f32 top_y = rounded_rect_top_y(x, left, top, right, rtl, rtr);
                const f32 bottom_y = rounded_rect_bottom_y(x, left, bottom, right, rbl, rbr);
                push_gradient_vertex(vertex_data, {x, top_y}, color, z_order, clip_id);
                push_gradient_vertex(vertex_data, {x, bottom_y}, color, z_order, clip_id);
            }
        }

        inline void append_gradient_strip_indices(GradientRectVertexData &vertex_data, u32 strip_count)
        {
            for (u32 segment = 0; segment + 1u < strip_count; ++segment)
            {
                const u32 base = segment * 2u;
                vertex_data.indices.push_back(base + 0u);
                vertex_data.indices.push_back(base + 1u);
                vertex_data.indices.push_back(base + 2u);
                vertex_data.indices.push_back(base + 2u);
                vertex_data.indices.push_back(base + 1u);
                vertex_data.indices.push_back(base + 3u);
            }
        }

        inline amal::vec4 sample_perimeter_color(const amal::rect &sample_rect, const amal::vec4 *colors,
                                                 u32 color_count, f32 x, f32 alpha)
        {
            const f32 clamped_x =
                amal::clamp(x, sample_rect.offset.x, sample_rect.offset.x + sample_rect.size.x);
            amal::vec4 color = sample_gradient_color(
                colors, color_count,
                (clamped_x - sample_rect.offset.x) / amal::max(sample_rect.size.x, 1e-5f));
            color.w *= alpha;
            return color;
        }

        inline void append_perimeter_point(acul::vector<PerimeterPoint> &points, const amal::vec2 &position,
                                           const amal::rect &sample_rect, const amal::vec4 *colors, u32 color_count,
                                           f32 alpha)
        {
            if (!points.empty())
            {
                const amal::vec2 &last = points.back().position;
                if (std::abs(last.x - position.x) < 1e-4f && std::abs(last.y - position.y) < 1e-4f) return;
            }
            points.push_back({position, sample_perimeter_color(sample_rect, colors, color_count, position.x, alpha)});
        }

        static acul::vector<f32> build_fringe_x_samples(const acul::vector<f32> &x_samples, f32 rtl, f32 rtr, f32 rbr,
                                                       f32 rbl)
        {
            acul::vector<f32> out;
            if (x_samples.empty()) return out;
            const f32 max_radius = amal::max(amal::max(rtl, rtr), amal::max(rbr, rbl));
            if (max_radius < 8.0f || x_samples.size() <= 16u) return x_samples;

            out.reserve(x_samples.size() / 2u + 2u);
            out.push_back(x_samples.front());
            for (size_t i = 1; i + 1 < x_samples.size(); i += 2u) out.push_back(x_samples[i]);
            if (x_samples.size() > 1u && std::abs(out.back() - x_samples.back()) >= 1e-4f) out.push_back(x_samples.back());
            return out;
        }

        static void build_rounded_rect_perimeter(acul::vector<PerimeterPoint> &points, const amal::rect &rect,
                                                 const acul::vector<f32> &x_samples, f32 rtl, f32 rtr, f32 rbr, f32 rbl,
                                                 const amal::vec4 *colors, u32 color_count, f32 alpha)
        {
            points.clear();
            const f32 left = rect.offset.x;
            const f32 top = rect.offset.y;
            const f32 right = rect.offset.x + rect.size.x;
            const f32 bottom = rect.offset.y + rect.size.y;

            for (f32 x : x_samples)
            {
                const f32 clamped_x = amal::clamp(x, left, right);
                append_perimeter_point(points, {clamped_x, rounded_rect_top_y(clamped_x, left, top, right, rtl, rtr)},
                                       rect, colors, color_count, alpha);
            }

            const f32 right_top = rounded_rect_top_y(right, left, top, right, rtl, rtr);
            const f32 right_bottom = rounded_rect_bottom_y(right, left, bottom, right, rbl, rbr);
            if (right_bottom - right_top > 1e-4f)
                append_perimeter_point(points, {right, (right_top + right_bottom) * 0.5f}, rect, colors, color_count,
                                       alpha);

            for (size_t i = x_samples.size(); i-- > 0u;)
            {
                const f32 clamped_x = amal::clamp(x_samples[i], left, right);
                append_perimeter_point(points,
                                       {clamped_x, rounded_rect_bottom_y(clamped_x, left, bottom, right, rbl, rbr)},
                                       rect, colors, color_count, alpha);
            }

            const f32 left_bottom = rounded_rect_bottom_y(left, left, bottom, right, rbl, rbr);
            const f32 left_top = rounded_rect_top_y(left, left, top, right, rtl, rtr);
            if (left_bottom - left_top > 1e-4f)
                append_perimeter_point(points, {left, (left_top + left_bottom) * 0.5f}, rect, colors, color_count,
                                       alpha);
        }

        static void append_gradient_fringe(GradientRectVertexData &vertex_data, const amal::rect &rect,
                                           const acul::vector<f32> &x_samples, f32 z_order, u16 clip_id,
                                           const amal::vec4 *colors, u32 color_count, f32 rtl, f32 rtr, f32 rbr,
                                           f32 rbl, f32 fringe_width)
        {
            if (fringe_width <= 0.0f) return;
            const acul::vector<f32> fringe_x_samples = build_fringe_x_samples(x_samples, rtl, rtr, rbr, rbl);
            if (fringe_x_samples.size() < 2u) return;

            acul::vector<PerimeterPoint> inner;
            acul::vector<PerimeterPoint> outer;
            build_rounded_rect_perimeter(inner, rect, fringe_x_samples, rtl, rtr, rbr, rbl, colors, color_count,
                                         1.0f);

            const amal::rect outer_rect = {{rect.offset.x - fringe_width, rect.offset.y - fringe_width},
                                           {rect.size.x + fringe_width * 2.0f, rect.size.y + fringe_width * 2.0f}};
            build_rounded_rect_perimeter(outer, outer_rect, fringe_x_samples, rtl + fringe_width, rtr + fringe_width,
                                         rbr + fringe_width, rbl + fringe_width, colors, color_count, 0.0f);

            if (inner.size() != outer.size() || inner.size() < 3u) return;

            const u32 base_vertex = static_cast<u32>(vertex_data.vertices.size());
            for (size_t i = 0; i < inner.size(); ++i)
            {
                push_gradient_vertex(vertex_data, inner[i].position, inner[i].color, z_order, clip_id);
                push_gradient_vertex(vertex_data, outer[i].position, outer[i].color, z_order, clip_id);
            }

            const u32 ring_count = static_cast<u32>(inner.size());
            for (u32 i = 0; i < ring_count; ++i)
            {
                const u32 next = (i + 1u) % ring_count;
                const u32 inner0 = base_vertex + i * 2u;
                const u32 outer0 = inner0 + 1u;
                const u32 inner1 = base_vertex + next * 2u;
                const u32 outer1 = inner1 + 1u;
                vertex_data.indices.push_back(inner0);
                vertex_data.indices.push_back(outer0);
                vertex_data.indices.push_back(inner1);
                vertex_data.indices.push_back(inner1);
                vertex_data.indices.push_back(outer0);
                vertex_data.indices.push_back(outer1);
            }
        }
    } // namespace

    bool build_gradient_rect_vertex_data(GradientRectVertexData &vertex_data, const amal::rect &rect, f32 z_order,
                                         u16 clip_id, const amal::vec4 *colors, u32 color_count, f32 rounding,
                                         u32 corner_mask, f32 fringe_width)
    {
        vertex_data.clear();
        if (!colors || color_count == 0u) return false;
        if (amal::is_rect_empty(rect)) return false;

        const u32 stop_count = color_count > 1u ? color_count : 2u;
        rounding = clamp_corner_rounding(rect, rounding, corner_mask);

        const f32 left = rect.offset.x;
        const f32 top = rect.offset.y;
        const f32 right = rect.offset.x + rect.size.x;
        const f32 bottom = rect.offset.y + rect.size.y;

        const f32 rtl = (corner_mask & 0x1u) ? rounding : 0.0f;
        const f32 rtr = (corner_mask & 0x2u) ? rounding : 0.0f;
        const f32 rbr = (corner_mask & 0x4u) ? rounding : 0.0f;
        const f32 rbl = (corner_mask & 0x8u) ? rounding : 0.0f;
        const u32 seg_tl = estimate_corner_segments(rtl);
        const u32 seg_tr = estimate_corner_segments(rtr);
        const u32 seg_br = estimate_corner_segments(rbr);
        const u32 seg_bl = estimate_corner_segments(rbl);

        acul::vector<f32> x_samples;
        x_samples.reserve(stop_count + seg_tl + seg_tr + seg_br + seg_bl + 6u);
        x_samples.push_back(left);
        x_samples.push_back(right);
        for (u32 i = 0; i < stop_count; ++i)
        {
            const f32 t = stop_count > 1u ? static_cast<f32>(i) / static_cast<f32>(stop_count - 1u) : 0.0f;
            x_samples.push_back(rect.offset.x + rect.size.x * t);
        }

        if (rounding >= 0.5f && corner_mask != 0u)
        {
            append_left_corner_x_samples(x_samples, left, rtl, seg_tl);
            append_left_corner_x_samples(x_samples, left, rbl, seg_bl);
            append_right_corner_x_samples(x_samples, right, rtr, seg_tr);
            append_right_corner_x_samples(x_samples, right, rbr, seg_br);
        }

        std::sort(x_samples.begin(), x_samples.end());
        acul::vector<f32> unique_x_samples;
        unique_x_samples.reserve(x_samples.size());
        for (f32 x : x_samples)
        {
            const f32 clamped_x = amal::clamp(x, left, right);
            if (!unique_x_samples.empty() && std::abs(unique_x_samples.back() - clamped_x) < 1e-4f) continue;
            unique_x_samples.push_back(clamped_x);
        }
        if (unique_x_samples.size() < 2u) return false;

        vertex_data.vertices.reserve(unique_x_samples.size() * 2u + 128u);
        vertex_data.indices.reserve((unique_x_samples.size() - 1u) * 6u + 384u);
        append_gradient_strip_vertices(vertex_data, unique_x_samples, rect, z_order, clip_id, colors, color_count,
                                       left, top, right, bottom, rtl, rtr, rbl, rbr);
        append_gradient_strip_indices(vertex_data, static_cast<u32>(unique_x_samples.size()));
        append_gradient_fringe(vertex_data, rect, unique_x_samples, z_order, clip_id, colors, color_count, rtl, rtr,
                               rbr, rbl, fringe_width);
        vertex_data.sync_batch();
        return true;
    }
} // namespace auik::v2::detail
