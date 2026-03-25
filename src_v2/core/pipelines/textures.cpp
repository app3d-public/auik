#include <auik/v2/detail/context.hpp>
#include <auik/v2/detail/gpu_context.hpp>
#include <auik/v2/pipelines.hpp>
#include <auik/v2/widgets/widget.hpp>
#include "stream_data.hpp"

namespace auik::v2::detail
{
    static void sync_frame_to_master(DrawStream *stream, CachedStreamData *state, Context &ctx, u32 frame_id)
    {
        assert(stream && state->buffer_versions);
        if (state->buffer_versions[frame_id] == state->master_version) return;
        ctx.gpu_ctx->textures.copy_stream_frame_data(stream, frame_id, state->master_id);
        state->buffer_versions[frame_id] = state->master_version;
        if (state->invalidation_count > 0) --state->invalidation_count;
        if (state->invalidation_count == 0)
        {
            state->stage_version = state->master_version;
            stream->flags &= ~StreamFlagBits::invalidate;
        }
    }

    static void mark_master_mutation(CachedStreamData *state, u32 frame_id, u32 frames_in_flight)
    {
        assert(state && state->buffer_versions);
        const bool already_mutating_same_frame = (state->master_id == frame_id) &&
                                                 (state->master_version != state->stage_version) &&
                                                 (state->buffer_versions[frame_id] == state->master_version);
        if (already_mutating_same_frame) return;

        const u32 base_version =
            (state->master_version > state->stage_version) ? state->master_version : state->stage_version;
        state->master_version = base_version + 1;
        state->master_id = frame_id;
        state->buffer_versions[frame_id] = state->master_version;
        state->invalidation_count = (frames_in_flight > 0) ? (frames_in_flight - 1) : 0;
    }

    static DrawDataID push_data_to_stream_cached(DrawStream *stream, const void *data)
    {
        auto &ctx = get_context();
        const u32 frame_id = ctx.frame_id;
        auto *state = static_cast<CachedStreamData *>(stream->runtime_data);
        auto *gpu_ctx = ctx.gpu_ctx;
        sync_frame_to_master(stream, state, ctx, frame_id);
        mark_master_mutation(state, frame_id, ctx.frames_in_flight);
        stream->flags |= StreamFlagBits::invalidate;
        ctx.dirty_flags |= DirtyFlagBits::streams;
        return gpu_ctx->textures.push_data_to_stream(stream, data, frame_id);
    }

    static void push_data_batch_to_stream_cached(DrawStream *stream, const void *data, u32 count, DrawDataID *out_ids)
    {
        if (count == 0) return;

        auto &ctx = get_context();
        const u32 frame_id = ctx.frame_id;
        auto *state = static_cast<CachedStreamData *>(stream->runtime_data);
        auto *gpu_ctx = ctx.gpu_ctx;
        sync_frame_to_master(stream, state, ctx, frame_id);
        mark_master_mutation(state, frame_id, ctx.frames_in_flight);
        stream->flags |= StreamFlagBits::invalidate;
        ctx.dirty_flags |= DirtyFlagBits::streams;
        assert(gpu_ctx->textures.push_data_batch_to_stream && "GPU textures batch push dispatch is not initialized");
        gpu_ctx->textures.push_data_batch_to_stream(stream, data, count, out_ids, frame_id);
    }

    static void update_data_textures_stream_cached(DrawStream *stream, DrawDataID draw_data_id, const void *data)
    {
        auto &ctx = get_context();
        auto *state = static_cast<CachedStreamData *>(stream->runtime_data);
        sync_frame_to_master(stream, state, ctx, ctx.frame_id);
        mark_master_mutation(state, ctx.frame_id, ctx.frames_in_flight);
        stream->flags |= StreamFlagBits::invalidate;
        ctx.dirty_flags |= DirtyFlagBits::streams;
        ctx.gpu_ctx->textures.update_stream_data(stream, draw_data_id, data, ctx.frame_id);
    }

    static void push_widget_textures_stream_transient(DrawStream *stream, Widget *widget)
    {
        auto *state = static_cast<TransientStreamData *>(stream->runtime_data);
        state->widgets_cache.push_back(widget);
    }

    static DrawDataID push_data_to_stream_transient(DrawStream *stream, const void *data)
    {
        auto &ctx = get_context();
        return ctx.gpu_ctx->textures.push_data_to_stream(stream, data, ctx.frame_id);
    }

    static void push_data_batch_to_stream_transient(DrawStream *stream, const void *data, u32 count,
                                                    DrawDataID *out_ids)
    {
        if (count == 0) return;

        auto &ctx = get_context();
        assert(ctx.gpu_ctx->textures.push_data_batch_to_stream &&
               "GPU textures batch push dispatch is not initialized");
        ctx.gpu_ctx->textures.push_data_batch_to_stream(stream, data, count, out_ids, ctx.frame_id);
    }

    static void update_data_textures_stream_transient(DrawStream *stream, DrawDataID draw_data_id, const void *data)
    {
        auto &ctx = get_context();
        ctx.gpu_ctx->textures.update_stream_data(stream, draw_data_id, data, ctx.frame_id);
    }

    static void render_textures_stream_cached(DrawStream *stream, void *render_ctx, GPUContext *gpu_context)
    {
        const u32 frame_id = get_context().frame_id;
        if (stream->draw_sizes[frame_id] == 0) return;
        gpu_context->textures.render_stream(stream, render_ctx, gpu_context, frame_id);
    }

    static void render_textures_stream_transient(DrawStream *stream, void *render_ctx, GPUContext *gpu_context)
    {
        auto *state = static_cast<TransientStreamData *>(stream->runtime_data);
        auto &widgets_cache = state->widgets_cache;
        if (widgets_cache.size() == 0) return;
        auto &global_ctx = get_context();
        global_ctx.dirty_flags |= DirtyFlagBits::redraw;
        for (auto &widget : widgets_cache) widget->update_draw_commands();

        const u32 frame_id = global_ctx.frame_id;
        gpu_context->textures.render_stream(stream, render_ctx, gpu_context, frame_id);
    }

    static void clear_textures_stream_cached(DrawStream *stream, u32 frame_id)
    {
        auto &ctx = get_context();
        auto *state = static_cast<CachedStreamData *>(stream->runtime_data);
        ctx.gpu_ctx->textures.clear_stream(stream, frame_id);
        stream->draw_sizes[frame_id] = 0;
        assert(state && state->buffer_versions);
        state->buffer_versions[frame_id] = state->master_version;
        stream->flags |= StreamFlagBits::invalidate;
        ctx.dirty_flags |= DirtyFlagBits::streams;
    }

    static void sync_textures_stream_cached(DrawStream *stream, u32 frame_id)
    {
        assert(stream && "Null stream provided");
        auto *state = static_cast<CachedStreamData *>(stream->runtime_data);
        auto &ctx = get_context();
        assert(state->buffer_versions);
        if (state->buffer_versions[frame_id] != state->master_version)
        {
            ctx.gpu_ctx->textures.copy_stream_frame_data(stream, frame_id, state->master_id);
            state->buffer_versions[frame_id] = state->master_version;
            if (state->invalidation_count > 0) --state->invalidation_count;
            if (state->invalidation_count == 0)
            {
                state->stage_version = state->master_version;
                stream->flags &= ~StreamFlagBits::invalidate;
            }
        }
    }

    static void destroy_textures_stream_cached(DrawStream *stream)
    {
        auto *gpu_ctx = get_context().gpu_ctx;
        gpu_ctx->textures.destroy_stream_gpu_data(stream);
        if (stream->runtime_data) acul::release(static_cast<CachedStreamData *>(stream->runtime_data));
        if (stream->draw_sizes) acul::release(stream->draw_sizes, get_context().frames_in_flight);
        stream->runtime_data = nullptr;
        stream->stream_instances = nullptr;
        stream->draw_sizes = nullptr;
    }

    static void destroy_textures_stream_transient(DrawStream *stream)
    {
        auto *gpu_ctx = get_context().gpu_ctx;
        gpu_ctx->textures.destroy_stream_gpu_data(stream);
        if (stream->runtime_data) acul::release(static_cast<TransientStreamData *>(stream->runtime_data));
        if (stream->draw_sizes) acul::release(stream->draw_sizes, get_context().frames_in_flight);
        stream->runtime_data = nullptr;
        stream->stream_instances = nullptr;
        stream->draw_sizes = nullptr;
    }

    static inline void setup_textures_stream(DrawStream &stream)
    {
        stream.draw_sizes = acul::alloc_n<u32>(get_context().frames_in_flight);
        for (u32 i = 0; i < get_context().frames_in_flight; ++i) stream.draw_sizes[i] = 0;
    }
} // namespace auik::v2::detail

namespace auik::v2
{
    void create_textures_stream_cached(DrawStream &stream)
    {
        detail::setup_textures_stream(stream);
        stream.push_data_to_stream = &detail::push_data_to_stream_cached;
        stream.push_data_batch_to_stream = &detail::push_data_batch_to_stream_cached;
        stream.update_data_in_stream = &detail::update_data_textures_stream_cached;
        stream.clear = &detail::clear_textures_stream_cached;
        stream.destroy = &detail::destroy_textures_stream_cached;

        auto &ctx = detail::get_context();
        auto *gpu_ctx = ctx.gpu_ctx;
        stream.stream_instances = gpu_ctx->textures.create_stream_gpu_data(ctx.frames_in_flight, gpu_ctx);
        auto *state = acul::alloc<detail::CachedStreamData>();
        state->master_id = 0;
        state->master_version = 0;
        state->stage_version = 0;
        state->invalidation_count = 0;
        state->buffer_versions = acul::alloc_n<u32>(ctx.frames_in_flight);
        for (u32 i = 0; i < ctx.frames_in_flight; ++i) state->buffer_versions[i] = 0;
        stream.runtime_data = state;
        stream.flags = StreamFlagBits::cached;
        stream.render = &detail::render_textures_stream_cached;
        stream.sync_stream = &detail::sync_textures_stream_cached;
    }

    void create_textures_stream_transient(DrawStream &stream)
    {
        detail::setup_textures_stream(stream);
        auto &ctx = detail::get_context();
        auto *gpu_ctx = ctx.gpu_ctx;
        stream.push_data_to_stream = &detail::push_data_to_stream_transient;
        stream.push_data_batch_to_stream = &detail::push_data_batch_to_stream_transient;
        stream.update_data_in_stream = &detail::update_data_textures_stream_transient;
        stream.push_widget_to_cache = &detail::push_widget_textures_stream_transient;
        stream.clear = gpu_ctx->textures.clear_stream;
        stream.destroy = &detail::destroy_textures_stream_transient;
        stream.runtime_data = acul::alloc<detail::TransientStreamData>();
        stream.stream_instances = gpu_ctx->textures.create_stream_gpu_data(ctx.frames_in_flight, gpu_ctx);
        stream.flags = StreamFlagBits::transient;
        stream.render = &detail::render_textures_stream_transient;
    }
} // namespace auik::v2
