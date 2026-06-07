#include <auik/v2/detail/context.hpp>
#include <auik/v2/detail/gpu_context.hpp>
#include <auik/v2/detail/pixel_snap.hpp>
#include <auik/v2/pipelines.hpp>
#include <auik/v2/widgets/widget.hpp>
#include "stream_data.hpp"

namespace auik::v2::detail
{
    static void sync_frame_to_master(DrawStream *stream, StreamSyncState *state, Context &ctx, u32 frame_id)
    {
        assert(stream && state->buffer_versions);
        if (state->buffer_versions[frame_id] == state->master_version) return;
        ctx.gpu_ctx->quads.copy_stream_frame_data(stream, frame_id, state->master_id);
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

        // Keep monotonic versioning even if a new mutation arrives while previous
        // invalidation is still being propagated to other frame slots.
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
        // Ensure slot has a complete baseline before any mutation.
        sync_frame_to_master(stream, state, ctx, frame_id);
        mark_master_mutation(state, frame_id, ctx.frames_in_flight);
        stream->flags |= StreamFlagBits::invalidate;
        ctx.dirty_flags |= DirtyFlagBits::streams;
        auto copy = *static_cast<const QuadsInstanceData *>(data);
        copy.rect = snap_rect_to_pixel_grid(copy.rect);
        return gpu_ctx->quads.push_data_to_stream(stream, &copy, frame_id);
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
        assert(gpu_ctx->quads.push_data_batch_to_stream && "GPU quads batch push dispatch is not initialized");
        acul::vector<QuadsInstanceData> copies;
        copies.resize(count);
        const auto *src = static_cast<const QuadsInstanceData *>(data);
        for (u32 i = 0; i < count; ++i)
        {
            copies[i] = src[i];
            copies[i].rect = snap_rect_to_pixel_grid(copies[i].rect);
        }
        gpu_ctx->quads.push_data_batch_to_stream(stream, copies.data(), count, out_ids, frame_id);
    }

    static void update_data_quads_stream(DrawStream *stream, DrawDataID draw_data_id, const void *data)
    {
        auto &ctx = get_context();
        auto *state = static_cast<StreamSyncState *>(stream->runtime_data);
        // Local updates require all draw ids/clip ids to exist in this frame slot.
        sync_frame_to_master(stream, state, ctx, ctx.frame_id);
        mark_master_mutation(state, ctx.frame_id, ctx.frames_in_flight);
        stream->flags |= StreamFlagBits::invalidate;
        ctx.dirty_flags |= DirtyFlagBits::streams;
        auto copy = *static_cast<const QuadsInstanceData *>(data);
        copy.rect = snap_rect_to_pixel_grid(copy.rect);
        ctx.gpu_ctx->quads.update_stream_data(stream, draw_data_id, &copy, ctx.frame_id);
    }

    static void invalidate_data_quads_stream(DrawStream *stream, DrawDataID draw_data_id)
    {
        if (draw_data_id.render_id == AUIK_INVALID_DRAW_DATA_ID) return;
        auto &ctx = get_context();
        auto *state = static_cast<StreamSyncState *>(stream->runtime_data);
        sync_frame_to_master(stream, state, ctx, ctx.frame_id);
        mark_master_mutation(state, ctx.frame_id, ctx.frames_in_flight);
        stream->flags |= StreamFlagBits::invalidate;
        ctx.dirty_flags |= DirtyFlagBits::streams;
        if (ctx.gpu_ctx->quads.invalidate_stream_data)
            ctx.gpu_ctx->quads.invalidate_stream_data(stream, draw_data_id, ctx.frame_id);
    }

    static void invalidate_data_batch_quads_stream(DrawStream *stream, const DrawDataID *draw_data_ids, u32 count)
    {
        if (count == 0) return;
        bool has_valid_id = false;
        for (u32 i = 0; i < count; ++i)
        {
            if (draw_data_ids[i].render_id == AUIK_INVALID_DRAW_DATA_ID) continue;
            has_valid_id = true;
            break;
        }
        if (!has_valid_id) return;

        auto &ctx = get_context();
        auto *state = static_cast<StreamSyncState *>(stream->runtime_data);
        sync_frame_to_master(stream, state, ctx, ctx.frame_id);
        mark_master_mutation(state, ctx.frame_id, ctx.frames_in_flight);
        stream->flags |= StreamFlagBits::invalidate;
        ctx.dirty_flags |= DirtyFlagBits::streams;
        if (!ctx.gpu_ctx->quads.invalidate_stream_data) return;
        for (u32 i = 0; i < count; ++i)
        {
            if (draw_data_ids[i].render_id == AUIK_INVALID_DRAW_DATA_ID) continue;
            ctx.gpu_ctx->quads.invalidate_stream_data(stream, draw_data_ids[i], ctx.frame_id);
        }
    }

    static void update_data_batch_quads_stream(DrawStream *stream, const DrawDataID *draw_data_ids, const void *data,
                                               u32 count)
    {
        if (count == 0) return;

        auto &ctx = get_context();
        auto *state = static_cast<StreamSyncState *>(stream->runtime_data);
        sync_frame_to_master(stream, state, ctx, ctx.frame_id);
        mark_master_mutation(state, ctx.frame_id, ctx.frames_in_flight);
        stream->flags |= StreamFlagBits::invalidate;
        ctx.dirty_flags |= DirtyFlagBits::streams;
        assert(ctx.gpu_ctx->quads.update_stream_data_batch && "GPU quads batch update dispatch is not initialized");
        acul::vector<QuadsInstanceData> copies;
        copies.resize(count);
        const auto *src = static_cast<const QuadsInstanceData *>(data);
        for (u32 i = 0; i < count; ++i)
        {
            copies[i] = src[i];
            copies[i].rect = snap_rect_to_pixel_grid(copies[i].rect);
        }
        ctx.gpu_ctx->quads.update_stream_data_batch(stream, draw_data_ids, copies.data(), count, ctx.frame_id);
    }

    static void render_quads_stream(DrawStream *stream, void *render_ctx, GPUContext *gpu_context)
    {
        const u32 frame_id = get_context().frame_id;
        if (stream->draw_sizes[frame_id] == 0) return;
        gpu_context->quads.render_stream(stream, render_ctx, gpu_context, frame_id);
    }

    static void clear_quads_stream(DrawStream *stream, u32 frame_id)
    {
        auto &ctx = get_context();
        auto *state = static_cast<StreamSyncState *>(stream->runtime_data);
        ctx.gpu_ctx->quads.clear_stream(stream, frame_id);
        stream->draw_sizes[frame_id] = 0;
        assert(state && state->buffer_versions);
        mark_master_mutation(state, frame_id, ctx.frames_in_flight);
        stream->flags |= StreamFlagBits::invalidate;
        ctx.dirty_flags |= DirtyFlagBits::streams;
    }

    static void sync_quads_stream(DrawStream *stream, u32 frame_id)
    {
        assert(stream && "Null stream provided");
        auto *state = static_cast<StreamSyncState *>(stream->runtime_data);
        auto &ctx = get_context();
        assert(state->buffer_versions);
        if (state->buffer_versions[frame_id] != state->master_version)
        {
            ctx.gpu_ctx->quads.copy_stream_frame_data(stream, frame_id, state->master_id);
            state->buffer_versions[frame_id] = state->master_version;
            if (state->invalidation_count > 0) --state->invalidation_count;
            if (state->invalidation_count == 0)
            {
                state->stage_version = state->master_version;
                stream->flags &= ~StreamFlagBits::invalidate;
            }
        }
    }

    static void destroy_quads_stream(DrawStream *stream)
    {
        auto *gpu_ctx = get_context().gpu_ctx;
        gpu_ctx->quads.destroy_stream_gpu_data(stream);
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

    static inline void setup_quads_stream(DrawStream &stream)
    {
        stream.draw_sizes = acul::alloc_n<u32>(get_context().frames_in_flight);
        for (u32 i = 0; i < get_context().frames_in_flight; ++i) stream.draw_sizes[i] = 0;
    }
} // namespace auik::v2::detail

namespace auik::v2
{
    void create_quads_stream(DrawStream &stream)
    {
        detail::setup_quads_stream(stream);
        stream.push_data_to_stream = &detail::push_data_to_stream;
        stream.push_data_batch_to_stream = &detail::push_data_batch_to_stream;
        stream.update_data_in_stream = &detail::update_data_quads_stream;
        stream.update_data_batch_in_stream = &detail::update_data_batch_quads_stream;
        stream.invalidate_data_in_stream = &detail::invalidate_data_quads_stream;
        stream.invalidate_data_batch_in_stream = &detail::invalidate_data_batch_quads_stream;
        stream.clear = &detail::clear_quads_stream;
        stream.destroy = &detail::destroy_quads_stream;

        auto &ctx = detail::get_context();
        auto *gpu_ctx = ctx.gpu_ctx;
        stream.stream_instances = gpu_ctx->quads.create_stream_gpu_data(ctx.frames_in_flight, gpu_ctx);
        auto *state = acul::alloc<detail::StreamSyncState>();
        construct_shared_buffer_sync_state(*state, ctx.frames_in_flight);
        stream.runtime_data = state;
        stream.flags = StreamFlagBits::none;
        stream.render = &detail::render_quads_stream;
        stream.sync_stream = &detail::sync_quads_stream;
        stream.post_slot_id = 0u;
    }
} // namespace auik::v2
