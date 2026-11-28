#pragma once

#include <algorithm>
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

    struct StrokeSample
    {
        f32 offset;
        u32 color;
    };

    inline constexpr u32 stroke_sample_count(f32 border_width) { return border_width > 0.f ? 8u : 2u; }

    inline void fill_stroke_samples(StrokeSample *samples, f32 width, u32 color, f32 border_width, u32 border_color)
    {
        const f32 inner = width * 0.5f;
        if (border_width <= 0.f)
        {
            samples[0] = {-inner, color};
            samples[1] = {inner, color};
            return;
        }

        const f32 outer = inner + border_width;
        const f32 solid_border = outer - std::min(border_width, 1.f);
        const u32 transparent = border_color & 0x00ffffffu;
        samples[0] = {-outer, transparent};
        samples[1] = {-solid_border, border_color};
        samples[2] = {-inner, border_color};
        samples[3] = {-inner, color};
        samples[4] = {inner, color};
        samples[5] = {inner, border_color};
        samples[6] = {solid_border, border_color};
        samples[7] = {outer, transparent};
    }

    inline void append_stroke_strip_indices(acul::vector<VertexStreamIndex> &indices, u32 first, u32 next, u32 count)
    {
        for (u32 j = 0; j + 1 < count; ++j)
            for (u32 index : {first + j, first + j + 1, next + j, next + j, first + j + 1, next + j + 1})
                indices.push_back(index);
    }

    inline constexpr u32 stroke_segment_sample_count(f32 border_width, u32 color, u32 border_color)
    {
        return border_width > 0.f && color == border_color ? 4u : stroke_sample_count(border_width);
    }

    inline void append_stroke_segment(acul::vector<VertexStreamVertex> &vertices,
                                      acul::vector<VertexStreamIndex> &indices, const amal::vec2 &from,
                                      const amal::vec2 &to, f32 width, f32 z_order, u32 color, u16 clip_id,
                                      f32 border_width = 0.f, u32 border_color = 0)
    {
        const auto delta = to - from;
        const auto length = amal::length(delta);
        if (length <= 1e-5f) return;
        const auto normal = amal::vec2{delta.y, -delta.x} / length;
        StrokeSample samples[8];
        const u32 count = stroke_segment_sample_count(border_width, color, border_color);
        if (count == 4)
        {
            const f32 outer = width * 0.5f + border_width;
            const f32 solid = outer - std::min(border_width, 1.f);
            const u32 transparent = color & 0x00ffffffu;
            samples[0] = {-outer, transparent};
            samples[1] = {-solid, color};
            samples[2] = {solid, color};
            samples[3] = {outer, transparent};
        }
        else fill_stroke_samples(samples, width, color, border_width, border_color);
        const u32 first = static_cast<u32>(vertices.size());
        for (auto point : {from, to})
            for (u32 j = 0; j < count; ++j)
                append_vertex(vertices, point + normal * samples[j].offset, z_order, samples[j].color, clip_id);
        append_stroke_strip_indices(indices, first, first + count, count);
    }
} // namespace auik::detail
