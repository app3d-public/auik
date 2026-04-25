#include <auik/v2/auik.hpp>
#include <auik/v2/detail/context.hpp>
#include <auik/v2/detail/events.hpp>

#define AUIK_MOUSE_DOUBLE_CLICK_TIME     0.45
#define AUIK_MOUSE_DOUBLE_CLICK_MAX_DIST 8.0
#define AUIK_HITBOX_PAD                  4.0f

namespace auik::v2
{
    Widget::~Widget()
    {
        if (detail::g_context)
        {
            cancel_delayed_tasks(id());
            auto &transient_cache = detail::g_context->transient_cache;
            for (size_t i = 0; i < transient_cache.size();)
            {
                if (transient_cache[i] == this) transient_cache.erase(transient_cache.begin() + i);
                else ++i;
            }
        }
        if (event_flags & EventFlagBits::shortcut) detail::deregister_widget_shortcuts(id());
    }

    namespace detail
    {
        HitboxZone get_hitbox_zone(const RectData &rect, const amal::vec2 &mouse_pos)
        {
            HitboxZone zone = HitboxZoneBits::none;
            if (!(rect.flags & RectBits::hitbox)) return zone;
            const auto &bounds = rect.bounds;

            const f32 left = amal::get_rect_left(bounds);
            const f32 right = amal::get_rect_right(bounds);
            const f32 top = amal::get_rect_top(bounds);
            const f32 bottom = amal::get_rect_bottom(bounds);

            if (amal::abs(mouse_pos.x - left) <= AUIK_HITBOX_PAD) zone |= HitboxZoneBits::left;
            if (amal::abs(mouse_pos.x - right) <= AUIK_HITBOX_PAD) zone |= HitboxZoneBits::right;
            if (amal::abs(mouse_pos.y - top) <= AUIK_HITBOX_PAD) zone |= HitboxZoneBits::top;
            if (amal::abs(mouse_pos.y - bottom) <= AUIK_HITBOX_PAD) zone |= HitboxZoneBits::bottom;
            return zone;
        }

        CursorID::enum_type get_cursor_for_hitbox_zone(HitboxZone zone)
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

        static inline DrawReasonFlags resolve_draw_reason_from_style(StyleUpdateFlags style_flags)
        {
            DrawReasonFlags out = DrawReasonBits::style;
            if ((style_flags & StyleUpdateFlagBits::layout) || (style_flags & StyleUpdateFlagBits::parent_layout))
                out |= DrawReasonBits::layout;
            return out;
        }

        template <class Traits>
        static void enqueue_style_refresh(Widget *widget)
        {
            if (!widget) return;
            add_render_command<Traits>(widget, [widget]() {
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
                    widget->update_draw_commands(resolve_draw_reason_from_style(style_flags));
                    ctx.dirty_flags |= DirtyFlagBits::redraw;
                }
            });
            detail::mark_host_refresh_request();
        }

        APPLIB_API void on_hover_id_updated(const ElementID &prev_hover, const ElementID &hover)
        {
            auto &ctx = get_context();
            const bool is_dragging = ctx.io.mouse_down && ctx.io.drag_id;
            const u32 prev_widget_id = prev_hover.widget_id;
            const u32 widget_id = hover.widget_id;
            const bool widget_changed = prev_widget_id != widget_id;
            const bool element_changed = prev_hover != hover;
            const bool selector_changed =
                !is_dragging && detail::set_style_selector(hover, hover ? StyleState::hover : StyleState::normal);
            auto dispatch_hover = [&](u32 id, HoverState state) {
                if (id == 0) return;
                auto it = ctx.id_map.find(id);
                if (it == ctx.id_map.end()) return;
                Widget *widget = it->second;
                assert(widget && "widget is null");
                if (widget->has_event_handler(EventFlagBits::hover)) widget->on_hover(state);
                if (apply_hover_style_state(*widget, state))
                {
                    enqueue_style_refresh<detail::HoverEventTraits>(widget);
                    mark_host_refresh_request();
                }
            };

            if (!is_dragging)
            {
                if (widget_changed)
                {
                    dispatch_hover(prev_widget_id, HoverState::leave);
                    dispatch_hover(widget_id, HoverState::enter);
                }
                else if (selector_changed)
                {
                    if (element_changed) dispatch_hover(widget_id, HoverState::active);
                    auto it = ctx.id_map.find(widget_id);
                    if (it != ctx.id_map.end()) enqueue_style_refresh<detail::HoverEventTraits>(it->second);
                }
            }
        }

        static inline Widget *resolve_input_root(Context &ctx)
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
            return target;
        }

        static inline Widget *resolve_focus_event_target(Context &ctx, EventFlagBits::enum_type event_flag)
        {
            Widget *node = resolve_input_root(ctx);
            while (node)
            {
                if (node->has_event_handler(event_flag)) return node;
                node = node->focus_parent();
            }
            return nullptr;
        }

        static inline bool is_modifier_key(Key key)
        {
            return key == Key::lshift || key == Key::rshift || key == Key::lcontrol || key == Key::rcontrol ||
                   key == Key::lalt || key == Key::ralt || key == Key::lsuper || key == Key::rsuper ||
                   key == Key::caps_lock || key == Key::num_lock;
        }

        static inline KeyMode build_active_shortcut_mods(const IO &io)
        {
            KeyMode mods = KeyModeBits::enum_type(0);
            if (io.active_mods & KeyModeBits::alt) mods |= KeyModeBits::alt;
            if (io.active_mods & KeyModeBits::control) mods |= KeyModeBits::control;
            if (io.active_mods & KeyModeBits::shift) mods |= KeyModeBits::shift;
            return mods;
        }

        static inline bool dispatch_shortcut(Context &ctx)
        {
            const auto &io = ctx.io;
            const KeyMode mods = build_active_shortcut_mods(io);
            if (!mods && io.active_keys.empty() && io.active_mouse_buttons.empty()) return false;

            Widget *node = resolve_input_root(ctx);
            while (node)
            {
                if (node->has_event_handler(EventFlagBits::shortcut))
                {
                    const u64 hash =
                        detail::make_shortcut_hash(io.active_keys, io.active_mouse_buttons, mods, node->id());
                    auto it = io.shortcuts.find(hash);
                    if (it != io.shortcuts.end())
                    {
                        it->second();
                        return true;
                    }
                }
                node = node->focus_parent();
            }

            const u64 hash = detail::make_shortcut_hash(io.active_keys, io.active_mouse_buttons, mods, AUIK_TAG_GLOBAL);
            auto global_it = io.shortcuts.find(hash);
            if (global_it == io.shortcuts.end()) return false;
            global_it->second();
            return true;
        }

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
                if (it != ctx.id_map.end() && it->second->has_event_handler(EventFlagBits::drag))
                    it->second->on_drag({0.0f, 0.0f}, KeyPressState::release);
            }

            io.mouse_down = false;
            io.clicked_id = {};
            io.drag_id = {};
            frame_cache.changes = FrameChangesBits::none;
            frame_cache.drag_widget_id = 0;
            frame_cache.drag_delta = {0.0f, 0.0f};
            frame_cache.scroll_delta = {0.0f, 0.0f};
            frame_cache.char_code = 0;
            frame_cache.char_repeat_count = 0;
            io.active_keys.clear();
            io.active_mouse_buttons.clear();
            io.active_mods = KeyMode{};
            ctx.active_id = 0;
            ctx.hover_id = {};
            detail::reset_style_selector();
            if (prev_active_id)
            {
                auto it = ctx.id_map.find(prev_active_id);
                if (it != ctx.id_map.end())
                {
                    if (it->second->id() == ctx.focus_id) it->second->set_style_state(StyleState::focus);
                    else it->second->set_style_state(StyleState::normal);
                    enqueue_style_refresh<detail::HoverEventTraits>(it->second);
                }
            }
            if (prev_hover.widget_id)
            {
                auto it = ctx.id_map.find(prev_hover.widget_id);
                if (it != ctx.id_map.end())
                {
                    Widget *widget = it->second;
                    assert(widget && "widget is null");
                    if (widget->has_event_handler(EventFlagBits::hover)) widget->on_hover(HoverState::leave);
                    if (apply_hover_style_state(*widget, HoverState::leave))
                        enqueue_style_refresh<detail::HoverEventTraits>(widget);
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

        APPLIB_API void on_key_event(Key key, KeyPressState state, KeyMode mods)
        {
            auto &ctx = get_context();
            auto &io = ctx.io;
            io.active_mods = mods;
            if (!is_modifier_key(key))
            {
                if (state == KeyPressState::release) io.active_keys.erase(key);
                else io.active_keys.insert(key);
            }

            if (state != KeyPressState::release)
            {
                if (dispatch_shortcut(ctx)) return;
            }

            Widget *target = resolve_focus_event_target(ctx, EventFlagBits::key_input);
            if (target) target->on_key(key, state, mods);
        }

        APPLIB_API void on_char_event(u32 char_code)
        {
            auto &ctx = get_context();
            auto &frame_cache = ctx.frame_cache;
            if (frame_cache.char_repeat_count == 0)
            {
                frame_cache.char_code = char_code;
                frame_cache.char_repeat_count = 1;
            }
            else if (frame_cache.char_code == char_code) ++frame_cache.char_repeat_count;
            else
            {
                if (frame_cache.char_repeat_count > 0)
                {
                    Widget *target = resolve_focus_event_target(ctx, EventFlagBits::char_input);
                    if (target) target->on_char_input(frame_cache.char_code, frame_cache.char_repeat_count);
                }
                frame_cache.char_code = char_code;
                frame_cache.char_repeat_count = 1;
            }
            frame_cache.changes |= FrameChangesBits::char_input;
            mark_host_refresh_request();
        }

        void deregister_widget_shortcuts(u32 widget_id)
        {
            if (!widget_id) return;
            auto &ctx = get_context();
            auto it = ctx.io.widget_shortcuts.find(widget_id);
            if (it == ctx.io.widget_shortcuts.end()) return;

            for (u64 shortcut_hash : it->second) ctx.io.shortcuts.erase(shortcut_hash);
            ctx.io.widget_shortcuts.erase(it);

            auto widget_it = ctx.id_map.find(widget_id);
            if (widget_it != ctx.id_map.end() && widget_it->second)
                widget_it->second->remove_event_flags(EventFlagBits::shortcut);
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

        static inline Widget *resolve_scroll_target(Context &ctx)
        {
            Widget *node = nullptr;
            auto it = ctx.id_map.find(ctx.hover_id.widget_id);
            if (it != ctx.id_map.end()) node = it->second;

            while (node)
            {
                if (node->has_event_handler(EventFlagBits::scroll)) return node;
                node = node->parent();
            }
            return nullptr;
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
                assert(w && "widget cannot be null");
                if (w->has_event_handler(EventFlagBits::focus)) w->on_focus(false);
                if (ctx.active_id != w->id()) w->set_style_state(StyleState::normal);
                enqueue_style_refresh<detail::FocusEventTraits>(w);
            }

            // Switch current focus leaf id before focus-gain callbacks.
            ctx.focus_id = next_focus_id;

            // Focus: from LCA child down to new leaf-side.
            for (int k = j; k >= 0; --k)
            {
                Widget *w = path_new[k];
                assert(w && "widget cannot be null");
                if (w->has_event_handler(EventFlagBits::focus)) w->on_focus(true);
                if (ctx.active_id != w->id()) w->set_style_state(StyleState::focus);
                enqueue_style_refresh<detail::FocusEventTraits>(w);
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
                if (drag_it != ctx.id_map.end() && drag_it->second->has_event_handler(EventFlagBits::drag))
                    drag_it->second->on_drag({0.0f, 0.0f}, KeyPressState::press);
            }

            auto it = ctx.id_map.find(ctx.hover_id.widget_id);
            if (it != ctx.id_map.end())
            {
                if (it->second->accepts_focus_on_mouse_press(ctx.hover_id)) set_focus_target(ctx, it->second);
                // Press transfers visual state from hover -> active immediately.
                // Hover re-enter is restored later by hover update/release path.
                if (it->second->has_event_handler(EventFlagBits::hover)) it->second->on_hover(HoverState::leave);
                const u32 prev_active_id = ctx.active_id;
                const u32 next_active_id = it->second->id();
                const StyleState prev_style_state = it->second->style_state();
                if (prev_active_id != next_active_id)
                    for_each_active_chain(ctx, prev_active_id, [&](Widget *w) {
                        w->set_style_state(StyleState::normal);
                        enqueue_style_refresh<detail::ClickEventTraits>(w);
                    });
                ctx.active_id = next_active_id;
                const StyleState prev_style = it->second->style_state();
                // Focus visuals are applied in focus transition path.
                // Press should not override an already-focused widget.
                const StyleState next_style =
                    (prev_style == StyleState::focus) ? StyleState::focus : StyleState::active;
                if (prev_style != next_style) it->second->set_style_state(next_style);
                // Pressed element (tag-based selector target) should always enter active visuals,
                // even if owning widget keeps focus state.
                detail::set_style_selector(io.drag_id, io.drag_id ? StyleState::active : StyleState::normal);
                const bool active_changed = prev_active_id != ctx.active_id;
                const bool style_changed = it->second->style_state() != prev_style_state;
                if (active_changed || style_changed)
                {
                    for_each_active_chain(ctx, ctx.active_id,
                                          [&](Widget *w) { enqueue_style_refresh<detail::ClickEventTraits>(w); });
                    detail::mark_host_refresh_request();
                }
                if (it->second->has_event_handler(EventFlagBits::click))
                    it->second->on_click(MouseKey::left, KeyPressState::press, io.click_count);
            }
            else set_focus_target(ctx, nullptr);
        }

        static void handle_left_mouse_release(Context &ctx, IO &io)
        {
            auto &frame_cache = ctx.frame_cache;
            const ElementID clicked_id = io.clicked_id;
            const ElementID hover_id = ctx.hover_id;
            const u32 clicked_widget_id = clicked_id.widget_id;
            auto it = ctx.id_map.find(io.clicked_id.widget_id);
            if (it != ctx.id_map.end() && it->second->has_event_handler(EventFlagBits::click))
                it->second->on_click(MouseKey::left, KeyPressState::release, io.click_count);

            // Interrupted drag: clear pending frame payload.
            frame_cache.drag_widget_id = 0;
            frame_cache.drag_delta = {0.0f, 0.0f};
            frame_cache.changes &= ~FrameChangesBits::drag_delta;

            it = ctx.id_map.find(io.drag_id.widget_id);
            if (it != ctx.id_map.end() && it->second->has_event_handler(EventFlagBits::drag))
                it->second->on_drag({0.0f, 0.0f}, KeyPressState::release);

            if (ctx.active_id != 0)
            {
                const u32 prev_active_id = ctx.active_id;
                ctx.active_id = 0;
                detail::set_style_selector(ctx.hover_id, ctx.hover_id ? StyleState::hover : StyleState::normal);
                for_each_active_chain(ctx, prev_active_id, [&](Widget *w) {
                    resolve_release_state(w, ctx.hover_id.widget_id);
                    enqueue_style_refresh<detail::HoverEventTraits>(w);
                });
                detail::mark_host_refresh_request();
            }

            if (clicked_widget_id != 0)
            {
                auto clicked_it = ctx.id_map.find(clicked_widget_id);
                if (clicked_it != ctx.id_map.end())
                {
                    const StyleState prev_state = clicked_it->second->style_state();
                    resolve_release_state(clicked_it->second, ctx.hover_id.widget_id);
                    const bool style_changed = prev_state != clicked_it->second->style_state();
                    if (hover_id == clicked_id)
                    {
                        if (clicked_it->second->has_event_handler(EventFlagBits::hover))
                            clicked_it->second->on_hover(HoverState::enter);
                    }
                    else if (style_changed)
                    {
                        enqueue_style_refresh<detail::HoverEventTraits>(clicked_it->second);
                        detail::mark_host_refresh_request();
                    }
                }
            }

            if (clicked_id || hover_id) on_hover_id_updated(clicked_id, hover_id);

            io.clicked_id = {};
            io.drag_id = {};
        }

        APPLIB_API void on_mouse_click_event(MouseKey key, KeyPressState state)
        {
            auto &ctx = get_context();
            auto &io = ctx.io;
            update_window_time(ctx.window_ctx);

            if (state == KeyPressState::release) io.active_mouse_buttons.erase(key);
            else io.active_mouse_buttons.insert(key);

            if (key != MouseKey::left)
            {
                const u32 target_id = ctx.hover_id.widget_id;
                auto it = ctx.id_map.find(target_id);
                if (it != ctx.id_map.end() && it->second->has_event_handler(EventFlagBits::click))
                    it->second->on_click(key, state, 1);
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
                    if (it != ctx.id_map.end() && it->second->has_event_handler(EventFlagBits::drag))
                        it->second->on_drag(drag_delta, KeyPressState::repeat);
                }
            }

            if (changes & FrameChangesBits::scroll_delta)
            {
                Widget *target = resolve_scroll_target(ctx);
                if (target) target->on_scroll(frame_cache.scroll_delta);
                frame_cache.scroll_delta = {0.0f, 0.0f};
            }

            if (changes & FrameChangesBits::char_input)
            {
                if (frame_cache.char_repeat_count > 0)
                {
                    Widget *target = resolve_focus_event_target(ctx, EventFlagBits::char_input);
                    if (target) target->on_char_input(frame_cache.char_code, frame_cache.char_repeat_count);
                }
                frame_cache.char_code = 0;
                frame_cache.char_repeat_count = 0;
            }

            frame_cache.changes = FrameChangesBits::none;
        }

    } // namespace detail
} // namespace auik::v2
