#pragma once

#include <acul/api.hpp>
#include <acul/memory/alloc.hpp>
#include <acul/scalars.hpp>
#include <cassert>
#include "detail/context.hpp"

#define AUIK_MAIN_LAYER      0
#define AUIK_RETAINED_STREAM 0

namespace auik::v2
{
    class Widget;

    struct DrawPipeline;

    struct DrawStream
    {
        void (*push_data_to_stream)(DrawStream *, void *) = nullptr;
        void (*push_widget_to_cache)(DrawStream *, Widget *) = nullptr;
        void (*render)(DrawStream *, void *, void *) = nullptr;
        void (*destroy)(DrawStream *) = nullptr;

        void *stream_instances = nullptr;
        void *runtime_data = nullptr;
        DrawPipeline *pipeline = nullptr;
        u32 *draw_sizes = nullptr;
    };

    struct DrawLayer
    {
        u32 subpass = 0;
        DrawStream *streams = nullptr;
        u32 stream_count = 0;
    };

    inline void push_data_to_stream(DrawStream *stream, void *data)
    {
        assert(stream->push_data_to_stream);
        stream->push_data_to_stream(stream, data);
    }

    inline void push_widget_to_cache(DrawStream *stream, Widget *widget)
    {
        assert(stream->push_widget_to_cache);
        stream->push_widget_to_cache(stream, widget);
    }

    inline void render_stream(DrawStream *stream, void *render_ctx)
    {
        assert(stream && stream->render);
        stream->render(stream, render_ctx, detail::get_context().gpu_backend);
    }

    inline void next_frame_id()
    {
        auto &ctx = detail::get_context();
        ctx.frame_id = (ctx.frame_id + 1) % ctx.frames_in_flight;
    }

    inline void render_layer(const DrawLayer &layer, void *render_ctx, u32 frame_id)
    {
        for (u32 stream_id = 0; stream_id < layer.stream_count; stream_id++)
        {
            auto &stream = layer.streams[stream_id];
            render_stream(&stream, render_ctx);
        }
    }

    inline void render_all_layers(DrawLayer *layers, u32 layer_count, void *render_ctx)
    {
        auto &ctx = detail::get_context();
        for (u32 layer_id = 0; layer_id < layer_count; layer_id++)
            render_layer(layers[layer_id], render_ctx, ctx.frame_id);
        next_frame_id();
    }
} // namespace auik::v2
