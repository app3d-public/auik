#pragma once

#include "draw.hpp"
#include "theme.hpp"

namespace auik::v2
{
    struct QuadsInstanceData
    {
        amal::vec2 position;
        amal::vec2 size;
        amal::vec4 background_color;
        amal::vec4 border_color;
        f32 border_radius;
        f32 border_thickness;
        f32 z_order;
        u32 corner_mask;
    };

    APPLIB_API void create_quads_stream_retained(DrawStream &stream);
    APPLIB_API void create_quads_stream_immediate(DrawStream &stream);

    inline void fill_quads_instance_by_style(const Style &style, QuadsInstanceData &data)
    {
        data.background_color = style.background_color();
        data.border_color = style.border_color();
        data.border_radius = style.border_radius();
        data.border_thickness = style.border_thickness();
        data.corner_mask = style.corner_mask();
    }

    struct TexturesInstanceData
    {
        amal::vec2 position;
        amal::vec2 size;
        f32 z_order;
        u32 texture_id;
        amal::vec2 texture_offset;
    };
} // namespace auik::v2
