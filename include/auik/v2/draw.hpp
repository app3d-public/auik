#pragma once

#include <acul/api.hpp>
#include <acul/scalars.hpp>
#include <cassert>
#include "detail/context.hpp"

namespace auik::v2
{
    class Widget;

    struct DrawPipeline;

    struct DrawStream
    {
        DrawDataID (*push_data_to_stream)(DrawStream *, const void *) = nullptr;
        void (*update_data_in_stream)(DrawStream *, DrawDataID, const void *) = nullptr;
        void (*push_widget_to_cache)(DrawStream *, Widget *) = nullptr;
        void (*clear)(DrawStream *, u32) = nullptr;
        void (*render)(DrawStream *, void *, detail::GPUContext *) = nullptr;
        void (*destroy)(DrawStream *) = nullptr;

        void *stream_instances = nullptr;
        void *runtime_data = nullptr;
        DrawPipeline *pipeline = nullptr;
        u32 *draw_sizes = nullptr;
    };

    inline DrawDataID push_data_to_stream(DrawStream *stream, void *data)
    {
        assert(stream->push_data_to_stream);
        return stream->push_data_to_stream(stream, data);
    }

    inline void update_data_in_stream(DrawStream *stream, DrawDataID draw_data_id, void *data)
    {
        assert(stream->update_data_in_stream);
        stream->update_data_in_stream(stream, draw_data_id, data);
    }

    inline void push_widget_to_cache(DrawStream *stream, Widget *widget)
    {
        assert(stream->push_widget_to_cache);
        stream->push_widget_to_cache(stream, widget);
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
        DrawDataID (*emit)(DrawStream *, DrawDataID &, const void *) = nullptr;
    };

    inline DrawDataID emit_draw_record(DrawStream *stream, DrawDataID &draw_id, const void *data)
    {
        assert(stream);
        draw_id = stream->push_data_to_stream(stream, data);
        return draw_id;
    }

    inline DrawDataID emit_draw_update(DrawStream *stream, DrawDataID &draw_id, const void *data)
    {
        assert(stream);
        assert(draw_id != AUIK_INVALID_DRAW_DATA_ID && "Update called before record");
        stream->update_data_in_stream(stream, draw_id, data);
        return draw_id;
    }
} // namespace auik::v2
