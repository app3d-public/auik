#include <auik/auik.hpp>
#include <auik/detail/context.hpp>
#include <auik/detail/depth.hpp>
#include <auik/detail/events.hpp>
#include <auik/detail/gpu_context.hpp>
#include <auik/sound.hpp>
#include <auik/widgets/containers.hpp>
#include <auik/widgets/dockspace.hpp>
#include <auik/widgets/image.hpp>
#include <auik/widgets/slider.hpp>
#include <auik/widgets/tabbar.hpp>
#include <auik/widgets/titlebar.hpp>
#include <auik/widgets/tooltip.hpp>
#include <auik/widgets/window.hpp>
#include <freetype/freetype.h>
#include "pipelines/stream_data.hpp"

namespace auik
{
    namespace
    {
        static inline amal::vec2 get_root_overlay_depth_range() { return detail::get_global_foreground_depth_range(); }

        static void __update_root_widgets_layout(Viewport *viewport);
        static void __update_root_widgets_layout_fast(Viewport *viewport);

        static void sync_plain_viewport(Viewport *viewport)
        {
            if (!viewport) return;
            const amal::vec4 rect{viewport->rect.offset.x, viewport->rect.offset.y, viewport->rect.size.x,
                                  viewport->rect.size.y};
            viewport->clip_id = push_clip_rect(rect);
        }

        static void update_viewport_layout(Viewport *viewport)
        {
            if (!viewport) return;
            sync_viewport(viewport);
            __update_root_widgets_layout(viewport);
        }

        static bool sync_viewport_fast_update(Viewport *viewport, DrawReasonFlags reason);
        static void record_viewport_draw_commands(Viewport *viewport, DrawReasonFlags reason);

        static void sync_viewport_group(Viewport *viewport)
        {
            auto *group = static_cast<ViewportGroup *>(viewport);
            if (!group) return;
            amal::rect base = group->rect;
            if (group->top)
            {
                group->top->rect = {{base.offset.x, base.offset.y}, {base.size.x, 0.0f}};
                update_viewport_layout(group->top);
                const f32 consumed = amal::clamp(group->top->rect.size.y, 0.0f, base.size.y);
                group->top->rect = {{base.offset.x, base.offset.y}, {base.size.x, consumed}};
                sync_plain_viewport(group->top);
                base.offset.y += consumed;
                base.size.y -= consumed;
            }
            if (group->bottom)
            {
                group->bottom->rect = {{base.offset.x, base.offset.y + base.size.y}, {base.size.x, 0.0f}};
                update_viewport_layout(group->bottom);
                const f32 consumed = amal::clamp(group->bottom->rect.size.y, 0.0f, base.size.y);
                group->bottom->rect = {{base.offset.x, base.offset.y + base.size.y - consumed},
                                       {base.size.x, consumed}};
                sync_plain_viewport(group->bottom);
                __update_root_widgets_layout(group->bottom);
                base.size.y -= consumed;
            }
            if (group->left)
            {
                group->left->rect = {{base.offset.x, base.offset.y}, {0.0f, base.size.y}};
                update_viewport_layout(group->left);
                const f32 consumed = amal::clamp(group->left->rect.size.x, 0.0f, base.size.x);
                group->left->rect = {{base.offset.x, base.offset.y}, {consumed, base.size.y}};
                sync_plain_viewport(group->left);
                base.offset.x += consumed;
                base.size.x -= consumed;
            }
            if (group->right)
            {
                group->right->rect = {{base.offset.x + base.size.x, base.offset.y}, {0.0f, base.size.y}};
                update_viewport_layout(group->right);
                const f32 consumed = amal::clamp(group->right->rect.size.x, 0.0f, base.size.x);
                group->right->rect = {{base.offset.x + base.size.x - consumed, base.offset.y}, {consumed, base.size.y}};
                sync_plain_viewport(group->right);
                __update_root_widgets_layout(group->right);
                base.size.x -= consumed;
            }
            group->rect = base;
            sync_plain_viewport(group);
            __update_root_widgets_layout(group);
        }

        static bool sync_viewport_group_fast(Viewport *viewport, DrawReasonFlags reason)
        {
            auto *group = static_cast<ViewportGroup *>(viewport);
            if (!group) return false;
            amal::rect base = group->rect;
            if (group->top)
            {
                f32 consumed = amal::clamp(group->top->rect.size.y, 0.0f, base.size.y);
                if (consumed <= 0.0f)
                {
                    group->top->rect = {{base.offset.x, base.offset.y}, {base.size.x, 0.0f}};
                    update_viewport_layout(group->top);
                    consumed = amal::clamp(group->top->rect.size.y, 0.0f, base.size.y);
                }
                group->top->rect = {{base.offset.x, base.offset.y}, {base.size.x, consumed}};
                sync_plain_viewport(group->top);
                if (!sync_viewport_fast_update(group->top, reason))
                {
                    __update_root_widgets_layout_fast(group->top);
                    record_viewport_draw_commands(group->top, reason);
                }
                base.offset.y += consumed;
                base.size.y -= consumed;
            }
            if (group->bottom)
            {
                f32 consumed = amal::clamp(group->bottom->rect.size.y, 0.0f, base.size.y);
                if (consumed <= 0.0f)
                {
                    group->bottom->rect = {{base.offset.x, base.offset.y + base.size.y}, {base.size.x, 0.0f}};
                    update_viewport_layout(group->bottom);
                    consumed = amal::clamp(group->bottom->rect.size.y, 0.0f, base.size.y);
                }
                group->bottom->rect = {{base.offset.x, base.offset.y + base.size.y - consumed},
                                       {base.size.x, consumed}};
                sync_plain_viewport(group->bottom);
                if (!sync_viewport_fast_update(group->bottom, reason))
                {
                    __update_root_widgets_layout_fast(group->bottom);
                    record_viewport_draw_commands(group->bottom, reason);
                }
                base.size.y -= consumed;
            }
            if (group->left)
            {
                f32 consumed = amal::clamp(group->left->rect.size.x, 0.0f, base.size.x);
                if (consumed <= 0.0f)
                {
                    group->left->rect = {{base.offset.x, base.offset.y}, {0.0f, base.size.y}};
                    update_viewport_layout(group->left);
                    consumed = amal::clamp(group->left->rect.size.x, 0.0f, base.size.x);
                }
                group->left->rect = {{base.offset.x, base.offset.y}, {consumed, base.size.y}};
                sync_plain_viewport(group->left);
                if (!sync_viewport_fast_update(group->left, reason))
                {
                    __update_root_widgets_layout_fast(group->left);
                    record_viewport_draw_commands(group->left, reason);
                }
                base.offset.x += consumed;
                base.size.x -= consumed;
            }
            if (group->right)
            {
                f32 consumed = amal::clamp(group->right->rect.size.x, 0.0f, base.size.x);
                if (consumed <= 0.0f)
                {
                    group->right->rect = {{base.offset.x + base.size.x, base.offset.y}, {0.0f, base.size.y}};
                    update_viewport_layout(group->right);
                    consumed = amal::clamp(group->right->rect.size.x, 0.0f, base.size.x);
                }
                group->right->rect = {{base.offset.x + base.size.x - consumed, base.offset.y}, {consumed, base.size.y}};
                sync_plain_viewport(group->right);
                if (!sync_viewport_fast_update(group->right, reason))
                {
                    __update_root_widgets_layout_fast(group->right);
                    record_viewport_draw_commands(group->right, reason);
                }
                base.size.x -= consumed;
            }
            group->rect = base;
            sync_plain_viewport(group);
            __update_root_widgets_layout_fast(group);
            record_viewport_draw_commands(group, reason);
            return true;
        }

        static bool sync_viewport_fast_update(Viewport *viewport, DrawReasonFlags reason)
        {
            if (!viewport) return false;
            if (viewport->sync_viewport == &sync_viewport_group) return sync_viewport_group_fast(viewport, reason);
            sync_plain_viewport(viewport);
            return false;
        }

        static f32 resolve_root_layout_axis(f32 requested, f32 viewport_size, f32 required)
        {
            if (is_size_concrete(requested)) return requested;
            if (is_size_fill(requested)) return viewport_size;
            if (is_size_fit(requested)) return required;
            return viewport_size > 0.0f ? viewport_size : required;
        }

        static amal::vec2 resolve_root_layout_size(const Widget *widget, const amal::vec4 &viewport_rect)
        {
            const auto requested = widget->style_size();
            const auto required = widget->required_size();
            return {resolve_root_layout_axis(requested.x, viewport_rect.z, required.x),
                    resolve_root_layout_axis(requested.y, viewport_rect.w, required.y)};
        }

        static bool update_root_widget_layout(Widget *widget, Viewport *viewport, const amal::vec4 &layout_rect)
        {
            if (!widget) return false;
            assert(widget->viewport() && "Root widget viewport is not assigned");
            if (widget->viewport() != viewport) return false;
            if (widget->parent())
            {
                widget->update_layout(false);
                return false;
            }
            widget->update_layout_min_size();
            const bool is_window = widget->get_rect().id.tag_id == AUIK_TAG_WINDOW;
            if (!is_window) widget->set_position({layout_rect.x, layout_rect.y});
            widget->set_layout_size(resolve_root_layout_size(widget, layout_rect));
            widget->update_layout(true);
            return !is_window && widget->is_visible() && detail::root_widget_depth_zone(widget) == DepthZone::work;
        }

        static void __update_root_widgets_layout(Viewport *viewport)
        {
            auto &ctx = detail::get_context();
            const auto viewport_rect = get_viewport_rect(viewport);
            amal::vec4 layout_rect = viewport_rect;
            f32 consumed_h = 0.0f;
            for (Widget *widget : ctx.widget_tree)
            {
                if (!widget || widget->parent()) continue;
                if (!update_root_widget_layout(widget, viewport, layout_rect)) continue;
                const f32 consumed = amal::max(widget->bounds().size.y, 0.0f);
                layout_rect.y += consumed;
                layout_rect.w = amal::max(layout_rect.w - consumed, 0.0f);
                consumed_h += consumed;
            }
            if (viewport && viewport != get_main_viewport())
            {
                viewport->rect = {{viewport_rect.x, viewport_rect.y}, {viewport_rect.z, consumed_h}};
                sync_viewport(viewport);
            }
        }

        static bool update_widget_layout_fast(Widget *widget, Viewport *viewport, const amal::vec4 &layout_rect)
        {
            if (!widget) return false;
            assert(widget->viewport() && "Root widget viewport is not assigned");
            if (widget->viewport() != viewport) return false;
            if (widget->parent()) return false;

            const bool is_window = widget->get_rect().id.tag_id == AUIK_TAG_WINDOW;
            const amal::vec2 next_pos = is_window ? widget->position() : amal::vec2{layout_rect.x, layout_rect.y};
            const amal::vec2 next_size = resolve_root_layout_size(widget, layout_rect);
            widget->set_position(next_pos);
            widget->set_layout_size(next_size);
            widget->update_layout(true);
            return !is_window && widget->is_visible() && detail::root_widget_depth_zone(widget) == DepthZone::work;
        }

        static void __update_root_widgets_layout_fast(Viewport *viewport)
        {
            auto &ctx = detail::get_context();
            const auto viewport_rect = get_viewport_rect(viewport);
            amal::vec4 layout_rect = viewport_rect;
            f32 consumed_h = 0.0f;
            for (Widget *widget : ctx.widget_tree)
            {
                if (!widget) continue;
                if (widget->parent()) continue;
                if (!update_widget_layout_fast(widget, viewport, layout_rect)) continue;
                const f32 consumed = amal::max(widget->bounds().size.y, 0.0f);
                layout_rect.y += consumed;
                layout_rect.w = amal::max(layout_rect.w - consumed, 0.0f);
                consumed_h += consumed;
            }
            if (viewport && viewport != get_main_viewport())
            {
                viewport->rect = {{viewport_rect.x, viewport_rect.y}, {viewport_rect.z, consumed_h}};
                sync_viewport(viewport);
            }
        }

        static bool viewport_tree_contains(Viewport *root, Viewport *target)
        {
            if (!root || !target) return false;
            if (root == target) return true;
            if (root->sync_viewport != &sync_viewport_group) return false;

            auto *group = static_cast<ViewportGroup *>(root);
            return viewport_tree_contains(group->top, target) || viewport_tree_contains(group->bottom, target) ||
                   viewport_tree_contains(group->left, target) || viewport_tree_contains(group->right, target);
        }

        static void record_viewport_draw_commands(Viewport *viewport, DrawReasonFlags reason)
        {
            if (!viewport) return;
            for (Widget *widget : detail::get_context().widget_tree)
            {
                if (!widget) continue;
                if (widget->viewport() != viewport) continue;
                widget->update_draw_commands(reason);
            }
        }

        static void record_viewport_tree_draw_commands(Viewport *viewport, DrawReasonFlags reason)
        {
            if (!viewport) return;
            if (viewport->sync_viewport == &sync_viewport_group)
            {
                auto *group = static_cast<ViewportGroup *>(viewport);
                record_viewport_tree_draw_commands(group->top, reason);
                record_viewport_tree_draw_commands(group->bottom, reason);
                record_viewport_tree_draw_commands(group->left, reason);
                record_viewport_tree_draw_commands(group->right, reason);
            }
            record_viewport_draw_commands(viewport, reason);
        }

        static void compact_delayed_tasks(detail::Context &ctx)
        {
            for (size_t i = 0; i < ctx.delayed_tasks.size();)
            {
                if (ctx.delayed_tasks[i].fn) ++i;
                else ctx.delayed_tasks.erase(ctx.delayed_tasks.begin() + i);
            }
            if (ctx.delayed_tasks.empty()) ctx.dirty_flags &= ~DirtyFlagBits::delayed_tasks;
            else ctx.dirty_flags |= DirtyFlagBits::delayed_tasks;
        }

        static void clear_all_streams(detail::Context &ctx)
        {
            for (u32 stream_id = 0; stream_id < ctx.streams.stream_count; ++stream_id)
            {
                auto &stream = ctx.streams.attached_streams[stream_id];
                auto *state = static_cast<detail::StreamSyncState *>(stream.runtime_data);
                const bool can_skip_clear =
                    state && state->buffer_versions && stream.draw_sizes && state->master_id < ctx.frames_in_flight &&
                    stream.draw_sizes[ctx.frame_id] == 0 && stream.draw_sizes[state->master_id] == 0 &&
                    state->buffer_versions[ctx.frame_id] == state->master_version && state->invalidation_count == 0;
                if (can_skip_clear) continue;
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

        static void reset_clip_rect_records()
        {
            auto &ctx = detail::get_context();
            for (Widget *widget : ctx.widget_tree)
            {
                if (!widget) continue;
                widget->reset_clip_rect_records();
            }
            for (Widget *widget : ctx.transient_cache)
            {
                if (!widget) continue;
                widget->reset_clip_rect_records();
            }

            reset_gpu_clip_rects();
            clear_hit_rects();
        }

        static void rebuild_clip_rect_records()
        {
            auto &ctx = detail::get_context();
            for (Widget *widget : ctx.widget_tree)
            {
                if (!widget) continue;
                widget->rebuild_clip_rects();
            }
            for (Widget *widget : ctx.transient_cache)
            {
                if (!widget) continue;
                widget->rebuild_clip_rects();
            }
        }

        static void release_viewport_tree(Viewport *viewport)
        {
            if (!viewport) return;
            if (detail::g_context)
            {
                auto &viewports = detail::get_context().viewports;
                for (size_t i = 0; i < viewports.size();)
                {
                    if (viewports[i] == viewport) viewports.erase(viewports.begin() + i);
                    else ++i;
                }
                if (detail::get_context().main_viewport == viewport) detail::get_context().main_viewport = nullptr;
            }
            if (viewport->sync_viewport == &sync_viewport_group)
            {
                auto *group = static_cast<ViewportGroup *>(viewport);
                release_viewport_tree(group->top);
                release_viewport_tree(group->bottom);
                release_viewport_tree(group->left);
                release_viewport_tree(group->right);
                group->top = nullptr;
                group->bottom = nullptr;
                group->left = nullptr;
                group->right = nullptr;
            }
            acul::release(viewport);
        }
    } // namespace

    namespace detail
    {
        static inline Window *as_counted_root_window(Widget *widget)
        {
            if (!widget || widget->parent()) return nullptr;
            if (widget->get_rect().id.tag_id != AUIK_TAG_WINDOW) return nullptr;
            auto *window = static_cast<Window *>(widget);
            if (window->window_flags & WindowFlagBits::docked) return nullptr;
            return window;
        }

        void setup_root_window(Widget *widget)
        {
            if (!as_counted_root_window(widget)) return;
            ++get_context().root_window_count;
        }

        void teardown_root_window(Widget *widget)
        {
            if (!as_counted_root_window(widget)) return;
            auto &count = get_context().root_window_count;
            if (count > 0u) --count;
        }
    } // namespace detail

    namespace detail
    {
        Context *g_context = nullptr;

        AUIK_EXPORT u64 schedule_delayed_task_fn(u64 owner_id, f64 due_time, acul::unique_function<void()> fn)
        {
            auto &ctx = detail::get_context();
            compact_delayed_tasks(ctx);
            detail::DelayedHostTask task{};
            task.id = ctx.next_delayed_task_id++;
            task.owner_id = owner_id;
            task.due_time = due_time;
            task.fn = std::move(fn);
            ctx.delayed_tasks.push_back(std::move(task));
            ctx.dirty_flags |= DirtyFlagBits::delayed_tasks;
            detail::mark_host_refresh_request();
            return ctx.delayed_tasks.back().id;
        }
    } // namespace detail

    AUIK_EXPORT u32 get_service_pipelines_count() { return 1; }
    AUIK_EXPORT u32 get_default_streams_pipelines_count() { return 4; }
    AUIK_EXPORT u32 get_default_streams_count() { return 5; }

    bool init_library(const CreateInfo &create_info)
    {
        if (detail::g_context) destroy_library();
        detail::g_context = acul::alloc<detail::Context>();
        auto &ctx = detail::get_context();
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
        ctx.host_refresh_request = create_info.sync_options.host_refresh_request;
        ctx.pending_filter = create_info.sync_options.pending_filter;
        ctx.widget_create_options = create_info.widget_create_options;
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
        detail::reset_style_selector();
        ctx.active_id = 0;
        ctx.focus_id = 0;
        ctx.viewports.clear();
        ctx.main_viewport = nullptr;
        ctx.root_window_count = 0;
        ctx.window_ctx = create_info.window_ctx;
        detail::construct_window_backend(ctx.window_ctx);
        ctx.sound_ctx = create_info.sound_ctx ? create_info.sound_ctx : get_default_sound_context();
        init_sound_system(ctx.sound_ctx);
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
        detail::g_context->dirty_flags |= DirtyFlagBits::destroying;
        cancel_all_delayed_tasks();
        detail::g_context->disposal_queue.discard();
        for (auto *widget : detail::g_context->widget_tree) acul::release(widget);
        detail::g_context->widget_tree.clear();
        detail::g_context->transient_cache.clear();
        detail::g_context->id_map.clear();
        detail::g_context->disposal_queue.discard();
        detail::destroy_atlas_state(detail::g_context->atlas_state);
        destroy_cached_images(*detail::g_context);
        for (auto *effect : detail::g_context->post_effects)
        {
            if (!effect) continue;
            destroy_post_effect(effect);
        }
        detail::g_context->post_effects.clear();
        destroy_dockspace_context();
        while (!detail::g_context->viewports.empty()) release_viewport_tree(detail::g_context->viewports.back());
        detail::g_context->viewports.clear();
        detail::g_context->main_viewport = nullptr;
        detail::destroy_shared_buffer_sync_state(detail::g_context->shared_sync_state[AUIK_SYNC_CLIP_RECT]);
        detail::destroy_shared_buffer_sync_state(detail::g_context->shared_sync_state[AUIK_SYNC_HIT_RECT]);
        if (detail::g_context->ft_library)
        {
            FT_Done_FreeType(detail::g_context->ft_library);
            detail::g_context->ft_library = nullptr;
        }
        detail::destroy_gpu_context(detail::g_context->gpu_ctx);
        destroy_sound_system(detail::g_context->sound_ctx);
        detail::g_context->sound_ctx = nullptr;
        detail::destroy_window_context(detail::g_context->window_ctx);
        acul::release(detail::g_context->streams.default_streams);
        acul::release(detail::g_context);
        detail::g_context = nullptr;
    }

    void update_root_widgets_layout(Viewport *viewport)
    {
        auto &ctx = detail::get_context();
        ctx.dirty_flags |= DirtyFlagBits::redraw;
        __update_root_widgets_layout(viewport);
    }

    static void update_all_viewports_layout()
    {
        auto &ctx = detail::get_context();
        if (!ctx.main_viewport) return;
        const amal::vec2 display = get_display_size();
        ctx.main_viewport->rect = {{0.0f, 0.0f}, display};
        sync_viewport(ctx.main_viewport);
        for (auto *viewport : ctx.viewports)
        {
            if (!viewport || viewport == ctx.main_viewport) continue;
            if (viewport_tree_contains(ctx.main_viewport, viewport)) continue;
            update_viewport_layout(viewport);
        }
    }

    static void record_all_viewports_draw_commands(DrawReasonFlags reason)
    {
        auto &ctx = detail::get_context();
        if (!ctx.main_viewport) return;
        record_viewport_tree_draw_commands(ctx.main_viewport, reason);
        for (auto *viewport : ctx.viewports)
        {
            if (!viewport || viewport == ctx.main_viewport) continue;
            if (viewport_tree_contains(ctx.main_viewport, viewport)) continue;
            record_viewport_tree_draw_commands(viewport, reason);
        }
    }

    static bool record_fast_update_commands();

    void record_layout_commands()
    {
        auto &ctx = detail::get_context();
        if (!(ctx.dirty_flags & detail::layout_dirty_mask)) return;
        const bool use_fast_path =
            (ctx.dirty_flags & DirtyFlagBits::fast_update) && !(ctx.dirty_flags & DirtyFlagBits::locale);
        if (use_fast_path)
        {
            record_fast_update_commands();
            return;
        }
        const bool need_hit_rect_draw = ctx.dirty_flags & DirtyFlagBits::hit_rect_draw;

        sync_hit_rect_cache();
        assert(detail::is_hit_rects_frame_synced(ctx.frame_id) &&
               "record_layout_commands() started with stale current-frame hit rect cache");
        ctx.dirty_flags |= DirtyFlagBits::hit_rect_draw;
        reset_clip_rect_records();
        clear_all_streams(ctx);
        update_all_viewports_layout();
        rebuild_clip_rect_records();
        record_all_viewports_draw_commands(DrawReasonBits::layout | DrawReasonBits::record);
        detail::unmark_layout_dirty();
        if (need_hit_rect_draw || (ctx.dirty_flags & DirtyFlagBits::hit_rect_draw))
        {
            ctx.dirty_flags &= ~DirtyFlagBits::hit_rect_draw;
            ctx.dirty_flags |= DirtyFlagBits::hit_rect_sync;
        }
    }

    static bool record_fast_update_commands()
    {
        auto &ctx = detail::get_context();
        if (!ctx.main_viewport) return false;

        const bool need_hit_rect_draw = ctx.dirty_flags & DirtyFlagBits::hit_rect_draw;
        sync_hit_rect_cache();

        const amal::vec2 display = get_display_size();
        ctx.main_viewport->rect = {{0.0f, 0.0f}, display};
        if (!sync_viewport_fast_update(ctx.main_viewport, DrawReasonBits::layout))
        {
            __update_root_widgets_layout_fast(ctx.main_viewport);
            record_viewport_draw_commands(ctx.main_viewport, DrawReasonBits::layout);
        }
        for (auto *viewport : ctx.viewports)
        {
            if (!viewport || viewport == ctx.main_viewport) continue;
            if (viewport_tree_contains(ctx.main_viewport, viewport)) continue;
            if (!sync_viewport_fast_update(viewport, DrawReasonBits::layout))
            {
                __update_root_widgets_layout_fast(viewport);
                record_viewport_draw_commands(viewport, DrawReasonBits::layout);
            }
        }

        detail::unmark_layout_dirty();
        ctx.dirty_flags |= DirtyFlagBits::redraw;
        if (need_hit_rect_draw)
        {
            ctx.dirty_flags &= ~DirtyFlagBits::hit_rect_draw;
            ctx.dirty_flags |= DirtyFlagBits::hit_rect_sync;
        }
        return true;
    }

    Viewport *make_viewport()
    {
        auto *viewport = acul::alloc<Viewport>();
        viewport->sync_viewport = &sync_plain_viewport;
        return viewport;
    }

    ViewportGroup *make_viewport_group()
    {
        auto *viewport = acul::alloc<ViewportGroup>();
        viewport->sync_viewport = &sync_viewport_group;
        return viewport;
    }

    void add_viewport(Viewport *viewport)
    {
        if (!viewport) return;
        auto &viewports = detail::get_context().viewports;
        for (auto *item : viewports)
            if (item == viewport) return;
        viewports.push_back(viewport);
    }

    bool destroy_viewport(Viewport *viewport)
    {
        if (!viewport) return false;
        auto &ctx = detail::get_context();
        for (size_t i = 0; i < ctx.viewports.size(); ++i)
        {
            if (ctx.viewports[i] != viewport) continue;
            ctx.viewports.erase(ctx.viewports.begin() + i);
            if (ctx.main_viewport == viewport) ctx.main_viewport = nullptr;
            release_viewport_tree(viewport);
            return true;
        }
        return false;
    }

    bool destroy_viewport_group(ViewportGroup *viewport) { return destroy_viewport(viewport); }

    amal::vec4 get_widget_viewport_rect(const Widget *widget)
    { return get_viewport_rect(widget ? widget->viewport() : get_main_viewport()); }

    void set_main_viewport(Viewport *viewport)
    {
        detail::get_context().main_viewport = viewport;
        add_viewport(viewport);
    }

    void sync_viewport(Viewport *viewport)
    {
        if (!viewport) return;
        if (viewport->sync_viewport) viewport->sync_viewport(viewport);
        else sync_plain_viewport(viewport);
    }

    void redraw_all_commands()
    {
        auto &ctx = detail::get_context();
        if (ctx.dirty_flags & detail::layout_update_dirty_mask) return;
        ctx.dirty_flags |= DirtyFlagBits::redraw | DirtyFlagBits::hit_rect_draw;
        sync_hit_rect_cache();
        assert(detail::is_hit_rects_frame_synced(ctx.frame_id) &&
               "redraw_all_commands() started with stale current-frame hit rect cache");
        clear_hit_rects();
        clear_all_streams(ctx);
        for (Widget *widget : ctx.widget_tree)
            widget->update_draw_commands(DrawReasonBits::full_redraw | DrawReasonBits::record);
        ctx.dirty_flags &= ~DirtyFlagBits::hit_rect_draw;
        ctx.dirty_flags |= DirtyFlagBits::hit_rect_sync;
    }

    AUIK_EXPORT void sync_clip_rect_cache()
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

    AUIK_EXPORT void sync_draw_streams()
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

    AUIK_EXPORT void sync_hit_rect_cache()
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

    AUIK_EXPORT void rebuild_root_widget_depths()
    {
        auto &ctx = detail::get_context();
        ctx.root_depth_counts[0] = 0u;
        ctx.root_depth_counts[1] = 0u;
        ctx.root_depth_counts[2] = 0u;

        for (Widget *widget : ctx.widget_tree)
        {
            if (!widget) continue;
            const DepthZone zone = detail::root_widget_depth_zone(widget);
            const u32 zone_index = static_cast<u32>(zone);
            assert(zone_index < 3u && "Invalid root depth zone");
            const int lane_index = ctx.root_depth_counts[zone_index];
            assert(lane_index < 32 && "Max depth zone exceeded");
            widget->update_depth(detail::get_root_depth_range(zone, lane_index));
            ++ctx.root_depth_counts[zone_index];
        }
    }

    AUIK_EXPORT void add_widget_to_root(Widget *widget, DepthZone zone)
    {
        assert(widget && "widget is null");
        assert(widget->parent() == nullptr && "Root widget must not have a parent");
        auto &ctx = detail::get_context();
        assert(widget->viewport() && "Root widget viewport is not assigned");
        if (auto *root_data = detail::root_widget_user_data(widget)) *root_data = zone;
        else widget->emplace_user_data_head<detail::RootWidgetUserData>(AUIK_UD_ROOT_DATA, zone);
        ctx.widget_tree.push_back(widget);
        if (widget->widget_flags & WidgetFlagBits::attachable) widget->on_attach();
        rebuild_root_widget_depths();
        widget->update_style();
    }

    AUIK_EXPORT void mark_locale_changed()
    {
        auto &ctx = detail::get_context();
        ctx.dirty_flags |= DirtyFlagBits::locale | DirtyFlagBits::layout | DirtyFlagBits::redraw;
        detail::mark_host_refresh_request();
    }

    AUIK_EXPORT bool remove_widget_from_root_unsync(Widget *widget)
    {
        if (!widget || widget->parent()) return false;
        auto &ctx = detail::get_context();
        for (size_t i = 0; i < ctx.widget_tree.size(); ++i)
        {
            if (ctx.widget_tree[i] != widget) continue;
            if (widget->widget_flags & WidgetFlagBits::attachable) widget->on_detach();
            widget->pop_user_data_head(AUIK_UD_ROOT_DATA);
            ctx.widget_tree.erase(ctx.widget_tree.begin() + i);
            if (ctx.focus_id && ctx.id_map.find(ctx.focus_id) == ctx.id_map.end()) ctx.focus_id = 0u;
            if (ctx.active_id && ctx.id_map.find(ctx.active_id) == ctx.id_map.end()) ctx.active_id = 0u;
            if (ctx.hover_id && ctx.id_map.find(ctx.hover_id.widget_id) == ctx.id_map.end()) ctx.hover_id = {};
            if (ctx.io.clicked_id && ctx.id_map.find(ctx.io.clicked_id.widget_id) == ctx.id_map.end())
                ctx.io.clicked_id = {};
            if (ctx.io.drag_id && ctx.id_map.find(ctx.io.drag_id.widget_id) == ctx.id_map.end()) ctx.io.drag_id = {};
            rebuild_root_widget_depths();
            detail::mark_host_refresh_request();
            return true;
        }
        return false;
    }

    AUIK_EXPORT bool remove_widget_from_root_unsync(u32 id)
    {
        if (!id) return false;
        auto &ctx = detail::get_context();
        auto it = ctx.id_map.find(id);
        return it != ctx.id_map.end() ? remove_widget_from_root_unsync(it->second) : false;
    }

    AUIK_EXPORT bool remove_widget(Widget *widget)
    {
        if (!remove_widget_from_root_unsync(widget)) return false;
        redraw_all_commands();
        return true;
    }

    AUIK_EXPORT bool remove_widget(u32 id)
    {
        if (!remove_widget_from_root_unsync(id)) return false;
        redraw_all_commands();
        return true;
    }

    AUIK_EXPORT bool hide_widget(u32 id)
    {
        if (!id) return false;
        auto &ctx = detail::get_context();
        const auto it = ctx.id_map.find(id);
        if (it == ctx.id_map.end() || !it->second || !it->second->is_visible()) return false;
        it->second->unset_visible();
        it->second->sync_widget_flags();
        return true;
    }

    AUIK_EXPORT bool show_widget(u32 id)
    {
        if (!id) return false;
        auto &ctx = detail::get_context();
        const auto it = ctx.id_map.find(id);
        if (it == ctx.id_map.end() || !it->second || it->second->is_visible()) return false;
        it->second->set_visible();
        it->second->sync_widget_flags();
        return true;
    }

    AUIK_EXPORT void push_widget_to_transient_cache(Widget *widget)
    {
        assert(widget && "widget is null");
        auto &ctx = detail::get_context();
        for (Widget *cached_widget : ctx.transient_cache)
        {
            if (cached_widget != widget) continue;
            return;
        }

        ctx.transient_cache.push_back(widget);
    }

    AUIK_EXPORT bool erase_widget_from_transient_cache(Widget *widget)
    {
        assert(widget && "widget is null");
        auto &transient_cache = detail::get_context().transient_cache;
        auto it = std::find(transient_cache.begin(), transient_cache.end(), widget);
        if (it == transient_cache.end()) return false;
        transient_cache.erase(it);
        return true;
    }

    AUIK_EXPORT void cancel_delayed_tasks(u64 owner_id)
    {
        auto &ctx = detail::get_context();
        for (auto &task : ctx.delayed_tasks)
        {
            if (task.owner_id != owner_id) continue;
            task.fn = nullptr;
        }
        compact_delayed_tasks(ctx);
    }

    AUIK_EXPORT void cancel_all_delayed_tasks()
    {
        auto &ctx = detail::get_context();
        for (auto &task : ctx.delayed_tasks) task.fn = nullptr;
        compact_delayed_tasks(ctx);
    }

    AUIK_EXPORT void pause_delayed_tasks(f64 now)
    {
        auto &ctx = detail::get_context();
        if (ctx.delayed_tasks_pause_time >= 0.0) return;
        ctx.delayed_tasks_pause_time = now;
    }

    AUIK_EXPORT void resume_delayed_tasks(f64 now)
    {
        auto &ctx = detail::get_context();
        if (ctx.delayed_tasks_pause_time < 0.0) return;
        const f64 paused_dt = now - ctx.delayed_tasks_pause_time;
        ctx.delayed_tasks_pause_time = -1.0;
        if (paused_dt <= 0.0) return;

        bool has_live_tasks = false;
        for (auto &task : ctx.delayed_tasks)
        {
            if (!task.fn) continue;
            task.due_time += paused_dt;
            has_live_tasks = true;
        }
        if (has_live_tasks) detail::mark_host_refresh_request();
    }

    AUIK_EXPORT f64 next_delayed_task_in(f64 now)
    {
        auto &ctx = detail::get_context();
        if (ctx.delayed_tasks_pause_time >= 0.0) return -1.0;
        f64 next = -1.0;
        for (const auto &task : ctx.delayed_tasks)
        {
            if (!task.fn) continue;
            const f64 remaining = task.due_time - now;
            if (next < 0.0 || remaining < next) next = remaining;
        }
        if (next < 0.0) return -1.0;
        return next > 0.0 ? next : 0.0;
    }

    AUIK_EXPORT bool dispatch_delayed_tasks(f64 now)
    {
        auto &ctx = detail::get_context();
        if (ctx.delayed_tasks_pause_time >= 0.0) return false;
        acul::vector<acul::unique_function<void()>> due_tasks;
        for (auto &task : ctx.delayed_tasks)
        {
            if (!task.fn || task.due_time > now) continue;
            due_tasks.push_back(std::move(task.fn));
            task.fn = nullptr;
        }
        compact_delayed_tasks(ctx);
        for (auto &fn : due_tasks)
        {
            assert(fn && "fn is null");
            fn();
        }
        return !due_tasks.empty();
    }

    AUIK_EXPORT void show_tooltip(f32 x, const acul::string *text_source)
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
            ctx.tooltip->update_depth(get_root_overlay_depth_range());
        }

        ctx.tooltip->show_at(x, text_source);
        push_widget_to_transient_cache(ctx.tooltip);
    }

    AUIK_EXPORT void hide_tooltip()
    {
        auto &ctx = detail::get_context();
        if (!ctx.tooltip) return;

        erase_widget_from_transient_cache(ctx.tooltip);
        redraw_all_commands();
        detail::mark_host_refresh_request();
    }

    AUIK_EXPORT void clear_tooltip_if_source(const acul::string *text_source)
    {
        auto &ctx = detail::get_context();
        if (!ctx.tooltip || !text_source) return;
        ctx.tooltip->clear_if_source(text_source);
    }
} // namespace auik
