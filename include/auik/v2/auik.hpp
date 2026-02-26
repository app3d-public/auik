#pragma once

#include <acul/disposal_queue.hpp>
#include <acul/event.hpp>
#include "detail/context.hpp"
#include "detail/events.hpp"

namespace auik::v2
{
    struct CreateInfo
    {
        acul::events::dispatcher *ed = nullptr;
        acul::disposal_queue *disposal_queue = nullptr;
        DrawStream *streams = nullptr;
        u32 streams_count = 0;
        detail::GPUContext *gpu_ctx = nullptr;
        detail::WindowContext *window_ctx = nullptr;
        u32 frames_in_flight = 0;
        acul::point2D<i32> window_size{0, 0};

        CreateInfo &set_ed(acul::events::dispatcher *ed)
        {
            this->ed = ed;
            return *this;
        }

        CreateInfo &set_disposal_queue(acul::disposal_queue *disposal_queue)
        {
            this->disposal_queue = disposal_queue;
            return *this;
        }

        CreateInfo &set_gpu_backend(detail::GPUContext *gpu_backend)
        {
            this->gpu_ctx = gpu_backend;
            return *this;
        }

        CreateInfo &set_draw_streams(DrawStream *streams, u32 streams_count)
        {
            this->streams = streams;
            this->streams_count = streams_count;
            return *this;
        }

        CreateInfo &set_window_backend(detail::WindowContext *window_backend)
        {
            this->window_ctx = window_backend;
            return *this;
        }

        CreateInfo &set_frames_in_flight(u32 frames_in_flight)
        {
            this->frames_in_flight = frames_in_flight;
            return *this;
        }

        CreateInfo &set_window_size(acul::point2D<i32> window_size)
        {
            this->window_size = window_size;
            return *this;
        }
    };

    APPLIB_API void init_library(const CreateInfo &create_info);
    APPLIB_API void destroy_library();

    inline void set_window_size(acul::point2D<i32> size) { detail::get_context().window_size = size; }

    inline void next_frame()
    {
        auto &ctx = detail::get_context();
        detail::new_window_frame(ctx.window_ctx);
        ctx.frame_id = (ctx.frame_id + 1) % ctx.frames_in_flight;
    }

    inline void mark_dirty() { detail::get_context().dirty_flags |= DirtyFlagBits::render; }

    inline void mark_layout_dirty() { detail::get_context().dirty_flags |= DirtyFlagBits::layout; }

    inline bool is_dirty_render() { return detail::get_context().dirty_flags & DirtyFlagBits::render; }

    inline bool is_dirty_layout() { return detail::get_context().dirty_flags & DirtyFlagBits::layout; }

    inline bool is_dirty() { return detail::get_context().dirty_flags != DirtyFlagBits::none; }

    inline void clear_dirty()
    {
        auto &ctx = detail::get_context();
        ctx.dirty_flags = DirtyFlagBits::none;
    }
} // namespace auik::v2
