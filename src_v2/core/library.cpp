#include <cstring>
#include <auik/v2/auik.hpp>
#include <auik/v2/detail/context.hpp>
#include <auik/v2/detail/depth.hpp>
#include <auik/v2/detail/events.hpp>
#include <auik/v2/detail/gpu_context.hpp>
#include <auik/v2/widgets/image.hpp>
#include <auik/v2/widgets/tooltip.hpp>
#include <freetype/freetype.h>

namespace auik::v2
{
    namespace
    {
        static void clear_all_streams(detail::Context &ctx)
        {
            for (u32 stream_id = 0; stream_id < ctx.streams.stream_count; ++stream_id)
            {
                auto &stream = ctx.streams.attached_streams[stream_id];
                if (stream.draw_sizes[ctx.frame_id] <= 0) continue;
                clear_draw_stream(&stream, ctx.frame_id);
            }
        }

        static void destroy_cached_images(detail::Context &ctx)
        {
            acul::vector<Image *> owned_images;
            for (auto it = ctx.image_cache.begin(); it != ctx.image_cache.end(); ++it)
            {
                Image *image = it->second;
                if (!image) continue;

                bool already_added = false;
                for (auto *owned : owned_images)
                {
                    if (owned != image) continue;
                    already_added = true;
                    break;
                }
                if (!already_added) owned_images.push_back(image);
            }

            ctx.image_cache.clear();
            for (auto *image : owned_images) acul::release(image);
        }

        static void reset_clip_rects()
        {
            auto &ctx = detail::get_context();
            reset_gpu_clip_rects();
            clear_hit_rects();
            for (Widget *widget : ctx.widget_tree)
            {
                if (!widget) continue;
                widget->rebuild_clip_rects();
            }
        }
    } // namespace

    namespace detail
    {
        Context *g_context = nullptr;
    }

    APPLIB_API u32 get_service_pipelines_count() { return 1; }
    APPLIB_API u32 get_default_streams_pipelines_count() { return 2; }
    APPLIB_API u32 get_default_streams_count() { return 2; }

    bool init_library(const CreateInfo &create_info)
    {
        if (detail::g_context) destroy_library();
        detail::g_context = acul::alloc<detail::Context>();
        auto &ctx = detail::get_context();
        ctx.ed = create_info.ed;
        ctx.image_cache.clear();
        ctx.tooltip = nullptr;
        ctx.transient_cache.clear();
        ctx.streams.attached_streams = create_info.streams;
        ctx.streams.stream_count = create_info.streams_count;
        const u32 default_streams_count = get_default_streams_count();
        ctx.streams.default_streams = acul::alloc_n<DrawStream *>(default_streams_count);
        std::memset(ctx.streams.default_streams, 0, sizeof(DrawStream *) * default_streams_count);
        ctx.window_ctx = create_info.window_ctx;
        ctx.gpu_ctx = create_info.gpu_ctx;
        if (FT_Init_FreeType(&ctx.ft_library) != 0)
        {
            acul::release(ctx.streams.default_streams);
            acul::release(detail::g_context);
            detail::g_context = nullptr;
            return false;
        }
        ctx.host_refresh_request = create_info.host_refresh_request;
        ctx.pending_filter = create_info.pending_filter;
        detail::init_atlas_state(ctx.atlas_state);
        ctx.raw_mouse_mode = false;
        ctx.frames_in_flight = create_info.frames_in_flight;
        ctx.max_textures_size = create_info.max_textures_size;
        auto &io = ctx.io;
        io.display_size = {0.0f, 0.0f};
        io.mouse_pos = {0.0f, 0.0f};
        io.last_click_pos = {0.0f, 0.0f};
        io.last_drag_pos = {0.0f, 0.0f};
        io.last_click_time = -1.0;
        io.click_count = 0;
        io.click_streak = 0;
        io.clicked_id = {};
        io.drag_id = {};
        io.mouse_down = false;
        auto &frame_cache = ctx.frame_cache;
        frame_cache.changes = detail::FrameChangesBits::none;
        frame_cache.drag_widget_id = 0;
        frame_cache.drag_delta = {0.0f, 0.0f};
        frame_cache.scroll_delta = {0.0f, 0.0f};
        frame_cache.char_code = 0;
        frame_cache.char_repeat_count = 0;
        ctx.hover_id = {};
        ctx.hover_hitbox_zone = detail::HitboxZoneBits::none;
        ctx.active_id = 0;
        ctx.focus_id = 0;
        ctx.screen_cursor = {0.0f, 0.0f};
        ctx.window_ctx = create_info.window_ctx;
        detail::construct_window_backend(ctx.window_ctx);
        ctx.dirty_flags = DirtyFlagBits::redraw | DirtyFlagBits::layout;
        detail::construct_shared_buffer_sync_state(ctx.shared_sync_state[AUIK_SYNC_CLIP_RECT], ctx.frames_in_flight);
        detail::construct_shared_buffer_sync_state(ctx.shared_sync_state[AUIK_SYNC_HIT_RECT], ctx.frames_in_flight);
        if (detail::create_gpu_resources(ctx.gpu_ctx)) return true;
        destroy_library();
        return false;
    }

    void destroy_library()
    {
        if (!detail::g_context) return;
        for (auto *widget : detail::g_context->widget_tree) acul::release(widget);
        detail::destroy_atlas_state(detail::g_context->atlas_state);
        destroy_cached_images(*detail::g_context);
        detail::destroy_shared_buffer_sync_state(detail::g_context->shared_sync_state[AUIK_SYNC_CLIP_RECT]);
        detail::destroy_shared_buffer_sync_state(detail::g_context->shared_sync_state[AUIK_SYNC_HIT_RECT]);
        if (detail::g_context->ft_library)
        {
            FT_Done_FreeType(detail::g_context->ft_library);
            detail::g_context->ft_library = nullptr;
        }
        detail::destroy_gpu_context(detail::g_context->gpu_ctx);
        detail::destroy_window_context(detail::g_context->window_ctx);
        acul::release(detail::g_context->streams.default_streams);
        acul::release(detail::g_context);
        detail::g_context = nullptr;
    }

    void record_layout_commands()
    {
        auto &ctx = detail::get_context();
        if (!(ctx.dirty_flags & DirtyFlagBits::layout)) return;
        ctx.dirty_flags |= DirtyFlagBits::redraw;
        const bool need_hit_rect_draw = ctx.dirty_flags & DirtyFlagBits::hit_rect_draw;
        assert(detail::is_hit_rects_frame_synced(ctx.frame_id) &&
               "record_layout_commands() started with stale current-frame hit rect cache");
        reset_clip_rects();
        clear_all_streams(ctx);
        for (Widget *widget : ctx.widget_tree)
        {
            widget->update_layout(false);
            widget->record_draw_commands(DrawReasonBits::layout);
        }
        ctx.dirty_flags &= ~DirtyFlagBits::layout;
        if (need_hit_rect_draw)
        {
            ctx.dirty_flags &= ~DirtyFlagBits::hit_rect_draw;
            ctx.dirty_flags |= DirtyFlagBits::hit_rect_sync;
        }
    }

    void redraw_all_commands()
    {
        auto &ctx = detail::get_context();
        if (ctx.dirty_flags & DirtyFlagBits::layout) return;
        ctx.dirty_flags |= DirtyFlagBits::redraw | DirtyFlagBits::hit_rect_draw;
        assert(detail::is_hit_rects_frame_synced(ctx.frame_id) &&
               "redraw_all_commands() started with stale current-frame hit rect cache");
        clear_hit_rects();
        clear_all_streams(ctx);
        for (Widget *widget : ctx.widget_tree) widget->record_draw_commands(DrawReasonBits::full_redraw);
        ctx.dirty_flags &= ~DirtyFlagBits::hit_rect_draw;
        ctx.dirty_flags |= DirtyFlagBits::hit_rect_sync;
    }

    APPLIB_API void sync_clip_rect_cache()
    {
        auto &ctx = detail::get_context();
        auto &state = detail::get_clip_rects_sync_state();
        const u32 frame_id = ctx.frame_id;
        assert(state.buffer_versions);
        if (state.buffer_versions[frame_id] == state.master_version)
        {
            if (state.invalidation_count == 0) ctx.dirty_flags &= ~DirtyFlagBits::clip_rect;
            return;
        }
        copy_clip_rects_frame(ctx.gpu_ctx, frame_id, state.master_id);
        state.buffer_versions[frame_id] = state.master_version;
        if (state.invalidation_count > 0) --state.invalidation_count;
        if (state.invalidation_count == 0) state.stage_version = state.master_version;
        if (state.invalidation_count == 0) ctx.dirty_flags &= ~DirtyFlagBits::clip_rect;
    }

    APPLIB_API void sync_draw_streams()
    {
        auto &ctx = detail::get_context();
        bool is_any_stream_invalidated = false;
        for (u32 i = 0; i < ctx.streams.stream_count; ++i)
        {
            auto &stream = ctx.streams.attached_streams[i];
            if (!(stream.flags & StreamFlagBits::invalidate)) continue;
            if (stream.sync_stream) stream.sync_stream(&stream, ctx.frame_id);
            is_any_stream_invalidated = is_any_stream_invalidated || stream.flags & StreamFlagBits::invalidate;
        }
        if (!is_any_stream_invalidated) ctx.dirty_flags &= ~DirtyFlagBits::streams;
    }

    APPLIB_API void sync_hit_rect_cache()
    {
        auto &ctx = detail::get_context();
        auto *gpu = ctx.gpu_ctx;
        assert(gpu && "GPU context is not initialized");
        auto &state = detail::get_hit_rects_sync_state();
        const u32 frame_id = ctx.frame_id;
        if (state.buffer_versions[frame_id] == state.master_version)
        {
            if (state.invalidation_count == 0) ctx.dirty_flags &= ~DirtyFlagBits::hit_rect_sync;
            return;
        }
        copy_hit_rects_frame(gpu, frame_id, state.master_id);
        state.buffer_versions[frame_id] = state.master_version;
        if (state.invalidation_count > 0) --state.invalidation_count;
        if (state.invalidation_count == 0) state.stage_version = state.master_version;
        if (state.invalidation_count == 0) ctx.dirty_flags &= ~DirtyFlagBits::hit_rect_sync;
    }

    APPLIB_API void add_widget_to_root(Widget *widget)
    {
        assert(widget && "widget is null");
        assert(widget->parent() == nullptr && "Root widget must not have a parent");
        auto &ctx = detail::get_context();
        ctx.widget_tree.push_back(widget);
        if (widget->widget_flags & WidgetFlagBits::attachable) widget->on_attach();
        const auto zone = detail::get_depth_zone_by_flags(widget->widget_flags);
        const int lane_index = ctx.root_depth_counts[zone];
        assert(lane_index < 32 && "Max depth zone exceeded");
        widget->update_depth(detail::get_root_depth_range(zone, lane_index));
        ++ctx.root_depth_counts[zone];
        widget->update_style();
        widget->update_layout(false);
        widget->record_draw_commands(DrawReasonBits::full_redraw);
        ctx.dirty_flags |= DirtyFlagBits::redraw;
    }

    APPLIB_API void push_widget_to_transient_cache(Widget *widget)
    {
        assert(widget && "widget is null");
        auto &ctx = detail::get_context();
        for (Widget *cached_widget : ctx.transient_cache)
        {
            if (cached_widget != widget) continue;
            return;
        }

        ctx.transient_cache.push_back(widget);
        detail::mark_host_refresh_request();
    }

    APPLIB_API bool erase_widget_from_transient_cache(Widget *widget)
    {
        assert(widget && "widget is null");
        auto &ctx = detail::get_context();
        for (size_t i = 0; i < ctx.transient_cache.size(); ++i)
        {
            if (ctx.transient_cache[i] != widget) continue;
            ctx.transient_cache.erase(ctx.transient_cache.begin() + i);
            detail::mark_host_refresh_request();
            return true;
        }
        return false;
    }

    APPLIB_API void show_tooltip(f32 x, const acul::string *text_source)
    {
        if (!text_source || text_source->empty())
        {
            hide_tooltip();
            return;
        }

        auto &ctx = detail::get_context();
        if (!ctx.tooltip)
        {
            ctx.tooltip = make_tooltip();
            add_widget_to_root(ctx.tooltip);
            ctx.tooltip->hide();
        }

        ctx.tooltip->show_at(x, text_source);
        push_widget_to_transient_cache(ctx.tooltip);
    }

    APPLIB_API void hide_tooltip()
    {
        auto &ctx = detail::get_context();
        if (!ctx.tooltip) return;

        erase_widget_from_transient_cache(ctx.tooltip);
        if (!ctx.tooltip->is_visible()) return;

        ctx.tooltip->hide();
        redraw_all_commands();
        detail::mark_host_refresh_request();
    }

    APPLIB_API void clear_tooltip_if_source(const acul::string *text_source)
    {
        auto &ctx = detail::get_context();
        if (!ctx.tooltip || !text_source) return;
        ctx.tooltip->clear_if_source(text_source);
        erase_widget_from_transient_cache(ctx.tooltip);
    }
} // namespace auik::v2
