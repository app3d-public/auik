#pragma once

#include "draw.hpp"
#include "theme.hpp"

#define AUIK_CLIP_RECT_BIT  0x1
#define AUIK_HAS_BORDER_BIT 0x2
#define AUIK_HAS_RADIUS_BIT 0x4

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
        struct
        {
            u32 boder_corner_mask : 4;
            u32 clip_rect_id : 16;
            u32 flags : 12;
        } mask;
    };

    static_assert(sizeof(QuadsInstanceData) == 64, "QuadsInstanceData must be exactly 64 bytes");

    APPLIB_API void create_quads_stream_cached(DrawStream &stream);
    APPLIB_API void create_quads_stream_transient(DrawStream &stream);

    inline void fill_quads_instance_by_style(const Style &style, QuadsInstanceData &data)
    {
        data.background_color = style.background_color();
        data.border_color = style.border_color();
        data.border_radius = style.border_radius();
        data.border_thickness = style.border_thickness();
        data.mask.clip_rect_id = 0;
        data.mask.boder_corner_mask = style.corner_mask();
        data.mask.flags = 0;
        if (data.border_thickness > 0.0f) data.mask.flags |= AUIK_HAS_BORDER_BIT;
        if (data.border_radius > 0.0f) data.mask.flags |= AUIK_HAS_RADIUS_BIT;
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
