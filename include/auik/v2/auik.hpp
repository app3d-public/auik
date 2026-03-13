#pragma once

#include <acul/disposal_queue.hpp>
#include <acul/event.hpp>
#include <utility>
#include "detail/context.hpp"
#include "detail/events.hpp"
#include "draw.hpp"
#include "pending_filter.hpp"
#include "widgets/widget.hpp"

namespace auik::v2
{
    struct CreateInfo
    {
        acul::events::dispatcher *ed = nullptr;
        DrawStream *streams = nullptr;
        u32 streams_count = 0;
        detail::GPUContext *gpu_ctx = nullptr;
        detail::WindowContext *window_ctx = nullptr;
        u32 frames_in_flight = 0;
        std::atomic_bool *host_refresh_request = nullptr;
        PendingFilter *pending_filter = nullptr;

        CreateInfo &set_event_dispatcher(acul::events::dispatcher *ed)
        {
            this->ed = ed;
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

        CreateInfo &set_host_refresh_request(std::atomic_bool *host_refresh_request)
        {
            this->host_refresh_request = host_refresh_request;
            return *this;
        }

        CreateInfo &set_pending_filter(PendingFilter *pending_filter)
        {
            this->pending_filter = pending_filter;
            return *this;
        }
    };

    APPLIB_API bool init_library(const CreateInfo &create_info);
    APPLIB_API void destroy_library();
    APPLIB_API void record_all_commands();
    APPLIB_API void add_widget_to_root(Widget *widget);
    APPLIB_API void sync_draw_streams();
    APPLIB_API void sync_clip_rect_cache();
    APPLIB_API void sync_hit_rect_cache();
    inline void sync_gpu_cache();
    template <class F>
    inline bool add_render_command(Widget *widget, F &&fn);

    inline void set_window_size(const amal::vec2 &size) { detail::get_io().display_size = size; }

    inline void sync_pending_events()
    {
        auto &ctx = detail::get_context();
        auto *pf = ctx.pending_filter;
        if (!pf || !pf->allow()) return;
        if (pf->has(PendingMaskBits::resize)) ctx.dirty_flags |= DirtyFlagBits::layout;
        if (pf->has(PendingMaskBits::mouse_move)) detail::on_mouse_move({0, 0});
    }

    inline void sync_frame()
    {
        auto &ctx = detail::get_context();
        if (ctx.pending_filter && ctx.pending_filter->mask != PendingMaskBits::none) sync_pending_events();
        detail::clear_widget_pending_bits();
        if (ctx.dirty_flags & DirtyFlagBits::hit_rect_sync) auik::v2::sync_hit_rect_cache();
        if (!ctx.disposal_queue.is_main_queue_empty()) ctx.disposal_queue.flush_main_queue();
    }

    inline void next_frame(void *sync_ctx)
    {
        auto &ctx = detail::get_context();
        auto &io = detail::get_io();
        detail::update_hover_id(ctx.gpu_ctx, sync_ctx);
        detail::new_window_frame(ctx.window_ctx);
        io.drag_delta = {0.0f, 0.0f};
        ctx.screen_cursor = {0.0f, 0.0f};
        ctx.frame_id = (ctx.frame_id + 1) % ctx.frames_in_flight;
        if (!(ctx.dirty_flags & DirtyFlagBits::hover_update)) ctx.dirty_flags &= ~DirtyFlagBits::redraw;
        ctx.dirty_flags &= ~(DirtyFlagBits::hover_update | DirtyFlagBits::host_update | DirtyFlagBits::hit_rect_update);
    }

    inline void sync_gpu_cache()
    {
        auto &ctx = detail::get_context();
        if (ctx.dirty_flags & DirtyFlagBits::layout) record_all_commands();
        else if (ctx.dirty_flags & DirtyFlagBits::clip_rect) sync_clip_rect_cache();
        if (ctx.dirty_flags & DirtyFlagBits::streams) sync_draw_streams();
    }

    inline bool is_dirty_render()
    {
        return detail::get_context().dirty_flags & (DirtyFlagBits::redraw | DirtyFlagBits::layout);
    }

    inline bool is_dirty_layout() { return detail::get_context().dirty_flags & DirtyFlagBits::layout; }

    inline bool is_dirty_stream() { return detail::get_context().dirty_flags & DirtyFlagBits::streams; }

    inline bool is_dirty_hit_rect() { return detail::get_context().dirty_flags & DirtyFlagBits::hit_rect_sync; }

    inline bool is_dirty_clip_rect() { return detail::get_context().dirty_flags & DirtyFlagBits::clip_rect; }

    inline bool is_host_update_pending() { return detail::get_context().dirty_flags & DirtyFlagBits::host_update; }

    template <class F>
    inline bool add_render_command(Widget *widget, F &&fn)
    {
        assert(widget && "widget is null");
        auto &ctx = detail::get_context();
        u32 slot_id = widget->render_slot_id();
        if (slot_id == AUIK_INVALID_RENDER_SLOT_ID) return false;
        const u32 word_id = slot_id >> 6;
        while (ctx.pending_task_bits.size() <= word_id) ctx.pending_task_bits.push_back(0ull);
        const u64 bit = (1ull << (slot_id & 63));
        if (ctx.pending_task_bits[word_id] & bit) return false;
        ctx.pending_task_bits[word_id] |= bit;
        ctx.disposal_queue.emplace(std::forward<F>(fn));
        return true;
    }
} // namespace auik::v2
