#pragma once

#include <amal/rect.hpp>
#include "draw.hpp"
#include "theme.hpp"

#define AUIK_HAS_BORDER_BIT 0x1
#define AUIK_HAS_RADIUS_BIT 0x2
#define AUIK_HAS_CHECKER_BIT 0x4
#define AUIK_TEXTURE_INSTANCE_TEXT_BIT 0x1

namespace auik::v2
{
    struct QuadsInstanceData
    {
        amal::rect rect;
        u32 background_color = detail::pack_rgba8(0, 0, 0, 0);
        u32 border_color = detail::pack_rgba8(0, 0, 0, 0);
        f32 border_radius;
        f32 border_thickness;
        f32 z_order;
        u32 mask = 0u;
    };

    static_assert(sizeof(QuadsInstanceData) == 40, "QuadsInstanceData must be exactly 40 bytes");

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
        data.background_color = style.background_color_packed();
        data.border_color = style.border_color_packed();
        data.border_radius = style.border_radius();
        data.border_thickness = style.border_thickness();
        u32 flags = 0u;
        if (data.border_thickness > 0.0f) flags |= AUIK_HAS_BORDER_BIT;
        if (data.border_radius > 0.0f) flags |= AUIK_HAS_RADIUS_BIT;
        data.mask = static_cast<u32>(clip_id) | ((style.corner_mask() & 0xFu) << 16u) | (flags << 20u);
    }

    struct TexturesInstanceData
    {
        amal::rect rect;
        amal::rect uv_rect;
        u32 tint_color = detail::pack_rgba8(255, 255, 255, 255);
        f32 z_order;
        u16 texture_id;
        u16 clip_id;
        u32 flags = 0u;
    };

    static_assert(sizeof(TexturesInstanceData) == 48, "TexturesInstanceData must be exactly 48 bytes");

    APPLIB_API void create_textured_quads_stream(DrawStream &stream);

    inline void push_textured_quads_batch_to_stream(DrawStream *stream, const TexturesInstanceData *data, u32 count,
                                              DrawDataID *out_draw_ids = nullptr)
    {
        push_data_batch_to_stream(stream, data, count, out_draw_ids);
    }

    inline void update_textured_quads_batch_in_stream(DrawStream *stream, const DrawDataID *draw_data_ids,
                                                const TexturesInstanceData *data, u32 count)
    {
        update_data_batch_in_stream(stream, draw_data_ids, data, count);
    }

    struct VertexStreamVertex
    {
        amal::vec2 position;
        f32 z_order = 0.0f;
        f32 _unused = 0.0f;
        u32 color = detail::pack_rgba8(255, 255, 255, 255);
        u32 clip_id = 0;
    };

    static_assert(offsetof(VertexStreamVertex, position) == 0, "VertexStreamVertex::position offset mismatch");
    static_assert(offsetof(VertexStreamVertex, z_order) == 8, "VertexStreamVertex::z_order offset mismatch");
    static_assert(offsetof(VertexStreamVertex, color) == 16, "VertexStreamVertex::color offset mismatch");
    static_assert(offsetof(VertexStreamVertex, clip_id) == 20, "VertexStreamVertex::clip_id offset mismatch");
    static_assert(sizeof(VertexStreamVertex) == 24, "VertexStreamVertex must be exactly 24 bytes");

    using VertexStreamIndex = u32;

    struct VertexStreamBatchData
    {
        const VertexStreamVertex *vertices = nullptr;
        const VertexStreamIndex *indices = nullptr;
        u32 vertex_count = 0;
        u32 index_count = 0;
    };

    APPLIB_API void create_vertex_stream(DrawStream &stream);

    inline DrawDataID push_vertex_stream_batch(DrawStream *stream, const VertexStreamBatchData &data)
    {
        return push_data_to_stream(stream, const_cast<VertexStreamBatchData *>(&data));
    }

    inline void push_vertex_stream_batch_list(DrawStream *stream, const VertexStreamBatchData *data, u32 count,
                                              DrawDataID *out_draw_ids = nullptr)
    {
        push_data_batch_to_stream(stream, data, count, out_draw_ids);
    }

    inline void update_vertex_stream_batch(DrawStream *stream, DrawDataID draw_data_id, const VertexStreamBatchData &data)
    {
        update_data_in_stream(stream, draw_data_id, const_cast<VertexStreamBatchData *>(&data));
    }

    inline void update_vertex_stream_batch_list(DrawStream *stream, const DrawDataID *draw_data_ids,
                                                const VertexStreamBatchData *data, u32 count)
    {
        update_data_batch_in_stream(stream, draw_data_ids, data, count);
    }

    struct TexturedVertexStreamVertex
    {
        amal::vec2 position;
        f32 z_order = 0.0f;
        f32 _unused0 = 0.0f;
        amal::vec2 uv;
        u32 clip_id = 0;
        u32 _unused1 = 0u;
    };

    static_assert(offsetof(TexturedVertexStreamVertex, position) == 0,
                  "TexturedVertexStreamVertex::position offset mismatch");
    static_assert(offsetof(TexturedVertexStreamVertex, z_order) == 8,
                  "TexturedVertexStreamVertex::z_order offset mismatch");
    static_assert(offsetof(TexturedVertexStreamVertex, uv) == 16, "TexturedVertexStreamVertex::uv offset mismatch");
    static_assert(offsetof(TexturedVertexStreamVertex, clip_id) == 24,
                  "TexturedVertexStreamVertex::clip_id offset mismatch");
    static_assert(sizeof(TexturedVertexStreamVertex) == 32, "TexturedVertexStreamVertex must be exactly 32 bytes");

    using TexturedVertexStreamIndex = u32;

    struct TexturedVertexStreamBatchData
    {
        const TexturedVertexStreamVertex *vertices = nullptr;
        const TexturedVertexStreamIndex *indices = nullptr;
        u32 vertex_count = 0;
        u32 index_count = 0;
        TextureID texture_id = AUIK_INVALID_TEXTURE_ID;
    };

    APPLIB_API void create_textured_vertex_stream(DrawStream &stream);

    inline DrawDataID push_textured_vertex_stream_batch(DrawStream *stream, const TexturedVertexStreamBatchData &data)
    {
        return push_data_to_stream(stream, const_cast<TexturedVertexStreamBatchData *>(&data));
    }

    inline void push_textured_vertex_stream_batch_list(DrawStream *stream, const TexturedVertexStreamBatchData *data,
                                                       u32 count, DrawDataID *out_draw_ids = nullptr)
    {
        push_data_batch_to_stream(stream, data, count, out_draw_ids);
    }

    inline void update_textured_vertex_stream_batch(DrawStream *stream, DrawDataID draw_data_id,
                                                    const TexturedVertexStreamBatchData &data)
    {
        update_data_in_stream(stream, draw_data_id, const_cast<TexturedVertexStreamBatchData *>(&data));
    }

    inline void update_textured_vertex_stream_batch_list(DrawStream *stream, const DrawDataID *draw_data_ids,
                                                         const TexturedVertexStreamBatchData *data, u32 count)
    {
        update_data_batch_in_stream(stream, draw_data_ids, data, count);
    }
} // namespace auik::v2
