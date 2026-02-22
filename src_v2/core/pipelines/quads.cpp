#include <auik/v2/detail/context.hpp>
#include <auik/v2/detail/gpu_dispatch.hpp>
#include <auik/v2/detail/quads_dispatch.hpp>
#include <auik/v2/pipelines.hpp>
#include <auik/v2/widget.hpp>
#include "stream_data.hpp"

namespace auik::v2::detail
{
    static void push_data_quads_stream_retained(DrawStream *stream, void *data)
    {
        push_data_to_quads_stream(stream, data, g_gpu_dispatch.quads);
        auto *write_id = static_cast<RetainedStreamData *>(stream->runtime_data);
        *write_id = get_context().frame_id;
    }

    static void push_widget_quads_stream_immediate(DrawStream *stream, Widget *widget)
    {
        auto &widgets_cache = static_cast<ImStreamData *>(stream->runtime_data)[get_context().frame_id];
        widgets_cache.push_back(widget);
    }

    static void render_quads_stream_retained(DrawStream *stream, void *render_ctx, void *gpu_backend)
    {
        u32 write_id = *static_cast<RetainedStreamData *>(stream->runtime_data);
        if (stream->draw_sizes[write_id] == 0) return;
        render_quads_stream(stream, render_ctx, gpu_backend, write_id, g_gpu_dispatch.quads);
    }

    static void render_quads_stream_immediate(DrawStream *stream, void *render_ctx, void *gpu_backend)
    {
        u32 frame_id = get_context().frame_id;
        auto &widgets_cache = static_cast<ImStreamData *>(stream->runtime_data)[frame_id];
        if (widgets_cache.size() == 0) return;

        clear_quads_stream(stream, frame_id, g_gpu_dispatch.quads);
        for (auto &widget : widgets_cache) widget->update_immediate_commands();
        render_quads_stream(stream, render_ctx, gpu_backend, frame_id, g_gpu_dispatch.quads);
    }

    static void destroy_quads_stream_retained(DrawStream *stream)
    {
        destroy_quads_stream_gpu_data(stream, g_gpu_dispatch.quads);
        if (stream->runtime_data) acul::release(static_cast<RetainedStreamData *>(stream->runtime_data));
        if (stream->draw_sizes) acul::release(stream->draw_sizes, get_context().frames_in_flight);
        stream->runtime_data = nullptr;
        stream->stream_instances = nullptr;
        stream->draw_sizes = nullptr;
    }

    static void destroy_quads_stream_immediate(DrawStream *stream)
    {
        destroy_quads_stream_gpu_data(stream, g_gpu_dispatch.quads);
        auto *widgets_cache = static_cast<ImStreamData *>(stream->runtime_data);
        acul::release(widgets_cache, get_context().frames_in_flight);
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
    void create_quads_stream_retained(DrawStream &stream)
    {
        detail::setup_quads_stream(stream);
        stream.push_data_to_stream = detail::push_data_quads_stream_retained;
        stream.destroy = detail::destroy_quads_stream_retained;
        auto &ctx = detail::get_context();
        stream.stream_instances =
            detail::create_quads_stream_gpu_data(ctx.frames_in_flight, ctx.gpu_backend, detail::g_gpu_dispatch.quads);
        stream.runtime_data = acul::alloc<detail::RetainedStreamData>();
        stream.render = detail::render_quads_stream_retained;
    }

    void create_quads_stream_immediate(DrawPipeline *pipeline, DrawStream &stream)
    {
        detail::setup_quads_stream(stream);
        stream.push_data_to_stream = detail::g_gpu_dispatch.quads.push_data_to_quads_stream;
        stream.push_widget_to_cache = detail::push_widget_quads_stream_immediate;
        stream.destroy = detail::destroy_quads_stream_immediate;
        auto &ctx = detail::get_context();
        stream.runtime_data = acul::alloc_n<detail::ImStreamData>(ctx.frames_in_flight);
        stream.stream_instances =
            detail::create_quads_stream_gpu_data(ctx.frames_in_flight, ctx.gpu_backend, detail::g_gpu_dispatch.quads);
        stream.render = detail::render_quads_stream_immediate;
    }
} // namespace auik::v2
