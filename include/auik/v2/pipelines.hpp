#pragma once

#include <amal/rect.hpp>
#include "draw.hpp"
#include "theme.hpp"

#define AUIK_HAS_BORDER_BIT 0x1
#define AUIK_HAS_RADIUS_BIT 0x2
#define AUIK_TEXTURE_INSTANCE_TEXT_BIT 0x1

namespace auik::v2
{
    struct QuadsInstanceData
    {
        amal::rect rect;
        amal::vec4 background_color;
        amal::vec4 border_color;
        f32 border_radius;
        f32 border_thickness;
        f32 z_order;
        u16 clip_id;
        struct
        {
            u16 boder_corner_mask : 4;
            u16 flags : 12;
        } mask;
    };

    static_assert(sizeof(QuadsInstanceData) == 64, "QuadsInstanceData must be exactly 64 bytes");

    APPLIB_API void create_quads_stream(DrawStream &stream);

    inline void push_quads_batch_to_stream(DrawStream *stream, const QuadsInstanceData *data, u32 count,
                                           DrawDataID *out_draw_ids = nullptr)
    {
        push_data_batch_to_stream(stream, data, count, out_draw_ids);
    }

    inline void update_quads_batch_in_stream(DrawStream *stream, const DrawDataID *draw_data_ids,
                                             const QuadsInstanceData *data, u32 count)
    {
        update_data_batch_in_stream(stream, draw_data_ids, data, count);
    }

    inline void fill_quads_instance_by_style(const Style &style, u16 clip_id, QuadsInstanceData &data)
    {
        data.background_color = style.background_color();
        data.border_color = style.border_color();
        data.border_radius = style.border_radius();
        data.border_thickness = style.border_thickness();
        data.clip_id = clip_id;
        data.mask.boder_corner_mask = style.corner_mask();
        data.mask.flags = 0;
        if (data.border_thickness > 0.0f) data.mask.flags |= AUIK_HAS_BORDER_BIT;
        if (data.border_radius > 0.0f) data.mask.flags |= AUIK_HAS_RADIUS_BIT;
    }

    struct TexturesInstanceData
    {
        amal::rect rect;
        amal::vec4 tint_color{0.0f};
        amal::rect uv_rect;
        f32 z_order;
        u16 texture_id;
        u16 clip_id;
        u32 flags = 0u;
        // std430 array-of-struct stride is 16-byte aligned, so keep CPU layout at 64 bytes.
        u32 _padding = 0u;
    };

    static_assert(sizeof(TexturesInstanceData) == 64, "TexturesInstanceData must be exactly 64 bytes");

    APPLIB_API void create_textures_stream(DrawStream &stream);

    inline void push_textures_batch_to_stream(DrawStream *stream, const TexturesInstanceData *data, u32 count,
                                              DrawDataID *out_draw_ids = nullptr)
    {
        push_data_batch_to_stream(stream, data, count, out_draw_ids);
    }

    inline void update_textures_batch_in_stream(DrawStream *stream, const DrawDataID *draw_data_ids,
                                                const TexturesInstanceData *data, u32 count)
    {
        update_data_batch_in_stream(stream, draw_data_ids, data, count);
    }
} // namespace auik::v2
