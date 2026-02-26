#pragma once

#include <acul/api.hpp>
#include <acul/disposal_queue.hpp>
#include <acul/enum.hpp>
#include <acul/event.hpp>
#include <acul/pair.hpp>
#include "fwd.hpp"

namespace auik::v2
{
    struct DirtyFlagBits
    {
        enum enum_type
        {
            none = 0x0,
            render = 0x1,
            layout = 0x2
        };
        using flag_bitmask = std::true_type;
    };

    using DirtyFlags = acul::flags<DirtyFlagBits>;

    namespace detail
    {
        extern APPLIB_API struct Context
        {
            acul::events::dispatcher *ed = nullptr;
            acul::disposal_queue *disposal_queue = nullptr;
            acul::vector<auik::v2::Widget *> widget_tree;
            GPUContext *gpu_ctx = nullptr;
            WindowContext *window_ctx = nullptr;
            acul::point2D<i32> window_size;
            u32 frame_id = 0;
            u32 frames_in_flight = 0;
            DirtyFlags dirty_flags = DirtyFlagBits::none;
            Theme *theme = nullptr;
            struct
            {
                DrawStream *attached_streams = nullptr;
                u32 stream_count = 0;
                DrawStream *primary_quad_stream = nullptr;
                DrawStream *primary_image_stream = nullptr;
                DrawStream *overlay_quad_stream = nullptr;
            } streams;
        } *g_context;

        inline Context &get_context()
        {
            assert(g_context && "auik context is not initialized");
            return *g_context;
        }

        APPLIB_API WindowContext *create_window_context();

        inline WindowContext *get_window_context()
        {
            auto *ctx = get_context().window_ctx;
            assert(ctx && "auik window context is not initialized");
            return ctx;
        }

    } // namespace detail

    inline Theme *get_theme() { return detail::get_context().theme; }
    inline void set_theme(Theme *theme) { detail::get_context().theme = theme; }

    inline DrawStream *get_primary_quad_stream() { return detail::get_context().streams.primary_quad_stream; }
    inline void set_primary_quad_stream(DrawStream *stream)
    {
        detail::get_context().streams.primary_quad_stream = stream;
    }

    inline DrawStream *get_primary_image_stream() { return detail::get_context().streams.primary_image_stream; }
    inline void set_primary_image_stream(DrawStream *stream)
    {
        detail::get_context().streams.primary_image_stream = stream;
    }

    inline DrawStream *get_overlay_quad_stream() { return detail::get_context().streams.overlay_quad_stream; }

    inline void set_overlay_quad_stream(DrawStream *stream)
    {
        detail::get_context().streams.overlay_quad_stream = stream;
    }
} // namespace auik::v2
