#pragma once

#include <auik/v2/pipelines.hpp>

namespace auik::v2::detail
{
    struct GradientRectVertexData
    {
        acul::vector<VertexStreamVertex> vertices;
        acul::vector<VertexStreamIndex> indices;
        VertexStreamBatchData batch{};

        void clear()
        {
            vertices.clear();
            indices.clear();
            batch = {};
        }

        void sync_batch()
        {
            batch.vertices = vertices.empty() ? nullptr : vertices.data();
            batch.indices = indices.empty() ? nullptr : indices.data();
            batch.vertex_count = static_cast<u32>(vertices.size());
            batch.index_count = static_cast<u32>(indices.size());
        }
    };

    inline amal::vec4 sample_gradient_color(const amal::vec4 *colors, u32 count, f32 t)
    {
        assert(colors && "colors is null");
        if (count == 0u) return {1.0f, 1.0f, 1.0f, 1.0f};
        if (count == 1u) return colors[0];

        const f32 clamped_t = amal::clamp(t, 0.0f, 1.0f);
        const f32 scaled = clamped_t * static_cast<f32>(count - 1u);
        const u32 left_index = static_cast<u32>(scaled);
        if (left_index >= count - 1u) return colors[count - 1u];

        const u32 right_index = left_index + 1u;
        const f32 local_t = scaled - static_cast<f32>(left_index);
        const amal::vec4 &a = colors[left_index];
        const amal::vec4 &b = colors[right_index];
        return {a.x + (b.x - a.x) * local_t, a.y + (b.y - a.y) * local_t, a.z + (b.z - a.z) * local_t,
                a.w + (b.w - a.w) * local_t};
    }

    inline f32 clamp_corner_rounding(const amal::rect &rect, f32 rounding, u32 corner_mask)
    {
        if (rounding < 0.5f || corner_mask == 0u) return 0.0f;

        const bool round_top = (corner_mask & 0x3u) != 0u;
        const bool round_bottom = (corner_mask & 0xCu) != 0u;
        const bool round_left = (corner_mask & 0x9u) != 0u;
        const bool round_right = (corner_mask & 0x6u) != 0u;

        rounding = amal::min(rounding, amal::abs(rect.size.x) * ((round_top || round_bottom) ? 0.5f : 1.0f) - 1.0f);
        rounding = amal::min(rounding, amal::abs(rect.size.y) * ((round_left || round_right) ? 0.5f : 1.0f) - 1.0f);
        return amal::max(rounding, 0.0f);
    }

    bool build_gradient_rect_vertex_data(GradientRectVertexData &vertex_data, const amal::rect &rect, f32 z_order,
                                         u16 clip_id, const amal::vec4 *colors, u32 color_count, f32 rounding = 0.0f,
                                         u32 corner_mask = 0xFu, f32 fringe_width = 1.0f);
} // namespace auik::v2::detail
