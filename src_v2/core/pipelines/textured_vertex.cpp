#include <auik/v2/detail/context.hpp>
#include <auik/v2/detail/gpu_context.hpp>
#include <auik/v2/pipelines.hpp>
#include <auik/v2/widgets/widget.hpp>
#include "stream_data.hpp"

namespace auik::v2::detail
{
    static void sync_frame_to_master(DrawStream *stream, StreamSyncState *state, Context &ctx, u32 frame_id)
    {
        assert(stream && state->buffer_versions);
        if (state->buffer_versions[frame_id] == state->master_version) return;
        ctx.gpu_ctx->textured_vertex_stream.copy_stream_frame_data(stream, frame_id, state->master_id);
        state->buffer_versions[frame_id] = state->master_version;
        if (state->invalidation_count > 0) --state->invalidation_count;
        if (state->invalidation_count == 0)
        {
            state->stage_version = state->master_version;
            stream->flags &= ~StreamFlagBits::invalidate;
        }
    }

    static void mark_master_mutation(StreamSyncState *state, u32 frame_id, u32 frames_in_flight)
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

    static DrawDataID push_data_to_stream(DrawStream *stream, const void *data)
    {
        auto &ctx = get_context();
        const u32 frame_id = ctx.frame_id;
        auto *state = static_cast<StreamSyncState *>(stream->runtime_data);
        auto *gpu_ctx = ctx.gpu_ctx;
        sync_frame_to_master(stream, state, ctx, frame_id);
        mark_master_mutation(state, frame_id, ctx.frames_in_flight);
        stream->flags |= StreamFlagBits::invalidate;
        ctx.dirty_flags |= DirtyFlagBits::streams;
        return gpu_ctx->textured_vertex_stream.push_data_to_stream(stream, data, frame_id);
    }

    static void push_data_batch_to_stream(DrawStream *stream, const void *data, u32 count, DrawDataID *out_ids)
    {
        if (count == 0) return;

        auto &ctx = get_context();
        const u32 frame_id = ctx.frame_id;
        auto *state = static_cast<StreamSyncState *>(stream->runtime_data);
        auto *gpu_ctx = ctx.gpu_ctx;
        sync_frame_to_master(stream, state, ctx, frame_id);
        mark_master_mutation(state, frame_id, ctx.frames_in_flight);
        stream->flags |= StreamFlagBits::invalidate;
        ctx.dirty_flags |= DirtyFlagBits::streams;
        assert(gpu_ctx->textured_vertex_stream.push_data_batch_to_stream &&
               "GPU textured vertex stream batch push dispatch is not initialized");
        gpu_ctx->textured_vertex_stream.push_data_batch_to_stream(stream, data, count, out_ids, frame_id);
    }

    static void update_data_textured_vertex_stream(DrawStream *stream, DrawDataID draw_data_id, const void *data)
    {
        auto &ctx = get_context();
        auto *state = static_cast<StreamSyncState *>(stream->runtime_data);
        sync_frame_to_master(stream, state, ctx, ctx.frame_id);
        mark_master_mutation(state, ctx.frame_id, ctx.frames_in_flight);
        stream->flags |= StreamFlagBits::invalidate;
        ctx.dirty_flags |= DirtyFlagBits::streams;
        ctx.gpu_ctx->textured_vertex_stream.update_stream_data(stream, draw_data_id, data, ctx.frame_id);
    }

    static void invalidate_data_textured_vertex_stream(DrawStream *stream, DrawDataID draw_data_id)
    {
        if (draw_data_id.render_id == AUIK_INVALID_DRAW_DATA_ID) return;
        auto &ctx = get_context();
        auto *state = static_cast<StreamSyncState *>(stream->runtime_data);
        sync_frame_to_master(stream, state, ctx, ctx.frame_id);
        mark_master_mutation(state, ctx.frame_id, ctx.frames_in_flight);
        stream->flags |= StreamFlagBits::invalidate;
        ctx.dirty_flags |= DirtyFlagBits::streams;
        if (ctx.gpu_ctx->textured_vertex_stream.invalidate_stream_data)
            ctx.gpu_ctx->textured_vertex_stream.invalidate_stream_data(stream, draw_data_id, ctx.frame_id);
    }

    static void update_data_batch_textured_vertex_stream(DrawStream *stream, const DrawDataID *draw_data_ids,
                                                         const void *data, u32 count)
    {
        if (count == 0) return;

        auto &ctx = get_context();
        auto *state = static_cast<StreamSyncState *>(stream->runtime_data);
        sync_frame_to_master(stream, state, ctx, ctx.frame_id);
        mark_master_mutation(state, ctx.frame_id, ctx.frames_in_flight);
        stream->flags |= StreamFlagBits::invalidate;
        ctx.dirty_flags |= DirtyFlagBits::streams;
        assert(ctx.gpu_ctx->textured_vertex_stream.update_stream_data_batch &&
               "GPU textured vertex stream batch update dispatch is not initialized");
        ctx.gpu_ctx->textured_vertex_stream.update_stream_data_batch(stream, draw_data_ids, data, count, ctx.frame_id);
    }

    static void render_textured_vertex_stream(DrawStream *stream, void *render_ctx, GPUContext *gpu_context)
    {
        const u32 frame_id = get_context().frame_id;
        if (stream->draw_sizes[frame_id] == 0) return;
        gpu_context->textured_vertex_stream.render_stream(stream, render_ctx, gpu_context, frame_id);
    }

    static void clear_textured_vertex_stream(DrawStream *stream, u32 frame_id)
    {
        auto &ctx = get_context();
        auto *state = static_cast<StreamSyncState *>(stream->runtime_data);
        ctx.gpu_ctx->textured_vertex_stream.clear_stream(stream, frame_id);
        stream->draw_sizes[frame_id] = 0;
        assert(state && state->buffer_versions);
        state->buffer_versions[frame_id] = state->master_version;
        stream->flags |= StreamFlagBits::invalidate;
        ctx.dirty_flags |= DirtyFlagBits::streams;
    }

    static void sync_textured_vertex_stream(DrawStream *stream, u32 frame_id)
    {
        assert(stream && "Null stream provided");
        auto *state = static_cast<StreamSyncState *>(stream->runtime_data);
        auto &ctx = get_context();
        assert(state->buffer_versions);
        if (state->buffer_versions[frame_id] != state->master_version)
        {
            ctx.gpu_ctx->textured_vertex_stream.copy_stream_frame_data(stream, frame_id, state->master_id);
            state->buffer_versions[frame_id] = state->master_version;
            if (state->invalidation_count > 0) --state->invalidation_count;
            if (state->invalidation_count == 0)
            {
                state->stage_version = state->master_version;
                stream->flags &= ~StreamFlagBits::invalidate;
            }
        }
    }

    static void destroy_textured_vertex_stream(DrawStream *stream)
    {
        auto *gpu_ctx = get_context().gpu_ctx;
        gpu_ctx->textured_vertex_stream.destroy_stream_gpu_data(stream);
        if (stream->runtime_data)
        {
            auto *state = static_cast<StreamSyncState *>(stream->runtime_data);
            destroy_shared_buffer_sync_state(*state);
            acul::release(state);
        }
        if (stream->draw_sizes) acul::release(stream->draw_sizes, get_context().frames_in_flight);
        stream->runtime_data = nullptr;
        stream->stream_instances = nullptr;
        stream->draw_sizes = nullptr;
    }

    static inline void setup_textured_vertex_stream(DrawStream &stream)
    {
        stream.draw_sizes = acul::alloc_n<u32>(get_context().frames_in_flight);
        for (u32 i = 0; i < get_context().frames_in_flight; ++i) stream.draw_sizes[i] = 0;
    }
} // namespace auik::v2::detail

namespace auik::v2
{
    void create_textured_vertex_stream(DrawStream &stream)
    {
        detail::setup_textured_vertex_stream(stream);
        stream.push_data_to_stream = &detail::push_data_to_stream;
        stream.push_data_batch_to_stream = &detail::push_data_batch_to_stream;
        stream.update_data_in_stream = &detail::update_data_textured_vertex_stream;
        stream.update_data_batch_in_stream = &detail::update_data_batch_textured_vertex_stream;
        stream.invalidate_data_in_stream = &detail::invalidate_data_textured_vertex_stream;
        stream.clear = &detail::clear_textured_vertex_stream;
        stream.destroy = &detail::destroy_textured_vertex_stream;

        auto &ctx = detail::get_context();
        auto *gpu_ctx = ctx.gpu_ctx;
        stream.stream_instances = gpu_ctx->textured_vertex_stream.create_stream_gpu_data(ctx.frames_in_flight, gpu_ctx);
        auto *state = acul::alloc<detail::StreamSyncState>();
        construct_shared_buffer_sync_state(*state, ctx.frames_in_flight);
        stream.runtime_data = state;
        stream.flags = StreamFlagBits::none;
        stream.render = &detail::render_textured_vertex_stream;
        stream.sync_stream = &detail::sync_textured_vertex_stream;
        stream.post_slot_id = 4u;
    }
} // namespace auik::v2
