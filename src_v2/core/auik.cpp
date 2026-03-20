#include <auik/v2/auik.hpp>
#include <auik/v2/detail/context.hpp>
#include <auik/v2/detail/events.hpp>
#include <auik/v2/detail/gpu_context.hpp>
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
        static inline acul::string encode_utf8(u32 cp)
        {
            acul::string out;
            if (cp <= 0x7Fu) out.push_back(static_cast<char>(cp));
            else if (cp <= 0x7FFu)
            {
                out.push_back(static_cast<char>(0xC0u | ((cp >> 6) & 0x1Fu)));
                out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
            }
            else if (cp <= 0xFFFFu)
            {
                out.push_back(static_cast<char>(0xE0u | ((cp >> 12) & 0x0Fu)));
                out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
                out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
            }
            else if (cp <= 0x10FFFFu)
            {
                out.push_back(static_cast<char>(0xF0u | ((cp >> 18) & 0x07u)));
                out.push_back(static_cast<char>(0x80u | ((cp >> 12) & 0x3Fu)));
                out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
                out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
            }
            return out;
        }

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
            const bool is_dragging = ctx.io.mouse_down && ctx.io.drag_id;
            auto enqueue_style_refresh = [](Widget *widget) {
                if (!widget) return;
                add_render_command<detail::HoverEventTraits>(widget, [widget]() {
                    auto &ctx = detail::get_context();
                    const auto style_flags = widget->update_style();
                    if (style_flags & StyleUpdateFlagBits::parent_layout)
                    {
                        if (auto *parent = widget->parent()) parent->update_layout(false);
                        else widget->update_layout(false);
                        ctx.dirty_flags |= DirtyFlagBits::layout;
                    }
                    else if (style_flags & StyleUpdateFlagBits::layout) widget->update_layout(false);
                    if (style_flags & StyleUpdateFlagBits::redraw)
                    {
                        widget->update_draw_commands();
                        ctx.dirty_flags |= DirtyFlagBits::redraw;
                    }
                });
                detail::mark_host_refresh_request();
            };
            auto dispatch_hover = [&](u32 id, HoverState state, u32 last_tag_id) {
                if (id == 0) return;
                auto it = ctx.id_map.find(id);
                if (it == ctx.id_map.end()) return;
                Widget *widget = it->second;
                widget->on_hover(state, last_tag_id);
                if (apply_hover_style_state(*widget, state))
                {
                    enqueue_style_refresh(widget);
                    mark_host_refresh_request();
                }
            };

            if (!is_dragging && prev_widget_id != widget_id)
            {
                dispatch_hover(prev_widget_id, HoverState::leave, prev_tag_id);
                dispatch_hover(widget_id, HoverState::enter, prev_tag_id);
            }
            else if (!is_dragging) dispatch_hover(widget_id, HoverState::active, prev_tag_id);

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

            if (prev_tag_id == AUIK_TAG_HITBOX && tag_id != AUIK_TAG_HITBOX && !is_dragging)
                set_window_cursor(CursorID::arrow, ctx.window_ctx);
        }

        static void enqueue_style_refresh(Widget *widget)
        {
            if (!widget) return;
            const u32 wid = widget->id();
            auto &ctx = detail::get_context();
            ctx.disposal_queue.emplace([wid]() {
                auto &ctx = detail::get_context();
                auto it = ctx.id_map.find(wid);
                if (it == ctx.id_map.end()) return;
                Widget *widget = it->second;
                const auto style_flags = widget->update_style();
                if (style_flags & StyleUpdateFlagBits::parent_layout)
                {
                    if (auto *parent = widget->parent()) parent->update_layout(false);
                    else widget->update_layout(false);
                    ctx.dirty_flags |= DirtyFlagBits::layout;
                }
                else if (style_flags & StyleUpdateFlagBits::layout) widget->update_layout(false);
                if (style_flags & StyleUpdateFlagBits::redraw)
                {
                    widget->update_draw_commands();
                    ctx.dirty_flags |= DirtyFlagBits::redraw;
                }
            });
            detail::mark_host_refresh_request();
        };

        APPLIB_API void reset_event_state()
        {
            auto &ctx = get_context();
            auto &io = ctx.io;
            auto &frame_cache = ctx.frame_cache;
            const u32 prev_active_id = ctx.active_id;
            const auto prev_drag = io.drag_id;
            const auto prev_hover = ctx.hover_id;

            // Focus loss/minimize may skip mouse-up event from backend.
            // Force drag release to clear widget-local drag state (e.g. scrollbar thumb active).
            if (prev_drag.widget_id)
            {
                auto it = ctx.id_map.find(prev_drag.widget_id);
                if (it != ctx.id_map.end()) it->second->on_drag({0.0f, 0.0f}, KeyPressState::release);
            }

            io.mouse_down = false;
            io.clicked_id = {};
            io.drag_id = {};
            frame_cache.changes = FrameChangesBits::none;
            frame_cache.drag_widget_id = 0;
            frame_cache.drag_delta = {0.0f, 0.0f};
            frame_cache.scroll_delta = {0.0f, 0.0f};
            frame_cache.char_input.clear();
            frame_cache.char_repeat_count = 0;
            frame_cache.last_char_code = 0;
            ctx.active_id = 0;
            ctx.hover_id = {};
            if (prev_active_id)
            {
                auto it = ctx.id_map.find(prev_active_id);
                if (it != ctx.id_map.end())
                {
                    if (it->second->id() == ctx.focus_id) it->second->set_style_state(StyleState::focus);
                    else it->second->set_style_state(StyleState::normal);
                    enqueue_style_refresh(it->second);
                }
            }
            if (prev_hover.widget_id)
            {
                auto it = ctx.id_map.find(prev_hover.widget_id);
                if (it != ctx.id_map.end())
                {
                    Widget *widget = it->second;
                    widget->on_hover(HoverState::leave, prev_hover.tag_id);
                    if (apply_hover_style_state(*widget, HoverState::leave)) enqueue_style_refresh(widget);
                }
            }
            ctx.hover_hitbox_zone = HitboxZoneBits::none;
            set_window_cursor(CursorID::arrow, ctx.window_ctx);
        }

        APPLIB_API void on_mouse_move(const amal::vec2 &delta)
        {
            auto &ctx = get_context();
            auto &io = ctx.io;
            auto &frame_cache = ctx.frame_cache;
            if (!io.mouse_down || !io.drag_id) return;

            amal::vec2 drag_delta = delta != amal::vec2{0.0f} ? delta : io.mouse_pos - io.last_drag_pos;
            if (drag_delta == amal::vec2{0.0f}) return;

            if (io.mouse_pos != io.last_drag_pos) io.last_drag_pos = io.mouse_pos;
            else io.last_drag_pos += drag_delta;

            frame_cache.drag_widget_id = io.drag_id.widget_id;
            frame_cache.drag_delta += drag_delta;
            frame_cache.changes |= FrameChangesBits::drag_delta;
            mark_host_refresh_request();
        }

        APPLIB_API void on_scroll_event(const amal::vec2 &pos)
        {
            auto &ctx = get_context();
            auto &frame_cache = ctx.frame_cache;
            frame_cache.scroll_delta += pos;
            frame_cache.changes |= FrameChangesBits::scroll_delta;
            mark_host_refresh_request();
        }

        APPLIB_API void on_key_event(u32 key, KeyPressState state, u32 mods)
        {
            auto &ctx = get_context();
            Widget *target = nullptr;
            if (ctx.focus_id != 0)
            {
                auto it = ctx.id_map.find(ctx.focus_id);
                if (it != ctx.id_map.end()) target = it->second;
            }
            if (!target && ctx.active_id != 0)
            {
                auto it = ctx.id_map.find(ctx.active_id);
                if (it != ctx.id_map.end()) target = it->second;
            }
            if (!target)
            {
                auto it = ctx.id_map.find(ctx.hover_id.widget_id);
                if (it != ctx.id_map.end()) target = it->second;
            }
            if (!target) return;

            add_render_command<KeyEventTraits>(target,
                                               [target, key, state, mods]() { target->on_key(key, state, mods); });
            mark_host_refresh_request();
        }

        APPLIB_API void on_char_event(u32 char_code)
        {
            auto &ctx = get_context();
            auto &frame_cache = ctx.frame_cache;
            if (frame_cache.char_input.empty() || frame_cache.last_char_code != char_code)
            {
                if (!frame_cache.char_input.empty()) frame_cache.changes |= FrameChangesBits::char_input;
                frame_cache.char_input = encode_utf8(char_code);
                frame_cache.char_repeat_count = 1;
            }
            else ++frame_cache.char_repeat_count;
            frame_cache.last_char_code = char_code;
            frame_cache.changes |= FrameChangesBits::char_input;
            mark_host_refresh_request();
        }

        template <class F>
        static inline void for_each_active_chain(Context &ctx, u32 leaf_id, F &&fn)
        {
            auto it = ctx.id_map.find(leaf_id);
            if (it == ctx.id_map.end()) return;
            Widget *node = it->second;
            if (!node) return;
            fn(node);
        }

        static inline void resolve_release_state(Widget *widget, u32 hovered_id)
        {
            assert(widget && "widget cannot be null");
            // Focus visuals are managed in focus transition path (on_focus callbacks).
            // Do not overwrite focused widgets during mouse release.
            if (widget->style_state() == StyleState::focus) widget->set_style_state(StyleState::focus);
            else if (widget->id() == hovered_id) widget->set_style_state(StyleState::hover);
            else widget->set_style_state(StyleState::normal);
        }

        static inline Widget *get_widget_by_id(Context &ctx, u32 id)
        {
            if (!id) return nullptr;
            auto it = ctx.id_map.find(id);
            if (it == ctx.id_map.end()) return nullptr;
            return it->second;
        }

        static inline Widget *resolve_focus_entry(Widget *leaf)
        {
            if (!leaf) return nullptr;
            return leaf->focus_parent() ? leaf->focus_parent() : leaf;
        }

        static void set_focus_target(Context &ctx, Widget *target)
        {
            const u32 next_focus_id = target ? target->id() : 0u;
            if (ctx.focus_id == next_focus_id) return;

            Widget *old_leaf = get_widget_by_id(ctx, ctx.focus_id);
            Widget *new_leaf = target;
            Widget *old_entry = resolve_focus_entry(old_leaf);
            Widget *new_entry = resolve_focus_entry(new_leaf);

            // If focus tree root is unchanged, keep parent focus state intact.
            if (old_entry == new_entry)
            {
                ctx.focus_id = next_focus_id;
                return;
            }

            constexpr int max_path = 64;
            Widget *path_old[max_path];
            Widget *path_new[max_path];
            int len_old = 0;
            int len_new = 0;

            for (Widget *w = old_entry; w && len_old < max_path; w = w->focus_parent()) path_old[len_old++] = w;
            for (Widget *w = new_entry; w && len_new < max_path; w = w->focus_parent()) path_new[len_new++] = w;

            int i = len_old - 1;
            int j = len_new - 1;
            while (i >= 0 && j >= 0 && path_old[i] == path_new[j])
            {
                --i;
                --j;
            }

            // Blur: leaf-side to (but excluding) LCA.
            for (int k = 0; k <= i; ++k)
            {
                Widget *w = path_old[k];
                w->on_focus(false);
                if (ctx.active_id != w->id()) w->set_style_state(StyleState::normal);
                enqueue_style_refresh(w);
            }

            // Switch current focus leaf id before focus-gain callbacks.
            ctx.focus_id = next_focus_id;

            // Focus: from LCA child down to new leaf-side.
            for (int k = j; k >= 0; --k)
            {
                Widget *w = path_new[k];
                w->on_focus(true);
                if (ctx.active_id != w->id()) w->set_style_state(StyleState::focus);
                enqueue_style_refresh(w);
            }

            detail::mark_host_refresh_request();
        }

        static void handle_left_mouse_press(Context &ctx, IO &io)
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

            io.clicked_id = ctx.hover_id;
            io.drag_id = ctx.hover_id;
            if (io.drag_id)
            {
                auto drag_it = ctx.id_map.find(io.drag_id.widget_id);
                if (drag_it != ctx.id_map.end()) drag_it->second->on_drag({0.0f, 0.0f}, KeyPressState::press);
            }

            auto it = ctx.id_map.find(ctx.hover_id.widget_id);
            if (it != ctx.id_map.end())
            {
                set_focus_target(ctx, it->second);
                // Press transfers visual state from hover -> active immediately.
                // Hover re-enter is restored later by hover update/release path.
                it->second->on_hover(HoverState::leave, ctx.hover_id.tag_id);
                const u32 prev_active_id = ctx.active_id;
                const u32 next_active_id = it->second->id();
                const StyleState prev_style_state = it->second->style_state();
                if (prev_active_id != next_active_id)
                    for_each_active_chain(ctx, prev_active_id, [&](Widget *w) {
                        w->set_style_state(StyleState::normal);
                        enqueue_style_refresh(w);
                    });
                ctx.active_id = next_active_id;
                const StyleState prev_style = it->second->style_state();
                // Focus visuals are applied in focus transition path.
                // Press should not override an already-focused widget.
                const StyleState next_style =
                    (prev_style == StyleState::focus) ? StyleState::focus : StyleState::active;
                if (prev_style != next_style) it->second->set_style_state(next_style);
                const bool active_changed = prev_active_id != ctx.active_id;
                const bool style_changed = it->second->style_state() != prev_style_state;
                if (active_changed || style_changed)
                {
                    for_each_active_chain(ctx, ctx.active_id, [&](Widget *w) { enqueue_style_refresh(w); });
                    detail::mark_host_refresh_request();
                }
                it->second->on_click(MouseKey::left, KeyPressState::press, io.click_count);
            }
            else set_focus_target(ctx, nullptr);
        }

        static void handle_left_mouse_release(Context &ctx, IO &io)
        {
            auto &frame_cache = ctx.frame_cache;
            const u32 clicked_widget_id = io.clicked_id.widget_id;
            auto it = ctx.id_map.find(io.clicked_id.widget_id);
            if (it != ctx.id_map.end()) it->second->on_click(MouseKey::left, KeyPressState::release, io.click_count);

            // Interrupted drag: clear pending frame payload.
            frame_cache.drag_widget_id = 0;
            frame_cache.drag_delta = {0.0f, 0.0f};
            frame_cache.changes &= ~FrameChangesBits::drag_delta;

            it = ctx.id_map.find(io.drag_id.widget_id);
            if (it != ctx.id_map.end()) it->second->on_drag({0.0f, 0.0f}, KeyPressState::release);

            if (ctx.active_id != 0)
            {
                const u32 prev_active_id = ctx.active_id;
                ctx.active_id = 0;
                for_each_active_chain(ctx, prev_active_id, [&](Widget *w) {
                    resolve_release_state(w, ctx.hover_id.widget_id);
                    enqueue_style_refresh(w);
                });
                detail::mark_host_refresh_request();
            }

            if (clicked_widget_id != 0)
            {
                auto clicked_it = ctx.id_map.find(clicked_widget_id);
                if (clicked_it != ctx.id_map.end())
                {
                    if (ctx.active_id == clicked_widget_id)
                    {
                        if (ctx.hover_id.widget_id == clicked_widget_id)
                            clicked_it->second->on_hover(HoverState::enter, io.clicked_id.tag_id);
                    }
                    else
                    {
                        const StyleState prev_state = clicked_it->second->style_state();
                        resolve_release_state(clicked_it->second, ctx.hover_id.widget_id);
                        const bool style_changed = prev_state != clicked_it->second->style_state();
                        if (ctx.hover_id.widget_id == clicked_widget_id)
                            clicked_it->second->on_hover(HoverState::enter, io.clicked_id.tag_id);
                        else if (style_changed)
                        {
                            enqueue_style_refresh(clicked_it->second);
                            detail::mark_host_refresh_request();
                        }
                    }
                }
            }

            if (ctx.hover_id && ctx.hover_id.widget_id != clicked_widget_id)
                on_hover_id_updated(0, 0, ctx.hover_id.widget_id, ctx.hover_id.tag_id);

            io.clicked_id = {};
            io.drag_id = {};
        }

        APPLIB_API void on_mouse_click_event(MouseKey key, KeyPressState state)
        {
            auto &ctx = get_context();
            auto &io = ctx.io;
            update_window_time(ctx.window_ctx);

            if (key != MouseKey::left)
            {
                const u32 target_id = ctx.hover_id.widget_id;
                auto it = ctx.id_map.find(target_id);
                if (it != ctx.id_map.end()) it->second->on_click(key, state, 1);
                return;
            }

            if (state == KeyPressState::press)
            {
                handle_left_mouse_press(ctx, io);
                return;
            }

            io.mouse_down = false;
            if (state == KeyPressState::release) handle_left_mouse_release(ctx, io);
        }

        APPLIB_API void flush_frame_changes()
        {
            auto &ctx = get_context();
            auto &frame_cache = ctx.frame_cache;
            const FrameChanges changes = frame_cache.changes;
            if (changes == FrameChangesBits::none) return;

            if (changes & FrameChangesBits::drag_delta)
            {
                const u32 drag_widget_id = frame_cache.drag_widget_id;
                const auto drag_delta = frame_cache.drag_delta;
                frame_cache.drag_widget_id = 0;
                frame_cache.drag_delta = {0.0f, 0.0f};
                if (drag_widget_id != 0 && drag_delta != amal::vec2{0.0f})
                {
                    auto it = ctx.id_map.find(drag_widget_id);
                    if (it != ctx.id_map.end()) it->second->on_drag(drag_delta, KeyPressState::repeat);
                }
            }

            if (changes & FrameChangesBits::scroll_delta)
            {
                auto it = ctx.id_map.find(ctx.hover_id.widget_id);
                if (it != ctx.id_map.end())
                {
                    const auto delta = frame_cache.scroll_delta;
                    it->second->on_scroll(delta);
                }
                frame_cache.scroll_delta = {0.0f, 0.0f};
            }

            if (changes & FrameChangesBits::key_input) {}

            if (changes & FrameChangesBits::char_input)
            {
                Widget *target = nullptr;
                if (ctx.focus_id != 0)
                {
                    auto it = ctx.id_map.find(ctx.focus_id);
                    if (it != ctx.id_map.end()) target = it->second;
                }
                if (!target && ctx.active_id != 0)
                {
                    auto it = ctx.id_map.find(ctx.active_id);
                    if (it != ctx.id_map.end()) target = it->second;
                }
                if (!target)
                {
                    auto it = ctx.id_map.find(ctx.hover_id.widget_id);
                    if (it != ctx.id_map.end()) target = it->second;
                }
                if (target && !frame_cache.char_input.empty())
                {
                    const acul::string text = frame_cache.char_input;
                    const u32 count = frame_cache.char_repeat_count;
                    target->on_char(text, count);
                }
                frame_cache.char_input.clear();
                frame_cache.char_repeat_count = 0;
                frame_cache.last_char_code = 0;
            }

            frame_cache.changes = FrameChangesBits::none;
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
            active_range = detail::depth_zone_range(_depth_range, detail::DepthZone::foreground);
        else if (widget_flags & WidgetFlagBits::background)
            active_range = detail::depth_zone_range(_depth_range, detail::DepthZone::background);
        else active_range = detail::depth_zone_range(_depth_range, detail::DepthZone::work);
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
        ctx.streams.attached_streams = create_info.streams;
        ctx.streams.stream_count = create_info.streams_count;
        ctx.gpu_ctx = create_info.gpu_ctx;
        ctx.host_refresh_request = create_info.host_refresh_request;
        ctx.pending_filter = create_info.pending_filter;
        ctx.raw_mouse_mode = false;
        ctx.frames_in_flight = create_info.frames_in_flight;
        auto &io = ctx.io;
        io.display_size = {0.0f, 0.0f};
        io.mouse_pos = {0.0f, 0.0f};
        io.last_click_pos = {0.0f, 0.0f};
        io.last_drag_pos = {0.0f, 0.0f};
        io.last_click_time = -1.0;
        io.click_count = 0;
        io.click_streak = 0;
        io.last_key = 0;
        io.last_key_mods = 0;
        io.last_key_state = KeyPressState::release;
        io.clicked_id = {};
        io.drag_id = {};
        io.mouse_down = false;
        auto &frame_cache = ctx.frame_cache;
        frame_cache.changes = detail::FrameChangesBits::none;
        frame_cache.drag_widget_id = 0;
        frame_cache.drag_delta = {0.0f, 0.0f};
        frame_cache.scroll_delta = {0.0f, 0.0f};
        frame_cache.char_repeat_count = 0;
        frame_cache.last_char_code = 0;
        frame_cache.char_input.clear();
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

    /**
     * @brief Rebuilds the widget tree's layout and draw commands.
     * This function must be called when the widget tree has changed in some way (e.g. a widget was added/removed, a
     * widget's size changed, etc.). It will rebuild the widget tree's layout and draw commands, clearing all streams
     * and resetting the current clip rectangle. If the widget tree has hit rectangles, this function will also clear
     * the hit rectangle cache and mark it for synchronization on the next frame.
     * @note This function assumes that the hit rectangle cache is up-to-date with the current frame.
     */
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
            widget->record_draw_commands();
        }
        ctx.dirty_flags &= ~DirtyFlagBits::layout;
        if (need_hit_rect_draw)
        {
            ctx.dirty_flags &= ~DirtyFlagBits::hit_rect_draw;
            ctx.dirty_flags |= DirtyFlagBits::hit_rect_sync;
        }
    }

    /**
     * @brief Clears all streams and redraws all draw commands from the widget tree.
     * This function will clear all streams and then redraw all draw commands from the widget tree.
     * This function assumes that the hit rectangle cache is up-to-date with the current frame, and will mark the hit
     * rectangle cache for synchronization on the next frame. If the hit rectangle cache is stale, this function will
     * assert.
     * @see record_layout_commands
     */
    void redraw_all_commands()
    {
        auto &ctx = detail::get_context();
        if (ctx.dirty_flags & DirtyFlagBits::layout) return;
        ctx.dirty_flags |= DirtyFlagBits::redraw | DirtyFlagBits::hit_rect_draw;
        assert(detail::is_hit_rects_frame_synced(ctx.frame_id) &&
               "redraw_all_commands() started with stale current-frame hit rect cache");
        clear_hit_rects();
        clear_all_streams(ctx);
        for (Widget *widget : ctx.widget_tree) widget->record_draw_commands();
        ctx.dirty_flags &= ~DirtyFlagBits::hit_rect_draw;
        ctx.dirty_flags |= DirtyFlagBits::hit_rect_sync;
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
        assert(lane_index < AUIK_ROOT_DEPTH_ATOMS_COUNT && "Max depth zone exceeded");
        widget->update_depth(detail::get_root_depth_range(zone, lane_index));
        ++ctx.root_depth_counts[zone];
        widget->update_style();
        widget->update_layout(false);
        widget->record_draw_commands();
        ctx.dirty_flags |= DirtyFlagBits::redraw;
    }
} // namespace auik::v2
