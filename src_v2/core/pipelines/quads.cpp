#include <auik/v2/detail/context.hpp>
#include <auik/v2/detail/gpu_context.hpp>
#include <auik/v2/detail/quads_dispatch.hpp>
#include <auik/v2/pipelines.hpp>
#include <auik/v2/widget.hpp>
#include "stream_data.hpp"

namespace auik::v2::detail
{
    static DrawDataID push_data_to_stream_cached(DrawStream *stream, const void *data)
    {
        auto &ctx = get_context();
        const u32 frame_id = ctx.frame_id;
        auto *state = static_cast<CachedStreamData *>(stream->runtime_data);
        auto *gpu_ctx = ctx.gpu_ctx;
        state->write_id = static_cast<i32>(frame_id);
        return gpu_ctx->quads.push_data_to_stream(stream, data, frame_id);
    }

    static void update_data_quads_stream_cached(DrawStream *stream, DrawDataID draw_data_id, const void *data)
    {
        auto &ctx = get_context();
        auto *state = static_cast<CachedStreamData *>(stream->runtime_data);
        if (state->write_id < 0) return;
        ctx.gpu_ctx->quads.update_quads_stream_data(stream, draw_data_id, data, static_cast<u32>(state->write_id));
    }

    static void push_widget_quads_stream_transient(DrawStream *stream, Widget *widget)
    {
        auto *state = static_cast<TransientStreamData *>(stream->runtime_data);
        state->widgets_cache.push_back(widget);
    }

    static DrawDataID push_data_to_stream_transient(DrawStream *stream, const void *data)
    {
        auto &ctx = get_context();
        return ctx.gpu_ctx->quads.push_data_to_stream(stream, data, ctx.frame_id);
    }

    static void update_data_quads_stream_transient(DrawStream *stream, DrawDataID draw_data_id, const void *data)
    {
        auto &ctx = get_context();
        ctx.gpu_ctx->quads.update_quads_stream_data(stream, draw_data_id, data, ctx.frame_id);
    }

    static void render_quads_stream_cached(DrawStream *stream, void *render_ctx, GPUContext *gpu_context)
    {
        auto *state = static_cast<CachedStreamData *>(stream->runtime_data);
        if (state->write_id < 0) return;

        const u32 write_id = static_cast<u32>(state->write_id);
        if (stream->draw_sizes[write_id] == 0) return;
        gpu_context->quads.render_quads_stream(stream, render_ctx, gpu_context, write_id);
    }

    static void render_quads_stream_transient(DrawStream *stream, void *render_ctx, GPUContext *gpu_context)
    {
        auto *state = static_cast<TransientStreamData *>(stream->runtime_data);
        auto &widgets_cache = state->widgets_cache;
        if (widgets_cache.size() == 0) return;
        // Transient stream is expected to refresh every frame while it has active widgets.
        auto &global_ctx = get_context();
        global_ctx.dirty_flags |= DirtyFlagBits::render;
        for (auto &widget : widgets_cache) widget->update_draw_commands();

        const u32 frame_id = global_ctx.frame_id;
        gpu_context->quads.render_quads_stream(stream, render_ctx, gpu_context, frame_id);
    }

    static void clear_quads_stream_cached(DrawStream *stream, u32 frame_id)
    {
        auto &ctx = get_context();
        auto *state = static_cast<CachedStreamData *>(stream->runtime_data);
        ctx.gpu_ctx->quads.clear_quads_stream(stream, frame_id);
        stream->draw_sizes[frame_id] = 0;
        state->write_id = -1;
    }

    static void destroy_quads_stream_cached(DrawStream *stream)
    {
        auto *gpu_ctx = get_context().gpu_ctx;
        gpu_ctx->quads.destroy_quads_stream_gpu_data(stream);
        if (stream->runtime_data) acul::release(static_cast<CachedStreamData *>(stream->runtime_data));
        if (stream->draw_sizes) acul::release(stream->draw_sizes, get_context().frames_in_flight);
        stream->runtime_data = nullptr;
        stream->stream_instances = nullptr;
        stream->draw_sizes = nullptr;
    }

    static void destroy_quads_stream_transient(DrawStream *stream)
    {
        auto *gpu_ctx = get_context().gpu_ctx;
        gpu_ctx->quads.destroy_quads_stream_gpu_data(stream);
        if (stream->runtime_data) acul::release(static_cast<TransientStreamData *>(stream->runtime_data));
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
    void create_quads_stream_cached(DrawStream &stream)
    {
        detail::setup_quads_stream(stream);
        stream.push_data_to_stream = &detail::push_data_to_stream_cached;
        stream.update_data_in_stream = &detail::update_data_quads_stream_cached;
        stream.clear = &detail::clear_quads_stream_cached;
        stream.destroy = &detail::destroy_quads_stream_cached;

        auto &ctx = detail::get_context();
        auto *gpu_ctx = ctx.gpu_ctx;
        stream.stream_instances = gpu_ctx->quads.create_quads_stream_gpu_data(ctx.frames_in_flight, gpu_ctx);
        stream.runtime_data = acul::alloc<detail::CachedStreamData>(-1);
        stream.render = &detail::render_quads_stream_cached;
    }

    void create_quads_stream_transient(DrawStream &stream)
    {
        detail::setup_quads_stream(stream);
        auto &ctx = detail::get_context();
        auto *gpu_ctx = ctx.gpu_ctx;
        stream.push_data_to_stream = &detail::push_data_to_stream_transient;
        stream.update_data_in_stream = &detail::update_data_quads_stream_transient;
        stream.push_widget_to_cache = &detail::push_widget_quads_stream_transient;
        stream.clear = gpu_ctx->quads.clear_quads_stream;
        stream.destroy = &detail::destroy_quads_stream_transient;
        stream.runtime_data = acul::alloc<detail::TransientStreamData>();
        stream.stream_instances = gpu_ctx->quads.create_quads_stream_gpu_data(ctx.frames_in_flight, gpu_ctx);
        stream.render = &detail::render_quads_stream_transient;
    }
} // namespace auik::v2
