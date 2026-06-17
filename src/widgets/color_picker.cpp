#include <amal/trigonometric.hpp>
#include <auik/auik.hpp>
#include <auik/detail/depth.hpp>
#include <auik/widgets/color_picker.hpp>
#include "../core/session_stream_utils.hpp"

#define AUIK_CIRCLE_WHEEL_FRINGE                  2.0f
#define AUIK_CIRCLE_TESSELLATION_MAX_ERROR        0.30f
#define AUIK_CIRCLE_FRINGE_TESSELLATION_MAX_ERROR 0.15f
#define AUIK_CIRCLE_SEGMENTS_MIN                  4u
#define AUIK_CIRCLE_SEGMENTS_MAX                  512u
#define AUIK_SQUARE_PICKER_RING_WIDTH_FACTOR      (16.0f / 180.0f)
#define AUIK_SQUARE_PICKER_RING_WIDTH_MIN         8.0f
#define AUIK_SQUARE_PICKER_RING_WIDTH_MAX         18.0f
#define AUIK_SQUARE_PICKER_RING_FRINGE            2.0f
#define AUIK_SQUARE_PICKER_INNER_PADDING          4.0f

namespace auik
{
    namespace detail
    {
        static inline f32 resolve_color_picker_size()
        {
            const f32 size = get_theme()->get_var<f32>(AUIK_STYLE_VAR_COLOR_PICKER_SIZE);
            return amal::max(size > 0.0f ? size : 120.0f, 1.0f);
        }

        static inline u32 calc_circle_auto_segment_count(f32 radius, f32 max_error = AUIK_CIRCLE_TESSELLATION_MAX_ERROR)
        {
            const f32 r = amal::max(radius, 1e-5f);
            const f32 err = amal::min(max_error, r);
            const f32 arg = amal::clamp(1.0f - err / r, -1.0f, 1.0f);
            const f32 angle_step = amal::acos(arg);
            if (angle_step <= 1e-5f) return AUIK_CIRCLE_SEGMENTS_MAX;

            u32 segments = static_cast<u32>(amal::ceil(amal::pi<f32>() / angle_step));
            if ((segments & 1u) != 0u) ++segments;
            return amal::clamp(segments, AUIK_CIRCLE_SEGMENTS_MIN, AUIK_CIRCLE_SEGMENTS_MAX);
        }

        static inline u32 calc_stable_color_picker_segment_count(f32 max_error = AUIK_CIRCLE_TESSELLATION_MAX_ERROR)
        {
            // Keep vertex batch topology stable across layout/scroll updates.
            // The draw update path can rewrite vertex data in place, but not resize batches.
            constexpr f32 reference_radius = 96.0f;
            return calc_circle_auto_segment_count(reference_radius, max_error);
        }

        static inline void fill_circle_grab_instance(const Style &style, const amal::rect &rect, f32 z_order,
                                                     u16 clip_id, u32 background_color, QuadsInstanceData &data)
        {
            data.rect = rect;
            data.z_order = z_order;
            fill_quads_instance_by_style(style, clip_id, data);
            data.background_color = background_color;
            data.border_color = style.border_color();
            data.border_thickness = style.has_visible_border() ? style.border_thickness() : 1.0f;
            data.border_radius = amal::max(0.0f, amal::min(rect.size.x, rect.size.y) * 0.5f);
            data.mask |= (static_cast<u32>(AUIK_HAS_BORDER_BIT) << 20u);
        }

        static inline StyleState resolve_grab_visual_state(StyleState state)
        {
            return (state == StyleState::active || state == StyleState::focus) ? state : StyleState::normal;
        }

        static inline amal::vec4 make_circle_color(f32 hue_deg, f32 radius_t)
        {
            const amal::vec4 white = {1.0f, 1.0f, 1.0f, 1.0f};
            const amal::vec4 hue = amal::hsl_to_rgba(hue_deg, 1.0f, 0.5f);
            const f32 t = amal::clamp(radius_t, 0.0f, 1.0f);
            return {white.x + (hue.x - white.x) * t, white.y + (hue.y - white.y) * t, white.z + (hue.z - white.z) * t,
                    1.0f};
        }

        static inline amal::vec4 hsv_to_rgba(f32 hue_deg, f32 saturation, f32 value_t, f32 alpha)
        {
            const f32 h =
                hue_deg < 0.0f ? hue_deg + 360.0f : (hue_deg >= 360.0f ? std::fmod(hue_deg, 360.0f) : hue_deg);
            const f32 s = amal::clamp(saturation, 0.0f, 1.0f);
            const f32 v = amal::clamp(value_t, 0.0f, 1.0f);

            if (s <= 1e-6f) return {v, v, v, alpha};

            const f32 c = v * s;
            const f32 hh = h / 60.0f;
            const f32 x = c * (1.0f - amal::abs(std::fmod(hh, 2.0f) - 1.0f));
            f32 r1 = 0.0f;
            f32 g1 = 0.0f;
            f32 b1 = 0.0f;
            if (hh < 1.0f)
            {
                r1 = c;
                g1 = x;
            }
            else if (hh < 2.0f)
            {
                r1 = x;
                g1 = c;
            }
            else if (hh < 3.0f)
            {
                g1 = c;
                b1 = x;
            }
            else if (hh < 4.0f)
            {
                g1 = x;
                b1 = c;
            }
            else if (hh < 5.0f)
            {
                r1 = x;
                b1 = c;
            }
            else
            {
                r1 = c;
                b1 = x;
            }

            const f32 m = v - c;
            return {r1 + m, g1 + m, b1 + m, alpha};
        }

        static inline void rgba_to_hsv(const amal::vec4 &color, f32 &hue_deg, f32 &saturation, f32 &value_t)
        {
            const f32 r = amal::clamp(color.x, 0.0f, 1.0f);
            const f32 g = amal::clamp(color.y, 0.0f, 1.0f);
            const f32 b = amal::clamp(color.z, 0.0f, 1.0f);
            const f32 max_c = amal::max(r, amal::max(g, b));
            const f32 min_c = amal::min(r, amal::min(g, b));
            const f32 delta = max_c - min_c;

            value_t = max_c;
            saturation = max_c > 1e-6f ? (delta / max_c) : 0.0f;
            if (delta <= 1e-6f) return;

            f32 h = 0.0f;
            if (max_c == r) h = 60.0f * std::fmod(((g - b) / delta), 6.0f);
            else if (max_c == g) h = 60.0f * (((b - r) / delta) + 2.0f);
            else h = 60.0f * (((r - g) / delta) + 4.0f);
            if (h < 0.0f) h += 360.0f;
            hue_deg = h;
        }

        static inline void append_hue_ring_vertices(acul::vector<VertexStreamVertex> &vertices,
                                                    const amal::vec2 &center, f32 radius, u32 segments, f32 alpha,
                                                    f32 z_order, u16 clip_id)
        {
            const f32 two_pi = amal::pi<f32>() * 2.0f;
            for (u32 i = 0; i <= segments; ++i)
            {
                const f32 t = static_cast<f32>(i) / static_cast<f32>(segments);
                const f32 angle = t * two_pi;
                const f32 hue_deg = t * 360.0f;
                const amal::vec4 hue = hsv_to_rgba(hue_deg, 1.0f, 1.0f, alpha);
                VertexStreamVertex v{};
                v.position = {center.x + amal::cos(angle) * radius, center.y + amal::sin(angle) * radius};
                v.z_order = z_order;
                v.color = detail::pack_rgba8(hue);
                v.clip_id = clip_id;
                vertices.push_back(v);
            }
        }

        static inline void append_ring_strip_indices(acul::vector<u32> &indices, u32 inner_start, u32 inner_segments,
                                                     u32 outer_start, u32 outer_segments)
        {
            u32 inner_i = 0u;
            u32 outer_i = 0u;
            while (inner_i < inner_segments || outer_i < outer_segments)
            {
                const f32 next_inner_t =
                    inner_i < inner_segments ? static_cast<f32>(inner_i + 1u) / static_cast<f32>(inner_segments) : 1.0f;
                const f32 next_outer_t =
                    outer_i < outer_segments ? static_cast<f32>(outer_i + 1u) / static_cast<f32>(outer_segments) : 1.0f;

                const u32 inner0 = inner_start + inner_i;
                const u32 outer0 = outer_start + outer_i;

                if (outer_i >= outer_segments || (inner_i < inner_segments && next_inner_t < next_outer_t - 1e-6f))
                {
                    const u32 inner1 = inner_start + inner_i + 1u;
                    indices.push_back(inner0);
                    indices.push_back(outer0);
                    indices.push_back(inner1);
                    ++inner_i;
                }
                else if (inner_i >= inner_segments || next_outer_t < next_inner_t - 1e-6f)
                {
                    const u32 outer1 = outer_start + outer_i + 1u;
                    indices.push_back(inner0);
                    indices.push_back(outer0);
                    indices.push_back(outer1);
                    ++outer_i;
                }
                else
                {
                    const u32 inner1 = inner_start + inner_i + 1u;
                    const u32 outer1 = outer_start + outer_i + 1u;
                    indices.push_back(inner0);
                    indices.push_back(outer0);
                    indices.push_back(inner1);
                    indices.push_back(inner1);
                    indices.push_back(outer0);
                    indices.push_back(outer1);
                    ++inner_i;
                    ++outer_i;
                }
            }
        }
    } // namespace detail

    CircleColorPicker::CircleColorPicker(u32 id, const amal::vec4 &value, f32 diameter, WidgetFlags widget_flags,
                                         Widget *parent)
        : Widget(id, widget_flags, EventFlagBits::click | EventFlagBits::drag, parent,
                 {{0.0f, 0.0f}, {diameter, diameter}}, AUIK_TAG_CIRCLE_COLOR_PICKER),
          _preferred_side(diameter),
          _value(value)
    {
        _grab_hit_rect = detail::make_rect_data(id, AUIK_TAG_CIRCLE_COLOR_PICKER_GRAB);
        _wheel_vertices.reserve(256);
        _wheel_indices.reserve(768);
        set_color(_value);
    }

    void CircleColorPicker::sync_batch()
    {
        _wheel_batch.vertices = _wheel_vertices.empty() ? nullptr : _wheel_vertices.data();
        _wheel_batch.indices = _wheel_indices.empty() ? nullptr : _wheel_indices.data();
        _wheel_batch.vertex_count = static_cast<u32>(_wheel_vertices.size());
        _wheel_batch.index_count = static_cast<u32>(_wheel_indices.size());
    }

    void CircleColorPicker::set_hue_radius(f32 hue_deg, f32 radius_t)
    {
        f32 hue = std::fmod(hue_deg, 360.0f);
        if (hue < 0.0f) hue += 360.0f;
        _hue_deg = hue;
        _radius_norm = amal::clamp(radius_t, 0.0f, 1.0f);
        _value = detail::make_circle_color(_hue_deg, _radius_norm);
    }

    void CircleColorPicker::set_color(const amal::vec4 &color)
    {
        const f32 r = amal::clamp(color.x, 0.0f, 1.0f);
        const f32 g = amal::clamp(color.y, 0.0f, 1.0f);
        const f32 b = amal::clamp(color.z, 0.0f, 1.0f);
        const f32 max_c = amal::max(r, amal::max(g, b));
        const f32 min_c = amal::min(r, amal::min(g, b));
        const f32 delta = max_c - min_c;

        f32 hue = _hue_deg;
        if (delta > 1e-6f)
        {
            if (max_c == r) hue = 60.0f * std::fmod(((g - b) / delta), 6.0f);
            else if (max_c == g) hue = 60.0f * (((b - r) / delta) + 2.0f);
            else hue = 60.0f * (((r - g) / delta) + 4.0f);
            if (hue < 0.0f) hue += 360.0f;
        }
        const f32 radius_t = amal::clamp(1.0f - min_c, 0.0f, 1.0f);
        set_hue_radius(hue, radius_t);
    }

    StyleUpdateFlags CircleColorPicker::update_style()
    {
        const u32 parent_id = parent() ? parent()->id() : 0u;
        StyleUpdateFlags out = StyleUpdateFlagBits::none;
        bool grab_changed = false;
        auto resolve_grab_state = [&](StyleState state) -> StyleUpdateFlags {
            const auto flags = resolve_style_selector(_grab_style, _grab_style.tag_id, parent_id,
                                                      detail::resolve_grab_visual_state(state));
            if (flags & StyleUpdateFlagBits::redraw) grab_changed = true;
            return flags;
        };

        if (_grab_style.id == Theme::STYLE_ID_INVALID) out |= resolve_grab_state(StyleState::normal);

        const auto transition = detail::get_widget_style_selector_transition(id());
        if (transition.prev_id.tag_id == _grab_hit_rect.id.tag_id &&
            (transition.current_id.tag_id != _grab_hit_rect.id.tag_id || transition.prev_state != transition.current_state))
            out |= resolve_grab_state(StyleState::normal);
        if (transition.current_id.tag_id == _grab_hit_rect.id.tag_id) out |= resolve_grab_state(transition.current_state);

        const StyleState widget_grab_state = detail::resolve_grab_visual_state(style_state());
        if (widget_grab_state == StyleState::active || widget_grab_state == StyleState::focus)
            out |= resolve_grab_state(widget_grab_state);

        if (grab_changed) rebuild_grab_visual();
        return out;
    }

    void CircleColorPicker::update_layout_min_size()
    {
        const Style *picker_style = get_theme()->get_desc_style(AUIK_TAG_CIRCLE_COLOR_PICKER);
        const amal::vec4 margin = picker_style ? picker_style->margin() : amal::vec4{0.0f};
        const f32 theme_side = detail::resolve_color_picker_size();
        const f32 side = amal::max(_preferred_side > 0.0f ? _preferred_side : theme_side, 1.0f);
        set_required_size({side + margin.x + margin.z, side + margin.y + margin.w});
    }

    void CircleColorPicker::update_layout(bool min_size_known)
    {
        if (!min_size_known) update_layout_min_size();
        const Style *picker_style = get_theme()->get_desc_style(AUIK_TAG_CIRCLE_COLOR_PICKER);
        const amal::vec4 margin = picker_style ? picker_style->margin() : amal::vec4{0.0f};
        const amal::vec2 layout_origin = position();
        const f32 theme_side = detail::resolve_color_picker_size();
        const f32 side = amal::max(_preferred_side > 0.0f ? _preferred_side : theme_side, 1.0f);
        const amal::vec2 next_pos = {layout_origin.x + margin.x, layout_origin.y + margin.y};

        set_position(next_pos);
        set_layout_size({side, side});
        set_required_size({side + margin.x + margin.z, side + margin.y + margin.w});
        Widget::update_layout(true);
        assert(parent() && "CircleColorPicker must have parent");
        set_clip_id(parent()->content_clip_id());
        _grab_hit_rect.clip_id = clip_id();
        rebuild_cached_visuals();
    }

    void CircleColorPicker::translate(const amal::vec2 &delta)
    {
        if (delta.x == 0.0f && delta.y == 0.0f) return;
        Widget::translate(delta);
        detail::get_context().dirty_flags |= DirtyFlagBits::hit_rect_update;
        if (!_cache_valid) rebuild_cached_visuals();
        else translate_cached_visuals(delta);
    }

    void CircleColorPicker::rebuild_clip_rects()
    {
        assert(parent() && "CircleColorPicker must have parent");
        set_clip_id(parent()->content_clip_id());
        _wheel_draw_id.hit_id = AUIK_INVALID_DRAW_DATA_ID;
        _grab_draw_id.hit_id = AUIK_INVALID_DRAW_DATA_ID;
        _grab_back_draw_id.hit_id = AUIK_INVALID_DRAW_DATA_ID;
        _grab_hit_rect.clip_id = clip_id();
        rebuild_cached_visuals();
    }

    void CircleColorPicker::update_depth(const amal::vec2 &depth_range)
    {
        Widget::update_depth(depth_range);
        assign_next_depth(this->depth_range(), _track_depth_range);
        assign_next_depth(_track_depth_range, _grab_depth_range);
        rebuild_cached_visuals();
    }

    void CircleColorPicker::back_hit_depth()
    {
        Widget::back_hit_depth();
        _grab_hit_rect.hit_depth = get_rect().hit_depth;
    }

    void CircleColorPicker::restore_hit_depth()
    {
        Widget::restore_hit_depth();
        _grab_hit_rect.hit_depth = _grab_hit_rect.depth;
    }

    void CircleColorPicker::draw(DrawCtx &ctx)
    {
        auto *vertex_stream = get_primary_vertex_stream();
        auto *overlay_stream = get_overlay_quads_stream();
        bool hit_pending = can_emit_hit(ctx);

        const bool wheel_visible = _wheel_batch.vertex_count > 0 && _wheel_batch.index_count > 0;
        if ((ctx.reason & DrawReasonBits::record) || wheel_visible || _wheel_draw_id.render_id != AUIK_INVALID_DRAW_DATA_ID)
        {
            emit_context_draw(ctx, vertex_stream, _wheel_draw_id, &_wheel_batch, get_rect(), hit_pending);
            hit_pending = false;
        }
        const bool grab_back_visible = _grab_back_visual.rect.size.x > 0.0f && _grab_back_visual.rect.size.y > 0.0f;
        if ((ctx.reason & DrawReasonBits::record) || grab_back_visible ||
            _grab_back_draw_id.render_id != AUIK_INVALID_DRAW_DATA_ID)
            emit_context_draw(ctx, overlay_stream, _grab_back_draw_id, &_grab_back_visual, _grab_hit_rect, false);
        emit_context_draw(ctx, overlay_stream, _grab_draw_id, &_grab_visual, _grab_hit_rect, can_emit_hit(ctx));
    }

    bool CircleColorPicker::has_draw_record() const
    {
        if (_wheel_draw_id.render_id == AUIK_INVALID_DRAW_DATA_ID) return false;
        if (_grab_draw_id.render_id == AUIK_INVALID_DRAW_DATA_ID) return false;
        if (_grab_back_draw_id.render_id == AUIK_INVALID_DRAW_DATA_ID) return false;
        return true;
    }

    amal::vec2 CircleColorPicker::resolve_grab_size(const Style &grab_style)
    {
        const amal::vec4 grab_padding = grab_style.padding();
        return {amal::max(grab_padding.x + grab_padding.z, 1.0f), amal::max(grab_padding.y + grab_padding.w, 1.0f)};
    }

    void CircleColorPicker::rebuild_wheel_visual()
    {
        _wheel_vertices.clear();
        _wheel_indices.clear();
        sync_batch();

        const f32 wheel_radius_outer = _layout.wheel_radius_outer;
        const f32 wheel_radius = _layout.wheel_radius;
        if (wheel_radius <= 1e-4f) return;

        const amal::vec2 center = _layout.center;
        const u32 segments = detail::calc_stable_color_picker_segment_count();
        const u32 fringe_segments =
            detail::calc_stable_color_picker_segment_count(AUIK_CIRCLE_FRINGE_TESSELLATION_MAX_ERROR);
        constexpr bool use_fringe_aa = true;

        _wheel_vertices.reserve(1u + (segments + 1u) + (use_fringe_aa ? (fringe_segments + 1u) * 2u : 0u));
        _wheel_indices.reserve(segments * 3u + (use_fringe_aa ? (segments + fringe_segments) * 6u : 0u) +
                               (use_fringe_aa ? fringe_segments * 6u : 0u));

        VertexStreamVertex center_v{};
        center_v.position = center;
        center_v.z_order = next_depth(_track_depth_range);
        center_v.color = detail::pack_rgba8(255, 255, 255, 255);
        center_v.clip_id = clip_id();
        _wheel_vertices.push_back(center_v);

        const f32 two_pi = amal::pi<f32>() * 2.0f;
        const f32 fringe_mid_radius = wheel_radius_outer - AUIK_CIRCLE_WHEEL_FRINGE * 0.5f;
        const u32 color_start = 1u;
        const u32 fringe_mid_start = color_start + (segments + 1u);
        const u32 fringe_outer_start = fringe_mid_start + (fringe_segments + 1u);
        for (u32 i = 0; i <= segments; ++i)
        {
            const f32 t = static_cast<f32>(i) / static_cast<f32>(segments);
            const f32 angle = t * two_pi;
            const f32 hue_deg = t * 360.0f;
            const amal::vec4 hue_color = amal::hsl_to_rgba(hue_deg, 1.0f, 0.5f);

            VertexStreamVertex inner{};
            inner.position = {center.x + amal::cos(angle) * wheel_radius, center.y + amal::sin(angle) * wheel_radius};
            inner.z_order = next_depth(_track_depth_range);
            inner.color = detail::pack_rgba8(hue_color);
            inner.clip_id = clip_id();
            _wheel_vertices.push_back(inner);
        }

        if (use_fringe_aa)
        {
            for (u32 i = 0; i <= fringe_segments; ++i)
            {
                const f32 t = static_cast<f32>(i) / static_cast<f32>(fringe_segments);
                const f32 angle = t * two_pi;
                const f32 hue_deg = t * 360.0f;
                const amal::vec4 hue_color = amal::hsl_to_rgba(hue_deg, 1.0f, 0.5f);

                VertexStreamVertex mid{};
                mid.position = {center.x + amal::cos(angle) * fringe_mid_radius,
                                center.y + amal::sin(angle) * fringe_mid_radius};
                mid.z_order = next_depth(_track_depth_range);
                mid.color = detail::pack_rgba8(amal::vec4{hue_color.x, hue_color.y, hue_color.z, 0.35f});
                mid.clip_id = clip_id();
                _wheel_vertices.push_back(mid);
            }

            for (u32 i = 0; i <= fringe_segments; ++i)
            {
                const f32 t = static_cast<f32>(i) / static_cast<f32>(fringe_segments);
                const f32 angle = t * two_pi;
                const f32 hue_deg = t * 360.0f;
                const amal::vec4 hue_color = amal::hsl_to_rgba(hue_deg, 1.0f, 0.5f);

                VertexStreamVertex outer{};
                outer.position = {center.x + amal::cos(angle) * wheel_radius_outer,
                                  center.y + amal::sin(angle) * wheel_radius_outer};
                outer.z_order = next_depth(_track_depth_range);
                outer.color = detail::pack_rgba8(amal::vec4{hue_color.x, hue_color.y, hue_color.z, 0.0f});
                outer.clip_id = clip_id();
                _wheel_vertices.push_back(outer);
            }
        }

        for (u32 i = 1u; i <= segments; ++i)
        {
            _wheel_indices.push_back(0u);
            _wheel_indices.push_back(color_start + (i - 1u));
            _wheel_indices.push_back(color_start + i);
        }

        if (use_fringe_aa)
        {
            detail::append_ring_strip_indices(_wheel_indices, color_start, segments, fringe_mid_start, fringe_segments);

            for (u32 i = 0u; i < fringe_segments; ++i)
            {
                const u32 mid0 = fringe_mid_start + i;
                const u32 mid1 = fringe_mid_start + i + 1u;
                const u32 out0 = fringe_outer_start + i;
                const u32 out1 = fringe_outer_start + i + 1u;
                _wheel_indices.push_back(mid0);
                _wheel_indices.push_back(out0);
                _wheel_indices.push_back(mid1);
                _wheel_indices.push_back(mid1);
                _wheel_indices.push_back(out0);
                _wheel_indices.push_back(out1);
            }
        }
        sync_batch();
    }

    void CircleColorPicker::rebuild_grab_visual()
    {
        if (_grab_style.id == Theme::STYLE_ID_INVALID) return;
        const auto &grab_style = get_theme()->get_style(_grab_style.id);
        const amal::vec2 grab_size = resolve_grab_size(grab_style);
        const f32 grab_w = amal::max(amal::round(grab_size.x), 3.0f);
        const f32 grab_h = amal::max(amal::round(grab_size.y), 3.0f);

        const amal::vec2 center = _layout.center;
        const f32 wheel_radius = _layout.wheel_radius;
        const f32 angle = (_hue_deg / 180.0f) * amal::pi<f32>();
        const f32 pick_r = wheel_radius * amal::clamp(_radius_norm, 0.0f, 1.0f);
        const amal::vec2 pick_pos = {center.x + amal::cos(angle) * pick_r, center.y + amal::sin(angle) * pick_r};

        _grab_hit_rect.bounds.offset = {pick_pos.x - grab_w * 0.5f, pick_pos.y - grab_h * 0.5f};
        _grab_hit_rect.bounds.size = {grab_w, grab_h};
        _grab_hit_rect.depth = next_depth(_grab_depth_range);
        _grab_hit_rect.hit_depth = _grab_hit_rect.depth;

        const u16 grab_clip = clip_id();
        const f32 grab_z = next_depth(_grab_depth_range);
        _grab_visual = {};
        _grab_back_visual = {};
        detail::fill_circle_grab_instance(grab_style, _grab_hit_rect.bounds, grab_z, grab_clip,
                                          detail::pack_rgba8(_value), _grab_visual);

        if (const Style *border_style = get_theme()->get_desc_style(AUIK_STYLE_TAG_GRADIENT_SLIDER_GRAB_BORDER))
        {
            const amal::vec2 border_size = resolve_grab_size(*border_style);
            const f32 border_w = amal::max(amal::round(border_size.x), _grab_hit_rect.bounds.size.x);
            const f32 border_h = amal::max(amal::round(border_size.y), _grab_hit_rect.bounds.size.y);
            amal::rect border_rect{};
            border_rect.size = {border_w, border_h};
            border_rect.offset = {pick_pos.x - border_w * 0.5f, pick_pos.y - border_h * 0.5f};
            detail::fill_circle_grab_instance(*border_style, border_rect, grab_z, grab_clip,
                                               border_style->background_color(), _grab_back_visual);
        }
    }

    void CircleColorPicker::rebuild_layout_cache()
    {
        const amal::vec2 wheel_size = size();
        _layout.center = position() + wheel_size * 0.5f;
        _layout.wheel_radius_outer = amal::max(amal::min(wheel_size.x, wheel_size.y) * 0.5f, 0.0f);
        _layout.wheel_radius = amal::max(_layout.wheel_radius_outer - AUIK_CIRCLE_WHEEL_FRINGE, 0.0f);
    }

    void CircleColorPicker::rebuild_cached_visuals()
    {
        rebuild_layout_cache();
        rebuild_wheel_visual();
        rebuild_grab_visual();
        _cache_valid = true;
    }

    void CircleColorPicker::translate_cached_visuals(const amal::vec2 &delta)
    {
        if (delta.x == 0.0f && delta.y == 0.0f) return;
        _layout.center += delta;
        _grab_hit_rect.bounds.offset += delta;
        _grab_visual.rect.offset += delta;
        _grab_back_visual.rect.offset += delta;
        for (auto &vertex : _wheel_vertices)
            vertex.position += delta;
        sync_batch();
    }

    void CircleColorPicker::update_value_from_mouse()
    {
        if (_layout.wheel_radius <= 1e-5f) rebuild_layout_cache();
        const amal::vec2 center = _layout.center;
        const amal::vec2 mouse = get_mouse_pos();
        const f32 dx = mouse.x - center.x;
        const f32 dy = mouse.y - center.y;
        const f32 wheel_radius = amal::max(_layout.wheel_radius, 1e-5f);
        const f32 dist = amal::sqrt(dx * dx + dy * dy);
        f32 hue = _hue_deg;
        if (dist > 1e-5f)
        {
            hue = amal::atan2(dy, dx) * (180.0f / amal::pi<f32>());
            if (hue < 0.0f) hue += 360.0f;
        }
        const f32 radius_t = amal::clamp(dist / wheel_radius, 0.0f, 1.0f);
        set_hue_radius(hue, radius_t);
        detail::get_context().dirty_flags |= DirtyFlagBits::hit_rect_update;
        rebuild_grab_visual();
    }

    void CircleColorPicker::on_click(MouseKey key, KeyPressState state, u32 click_count)
    {
        (void)click_count;
        if (key != MouseKey::left) return;
        if (state == KeyPressState::press)
        {
            update_value_from_mouse();
            add_render_command<detail::ClickEventTraits>(this, [this]() { redraw_external(has_draw_record()); });
            return;
        }
        if (state == KeyPressState::release)
            add_render_command<detail::ClickEventTraits>(this, [this]() { redraw_external(has_draw_record()); });
    }

    void CircleColorPicker::on_drag(const amal::vec2 &delta, KeyPressState state)
    {
        (void)delta;
        if (state == KeyPressState::press || state == KeyPressState::repeat) update_value_from_mouse();
        else if (state == KeyPressState::release) {}
        else return;

        add_render_command<detail::DragEventTraits>(this, [this]() { redraw_external(has_draw_record()); });
    }

    GradientColorPicker::GradientColorPicker(u32 id, const amal::vec4 &value, const amal::vec2 &size,
                                             WidgetFlags widget_flags, Widget *parent)
        : Widget(id, widget_flags, EventFlagBits::click | EventFlagBits::drag, parent,
                 {{0.0f, 0.0f}, size}, AUIK_TAG_GRADIENT_COLOR_PICKER),
          _preferred_size(size)
    {
        _grab_hit_rect = detail::make_rect_data(id, AUIK_TAG_GRADIENT_COLOR_PICKER_GRAB);
        _gradient_vertices.reserve(8);
        _gradient_indices.reserve(12);
        set_color(value);
    }

    StyleUpdateFlags GradientColorPicker::update_style()
    {
        const u32 parent_id = parent() ? parent()->id() : 0u;
        StyleUpdateFlags out = StyleUpdateFlagBits::none;
        bool grab_changed = false;
        auto resolve_grab_state = [&](StyleState state) -> StyleUpdateFlags {
            const auto flags = resolve_style_selector(_grab_style, _grab_style.tag_id, parent_id,
                                                      detail::resolve_grab_visual_state(state));
            if (flags & StyleUpdateFlagBits::redraw) grab_changed = true;
            return flags;
        };

        if (_grab_style.id == Theme::STYLE_ID_INVALID) out |= resolve_grab_state(StyleState::normal);
        const StyleState widget_grab_state = detail::resolve_grab_visual_state(style_state());
        if (widget_grab_state == StyleState::active || widget_grab_state == StyleState::focus)
            out |= resolve_grab_state(widget_grab_state);

        if (grab_changed) rebuild_grab_visual();
        return out;
    }

    void GradientColorPicker::update_layout_min_size()
    {
        const Style *picker_style = get_theme()->get_desc_style(AUIK_TAG_GRADIENT_COLOR_PICKER);
        const amal::vec4 margin = picker_style ? picker_style->margin() : amal::vec4{0.0f};
        const f32 theme_side = detail::resolve_color_picker_size();
        const f32 w = amal::max(_preferred_size.x > 0.0f ? _preferred_size.x : theme_side, 1.0f);
        const f32 h = amal::max(_preferred_size.y > 0.0f ? _preferred_size.y : theme_side, 1.0f);
        set_required_size({w + margin.x + margin.z, h + margin.y + margin.w});
    }

    void GradientColorPicker::update_layout(bool min_size_known)
    {
        if (!min_size_known) update_layout_min_size();
        const Style *picker_style = get_theme()->get_desc_style(AUIK_TAG_GRADIENT_COLOR_PICKER);
        const amal::vec4 margin = picker_style ? picker_style->margin() : amal::vec4{0.0f};
        const amal::vec2 layout_origin = position();
        const amal::vec2 widget_size = {amal::max(required_size().x - margin.x - margin.z, 1.0f),
                                        amal::max(required_size().y - margin.y - margin.w, 1.0f)};
        const amal::vec2 next_pos = {layout_origin.x + margin.x, layout_origin.y + margin.y};

        set_position(next_pos);
        set_layout_size(widget_size);
        Widget::update_layout(true);
        assert(parent() && "GradientColorPicker must have parent");
        set_clip_id(parent()->content_clip_id());
        _grab_hit_rect.clip_id = clip_id();
        _gradient_rect = bounds();
        rebuild_cached_visuals();
    }

    void GradientColorPicker::translate(const amal::vec2 &delta)
    {
        if (delta.x == 0.0f && delta.y == 0.0f) return;
        Widget::translate(delta);
        detail::get_context().dirty_flags |= DirtyFlagBits::hit_rect_update;
        if (!_cache_valid) rebuild_cached_visuals();
        else translate_cached_visuals(delta);
    }

    void GradientColorPicker::rebuild_clip_rects()
    {
        assert(parent() && "GradientColorPicker must have parent");
        set_clip_id(parent()->content_clip_id());
        _gradient_draw_id.hit_id = AUIK_INVALID_DRAW_DATA_ID;
        _grab_draw_id.hit_id = AUIK_INVALID_DRAW_DATA_ID;
        _grab_back_draw_id.hit_id = AUIK_INVALID_DRAW_DATA_ID;
        _grab_hit_rect.clip_id = clip_id();
        rebuild_cached_visuals();
    }

    void GradientColorPicker::update_depth(const amal::vec2 &depth_range)
    {
        Widget::update_depth(depth_range);
        assign_next_depth(this->depth_range(), _gradient_depth_range);
        assign_next_depth(_gradient_depth_range, _overlay_depth_range);
        assign_next_depth(_overlay_depth_range, _grab_depth_range);
        rebuild_cached_visuals();
    }

    void GradientColorPicker::back_hit_depth()
    {
        Widget::back_hit_depth();
        _grab_hit_rect.hit_depth = get_rect().hit_depth;
    }

    void GradientColorPicker::restore_hit_depth()
    {
        Widget::restore_hit_depth();
        _grab_hit_rect.hit_depth = _grab_hit_rect.depth;
    }

    void GradientColorPicker::draw(DrawCtx &ctx)
    {
        auto *vertex_stream = get_primary_vertex_stream();
        auto *overlay_stream = get_overlay_quads_stream();
        bool hit_pending = can_emit_hit(ctx);

        const bool gradient_visible = _gradient_batch.vertex_count > 0 && _gradient_batch.index_count > 0;
        if ((ctx.reason & DrawReasonBits::record) || gradient_visible ||
            _gradient_draw_id.render_id != AUIK_INVALID_DRAW_DATA_ID)
        {
            emit_context_draw(ctx, vertex_stream, _gradient_draw_id, &_gradient_batch, get_rect(), hit_pending);
            hit_pending = false;
        }

        const bool back_visible = _grab_back_visual.rect.size.x > 0.0f && _grab_back_visual.rect.size.y > 0.0f;
        if ((ctx.reason & DrawReasonBits::record) || back_visible ||
            _grab_back_draw_id.render_id != AUIK_INVALID_DRAW_DATA_ID)
            emit_context_draw(ctx, overlay_stream, _grab_back_draw_id, &_grab_back_visual, _grab_hit_rect, false);

        emit_context_draw(ctx, overlay_stream, _grab_draw_id, &_grab_visual, _grab_hit_rect, can_emit_hit(ctx));
    }

    bool GradientColorPicker::has_draw_record() const
    {
        if (_gradient_draw_id.render_id == AUIK_INVALID_DRAW_DATA_ID) return false;
        if (_grab_draw_id.render_id == AUIK_INVALID_DRAW_DATA_ID) return false;
        if (_grab_back_draw_id.render_id == AUIK_INVALID_DRAW_DATA_ID) return false;
        return true;
    }

    void GradientColorPicker::set_hsv(f32 hue_deg, f32 saturation, f32 value_t)
    {
        f32 hue = std::fmod(hue_deg, 360.0f);
        if (hue < 0.0f) hue += 360.0f;
        _hue_deg = hue;
        _saturation = amal::clamp(saturation, 0.0f, 1.0f);
        _value_norm = amal::clamp(value_t, 0.0f, 1.0f);
        const f32 alpha = amal::clamp(_value.w, 0.0f, 1.0f);
        _value = detail::hsv_to_rgba(_hue_deg, _saturation, _value_norm, alpha);
    }

    void GradientColorPicker::set_color(const amal::vec4 &color)
    {
        const f32 alpha = amal::clamp(color.w, 0.0f, 1.0f);
        f32 hue = _hue_deg;
        f32 sat = _saturation;
        f32 val = _value_norm;
        detail::rgba_to_hsv(color, hue, sat, val);
        set_hsv(hue, sat, val);
        _value.w = alpha;
    }

    void GradientColorPicker::sync_batch()
    {
        _gradient_batch.vertices = _gradient_vertices.empty() ? nullptr : _gradient_vertices.data();
        _gradient_batch.indices = _gradient_indices.empty() ? nullptr : _gradient_indices.data();
        _gradient_batch.vertex_count = static_cast<u32>(_gradient_vertices.size());
        _gradient_batch.index_count = static_cast<u32>(_gradient_indices.size());
    }

    void GradientColorPicker::rebuild_gradient_visual()
    {
        _gradient_vertices.clear();
        _gradient_indices.clear();
        sync_batch();
        if (amal::is_rect_empty(_gradient_rect)) return;

        const amal::vec4 hue_color = detail::hsv_to_rgba(_hue_deg, 1.0f, 1.0f, 1.0f);
        const amal::vec4 white = {1.0f};
        const amal::vec4 black_a0 = {0.0f, 0.0f, 0.0f, 0.0f};
        const amal::vec4 black_a1 = {0.0f, 0.0f, 0.0f, 1.0f};
        const f32 z_base = next_depth(_gradient_depth_range);
        const f32 z_overlay = next_depth(_overlay_depth_range);
        const u16 cid = clip_id();
        _gradient_vertices.reserve(8);
        _gradient_indices.reserve(12);

        VertexStreamVertex v0{};
        v0.position = {_gradient_rect.offset.x, _gradient_rect.offset.y};
        v0.z_order = z_base;
        v0.color = detail::pack_rgba8(white);
        v0.clip_id = cid;
        VertexStreamVertex v1{};
        v1.position = {_gradient_rect.offset.x + _gradient_rect.size.x, _gradient_rect.offset.y};
        v1.z_order = z_base;
        v1.color = detail::pack_rgba8(hue_color);
        v1.clip_id = cid;
        VertexStreamVertex v2{};
        v2.position = {_gradient_rect.offset.x + _gradient_rect.size.x,
                       _gradient_rect.offset.y + _gradient_rect.size.y};
        v2.z_order = z_base;
        v2.color = detail::pack_rgba8(hue_color);
        v2.clip_id = cid;
        VertexStreamVertex v3{};
        v3.position = {_gradient_rect.offset.x, _gradient_rect.offset.y + _gradient_rect.size.y};
        v3.z_order = z_base;
        v3.color = detail::pack_rgba8(white);
        v3.clip_id = cid;
        _gradient_vertices.push_back(v0);
        _gradient_vertices.push_back(v1);
        _gradient_vertices.push_back(v2);
        _gradient_vertices.push_back(v3);
        _gradient_indices.push_back(0u);
        _gradient_indices.push_back(1u);
        _gradient_indices.push_back(2u);
        _gradient_indices.push_back(0u);
        _gradient_indices.push_back(2u);
        _gradient_indices.push_back(3u);

        VertexStreamVertex o0{};
        o0.position = {_gradient_rect.offset.x, _gradient_rect.offset.y};
        o0.z_order = z_overlay;
        o0.color = detail::pack_rgba8(black_a0);
        o0.clip_id = cid;
        VertexStreamVertex o1{};
        o1.position = {_gradient_rect.offset.x + _gradient_rect.size.x, _gradient_rect.offset.y};
        o1.z_order = z_overlay;
        o1.color = detail::pack_rgba8(black_a0);
        o1.clip_id = cid;
        VertexStreamVertex o2{};
        o2.position = {_gradient_rect.offset.x + _gradient_rect.size.x,
                       _gradient_rect.offset.y + _gradient_rect.size.y};
        o2.z_order = z_overlay;
        o2.color = detail::pack_rgba8(black_a1);
        o2.clip_id = cid;
        VertexStreamVertex o3{};
        o3.position = {_gradient_rect.offset.x, _gradient_rect.offset.y + _gradient_rect.size.y};
        o3.z_order = z_overlay;
        o3.color = detail::pack_rgba8(black_a1);
        o3.clip_id = cid;
        _gradient_vertices.push_back(o0);
        _gradient_vertices.push_back(o1);
        _gradient_vertices.push_back(o2);
        _gradient_vertices.push_back(o3);
        _gradient_indices.push_back(4u);
        _gradient_indices.push_back(5u);
        _gradient_indices.push_back(6u);
        _gradient_indices.push_back(4u);
        _gradient_indices.push_back(6u);
        _gradient_indices.push_back(7u);
        sync_batch();
    }

    amal::vec2 GradientColorPicker::resolve_grab_size(const Style &grab_style)
    {
        const amal::vec4 grab_padding = grab_style.padding();
        return {amal::max(grab_padding.x + grab_padding.z, 1.0f), amal::max(grab_padding.y + grab_padding.w, 1.0f)};
    }

    void GradientColorPicker::rebuild_grab_visual()
    {
        if (_grab_style.id == Theme::STYLE_ID_INVALID) return;
        const auto &grab_style = get_theme()->get_style(_grab_style.id);
        const amal::vec2 grab_size = resolve_grab_size(grab_style);
        const f32 grab_w = amal::max(amal::round(grab_size.x), 3.0f);
        const f32 grab_h = amal::max(amal::round(grab_size.y), 3.0f);
        const f32 grab_z = next_depth(_grab_depth_range);
        const u16 grab_clip = clip_id();

        const amal::vec2 grab_pos = {_gradient_rect.offset.x + _gradient_rect.size.x * _saturation,
                                     _gradient_rect.offset.y + _gradient_rect.size.y * (1.0f - _value_norm)};

        _grab_hit_rect.bounds.offset = {grab_pos.x - grab_w * 0.5f, grab_pos.y - grab_h * 0.5f};
        _grab_hit_rect.bounds.size = {grab_w, grab_h};
        _grab_hit_rect.depth = grab_z;
        _grab_hit_rect.hit_depth = _grab_hit_rect.depth;

        _grab_visual = {};
        _grab_back_visual = {};
        detail::fill_circle_grab_instance(grab_style, _grab_hit_rect.bounds, grab_z, grab_clip,
                                          detail::pack_rgba8(_value), _grab_visual);

        if (const Style *border_style = get_theme()->get_desc_style(AUIK_STYLE_TAG_GRADIENT_SLIDER_GRAB_BORDER))
        {
            const amal::vec2 border_size = resolve_grab_size(*border_style);
            const f32 border_w = amal::max(amal::round(border_size.x), grab_w);
            const f32 border_h = amal::max(amal::round(border_size.y), grab_h);
            amal::rect border_rect{};
            border_rect.size = {border_w, border_h};
            border_rect.offset = {grab_pos.x - border_w * 0.5f, grab_pos.y - border_h * 0.5f};
            detail::fill_circle_grab_instance(*border_style, border_rect, grab_z, grab_clip,
                                              border_style->background_color(), _grab_back_visual);
        }
    }

    void GradientColorPicker::rebuild_cached_visuals()
    {
        _gradient_rect = bounds();
        rebuild_gradient_visual();
        rebuild_grab_visual();
        _cache_valid = true;
    }

    void GradientColorPicker::translate_cached_visuals(const amal::vec2 &delta)
    {
        if (delta.x == 0.0f && delta.y == 0.0f) return;
        _gradient_rect.offset += delta;
        _grab_hit_rect.bounds.offset += delta;
        _grab_visual.rect.offset += delta;
        _grab_back_visual.rect.offset += delta;
        for (auto &vertex : _gradient_vertices)
            vertex.position += delta;
        sync_batch();
    }

    void GradientColorPicker::update_value_from_mouse()
    {
        const amal::vec2 mouse = get_mouse_pos();
        const f32 s = amal::clamp((mouse.x - _gradient_rect.offset.x) / amal::max(_gradient_rect.size.x, 1e-5f),
                                  0.0f, 1.0f);
        const f32 v = amal::clamp(1.0f - (mouse.y - _gradient_rect.offset.y) / amal::max(_gradient_rect.size.y, 1e-5f),
                                  0.0f, 1.0f);
        set_hsv(_hue_deg, s, v);

        detail::get_context().dirty_flags |= DirtyFlagBits::hit_rect_update;
        rebuild_grab_visual();
        if (!mark_changed()) redraw_external(has_draw_record());
    }

    void GradientColorPicker::on_click(MouseKey key, KeyPressState state, u32 click_count)
    {
        (void)click_count;
        if (key != MouseKey::left) return;
        if (state == KeyPressState::press)
        {
            update_value_from_mouse();
            add_render_command<detail::ClickEventTraits>(this, [this]() { redraw_external(has_draw_record()); });
            return;
        }
        if (state == KeyPressState::release)
            add_render_command<detail::ClickEventTraits>(this, [this]() { redraw_external(has_draw_record()); });
    }

    void GradientColorPicker::on_drag(const amal::vec2 &delta, KeyPressState state)
    {
        (void)delta;
        if (state == KeyPressState::press || state == KeyPressState::repeat) update_value_from_mouse();
        else if (state == KeyPressState::release) {}
        else return;

        add_render_command<detail::DragEventTraits>(this, [this]() { redraw_external(has_draw_record()); });
    }

    SquareColorPicker::SquareColorPicker(u32 id, const amal::vec4 &value, f32 size, WidgetFlags widget_flags, Widget *parent)
        : Widget(id, widget_flags, EventFlagBits::click | EventFlagBits::drag, parent,
                 {{0.0f, 0.0f}, {size, size}},
                 AUIK_TAG_SQUARE_COLOR_PICKER),
          _preferred_side(size)
    {
        _ring_grab_hit_rect = detail::make_rect_data(id, AUIK_TAG_SQUARE_COLOR_PICKER_GRAB);
        _gradient = acul::alloc<GradientColorPicker>(id, value, amal::vec2{0.0f, 0.0f},
                                                     WidgetFlagBits::visible, this);
        _ring_vertices.reserve(384);
        _ring_indices.reserve(1152);
        _hue_deg = _gradient->hue_deg();
    }

    SquareColorPicker::~SquareColorPicker() { acul::release(_gradient); }

    StyleUpdateFlags SquareColorPicker::update_style()
    {
        const u32 parent_id = parent() ? parent()->id() : 0u;
        StyleUpdateFlags out = StyleUpdateFlagBits::none;
        bool grab_changed = false;
        auto resolve_grab_state = [&](StyleState state) -> StyleUpdateFlags {
            const auto flags = resolve_style_selector(_grab_style, _grab_style.tag_id, parent_id,
                                                      detail::resolve_grab_visual_state(state));
            if (flags & StyleUpdateFlagBits::redraw) grab_changed = true;
            return flags;
        };

        if (_grab_style.id == Theme::STYLE_ID_INVALID) out |= resolve_grab_state(StyleState::normal);
        const StyleState widget_grab_state = detail::resolve_grab_visual_state(style_state());
        if (widget_grab_state == StyleState::active || widget_grab_state == StyleState::focus)
            out |= resolve_grab_state(widget_grab_state);

        if (_gradient) out |= _gradient->update_style();
        if (grab_changed) rebuild_grab_visual();
        return out;
    }

    void SquareColorPicker::update_layout_min_size()
    {
        const Style *picker_style = get_theme()->get_desc_style(AUIK_TAG_SQUARE_COLOR_PICKER);
        const amal::vec4 margin = picker_style ? picker_style->margin() : amal::vec4{0.0f};
        const f32 theme_side = detail::resolve_color_picker_size();
        const f32 side = amal::max(_preferred_side > 0.0f ? _preferred_side : theme_side, 1.0f);
        set_required_size({side + margin.x + margin.z, side + margin.y + margin.w});
    }

    void SquareColorPicker::update_layout(bool min_size_known)
    {
        if (!min_size_known) update_layout_min_size();
        const Style *picker_style = get_theme()->get_desc_style(AUIK_TAG_SQUARE_COLOR_PICKER);
        const amal::vec4 margin = picker_style ? picker_style->margin() : amal::vec4{0.0f};
        const amal::vec2 layout_origin = position();
        const f32 theme_side = detail::resolve_color_picker_size();
        const f32 side = amal::max(_preferred_side > 0.0f ? _preferred_side : theme_side, 1.0f);
        const amal::vec2 next_pos = {layout_origin.x + margin.x, layout_origin.y + margin.y};

        set_position(next_pos);
        set_layout_size({side, side});
        set_required_size({side + margin.x + margin.z, side + margin.y + margin.w});
        Widget::update_layout(true);
        assert(parent() && "SquareColorPicker must have parent");
        set_clip_id(parent()->content_clip_id());
        _ring_grab_hit_rect.clip_id = clip_id();
        rebuild_cached_visuals();
    }

    void SquareColorPicker::translate(const amal::vec2 &delta)
    {
        if (delta.x == 0.0f && delta.y == 0.0f) return;
        Widget::translate(delta);
        detail::get_context().dirty_flags |= DirtyFlagBits::hit_rect_update;
        if (!_cache_valid) rebuild_cached_visuals();
        else translate_cached_visuals(delta);
    }

    void SquareColorPicker::rebuild_clip_rects()
    {
        assert(parent() && "SquareColorPicker must have parent");
        set_clip_id(parent()->content_clip_id());
        _ring_draw_id.hit_id = AUIK_INVALID_DRAW_DATA_ID;
        _ring_grab_draw_id.hit_id = AUIK_INVALID_DRAW_DATA_ID;
        _ring_grab_back_draw_id.hit_id = AUIK_INVALID_DRAW_DATA_ID;
        _ring_grab_hit_rect.clip_id = clip_id();
        if (_gradient) _gradient->rebuild_clip_rects();
        rebuild_cached_visuals();
    }

    void SquareColorPicker::update_depth(const amal::vec2 &depth_range)
    {
        Widget::update_depth(depth_range);
        assign_next_depth(this->depth_range(), _ring_depth_range);
        amal::vec2 gradient_depth_range{};
        assign_next_depth(_ring_depth_range, gradient_depth_range);
        if (_gradient) _gradient->update_depth(gradient_depth_range);
        assign_next_depth(gradient_depth_range, _grab_depth_range);
        rebuild_cached_visuals();
    }

    void SquareColorPicker::back_hit_depth()
    {
        Widget::back_hit_depth();
        _ring_grab_hit_rect.hit_depth = get_rect().hit_depth;
        if (_gradient) _gradient->back_hit_depth();
    }

    void SquareColorPicker::restore_hit_depth()
    {
        Widget::restore_hit_depth();
        _ring_grab_hit_rect.hit_depth = _ring_grab_hit_rect.depth;
        if (_gradient) _gradient->restore_hit_depth();
    }

    void SquareColorPicker::draw(DrawCtx &ctx)
    {
        auto *vertex_stream = get_primary_vertex_stream();
        auto *overlay_stream = get_overlay_quads_stream();
        bool hit_pending = can_emit_hit(ctx);

        const bool ring_visible = _ring_batch.vertex_count > 0 && _ring_batch.index_count > 0;
        if ((ctx.reason & DrawReasonBits::record) || ring_visible || _ring_draw_id.render_id != AUIK_INVALID_DRAW_DATA_ID)
        {
            emit_context_draw(ctx, vertex_stream, _ring_draw_id, &_ring_batch, get_rect(), hit_pending);
            hit_pending = false;
        }
        if (_gradient) _gradient->draw(ctx);

        const bool ring_back_visible =
            _ring_grab_back_visual.rect.size.x > 0.0f && _ring_grab_back_visual.rect.size.y > 0.0f;
        if ((ctx.reason & DrawReasonBits::record) || ring_back_visible ||
            _ring_grab_back_draw_id.render_id != AUIK_INVALID_DRAW_DATA_ID)
            emit_context_draw(ctx, overlay_stream, _ring_grab_back_draw_id, &_ring_grab_back_visual, _ring_grab_hit_rect, false);

        emit_context_draw(ctx, overlay_stream, _ring_grab_draw_id, &_ring_grab_visual, _ring_grab_hit_rect, can_emit_hit(ctx));
    }

    bool SquareColorPicker::has_draw_record() const
    {
        if (_ring_draw_id.render_id == AUIK_INVALID_DRAW_DATA_ID) return false;
        if (_ring_grab_draw_id.render_id == AUIK_INVALID_DRAW_DATA_ID) return false;
        if (_ring_grab_back_draw_id.render_id == AUIK_INVALID_DRAW_DATA_ID) return false;
        return _gradient ? _gradient->has_draw_record() : true;
    }

    void SquareColorPicker::set_hsv(f32 hue_deg, f32 saturation, f32 value_t)
    {
        f32 hue = std::fmod(hue_deg, 360.0f);
        if (hue < 0.0f) hue += 360.0f;
        _hue_deg = hue;
        if (_gradient) _gradient->set_hsv(_hue_deg, saturation, value_t);
    }

    void SquareColorPicker::set_color(const amal::vec4 &color)
    {
        if (!_gradient) return;
        _gradient->set_color(color);
        _hue_deg = _gradient->hue_deg();
    }

    const amal::vec4 &SquareColorPicker::color() const
    {
        static const amal::vec4 fallback{1.0f, 0.0f, 0.0f, 1.0f};
        return _gradient ? _gradient->color() : fallback;
    }

    f32 SquareColorPicker::saturation() const { return _gradient ? _gradient->saturation() : 1.0f; }

    f32 SquareColorPicker::value_norm() const { return _gradient ? _gradient->value_norm() : 1.0f; }

    void SquareColorPicker::sync_batches()
    {
        _ring_batch.vertices = _ring_vertices.empty() ? nullptr : _ring_vertices.data();
        _ring_batch.indices = _ring_indices.empty() ? nullptr : _ring_indices.data();
        _ring_batch.vertex_count = static_cast<u32>(_ring_vertices.size());
        _ring_batch.index_count = static_cast<u32>(_ring_indices.size());
    }

    void SquareColorPicker::rebuild_layout_geometry()
    {
        const amal::vec2 picker_size = size();
        const f32 side = amal::max(amal::min(picker_size.x, picker_size.y), 0.0f);
        _layout.center = position() + picker_size * 0.5f;

        _layout.ring_outer_radius = side * 0.5f;
        const f32 ring_width = amal::clamp(side * AUIK_SQUARE_PICKER_RING_WIDTH_FACTOR,
                                           AUIK_SQUARE_PICKER_RING_WIDTH_MIN, AUIK_SQUARE_PICKER_RING_WIDTH_MAX);
        _layout.ring_inner_radius = amal::max(_layout.ring_outer_radius - ring_width, 1.0f);

        const f32 inner_for_square = amal::max(_layout.ring_inner_radius - AUIK_SQUARE_PICKER_INNER_PADDING, 1.0f);
        const f32 half_square = inner_for_square * 0.70710678f;
        _layout.sv_rect.offset = {_layout.center.x - half_square, _layout.center.y - half_square};
        _layout.sv_rect.size = {half_square * 2.0f, half_square * 2.0f};
    }

    void SquareColorPicker::rebuild_ring_visual()
    {
        _ring_vertices.clear();
        _ring_indices.clear();
        sync_batches();

        if (_layout.ring_outer_radius <= 1e-4f || _layout.ring_inner_radius <= 1e-4f ||
            _layout.ring_outer_radius <= _layout.ring_inner_radius)
            return;

        const f32 fringe =
            amal::min(AUIK_SQUARE_PICKER_RING_FRINGE, (_layout.ring_outer_radius - _layout.ring_inner_radius) * 0.49f);
        const f32 main_inner = _layout.ring_inner_radius + fringe;
        const f32 main_outer = _layout.ring_outer_radius - fringe;
        if (main_outer <= main_inner) return;

        const u32 main_segments = detail::calc_stable_color_picker_segment_count();
        const u32 fringe_segments =
            detail::calc_stable_color_picker_segment_count(AUIK_CIRCLE_FRINGE_TESSELLATION_MAX_ERROR);

        const u32 inner_fade_start = 0u;
        const u32 inner_main_start = inner_fade_start + (fringe_segments + 1u);
        const u32 outer_main_start = inner_main_start + (main_segments + 1u);
        const u32 outer_fade_start = outer_main_start + (main_segments + 1u);

        _ring_vertices.reserve((fringe_segments + 1u) + (main_segments + 1u) + (main_segments + 1u) +
                               (fringe_segments + 1u));
        _ring_indices.reserve((main_segments + fringe_segments + fringe_segments) * 6u);

        const f32 z = next_depth(_ring_depth_range);
        const u16 cid = clip_id();
        detail::append_hue_ring_vertices(_ring_vertices, _layout.center, _layout.ring_inner_radius, fringe_segments, 0.0f,
                                         z, cid);
        detail::append_hue_ring_vertices(_ring_vertices, _layout.center, main_inner, main_segments, 1.0f, z, cid);
        detail::append_hue_ring_vertices(_ring_vertices, _layout.center, main_outer, main_segments, 1.0f, z, cid);
        detail::append_hue_ring_vertices(_ring_vertices, _layout.center, _layout.ring_outer_radius, fringe_segments,
                                         0.0f, z, cid);

        detail::append_ring_strip_indices(_ring_indices, inner_fade_start, fringe_segments, inner_main_start,
                                          main_segments);
        detail::append_ring_strip_indices(_ring_indices, inner_main_start, main_segments, outer_main_start,
                                          main_segments);
        detail::append_ring_strip_indices(_ring_indices, outer_main_start, main_segments, outer_fade_start,
                                          fringe_segments);
        sync_batches();
    }

    amal::vec2 SquareColorPicker::resolve_grab_size(const Style &grab_style)
    {
        const amal::vec4 grab_padding = grab_style.padding();
        return {amal::max(grab_padding.x + grab_padding.z, 1.0f), amal::max(grab_padding.y + grab_padding.w, 1.0f)};
    }

    void SquareColorPicker::rebuild_grab_visual()
    {
        if (_grab_style.id == Theme::STYLE_ID_INVALID) return;
        const auto &grab_style = get_theme()->get_style(_grab_style.id);
        const amal::vec2 grab_size = resolve_grab_size(grab_style);
        const f32 grab_w = amal::max(amal::round(grab_size.x), 3.0f);
        const f32 grab_h = amal::max(amal::round(grab_size.y), 3.0f);
        const f32 grab_z = next_depth(_grab_depth_range);
        const u16 grab_clip = clip_id();

        const f32 ring_mid = (_layout.ring_inner_radius + _layout.ring_outer_radius) * 0.5f;
        const f32 ring_angle = (_hue_deg / 180.0f) * amal::pi<f32>();
        const amal::vec2 ring_pos = {_layout.center.x + amal::cos(ring_angle) * ring_mid,
                                     _layout.center.y + amal::sin(ring_angle) * ring_mid};

        _ring_grab_hit_rect.bounds.offset = {ring_pos.x - grab_w * 0.5f, ring_pos.y - grab_h * 0.5f};
        _ring_grab_hit_rect.bounds.size = {grab_w, grab_h};
        _ring_grab_hit_rect.depth = grab_z;
        _ring_grab_hit_rect.hit_depth = _ring_grab_hit_rect.depth;

        _ring_grab_visual = {};
        _ring_grab_back_visual = {};
        detail::fill_circle_grab_instance(grab_style, _ring_grab_hit_rect.bounds, grab_z, grab_clip,
                                          detail::pack_rgba8(detail::hsv_to_rgba(_hue_deg, 1.0f, 1.0f, 1.0f)),
                                          _ring_grab_visual);

        if (const Style *border_style = get_theme()->get_desc_style(AUIK_STYLE_TAG_GRADIENT_SLIDER_GRAB_BORDER))
        {
            const amal::vec2 border_size = resolve_grab_size(*border_style);
            const f32 border_w = amal::max(amal::round(border_size.x), grab_w);
            const f32 border_h = amal::max(amal::round(border_size.y), grab_h);
            amal::rect ring_border_rect{};
            ring_border_rect.size = {border_w, border_h};
            ring_border_rect.offset = {ring_pos.x - border_w * 0.5f, ring_pos.y - border_h * 0.5f};
            detail::fill_circle_grab_instance(*border_style, ring_border_rect, grab_z, grab_clip,
                                              border_style->background_color(), _ring_grab_back_visual);
        }
    }

    void SquareColorPicker::rebuild_cached_visuals()
    {
        rebuild_layout_geometry();
        rebuild_ring_visual();
        if (_gradient)
        {
            _gradient->set_position(_layout.sv_rect.offset);
            _gradient->set_layout_size(_layout.sv_rect.size);
            _gradient->set_required_size(_layout.sv_rect.size);
            _gradient->set_clip_id(clip_id());
            _gradient->update_layout(true);
        }
        rebuild_grab_visual();
        _cache_valid = true;
    }

    void SquareColorPicker::translate_cached_visuals(const amal::vec2 &delta)
    {
        if (delta.x == 0.0f && delta.y == 0.0f) return;
        _layout.center += delta;
        _layout.sv_rect.offset += delta;
        _ring_grab_hit_rect.bounds.offset += delta;
        _ring_grab_visual.rect.offset += delta;
        _ring_grab_back_visual.rect.offset += delta;
        for (auto &vertex : _ring_vertices)
            vertex.position += delta;
        if (_gradient) _gradient->translate(delta);
        sync_batches();
    }

    SquareColorPicker::ActiveZone SquareColorPicker::pick_active_zone_from_mouse() const
    {
        const amal::vec2 mouse = get_mouse_pos();
        if (mouse.x >= _layout.sv_rect.offset.x && mouse.y >= _layout.sv_rect.offset.y &&
            mouse.x <= _layout.sv_rect.offset.x + _layout.sv_rect.size.x &&
            mouse.y <= _layout.sv_rect.offset.y + _layout.sv_rect.size.y)
            return ActiveZone::square;

        const f32 dx = mouse.x - _layout.center.x;
        const f32 dy = mouse.y - _layout.center.y;
        const f32 d2 = dx * dx + dy * dy;
        const f32 inner2 = _layout.ring_inner_radius * _layout.ring_inner_radius;
        const f32 outer2 = _layout.ring_outer_radius * _layout.ring_outer_radius;
        if (d2 >= inner2 && d2 <= outer2) return ActiveZone::ring;
        return ActiveZone::none;
    }

    void SquareColorPicker::update_value_from_mouse()
    {
        const amal::vec2 mouse = get_mouse_pos();
        if (_active_zone == ActiveZone::ring)
        {
            const f32 dx = mouse.x - _layout.center.x;
            const f32 dy = mouse.y - _layout.center.y;
            f32 hue = amal::atan2(dy, dx) * (180.0f / amal::pi<f32>());
            if (hue < 0.0f) hue += 360.0f;
            set_hsv(hue, saturation(), value_norm());
        }
        else if (_active_zone == ActiveZone::square)
        {
            const f32 s = amal::clamp((mouse.x - _layout.sv_rect.offset.x) / amal::max(_layout.sv_rect.size.x, 1e-5f),
                                      0.0f, 1.0f);
            const f32 v = amal::clamp(1.0f - (mouse.y - _layout.sv_rect.offset.y) / amal::max(_layout.sv_rect.size.y, 1e-5f),
                                      0.0f, 1.0f);
            set_hsv(_hue_deg, s, v);
        }
        else return;

        detail::get_context().dirty_flags |= DirtyFlagBits::hit_rect_update;
        if (_gradient) _gradient->update_layout(true);
        rebuild_grab_visual();
    }

    void SquareColorPicker::on_click(MouseKey key, KeyPressState state, u32 click_count)
    {
        (void)click_count;
        if (key != MouseKey::left) return;

        if (state == KeyPressState::press)
        {
            _active_zone = pick_active_zone_from_mouse();
            update_value_from_mouse();
            add_render_command<detail::ClickEventTraits>(this, [this]() { redraw_external(has_draw_record()); });
            return;
        }

        if (state == KeyPressState::release)
        {
            _active_zone = ActiveZone::none;
            add_render_command<detail::ClickEventTraits>(this, [this]() { redraw_external(has_draw_record()); });
        }
    }

    void SquareColorPicker::on_drag(const amal::vec2 &delta, KeyPressState state)
    {
        (void)delta;
        if (state == KeyPressState::press)
        {
            _active_zone = pick_active_zone_from_mouse();
            update_value_from_mouse();
        }
        else if (state == KeyPressState::repeat)
        {
            if (_active_zone == ActiveZone::none) _active_zone = pick_active_zone_from_mouse();
            update_value_from_mouse();
        }
        else if (state == KeyPressState::release) _active_zone = ActiveZone::none;
        else return;

        add_render_command<detail::DragEventTraits>(this, [this]() { redraw_external(has_draw_record()); });
    }

    namespace
    {
        void write_circle_color_picker(acul::bin_stream &stream, umbf::Block *block)
        {
            auto *widget = static_cast<CircleColorPicker *>(block);
            detail::write_widget_common_data(stream, *widget);
            stream.write(widget->color()).write(widget->hue_deg()).write(widget->radius_norm());
        }

        umbf::Block *read_circle_color_picker(acul::bin_stream &stream)
        {
            const auto common = detail::read_widget_common_data(stream);
            amal::vec4 color{1.0f};
            f32 hue = 0.0f;
            f32 radius = 0.0f;
            stream.read(color).read(hue).read(radius);

            const f32 diameter = common.requested_size.x > 0.0f ? common.requested_size.x : common.requested_size.y;
            auto *widget = acul::alloc<CircleColorPicker>(common.id, color, diameter,
                                                          WidgetFlags(common.widget_flags), nullptr);
            widget->set_hue_radius(hue, radius);
            detail::apply_widget_common_data(widget, common);
            return widget;
        }

        void write_gradient_color_picker(acul::bin_stream &stream, umbf::Block *block)
        {
            auto *widget = static_cast<GradientColorPicker *>(block);
            detail::write_widget_common_data(stream, *widget);
            stream.write(widget->color())
                .write(widget->hue_deg())
                .write(widget->saturation())
                .write(widget->value_norm());
        }

        umbf::Block *read_gradient_color_picker(acul::bin_stream &stream)
        {
            const auto common = detail::read_widget_common_data(stream);
            amal::vec4 color{1.0f, 0.0f, 0.0f, 1.0f};
            f32 hue = 0.0f;
            f32 saturation = 1.0f;
            f32 value = 1.0f;
            stream.read(color).read(hue).read(saturation).read(value);

            auto *widget = acul::alloc<GradientColorPicker>(common.id, color, common.requested_size,
                                                            WidgetFlags(common.widget_flags), nullptr);
            widget->set_hsv(hue, saturation, value);
            detail::apply_widget_common_data(widget, common);
            return widget;
        }

        void write_square_color_picker(acul::bin_stream &stream, umbf::Block *block)
        {
            auto *widget = static_cast<SquareColorPicker *>(block);
            detail::write_widget_common_data(stream, *widget);
            stream.write(widget->color())
                .write(widget->hue_deg())
                .write(widget->saturation())
                .write(widget->value_norm());
        }

        umbf::Block *read_square_color_picker(acul::bin_stream &stream)
        {
            const auto common = detail::read_widget_common_data(stream);
            amal::vec4 color{1.0f, 0.0f, 0.0f, 1.0f};
            f32 hue = 0.0f;
            f32 saturation = 1.0f;
            f32 value = 1.0f;
            stream.read(color).read(hue).read(saturation).read(value);

            const f32 size = common.requested_size.x > 0.0f ? common.requested_size.x : common.requested_size.y;
            auto *widget = acul::alloc<SquareColorPicker>(common.id, color, size,
                                                          WidgetFlags(common.widget_flags), nullptr);
            widget->set_hsv(hue, saturation, value);
            detail::apply_widget_common_data(widget, common);
            return widget;
        }
    } // namespace

    namespace streams
    {
        AUIK_EXPORT const umbf::streams::Stream circle_color_picker{read_circle_color_picker, write_circle_color_picker};
        AUIK_EXPORT const umbf::streams::Stream gradient_color_picker{read_gradient_color_picker, write_gradient_color_picker};
        AUIK_EXPORT const umbf::streams::Stream square_color_picker{read_square_color_picker, write_square_color_picker};
    } // namespace streams
} // namespace auik

