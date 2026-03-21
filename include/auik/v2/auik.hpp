#pragma once

#include <acul/disposal_queue.hpp>
#include <acul/event.hpp>
#include <acul/functional/unique_function.hpp>
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
        bool *host_refresh_request = nullptr;
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

        CreateInfo &set_host_refresh_request(bool *host_refresh_request)
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
    APPLIB_API void record_layout_commands();
    APPLIB_API void redraw_all_commands();
    APPLIB_API void add_widget_to_root(Widget *widget);
    APPLIB_API void sync_draw_streams();
    APPLIB_API void sync_clip_rect_cache();
    APPLIB_API void sync_hit_rect_cache();
    inline void sync_gpu_cache();
    template <class Traits, class F>
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
        if (ctx.dirty_flags & DirtyFlagBits::hit_rect_sync) auik::v2::sync_hit_rect_cache();
        detail::flush_frame_changes();
        if (!ctx.disposal_queue.is_main_queue_empty()) ctx.disposal_queue.flush_main_queue();
    }

    inline void next_frame(void *sync_ctx)
    {
        auto &ctx = detail::get_context();
        detail::update_hover_id(ctx.gpu_ctx, sync_ctx);
        detail::new_window_frame(ctx.window_ctx);
        ctx.screen_cursor = {0.0f, 0.0f};
        ctx.frame_id = (ctx.frame_id + 1) % ctx.frames_in_flight;
        if (!(ctx.dirty_flags & DirtyFlagBits::hover_update)) ctx.dirty_flags &= ~DirtyFlagBits::redraw;
        ctx.dirty_flags &= ~(DirtyFlagBits::hover_update | DirtyFlagBits::host_update | DirtyFlagBits::hit_rect_update);
    }

    inline void sync_gpu_cache()
    {
        auto &ctx = detail::get_context();
        if (ctx.dirty_flags & DirtyFlagBits::layout) record_layout_commands();
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

    inline void set_raw_mouse_mode(bool value) { detail::get_context().raw_mouse_mode = value; }

    inline bool is_raw_mouse_mode() { return detail::get_context().raw_mouse_mode; }

    inline HostWindowState get_host_window_state() { return detail::get_window_context()->host_state; }

    template <class F>
    inline void register_shortcut(const Shortcut &shortcut, F &&fn)
    {
        auto &ctx = detail::get_context();
        const u64 shortcut_hash = detail::make_shortcut_hash(shortcut.keys, shortcut.mouse, shortcut.mods, 0);
        ctx.io.shortcuts[shortcut_hash] = acul::unique_function<void()>(std::forward<F>(fn));
    }

    template <class F>
    inline void register_shortcut(u32 widget_id, const Shortcut &shortcut, F &&fn)
    {
        auto &ctx = detail::get_context();
        const u64 shortcut_hash = detail::make_shortcut_hash(shortcut.keys, shortcut.mouse, shortcut.mods, widget_id);
        ctx.io.shortcuts[shortcut_hash] = acul::unique_function<void()>(std::forward<F>(fn));
        auto &hashes = ctx.io.widget_shortcuts[widget_id];
        bool exists = false;
        for (u64 hash : hashes)
        {
            if (hash != shortcut_hash) continue;
            exists = true;
            break;
        }
        if (!exists) hashes.push_back(shortcut_hash);
        auto it = ctx.id_map.find(widget_id);
        if (it != ctx.id_map.end() && it->second) it->second->add_event_flags(EventFlagBits::shortcut);
    }

    inline void deregister_shortcut(const Shortcut &shortcut)
    {
        auto &ctx = detail::get_context();
        const u64 shortcut_hash = detail::make_shortcut_hash(shortcut.keys, shortcut.mouse, shortcut.mods, 0);
        ctx.io.shortcuts.erase(shortcut_hash);
    }

    inline void deregister_shortcut(u32 widget_id, const Shortcut &shortcut)
    {
        auto &ctx = detail::get_context();
        const u64 shortcut_hash = detail::make_shortcut_hash(shortcut.keys, shortcut.mouse, shortcut.mods, widget_id);
        ctx.io.shortcuts.erase(shortcut_hash);

        auto hashes_it = ctx.io.widget_shortcuts.find(widget_id);
        if (hashes_it != ctx.io.widget_shortcuts.end())
        {
            auto &hashes = hashes_it->second;
            for (size_t i = 0; i < hashes.size(); ++i)
            {
                if (hashes[i] != shortcut_hash) continue;
                hashes.erase(hashes.begin() + i);
                break;
            }
            if (hashes.empty()) ctx.io.widget_shortcuts.erase(hashes_it);
        }

        auto it = ctx.id_map.find(widget_id);
        if (it != ctx.id_map.end() && it->second && ctx.io.widget_shortcuts.find(widget_id) == ctx.io.widget_shortcuts.end())
            it->second->remove_event_flags(EventFlagBits::shortcut);
    }

    inline void deregister_shortcuts(u32 widget_id) { detail::deregister_widget_shortcuts(widget_id); }

    template <class Traits, class F>
    inline bool add_render_command(Widget *widget, F &&fn)
    {
        if constexpr (std::is_same_v<typename Traits::category, detail::immediate_event_traits_tag>)
        {
            fn();
            return true;
        }

        assert(widget && "widget is null");
        auto &ctx = detail::get_context();
        ctx.disposal_queue.emplace(std::forward<F>(fn));
        detail::mark_host_refresh_request();
        return true;
    }
} // namespace auik::v2
