#pragma once

#include <acul/api.hpp>
#include <acul/pair.hpp>
#include <acul/disposal_queue.hpp>
#include <acul/event.hpp>
#include "fwd.hpp"

namespace auik::v2
{
    namespace detail
    {
        extern APPLIB_API struct Context
        {
            acul::events::dispatcher *ed = nullptr;
            acul::disposal_queue *disposal_queue = nullptr;
            acul::vector<auik::v2::Widget *> widget_tree;
            void *gpu_backend = nullptr;
            acul::point2D<i32> window_size;
            u32 frame_id = 0;
            u32 frames_in_flight = 0;
            Theme *theme = nullptr;
            struct
            {
                DrawStream *default_quad_stream = nullptr;
                DrawStream *default_image_stream = nullptr;
                DrawStream *overlay_quad_stream = nullptr;
            } streams;
        } *g_context;

        inline Context &get_context()
        {
            assert(g_context && "auik context is not initialized");
            return *g_context;
        }

    } // namespace detail

    inline Theme *get_theme() { return detail::get_context().theme; }
    inline void set_theme(Theme *theme) { detail::get_context().theme = theme; }

    inline DrawStream *get_default_quad_stream() { return detail::get_context().streams.default_quad_stream; }
    inline void set_default_quad_stream(DrawStream *stream)
    {
        detail::get_context().streams.default_quad_stream = stream;
    }

    inline DrawStream *get_default_image_stream() { return detail::get_context().streams.default_image_stream; }
    inline void set_default_image_stream(DrawStream *stream)
    {
        detail::get_context().streams.default_image_stream = stream;
    }

    inline DrawStream *get_overlay_quad_stream() { return detail::get_context().streams.overlay_quad_stream; }
    inline void set_overlay_quad_stream(DrawStream *stream)
    {
        detail::get_context().streams.overlay_quad_stream = stream;
    }
} // namespace auik::v2
