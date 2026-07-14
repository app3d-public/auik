#pragma once

#include <amal/rect.hpp>
#include "draw.hpp"
#include "theme.hpp"

#define AUIK_HAS_BORDER_BIT            0x1
#define AUIK_HAS_RADIUS_BIT            0x2
#define AUIK_HAS_CHECKER_BIT           0x4
#define AUIK_TEXTURE_INSTANCE_TEXT_BIT 0x1

namespace auik
{
    struct QuadsInstanceData
    {
        amal::rect rect;
        u32 background_color = 0;
        u32 border_color = 0;
        f32 border_radius;
        f32 border_thickness;
        f32 z_order;
        u32 mask = 0u;
    };

    static_assert(sizeof(QuadsInstanceData) == 40, "QuadsInstanceData must be exactly 40 bytes");

    AUIK_EXPORT void create_quads_stream(DrawStream &stream);

    inline void push_quads_batch_to_stream(DrawStream *stream, const QuadsInstanceData *data, u32 count,
                                           DrawDataID *out_draw_ids = nullptr)
    { push_data_batch_to_stream(stream, data, count, out_draw_ids); }

    inline void update_quads_batch_in_stream(DrawStream *stream, const DrawDataID *draw_data_ids,
                                             const QuadsInstanceData *data, u32 count)
    { update_data_batch_in_stream(stream, draw_data_ids, data, count); }

    inline bool fill_quads_instance_by_style(const Style &style, u16 clip_id, QuadsInstanceData &data)
    {
        data.mask = static_cast<u32>(clip_id);
        if (!(style.mask() & detail::g_style_visible_draw_mask)) return false;
        data.background_color = style.background_color();
        data.border_color = style.border_color();
        data.border_radius = style.border_radius();
        data.border_thickness = style.border_thickness();
        u32 flags = 0u;
        if (data.border_thickness > 0.0f) flags |= AUIK_HAS_BORDER_BIT;
        if (data.border_radius > 0.0f) flags |= AUIK_HAS_RADIUS_BIT;
        data.mask = static_cast<u32>(clip_id) | ((style.corner_mask() & 0xFu) << 16u) | (flags << 20u);
        return true;
    }

    inline void emit_quads_hit_rect_only(const DrawCtx &ctx, DrawDataID &draw_id, const detail::RectData &rect,
                                         bool emit_hit_rect)
    {
        emit_hit_rect = emit_hit_rect && ctx.is_hit_allowed;
        if (!emit_hit_rect) return;
        if (ctx.reason & DrawReasonBits::invalidate)
        {
            if (draw_id.hit_id != AUIK_INVALID_DRAW_DATA_ID)
            {
                detail::RectData hidden_rect = rect;
                hidden_rect.bounds.size = {0.0f, 0.0f};
                update_hit_rect(draw_id.hit_id, hidden_rect, true);
            }
            return;
        }
        const bool force_update = (ctx.reason & DrawReasonBits::record) ||
                                  (detail::get_context().dirty_flags & DirtyFlagBits::hit_rect_update);
        update_hit_rect(draw_id.hit_id, rect, force_update);
    }

    inline void emit_quads_instance(const DrawCtx &ctx, DrawStream *stream, DrawDataID &draw_id,
                                    const QuadsInstanceData &data, const detail::RectData &rect, bool visible,
                                    bool emit_hit_rect)
    {
        if (visible)
        {
            DrawCtx emit_ctx = ctx;
            emit_context_draw(emit_ctx, stream, draw_id, &data, rect, emit_hit_rect);
            return;
        }

        const bool keep_hit_rect = emit_hit_rect && ctx.is_hit_allowed && !(ctx.reason & DrawReasonBits::invalidate);
        if (draw_id.render_id != AUIK_INVALID_DRAW_DATA_ID)
        {
            if (ctx.reason & DrawReasonBits::record) draw_id.render_id = AUIK_INVALID_DRAW_DATA_ID;
            else invalidate_render_draw(stream, draw_id);
        }
        if (keep_hit_rect) emit_quads_hit_rect_only(ctx, draw_id, rect, true);
        else invalidate_hit_rect(draw_id);
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

    AUIK_EXPORT void create_textured_quads_stream(DrawStream &stream);

    inline void push_textured_quads_batch_to_stream(DrawStream *stream, const TexturesInstanceData *data, u32 count,
                                                    DrawDataID *out_draw_ids = nullptr)
    { push_data_batch_to_stream(stream, data, count, out_draw_ids); }

    inline void update_textured_quads_batch_in_stream(DrawStream *stream, const DrawDataID *draw_data_ids,
                                                      const TexturesInstanceData *data, u32 count)
    { update_data_batch_in_stream(stream, draw_data_ids, data, count); }

    struct VertexStreamVertex
    {
        amal::vec2 position;
        f32 z_order = 0.0f;
        f32 batch_id = 0.0f;
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
        amal::vec2 offset{0.0f, 0.0f};
    };

    AUIK_EXPORT void create_vertex_stream(DrawStream &stream);

    inline DrawDataID push_vertex_stream_batch(DrawStream *stream, const VertexStreamBatchData &data)
    { return push_data_to_stream(stream, const_cast<VertexStreamBatchData *>(&data)); }

    inline void push_vertex_stream_batch_list(DrawStream *stream, const VertexStreamBatchData *data, u32 count,
                                              DrawDataID *out_draw_ids = nullptr)
    { push_data_batch_to_stream(stream, data, count, out_draw_ids); }

    inline void update_vertex_stream_batch(DrawStream *stream, DrawDataID draw_data_id,
                                           const VertexStreamBatchData &data)
    { update_data_in_stream(stream, draw_data_id, const_cast<VertexStreamBatchData *>(&data)); }

    inline void update_vertex_stream_batch_offset(DrawStream *stream, DrawDataID draw_data_id, const amal::vec2 &offset)
    {
        VertexStreamBatchData data{};
        data.offset = offset;
        update_vertex_stream_batch(stream, draw_data_id, data);
    }

    inline DrawDataID emit_vertex_stream_batch(DrawCtx &ctx, DrawStream *stream, DrawDataID &draw_id,
                                               const VertexStreamBatchData &batch, const detail::RectData &rect,
                                               bool emit_hit_rect, bool offset_only)
    {
        if (offset_only && !(ctx.reason & (DrawReasonBits::record | DrawReasonBits::invalidate)) &&
            draw_id.render_id != AUIK_INVALID_DRAW_DATA_ID)
        {
            VertexStreamBatchData data{};
            data.offset = batch.offset;
            return emit_context_draw(ctx, stream, draw_id, &data, rect, emit_hit_rect);
        }
        return emit_context_draw(ctx, stream, draw_id, &batch, rect, emit_hit_rect);
    }

    inline void update_vertex_stream_batch_list(DrawStream *stream, const DrawDataID *draw_data_ids,
                                                const VertexStreamBatchData *data, u32 count)
    { update_data_batch_in_stream(stream, draw_data_ids, data, count); }

    struct TexturedVertexStreamVertex
    {
        amal::vec2 position;
        f32 z_order = 0.0f;
        f32 batch_id = 0.0f;
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
        u32 flags = 0u;
        amal::vec2 offset{0.0f, 0.0f};
    };

    AUIK_EXPORT void create_textured_vertex_stream(DrawStream &stream);

    inline DrawDataID push_textured_vertex_stream_batch(DrawStream *stream, const TexturedVertexStreamBatchData &data)
    { return push_data_to_stream(stream, const_cast<TexturedVertexStreamBatchData *>(&data)); }

    inline void push_textured_vertex_stream_batch_list(DrawStream *stream, const TexturedVertexStreamBatchData *data,
                                                       u32 count, DrawDataID *out_draw_ids = nullptr)
    { push_data_batch_to_stream(stream, data, count, out_draw_ids); }

    inline void update_textured_vertex_stream_batch(DrawStream *stream, DrawDataID draw_data_id,
                                                    const TexturedVertexStreamBatchData &data)
    { update_data_in_stream(stream, draw_data_id, const_cast<TexturedVertexStreamBatchData *>(&data)); }

    inline void update_textured_vertex_stream_batch_offset(DrawStream *stream, DrawDataID draw_data_id,
                                                           const amal::vec2 &offset)
    {
        TexturedVertexStreamBatchData data{};
        data.offset = offset;
        update_textured_vertex_stream_batch(stream, draw_data_id, data);
    }

    inline DrawDataID emit_textured_vertex_stream_batch(DrawCtx &ctx, DrawStream *stream, DrawDataID &draw_id,
                                                        const TexturedVertexStreamBatchData &batch,
                                                        const detail::RectData &rect, bool emit_hit_rect,
                                                        bool offset_only)
    {
        if (offset_only && !(ctx.reason & (DrawReasonBits::record | DrawReasonBits::invalidate)) &&
            draw_id.render_id != AUIK_INVALID_DRAW_DATA_ID)
        {
            TexturedVertexStreamBatchData data{};
            data.offset = batch.offset;
            return emit_context_draw(ctx, stream, draw_id, &data, rect, emit_hit_rect);
        }
        return emit_context_draw(ctx, stream, draw_id, &batch, rect, emit_hit_rect);
    }

    inline void update_textured_vertex_stream_batch_list(DrawStream *stream, const DrawDataID *draw_data_ids,
                                                         const TexturedVertexStreamBatchData *data, u32 count)
    { update_data_batch_in_stream(stream, draw_data_ids, data, count); }
} // namespace auik
