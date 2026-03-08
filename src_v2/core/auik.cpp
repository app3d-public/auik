#include <auik/v2/auik.hpp>
#include <auik/v2/detail/context.hpp>
#include <auik/v2/detail/events.hpp>
#include <auik/v2/detail/gpu_context.hpp>
#include <auik/v2/widgets/widget.hpp>
#include <auik/v2/widgets/window.hpp>

#define AUIK_ROOT_DEPTH_ATOMS_COUNT      32
#define AUIK_CHILD_DEPTH_ATOMS_COUNT     16
#define AUIK_DEPTH_MIN_STEP              1e-6f
#define AUIK_MOUSE_DOUBLE_CLICK_TIME     0.45
#define AUIK_MOUSE_DOUBLE_CLICK_MAX_DIST 8.0
#define AUIK_HITBOX_PAD                  4.0f

namespace auik::v2
{
    static void clear_all_streams(detail::Context &ctx)
    {
        for (u32 stream_id = 0; stream_id < ctx.streams.stream_count; ++stream_id)
        {
            auto &stream = ctx.streams.attached_streams[stream_id];
            // If cached stream and non-empty
            if (stream.draw_sizes[ctx.frame_id] <= 0) continue;
            clear_draw_stream(&stream, ctx.frame_id);
        }
    }

    namespace detail
    {
        Context *g_context = nullptr;

        APPLIB_API void on_resize_event(const amal::vec2 &size)
        {
            set_window_size(size);
            get_context().dirty_flags |= DirtyFlagBits::layout;
        }

        APPLIB_API void on_mouse_move_event(const amal::vec2 &pos) { get_io().mouse_pos = pos; }

        APPLIB_API HitboxZone get_hitbox_zone(const RectData &rect, const amal::vec2 &mouse_pos)
        {
            HitboxZone zone = HitboxZoneBits::none;
            if (!(rect.flags & RectBits::hitbox)) return zone;
            const auto absf = [](f32 x) { return x < 0.0f ? -x : x; };

            const f32 left = rect.position.x;
            const f32 right = rect.position.x + rect.size.x;
            const f32 top = rect.position.y;
            const f32 bottom = rect.position.y + rect.size.y;

            if (absf(mouse_pos.x - left) <= AUIK_HITBOX_PAD) zone |= HitboxZoneBits::left;
            if (absf(mouse_pos.x - right) <= AUIK_HITBOX_PAD) zone |= HitboxZoneBits::right;
            if (absf(mouse_pos.y - top) <= AUIK_HITBOX_PAD) zone |= HitboxZoneBits::top;
            if (absf(mouse_pos.y - bottom) <= AUIK_HITBOX_PAD) zone |= HitboxZoneBits::bottom;
            return zone;
        }

        APPLIB_API CursorID::enum_type get_cursor_for_hitbox_zone(HitboxZone zone)
        {
            const bool has_left = zone & HitboxZoneBits::left;
            const bool has_right = zone & HitboxZoneBits::right;
            const bool has_top = zone & HitboxZoneBits::top;
            const bool has_bottom = zone & HitboxZoneBits::bottom;

            if ((has_left && has_top) || (has_right && has_bottom)) return CursorID::resize_nwse;
            if ((has_right && has_top) || (has_left && has_bottom)) return CursorID::resize_nesw;
            if (has_left || has_right) return CursorID::resize_ew;
            if (has_top || has_bottom) return CursorID::resize_ns;
            return CursorID::arrow;
        }

        APPLIB_API void on_hover_id_updated(u32 prev_widget_id, u32 prev_tag_id, u32 widget_id, u32 tag_id)
        {
            auto &ctx = get_context();
            const bool is_dragging = ctx.io.mouse_down && ctx.io.drag_id != 0;
            auto dispatch_hover = [&](u32 id, HoverState state, u32 last_tag_id) {
                if (id == 0) return;
                auto it = ctx.id_map.find(id);
                if (it == ctx.id_map.end()) return;
                it->second->on_hover(state, last_tag_id);
            };

            if (prev_widget_id != widget_id)
            {
                dispatch_hover(prev_widget_id, HoverState::leave, prev_tag_id);
                dispatch_hover(widget_id, HoverState::enter, prev_tag_id);
            }
            else dispatch_hover(widget_id, HoverState::active, prev_tag_id);

            ctx.hover_hitbox_zone = HitboxZoneBits::none;
            if (tag_id == AUIK_TAG_HITBOX && widget_id != 0)
            {
                auto it = ctx.id_map.find(widget_id);
                if (it != ctx.id_map.end())
                {
                    const auto &rect = it->second->get_rect();
                    ctx.hover_hitbox_zone = get_hitbox_zone(rect, ctx.io.mouse_pos);
                    if (!is_dragging)
                        set_window_cursor(get_cursor_for_hitbox_zone(ctx.hover_hitbox_zone), ctx.window_ctx);
                    return;
                }
            }

            if (prev_tag_id == AUIK_TAG_HITBOX && tag_id != AUIK_TAG_HITBOX)
            {
                if (!is_dragging) set_window_cursor(CursorID::arrow, ctx.window_ctx);
            }
        }

        APPLIB_API void reset_event_state()
        {
            auto &ctx = get_context();
            const u32 prev_hover_widget_id = ctx.hover_widget_id;
            const u32 prev_hover_tag_id = ctx.hover_tag_id;
            ctx.hover_widget_id = 0;
            ctx.hover_tag_id = 0;
            ctx.hover_hitbox_zone = HitboxZoneBits::none;
            on_hover_id_updated(prev_hover_widget_id, prev_hover_tag_id, 0, 0);
            set_window_cursor(CursorID::arrow, ctx.window_ctx);
        }

        APPLIB_API void on_drag_event(const amal::vec2 &delta)
        {
            auto &ctx = get_context();
            auto &io = ctx.io;
            if (!io.mouse_down || io.drag_id == 0) return;

            amal::vec2 drag_delta = delta != amal::vec2{0.0f} ? delta : io.mouse_pos - io.last_drag_pos;
            if (drag_delta == amal::vec2{0.0f}) return;

            io.drag_delta = drag_delta;
            if (io.mouse_pos != io.last_drag_pos) io.last_drag_pos = io.mouse_pos;
            else io.last_drag_pos += drag_delta;
            auto it = ctx.id_map.find(io.drag_id);
            if (it != ctx.id_map.end()) it->second->on_drag(drag_delta, KeyPressState::repeat);
        }

        APPLIB_API void on_scroll_event(const amal::vec2 &pos)
        {
            auto &ctx = get_context();
            auto it = ctx.id_map.find(ctx.hover_widget_id);
            if (it == ctx.id_map.end()) return;

            const u32 prev_active_id = ctx.active_widget_id;
            const StyleState prev_style_state = it->second->style_state();
            it->second->on_active();
            const bool active_changed = prev_active_id != ctx.active_widget_id;
            const bool style_changed = it->second->style_state() != prev_style_state;
            if (active_changed || style_changed)
            {
                auto refresh_active_visual = [&](u32 widget_id) {
                    auto wid_it = ctx.id_map.find(widget_id);
                    if (wid_it == ctx.id_map.end()) return;
                    if (widget_id != 0 && widget_id != ctx.active_widget_id)
                        wid_it->second->set_style_state(StyleState::normal);
                    wid_it->second->update_style();
                    wid_it->second->update_draw_commands();
                };
                if (active_changed) refresh_active_visual(prev_active_id);
                refresh_active_visual(ctx.active_widget_id);
                ctx.dirty_flags |= DirtyFlagBits::redraw;
            }

            it->second->on_scroll(pos);
        }

        APPLIB_API void on_mouse_click_event(MouseKey key, KeyPressState state)
        {
            auto &ctx = get_context();
            auto &io = ctx.io;
            update_window_time(ctx.window_ctx);

            if (key != MouseKey::left)
            {
                const u32 target_id = ctx.hover_widget_id;
                auto it = ctx.id_map.find(target_id);
                if (it != ctx.id_map.end()) it->second->on_click(key, state, 1);
                return;
            }

            if (state == KeyPressState::press)
            {
                io.mouse_down = true;
                const f64 now = ctx.window_ctx->time;
                const f64 multi_click_time = AUIK_MOUSE_DOUBLE_CLICK_TIME;
                const f32 multi_click_dist = AUIK_MOUSE_DOUBLE_CLICK_MAX_DIST;
                const f64 elapsed = now - io.last_click_time;
                const amal::vec2 click_delta = io.mouse_pos - io.last_click_pos;
                const f32 click_dist_sqr = click_delta.x * click_delta.x + click_delta.y * click_delta.y;
                const f32 click_eps_sqr = multi_click_dist * multi_click_dist;
                const bool is_multi_click =
                    io.last_click_time >= 0.0 && elapsed <= multi_click_time && click_dist_sqr <= click_eps_sqr;

                io.click_streak = is_multi_click ? (io.click_streak + 1) : 1;
                io.click_count = io.click_streak;
                io.last_click_time = now;
                io.last_click_pos = io.mouse_pos;
                io.last_drag_pos = io.mouse_pos;

                io.clicked_widget_id = ctx.hover_widget_id;
                const bool is_drag_origin =
                    ctx.hover_tag_id == AUIK_TAG_WINDOW_HEADER || ctx.hover_tag_id == AUIK_TAG_SCROLLBAR_TRACK ||
                    ctx.hover_tag_id == AUIK_TAG_SCROLLBAR_THUMB || ctx.hover_tag_id == AUIK_TAG_HITBOX;
                io.drag_id = is_drag_origin ? ctx.hover_widget_id : 0;
                if (io.drag_id != 0)
                {
                    auto drag_it = ctx.id_map.find(io.drag_id);
                    if (drag_it != ctx.id_map.end()) drag_it->second->on_drag({0.0f, 0.0f}, KeyPressState::press);
                }

                auto it = ctx.id_map.find(ctx.hover_widget_id);
                if (it != ctx.id_map.end())
                {
                    const u32 prev_active_id = ctx.active_widget_id;
                    const StyleState prev_style_state = it->second->style_state();
                    it->second->on_active();
                    const bool active_changed = prev_active_id != ctx.active_widget_id;
                    const bool style_changed = it->second->style_state() != prev_style_state;
                    if (active_changed || style_changed)
                    {
                        auto refresh_active_visual = [&](u32 widget_id) {
                            auto wid_it = ctx.id_map.find(widget_id);
                            if (wid_it == ctx.id_map.end()) return;
                            if (widget_id != 0 && widget_id != ctx.active_widget_id)
                                wid_it->second->set_style_state(StyleState::normal);
                            wid_it->second->update_style();
                            wid_it->second->update_draw_commands();
                        };
                        if (active_changed) refresh_active_visual(prev_active_id);
                        refresh_active_visual(ctx.active_widget_id);
                        ctx.dirty_flags |= DirtyFlagBits::redraw;
                    }
                    it->second->on_click(key, state, io.click_count);
                }
                return;
            }

            io.mouse_down = false;
            if (state == KeyPressState::release)
            {
                auto it = ctx.id_map.find(io.clicked_widget_id);
                if (it != ctx.id_map.end()) it->second->on_click(key, state, io.click_count);

                it = ctx.id_map.find(io.drag_id);
                if (it != ctx.id_map.end()) it->second->on_drag({0.0f, 0.0f}, KeyPressState::release);

                // Click/release on empty space should clear active widget.
                if (io.clicked_widget_id == 0 && ctx.hover_widget_id == 0 && ctx.active_widget_id != 0)
                {
                    const u32 prev_active_id = ctx.active_widget_id;
                    ctx.active_widget_id = 0;
                    auto prev_it = ctx.id_map.find(prev_active_id);
                    if (prev_it != ctx.id_map.end())
                    {
                        prev_it->second->set_style_state(StyleState::normal);
                        prev_it->second->update_style();
                        prev_it->second->update_layout();
                        prev_it->second->update_draw_commands();
                    }
                    ctx.dirty_flags |= DirtyFlagBits::redraw;
                }

                io.clicked_widget_id = 0;
                io.drag_id = 0;
            }
        }

        struct DepthZone
        {
            enum enum_type
            {
                foreground,
                work,
                background
            };
        };

        static inline amal::vec2 depth_zone_range(const amal::vec2 &base, DepthZone::enum_type zone)
        {
            const f32 span = base.y - base.x;
            switch (zone)
            {
                // With GreaterOrEqual depth testing: higher Z is closer.
                // Keep semantic order:
                // background (farthest) -> work -> foreground (closest)
                case DepthZone::background:
                    return {base.x + span * 0.00f, base.x + span * (1.0f / 3.0f)};
                case DepthZone::work:
                    return {base.x + span * (1.0f / 3.0f), base.x + span * (2.0f / 3.0f)};
                case DepthZone::foreground:
                    return {base.x + span * (2.0f / 3.0f), base.x + span * 1.00f};
                default:
                    return {base.x + span * (1.0f / 3.0f), base.x + span * (2.0f / 3.0f)};
            }
        }

        APPLIB_API amal::vec2 get_depth_workzone_range(const amal::vec2 &r)
        {
            return depth_zone_range(r, DepthZone::work);
        }

        static inline amal::vec2 normalize_depth_range(const amal::vec2 &src)
        {
            f32 z_min = src.x;
            f32 z_max = src.y;
            if (z_min > z_max)
            {
                const f32 t = z_min;
                z_min = z_max;
                z_max = t;
            }
            return {z_min, z_max};
        }

        static inline DepthZone::enum_type get_depth_zone_by_flags(WidgetFlags flags)
        {
            if (flags & WidgetFlagBits::foreground) return DepthZone::foreground;
            if (flags & WidgetFlagBits::background) return DepthZone::background;
            return DepthZone::work;
        }

        static inline amal::vec2 get_root_depth_range(DepthZone::enum_type zone, int lane_index)
        {
            constexpr amal::vec2 global = {0.0f, 1.0f};

            const amal::vec2 lane_range = depth_zone_range(global, zone);
            const f32 span = lane_range.y - lane_range.x;
            const f32 step = amal::max(span / (f32)AUIK_ROOT_DEPTH_ATOMS_COUNT, AUIK_DEPTH_MIN_STEP);

            const f32 r0 = lane_range.x + step * static_cast<f32>(lane_index);
            const f32 r1 = (r0 + step <= lane_range.y) ? (r0 + step) : lane_range.y;

            return {r0, r1};
        }
    } // namespace detail

    void Widget::update_depth(const amal::vec2 &depth_range)
    {
        _depth_range = detail::normalize_depth_range(depth_range);
        amal::vec2 active_range = _depth_range;
        if (widget_flags & WidgetFlagBits::foreground)
        {
            active_range = detail::depth_zone_range(_depth_range, detail::DepthZone::foreground);
        }
        else if (widget_flags & WidgetFlagBits::background)
        {
            active_range = detail::depth_zone_range(_depth_range, detail::DepthZone::background);
        }
        else
        {
            active_range = detail::depth_zone_range(_depth_range, detail::DepthZone::work);
        }
        _depth_range = detail::normalize_depth_range(active_range);
        _rect.depth = (_depth_range.x + _depth_range.y) * 0.5f;
    }

    APPLIB_API void assign_next_depth(const amal::vec2 &parent_range, amal::vec2 &dst_range)
    {
        const amal::vec2 w = detail::normalize_depth_range(parent_range);

        const f32 span = w.y - w.x;
        if (span <= 0.0f)
        {
            dst_range = {w.x, w.x};
            return;
        }

        const f32 step = amal::max(span / (f32)AUIK_CHILD_DEPTH_ATOMS_COUNT, AUIK_DEPTH_MIN_STEP);
        const f32 r1 = w.y;
        const f32 r0 = (r1 - step >= w.x) ? (r1 - step) : w.x;
        dst_range = {r0, r1};
    }

    bool init_library(const CreateInfo &create_info)
    {
        if (detail::g_context) destroy_library();
        detail::g_context = acul::alloc<detail::Context>();
        auto &ctx = detail::get_context();
        ctx.ed = create_info.ed;
        ctx.disposal_queue = create_info.disposal_queue;
        ctx.streams.attached_streams = create_info.streams;
        ctx.streams.stream_count = create_info.streams_count;
        ctx.gpu_ctx = create_info.gpu_ctx;
        ctx.frames_in_flight = create_info.frames_in_flight;
        auto &io = ctx.io;
        io.display_size = {0.0f, 0.0f};
        io.mouse_pos = {0.0f, 0.0f};
        io.last_click_pos = {0.0f, 0.0f};
        io.last_drag_pos = {0.0f, 0.0f};
        io.drag_delta = {0.0f, 0.0f};
        io.last_click_time = -1.0;
        io.click_count = 0;
        io.click_streak = 0;
        io.clicked_widget_id = 0;
        io.drag_id = 0;
        io.mouse_down = false;
        ctx.hover_hitbox_zone = detail::HitboxZoneBits::none;
        ctx.active_widget_id = 0;
        ctx.screen_cursor = {0.0f, 0.0f};
        ctx.window_ctx = create_info.window_ctx;
        detail::construct_window_backend(ctx.window_ctx);
        ctx.dirty_flags = DirtyFlagBits::redraw | DirtyFlagBits::layout;
        detail::construct_shared_buffer_sync_state(ctx.shared_sync_state[AUIK_SYNC_CLIP_RECT], ctx.frames_in_flight);
        detail::construct_shared_buffer_sync_state(ctx.shared_sync_state[AUIK_SYNC_HIT_RECT], ctx.frames_in_flight);
        return detail::create_gpu_resources(ctx.gpu_ctx);
    }

    void destroy_library()
    {
        if (!detail::g_context) return;
        for (auto *widget : detail::g_context->widget_tree) acul::release(widget);
        detail::destroy_shared_buffer_sync_state(detail::g_context->shared_sync_state[AUIK_SYNC_CLIP_RECT]);
        detail::destroy_shared_buffer_sync_state(detail::g_context->shared_sync_state[AUIK_SYNC_HIT_RECT]);
        detail::destroy_gpu_context(detail::g_context->gpu_ctx);
        detail::destroy_window_context(detail::g_context->window_ctx);
        acul::release(detail::g_context);
        detail::g_context = nullptr;
    }

    void reset_clip_rects()
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

    void record_all_commands()
    {
        auto &ctx = detail::get_context();
        if (ctx.dirty_flags & DirtyFlagBits::layout)
        {
            reset_clip_rects();
            clear_all_streams(ctx);
            for (Widget *widget : ctx.widget_tree)
            {
                widget->update_layout();
                widget->record_draw_commands();
            }
            ctx.dirty_flags &= ~DirtyFlagBits::layout;
        }
        else
        {
            clear_all_streams(ctx);
            for (Widget *widget : ctx.widget_tree) widget->record_draw_commands();
        }
    }

    APPLIB_API void sync_clip_rect_cache()
    {
        auto &ctx = detail::get_context();
        auto &state = detail::get_clip_rects_sync_state();
        u32 frame_id = ctx.frame_id;
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
        u32 frame_id = ctx.frame_id;
        if (state.buffer_versions[frame_id] == state.master_version)
        {
            if (state.invalidation_count == 0) ctx.dirty_flags &= ~DirtyFlagBits::hit_rect;
            return;
        }
        copy_hit_rects_frame(gpu, frame_id, state.master_id);
        state.buffer_versions[frame_id] = state.master_version;
        if (state.invalidation_count > 0) --state.invalidation_count;
        if (state.invalidation_count == 0) state.stage_version = state.master_version;
        if (state.invalidation_count == 0) ctx.dirty_flags &= ~DirtyFlagBits::hit_rect;
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
        assert(lane_index < AUIK_ROOT_DEPTH_ATOMS_COUNT && "Max depth zone exceeded");
        widget->update_depth(detail::get_root_depth_range(zone, lane_index));
        ++ctx.root_depth_counts[zone];
        widget->update_style();
        widget->update_layout();
        widget->record_draw_commands();
        ctx.dirty_flags |= DirtyFlagBits::redraw;
    }
} // namespace auik::v2
