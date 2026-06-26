#pragma once

#include <acul/enum.hpp>
#include <acul/scalars.hpp>
#include "detail/context.hpp"

namespace auik
{
    using PFN_post_effect_record = DrawDataID (*)(void *effect_data, DrawStream *, const void *draw_data,
                                                  const void *post_data);
    using PFN_post_effect_update = void (*)(void *effect_data, DrawStream *, DrawDataID draw_id, const void *draw_data,
                                            const void *post_data);
    using PFN_post_effect_record_batch = void (*)(void *effect_data, DrawStream *, DrawDataID *draw_ids,
                                                  const void *draw_data, u32 count, const void *post_data);
    using PFN_post_effect_update_batch = void (*)(void *effect_data, DrawStream *, DrawDataID *draw_ids,
                                                  const void *draw_data, u32 count, const void *post_data);
    using PFN_post_effect_destroy = void (*)(void *effect_data);
    using PFN_post_effect_runtime_clear = void (*)(void *runtime_data);
    using PFN_post_effect_runtime_destroy = void (*)(void *runtime_data);
    using PFN_post_effect_push_instance = u32 (*)(PostEffect *, Widget *, const void *);
    using PFN_post_effect_update_instance = void (*)(PostEffect *, u32, Widget *, const void *);
    using PFN_post_effect_retain_instance = void (*)(PostEffect *, u32);
    using PFN_post_effect_release_instance = void (*)(PostEffect *, u32);
    using PFN_post_effect_is_instance_valid = bool (*)(const PostEffect *, u32);

    struct PostEffectNode
    {
        void *data = nullptr;
        PFN_post_effect_record record = nullptr;
        PFN_post_effect_update update = nullptr;
        PFN_post_effect_record_batch record_batch = nullptr;
        PFN_post_effect_update_batch update_batch = nullptr;
        PFN_post_effect_destroy destroy = nullptr;
    };

    struct PostEffect
    {
        PostEffectNode **slots = nullptr;
        u32 slot_count = 0;
        void *runtime_data = nullptr;
        PFN_post_effect_runtime_clear clear_runtime = nullptr;
        PFN_post_effect_runtime_destroy destroy_runtime = nullptr;
        PFN_post_effect_push_instance push_instance = nullptr;
        PFN_post_effect_update_instance update_instance = nullptr;
        PFN_post_effect_retain_instance retain_instance = nullptr;
        PFN_post_effect_release_instance release_instance = nullptr;
        PFN_post_effect_is_instance_valid is_instance_valid = nullptr;
    };

    struct PostFxChain
    {
        PostEffect *post_effect = nullptr;
        const void *post_data = nullptr;
        u32 id = 0xFFFFFFFFu;
        PostFxChain *next = nullptr;
    };

    struct DrawReasonBits
    {
        enum enum_type : u16
        {
            none = 0x0,
            layout = 0x1,
            full_redraw = 0x2,
            external = 0x4,
            transient = 0x8,
            record = 0x10,
            invalidate = 0x20
        };
        using flag_bitmask = std::true_type;
    };
    using DrawReasonFlags = acul::flags<DrawReasonBits>;

    struct StreamFlagBits
    {
        enum enum_type : u8
        {
            none = 0x0,
            invalidate = 0x1
        };
    };

    struct DrawStream
    {
        DrawDataID (*push_data_to_stream)(DrawStream *, const void *) = nullptr;
        void (*push_data_batch_to_stream)(DrawStream *, const void *, u32, DrawDataID *) = nullptr;
        void (*update_data_in_stream)(DrawStream *, DrawDataID, const void *) = nullptr;
        void (*update_data_batch_in_stream)(DrawStream *, const DrawDataID *, const void *, u32) = nullptr;
        void (*invalidate_data_in_stream)(DrawStream *, DrawDataID) = nullptr;
        void (*invalidate_data_batch_in_stream)(DrawStream *, const DrawDataID *, u32) = nullptr;
        void (*clear)(DrawStream *, u32) = nullptr;
        void (*render)(DrawStream *, void *, detail::GPUContext *) = nullptr;
        void (*sync_stream)(DrawStream *, u32) = nullptr;
        void (*destroy)(DrawStream *) = nullptr;

        void *stream_instances = nullptr;
        void *runtime_data = nullptr;
        DrawPipeline *pipeline = nullptr;
        u32 *draw_sizes = nullptr;
        u8 flags = StreamFlagBits::none;
        u16 post_slot_id = 0xFFFFu;
    };

    inline DrawDataID push_data_to_stream(DrawStream *stream, void *data)
    {
        assert(stream->push_data_to_stream);
        return stream->push_data_to_stream(stream, data);
    }

    inline void push_data_batch_to_stream(DrawStream *stream, const void *data, u32 count,
                                          DrawDataID *out_draw_ids = nullptr)
    {
        if (count == 0) return;
        assert(stream && "stream is null");
        assert(stream->push_data_batch_to_stream && "batch push is not configured for this stream");
        stream->push_data_batch_to_stream(stream, data, count, out_draw_ids);
    }

    inline void update_data_in_stream(DrawStream *stream, DrawDataID draw_data_id, void *data)
    {
        assert(stream->update_data_in_stream);
        stream->update_data_in_stream(stream, draw_data_id, data);
    }

    inline void update_data_batch_in_stream(DrawStream *stream, const DrawDataID *draw_data_ids, const void *data,
                                            u32 count)
    {
        if (count == 0) return;
        assert(stream && "stream is null");
        assert(draw_data_ids && "draw_data_ids is null");
        assert(data && "data is null");
        assert(stream->update_data_batch_in_stream && "batch update is not configured for this stream");
        stream->update_data_batch_in_stream(stream, draw_data_ids, data, count);
    }

    inline void invalidate_data_batch_in_stream(DrawStream *stream, const DrawDataID *draw_data_ids, u32 count)
    {
        if (count == 0) return;
        assert(stream && "stream is null");
        assert(draw_data_ids && "draw_data_ids is null");
        assert(stream->invalidate_data_batch_in_stream && "batch invalidate is not configured for this stream");
        stream->invalidate_data_batch_in_stream(stream, draw_data_ids, count);
    }

    inline void render_stream(DrawStream &stream, void *render_ctx)
    {
        assert(stream.render);
        stream.render(&stream, render_ctx, detail::get_context().gpu_ctx);
    }

    inline void clear_draw_stream(DrawStream *stream, u32 frame_id)
    {
        assert(stream && stream->clear);
        stream->clear(stream, frame_id);
    }

    inline void destroy_draw_stream(DrawStream *stream)
    {
        assert(stream && stream->destroy);
        stream->destroy(stream);
    }

    struct DrawCtx
    {
        DrawReasonFlags reason = DrawReasonBits::none;
        bool is_hit_allowed = true;
        PostFxChain *post_fx_chain = nullptr;
    };

    AUIK_EXPORT DrawDataID emit_context_draw(DrawCtx &ctx, DrawStream *stream, DrawDataID &draw_id, const void *data,
                                            const detail::RectData &rect, bool emit_hit_rect);
    AUIK_EXPORT void emit_context_draw_batch(DrawCtx &ctx, DrawStream *stream, DrawDataID *draw_ids, const void *data,
                                            u32 count, const detail::RectData *rects = nullptr,
                                            bool emit_hit_rects = false);

    inline void update_hit_rect(u32 &hit_id, const detail::RectData &rect, bool force_update)
    {
        auto &ctx = detail::get_context();
        auto *gpu = ctx.gpu_ctx;
        assert(gpu && gpu->push_hit_rect && "GPU hover rect dispatch is not initialized");
        detail::RectData snapped_rect = rect;
        snapped_rect.bounds = detail::snap_rect_to_pixel_grid(rect.bounds);
        const bool force_draw_recreate = ctx.dirty_flags & DirtyFlagBits::hit_rect_draw;
        if (hit_id == AUIK_INVALID_DRAW_DATA_ID || force_draw_recreate)
        {
            hit_id = detail::push_hit_rect(gpu, snapped_rect);
            detail::mark_hit_rects_mutation();
        }
        else if (force_update)
        {
            detail::update_hit_rect(gpu, hit_id, snapped_rect);
            detail::mark_hit_rects_mutation();
        }
    }

    inline bool invalidate_hit_rect_no_mark(u32 &hit_id)
    {
        if (hit_id == AUIK_INVALID_DRAW_DATA_ID) return false;
        auto &ctx = detail::get_context();
        auto *gpu = ctx.gpu_ctx;
        assert(gpu && gpu->push_hit_rect && "GPU hover rect dispatch is not initialized");
        detail::RectData hidden_rect{};
        const bool force_draw_recreate = ctx.dirty_flags & DirtyFlagBits::hit_rect_draw;
        if (force_draw_recreate) hit_id = detail::push_hit_rect(gpu, hidden_rect);
        else detail::update_hit_rect(gpu, hit_id, hidden_rect);
        return true;
    }

    inline void invalidate_hit_rect(u32 &hit_id)
    {
        if (!invalidate_hit_rect_no_mark(hit_id)) return;
        detail::mark_hit_rects_mutation();
        detail::get_context().dirty_flags |= DirtyFlagBits::hit_rect_update;
    }

    inline void invalidate_hit_rect(DrawDataID &draw_id)
    {
        invalidate_hit_rect(draw_id.hit_id);
    }

    inline void invalidate_hit_rect_batch(u32 **hit_ids, u32 count)
    {
        bool changed = false;
        for (u32 i = 0; i < count; ++i)
        {
            u32 *hit_id = hit_ids[i];
            if (!hit_id) continue;
            changed |= invalidate_hit_rect_no_mark(*hit_id);
        }
        if (!changed) return;
        detail::mark_hit_rects_mutation();
        detail::get_context().dirty_flags |= DirtyFlagBits::hit_rect_update;
    }

    inline void invalidate_hit_rect_batch(DrawDataID **draw_ids, u32 count)
    {
        bool changed = false;
        for (u32 i = 0; i < count; ++i)
        {
            DrawDataID *draw_id = draw_ids[i];
            if (!draw_id) continue;
            changed |= invalidate_hit_rect_no_mark(draw_id->hit_id);
        }
        if (!changed) return;
        detail::mark_hit_rects_mutation();
        detail::get_context().dirty_flags |= DirtyFlagBits::hit_rect_update;
    }

    AUIK_EXPORT bool is_post_effect_supported(const DrawStream *stream, const PostEffect *effect);
    AUIK_EXPORT void destroy_post_effect(PostEffect *effect);
} // namespace auik
