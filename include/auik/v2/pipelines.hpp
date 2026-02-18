#pragma once

#pragma once

#include "draw.hpp"

namespace auik::v2::detail
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

    APPLIB_API void create_quads_stream_retained(DrawPipeline *pipeline, DrawStream &stream);
    APPLIB_API void create_quads_stream_immediate(DrawPipeline *pipeline, DrawStream &stream);

    struct TexturesInstanceData
    {
        amal::vec2 position;
        amal::vec2 size;
        f32 z_order;
        u32 texture_id;
        amal::vec2 texture_offset;
    };
} // namespace auik::v2::detail