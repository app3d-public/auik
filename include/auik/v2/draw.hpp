#pragma once

#include <acul/api.hpp>
#include <acul/enum.hpp>
#include <acul/scalars.hpp>
#include "detail/context.hpp"

namespace auik::v2
{
    using PostEffectRecordFn = DrawDataID (*)(void *effect_data, DrawStream *, const void *draw_data,
                                              const void *post_data);
    using PostEffectUpdateFn = void (*)(void *effect_data, DrawStream *, DrawDataID draw_id, const void *draw_data,
                                        const void *post_data);
    using PostEffectDestroyFn = void (*)(void *effect_data);
    using PostEffectRuntimeClearFn = void (*)(void *runtime_data);
    using PostEffectRuntimeDestroyFn = void (*)(void *runtime_data);
    using PostEffectPushInstanceFn = u32 (*)(PostEffect *, Widget *, const void *);
    using PostEffectUpdateInstanceFn = void (*)(PostEffect *, u32, Widget *, const void *);
    using PostEffectRetainInstanceFn = void (*)(PostEffect *, u32);
    using PostEffectReleaseInstanceFn = void (*)(PostEffect *, u32);
    using PostEffectIsInstanceValidFn = bool (*)(const PostEffect *, u32);

    struct PostEffectNode
    {
        void *data = nullptr;
        PostEffectRecordFn record = nullptr;
        PostEffectUpdateFn update = nullptr;
        PostEffectDestroyFn destroy = nullptr;
    };

    struct PostEffect
    {
        PostEffectNode **slots = nullptr;
        u32 slot_count = 0;
        void *runtime_data = nullptr;
        PostEffectRuntimeClearFn clear_runtime = nullptr;
        PostEffectRuntimeDestroyFn destroy_runtime = nullptr;
        PostEffectPushInstanceFn push_instance = nullptr;
        PostEffectUpdateInstanceFn update_instance = nullptr;
        PostEffectRetainInstanceFn retain_instance = nullptr;
        PostEffectReleaseInstanceFn release_instance = nullptr;
        PostEffectIsInstanceValidFn is_instance_valid = nullptr;
    };

    struct DrawReasonBits
    {
        enum enum_type : u16
        {
            none = 0x0,
            style = 0x1,
            layout = 0x2,
            host_resize = 0x4,
            full_redraw = 0x8,
            external = 0x10,
            transient = 0x20
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

    inline void push_data_batch_to_stream(DrawStream *stream, const void *data, u32 count, DrawDataID *out_draw_ids = nullptr)
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

    struct DrawCtx;
    using PFN_DrawEmit = DrawDataID (*)(const DrawCtx &, DrawStream *, DrawDataID &, const void *,
                                        const detail::RectData &, bool);
    APPLIB_API DrawDataID emit_draw_record(const DrawCtx &ctx, DrawStream *stream, DrawDataID &draw_id, const void *data,
                                           const detail::RectData &rect, bool emit_hit_rect);
    APPLIB_API DrawDataID emit_draw_update(const DrawCtx &ctx, DrawStream *stream, DrawDataID &draw_id, const void *data,
                                           const detail::RectData &rect, bool emit_hit_rect);
    APPLIB_API DrawDataID emit_draw_invalidate(const DrawCtx &ctx, DrawStream *stream, DrawDataID &draw_id,
                                               const void *data, const detail::RectData &rect, bool emit_hit_rect);

    struct DrawCtx
    {
        PFN_DrawEmit emit_fn = nullptr;
        bool emit_hit_rect = true;
        DrawReasonFlags reason = DrawReasonBits::none;
        PostEffect *post_effect = nullptr;
        const void *post_data = nullptr;

        DrawDataID emit(DrawStream *stream, DrawDataID &draw_id, const void *data, const detail::RectData &rect,
                        bool emit_hit_rect_value) const
        {
            assert(emit_fn && "DrawCtx emitter is not configured");
            return emit_fn(*this, stream, draw_id, data, rect, emit_hit_rect_value);
        }

        bool is_recording() const { return emit_fn == &emit_draw_record; }
        bool is_updating() const { return emit_fn == &emit_draw_update || emit_fn == &emit_draw_invalidate; }
        bool is_invalidating() const { return emit_fn == &emit_draw_invalidate; }
    };

    inline void update_hit_rect(u32 &hit_id, const detail::RectData &rect, bool force_update)
    {
        auto &ctx = detail::get_context();
        auto *gpu = ctx.gpu_ctx;
        assert(gpu && gpu->push_hit_rect && "GPU hover rect dispatch is not initialized");
        const bool force_draw_recreate = ctx.dirty_flags & DirtyFlagBits::hit_rect_draw;
        if (hit_id == AUIK_INVALID_DRAW_DATA_ID || force_draw_recreate)
        {
            hit_id = detail::push_hit_rect(gpu, rect);
            detail::mark_hit_rects_mutation();
        }
        else if (force_update)
        {
            detail::update_hit_rect(gpu, hit_id, rect);
            detail::mark_hit_rects_mutation();
        }
    }

    APPLIB_API bool is_post_effect_supported(const DrawStream *stream, const PostEffect *effect);
    APPLIB_API void destroy_post_effect(PostEffect *effect);

} // namespace auik::v2
