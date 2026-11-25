#pragma once

#include <amal/geometric.hpp>
#include <auik/pipelines.hpp>

namespace auik::detail
{
    inline void append_aa_strip_indices(acul::vector<VertexStreamIndex> &indices, u32 first, u32 next)
    {
        for (u32 band = 0u; band < 3u; ++band)
        {
            indices.push_back(first + band);
            indices.push_back(first + band + 1u);
            indices.push_back(next + band);
            indices.push_back(first + band + 1u);
            indices.push_back(next + band + 1u);
            indices.push_back(next + band);
        }
    }

    inline void append_vertex(acul::vector<VertexStreamVertex> &vertices, const amal::vec2 &position, f32 z_order,
                              u32 color, u16 clip_id)
    {
        VertexStreamVertex vertex{};
        vertex.position = position;
        vertex.z_order = z_order;
        vertex.color = color;
        vertex.clip_id = clip_id;
        vertices.push_back(vertex);
    }

    inline void append_segment(acul::vector<VertexStreamVertex> &vertices, acul::vector<VertexStreamIndex> &indices,
                               const amal::vec2 &from, const amal::vec2 &to, f32 thickness, f32 z_order, u32 color,
                               u16 clip_id)
    {
        const amal::vec2 delta = to - from;
        const f32 segment_length = amal::length(delta);
        if (segment_length <= 1e-5f) return;
        const amal::vec2 direction = delta / segment_length;
        const f32 half_width = thickness * 0.5f;
        const amal::vec2 inner = amal::vec2{-direction.y, direction.x} * half_width;
        const amal::vec2 outer = amal::vec2{-direction.y, direction.x} * (half_width + 1.0f);
        const u32 transparent_color = color & 0x00FFFFFFu;
        const u32 first = static_cast<u32>(vertices.size());
        append_vertex(vertices, from + outer, z_order, transparent_color, clip_id);
        append_vertex(vertices, from + inner, z_order, color, clip_id);
        append_vertex(vertices, from - inner, z_order, color, clip_id);
        append_vertex(vertices, from - outer, z_order, transparent_color, clip_id);
        append_vertex(vertices, to + outer, z_order, transparent_color, clip_id);
        append_vertex(vertices, to + inner, z_order, color, clip_id);
        append_vertex(vertices, to - inner, z_order, color, clip_id);
        append_vertex(vertices, to - outer, z_order, transparent_color, clip_id);
        append_aa_strip_indices(indices, first, first + 4u);
    }

} // namespace auik::detail
