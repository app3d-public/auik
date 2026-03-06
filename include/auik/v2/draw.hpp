#pragma once

#include <acul/api.hpp>
#include <acul/enum.hpp>
#include <acul/scalars.hpp>
#include <cassert>
#include <cstddef>
#include "detail/context.hpp"

namespace auik::v2
{
    class Widget;

    struct DrawPipeline;

    struct StreamFlagBits
    {
        enum enum_type : u8
        {
            none = 0x0,
            cached = 0x1,
            transient = 0x2,
            invalidate = 0x4
        };
    };

    struct DrawStream
    {
        DrawDataID (*push_data_to_stream)(DrawStream *, const void *) = nullptr;
        void (*update_data_in_stream)(DrawStream *, DrawDataID, const void *) = nullptr;
        void (*push_widget_to_cache)(DrawStream *, Widget *) = nullptr;
        void (*clear)(DrawStream *, u32) = nullptr;
        void (*render)(DrawStream *, void *, detail::GPUContext *) = nullptr;
        void (*sync_stream)(DrawStream *, u32) = nullptr;
        void (*destroy)(DrawStream *) = nullptr;

        void *stream_instances = nullptr;
        void *runtime_data = nullptr;
        DrawPipeline *pipeline = nullptr;
        u32 *draw_sizes = nullptr;
        u8 flags = StreamFlagBits::none;
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

    APPLIB_API void sync_draw_streams();

    struct DrawCtx
    {
        DrawDataID (*emit)(DrawStream *, DrawDataID &, const void *, const detail::RectData &) = nullptr;
    };

    inline void update_hit_rect(u32 &hit_id, const detail::RectData &rect, bool force_update)
    {
        auto *gpu = detail::get_context().gpu_ctx;
        assert(gpu && gpu->push_hit_rect && "GPU hover rect dispatch is not initialized");
        if (hit_id == AUIK_INVALID_DRAW_DATA_ID)
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

    inline DrawDataID emit_draw_record(DrawStream *stream, DrawDataID &draw_id, const void *data,
                                       const detail::RectData &rect)
    {
        assert(stream);
        const DrawDataID stream_id = stream->push_data_to_stream(stream, data);
        draw_id.render_id = stream_id.render_id;
        update_hit_rect(draw_id.hit_id, rect, true);
        return draw_id;
    }

    inline DrawDataID emit_draw_update(DrawStream *stream, DrawDataID &draw_id, const void *data,
                                       const detail::RectData &rect)
    {
        assert(stream);
        assert(draw_id.render_id != AUIK_INVALID_DRAW_DATA_ID && "Update called before record");
        stream->update_data_in_stream(stream, draw_id, data);
        update_hit_rect(draw_id.hit_id, rect, false);
        return draw_id;
    }

    APPLIB_API void sync_clip_rect_cache();
    APPLIB_API void sync_hit_rect_cache();
} // namespace auik::v2
