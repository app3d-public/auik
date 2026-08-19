#include <auik/auik.hpp>
#include <auik/detail/context.hpp>
#include <auik/detail/events.hpp>

#define AUIK_MOUSE_DOUBLE_CLICK_TIME     0.45
#define AUIK_MOUSE_DOUBLE_CLICK_MAX_DIST 8.0

namespace auik
{
    namespace
    {
        bool is_at_drag_boundary(const detail::Context &ctx)
        {
            constexpr f32 boundary_margin = 1.0f;
            const auto &position = ctx.io.mouse_pos;
            const auto &display_size = ctx.io.display_size;
            return position.x <= boundary_margin || position.y <= boundary_margin ||
                   position.x >= display_size.x - boundary_margin || position.y >= display_size.y - boundary_margin;
        }

        inline void try_begin_unbound_drag(detail::Context &ctx, Widget *widget)
        {
            if (!widget->allows_unbounded_drag() || !is_at_drag_boundary(ctx)) return;
            begin_unbound_drag();
        }

    } // namespace

    static void erase_user_data_tag(WidgetUserData *&head, u32 tag)
    {
        WidgetUserData *prev = nullptr;
        for (auto *node = head; node;)
        {
            auto *next = node->pNext;
            if (node->tag_id == tag)
            {
                if (prev) prev->pNext = next;
                else head = next;
                node->pNext = nullptr;
                if (node->destroy && node->handle) node->destroy(node->handle);
                acul::release(node);
                node = next;
                continue;
            }
            prev = node;
            node = next;
        }
    }

    static WidgetUserData *make_user_data_ref_node(u32 tag, void *handle)
    {
        auto *node = acul::alloc<WidgetUserData>();
        node->tag_id = tag;
        node->handle = handle;
        node->destroy = nullptr;
        node->pNext = nullptr;
        return node;
    }

    void Widget::clear_user_data()
    {
        while (_user_data)
        {
            auto *next = _user_data->pNext;
            if (_user_data->destroy && _user_data->handle) _user_data->destroy(_user_data->handle);
            acul::release(_user_data);
            _user_data = next;
        }
    }

    void Widget::pop_user_data_head(u32 expected_tag)
    {
        if (!_user_data || _user_data->tag_id != expected_tag) return;
        auto *next = _user_data->pNext;
        _user_data->pNext = nullptr;
        if (_user_data->destroy && _user_data->handle) _user_data->destroy(_user_data->handle);
        acul::release(_user_data);
        _user_data = next;
    }

    void Widget::emplace_user_data_ref_head(u32 tag, void *handle)
    {
        erase_user_data_tag(_user_data, tag);
        if (!handle) return;
        auto *node = make_user_data_ref_node(tag, handle);
        node->pNext = _user_data;
        _user_data = node;
    }

    void Widget::emplace_user_data_ref_after_head(u32 tag, void *handle)
    {
        erase_user_data_tag(_user_data, tag);
        if (!handle) return;
        auto *node = make_user_data_ref_node(tag, handle);
        if (!_user_data)
        {
            _user_data = node;
            return;
        }
        node->pNext = _user_data->pNext;
        _user_data->pNext = node;
    }

    Widget::~Widget()
    {
        clear_post_effects();
        if (_user_bind)
        {
            acul::release(_user_bind);
            _user_bind = nullptr;
        }
        clear_user_data();
        if (detail::g_context)
        {
            cancel_delayed_tasks(id());
            auto &transient_cache = detail::g_context->transient_cache;
            for (size_t i = 0; i < transient_cache.size();)
            {
                if (transient_cache[i] == this) transient_cache.erase(transient_cache.begin() + i);
                else ++i;
            }
            if (requested_event_flags & EventFlagBits::shortcut) detail::deregister_widget_shortcuts(id());
        }
    }

    void Widget::sync_widget_flags(EventFlags flags)
    {
        const bool visible_changed = static_cast<bool>(_synced_widget_flags & WidgetFlagBits::visible) != is_visible();
        const bool disabled_changed =
            static_cast<bool>(_synced_widget_flags & WidgetFlagBits::disabled) != is_disabled();
        const bool read_only_changed =
            static_cast<bool>(_synced_widget_flags & WidgetFlagBits::read_only) != is_read_only();
        const bool hittable_changed =
            static_cast<bool>(_synced_widget_flags & WidgetFlagBits::hittable) != is_hittable();

        event_flags = flags;
        _synced_widget_flags = widget_flags;

        // Before the widget participates in either draw path, synchronization only commits the requested state.
        // Disposal-queue ordering keeps a live attached/transient widget valid until queued work completes.
        if (!is_attached() && !is_transient()) return;

        auto &ctx = detail::get_context();
        if (visible_changed || disabled_changed || read_only_changed || hittable_changed)
            ctx.dirty_flags |= DirtyFlagBits::hit_rect_update;
        if (visible_changed)
        {
            invalidate_layout_measure();
            if (is_attached() && !(ctx.dirty_flags & DirtyFlagBits::destroying)) rebuild_root_widget_depths();
        }

        const bool disabled_post_missing = is_disabled() && !_disabled_post_fx;
        if (disabled_changed || disabled_post_missing)
        {
            if (is_disabled()) apply_disabled_post_effect();
            else restore_pre_disabled_post_effect();
        }
        if (disabled_changed) on_disabled_changed(is_disabled());

        if (!(visible_changed || disabled_changed || disabled_post_missing)) return;
        if (ctx.dirty_flags & DirtyFlagBits::destroying) return;

        Widget *widget = this;
        ctx.disposal_queue.emplace([widget]() {
            auto &ctx = detail::get_context();
            if (ctx.dirty_flags & DirtyFlagBits::destroying) return;

            if (!widget->is_visible() || widget->is_disabled())
            {
                auto belongs_to_widget = [widget, &ctx](u32 id) {
                    if (!id) return false;
                    const auto it = ctx.id_map.find(id);
                    if (it == ctx.id_map.end()) return false;
                    for (Widget *node = it->second; node; node = node->parent())
                        if (node == widget) return true;
                    return false;
                };
                auto element_belongs_to_widget = [&](ElementID id) {
                    return id.widget_id && belongs_to_widget(id.widget_id);
                };
                if (belongs_to_widget(ctx.focus_id)) ctx.focus_id = 0u;
                if (belongs_to_widget(ctx.active_id)) ctx.active_id = 0u;
                if (element_belongs_to_widget(ctx.hover_id)) ctx.hover_id = {};
                if (element_belongs_to_widget(ctx.io.clicked_id)) ctx.io.clicked_id = {};
                if (element_belongs_to_widget(ctx.io.drag_id))
                {
                    detail::cancel_unbounded_mouse_drag();
                    ctx.io.drag_id = {};
                    ctx.io.drag_key_flags = {};
                }
            }

            widget->reset_external_draw_cull_state();
            if (widget->is_visible())
            {
                if (widget->is_attached()) widget->update_draw_commands(DrawReasonBits::external);
                if (widget->is_transient()) widget->update_draw_commands(DrawReasonBits::transient);
            }
            else
            {
                if (widget->is_attached()) widget->invalidate_draw_commands(DrawReasonBits::external);
                if (widget->is_transient()) widget->invalidate_draw_commands(DrawReasonBits::transient);
            }
            ctx.dirty_flags |= DirtyFlagBits::redraw | DirtyFlagBits::hit_rect_update;
        });
        mark_host_refresh_request();
    }

    PostFxChain *Widget::add_post_effect(PostEffect *effect, const void *instance_data, const void *post_data)
    {
        if (!effect) return nullptr;
        auto *chain = acul::alloc<PostFxChain>();
        chain->post_effect = effect;
        chain->id = effect->push_instance ? effect->push_instance(effect, this, instance_data)
                                          : AUIK_INVALID_POST_EFFECT_DATA_ID;
        chain->post_data =
            post_data ? post_data : (chain->id != AUIK_INVALID_POST_EFFECT_DATA_ID ? &chain->id : nullptr);
        chain->next = nullptr;

        if (!_post_fx_chain)
        {
            _post_fx_chain = chain;
            return chain;
        }

        auto *tail = _post_fx_chain;
        while (tail->next) tail = tail->next;
        tail->next = chain;
        return chain;
    }

    bool Widget::remove_post_effect(PostFxChain *chain)
    {
        if (!chain) return false;
        PostFxChain *prev = nullptr;
        for (auto *node = _post_fx_chain; node; node = node->next)
        {
            if (node != chain)
            {
                prev = node;
                continue;
            }

            if (prev) prev->next = node->next;
            else _post_fx_chain = node->next;
            if (_disabled_post_fx == node) _disabled_post_fx = nullptr;
            if (node->post_effect && node->post_effect->release_instance &&
                node->id != AUIK_INVALID_POST_EFFECT_DATA_ID)
                node->post_effect->release_instance(node->post_effect, node->id);
            node->next = nullptr;
            acul::release(node);
            return true;
        }
        return false;
    }

    void Widget::clear_post_effects()
    {
        while (_post_fx_chain)
        {
            auto *node = _post_fx_chain;
            _post_fx_chain = node->next;
            if (node->post_effect && node->post_effect->release_instance &&
                node->id != AUIK_INVALID_POST_EFFECT_DATA_ID)
                node->post_effect->release_instance(node->post_effect, node->id);
            acul::release(node);
        }
        _disabled_post_fx = nullptr;
    }

    namespace detail
    {
        static bool accepts_style_update(Widget *widget)
        {
            for (auto *child = widget; child && child->parent(); child = child->parent())
            {
                auto *parent = child->parent();
                if (!parent->accepts_child_style_update(child)) return false;
            }
            return true;
        }

        template <class Traits>
        static void enqueue_style_refresh(Widget *widget)
        {
            if (!widget) return;
            const u32 widget_id = widget->id();
            Widget *expected_widget = widget;
            add_render_command<Traits>(widget, [widget_id, expected_widget]() {
                auto &ctx = detail::get_context();
                auto it = ctx.id_map.find(widget_id);
                if (it == ctx.id_map.end() || it->second != expected_widget) return;
                Widget *widget = it->second;
                if (!accepts_style_update(widget)) return;
                const auto style_flags = widget->update_style_invalidated();
                if (style_flags == StyleUpdateFlagBits::none) return;
                if (style_flags & StyleUpdateFlagBits::parent_layout)
                {
                    if (Widget *target = resolve_parent_layout_update_target(widget))
                    {
                        target->update_layout(false);
                        target->update_draw_commands(get_draw_reason_from_style_update(style_flags));
                    }
                    ctx.dirty_flags |= DirtyFlagBits::redraw | DirtyFlagBits::hit_rect_update;
                    return;
                }
                else if (style_flags & StyleUpdateFlagBits::layout) widget->update_layout(false);
                if (style_flags & StyleUpdateFlagBits::redraw)
                {
                    widget->update_draw_commands(get_draw_reason_from_style_update(style_flags));
                    ctx.dirty_flags |= DirtyFlagBits::redraw;
                }
            });
            mark_host_refresh_request();
        }

        template <class Traits>
        static void enqueue_style_refresh_by_id(Context &ctx, u32 widget_id)
        {
            if (!widget_id) return;
            auto it = ctx.id_map.find(widget_id);
            if (it == ctx.id_map.end()) return;
            enqueue_style_refresh<Traits>(it->second);
        }

        static void enqueue_hover_dispatch(Widget *widget, HoverState state)
        {
            if (!widget || !widget->has_event_handler(EventFlagBits::hover)) return;
            const u32 widget_id = widget->id();
            Widget *expected_widget = widget;
            add_render_command<detail::HoverEventTraits>(widget, [widget_id, expected_widget, state]() {
                auto &ctx = detail::get_context();
                auto it = ctx.id_map.find(widget_id);
                if (it == ctx.id_map.end() || it->second != expected_widget) return;
                it->second->dispatch_hover(state);
            });
        }

        static inline StyleState resolve_focus_visual_state(Widget *widget)
        {
            assert(widget && "widget cannot be null");
            return resolve_widget_visual_state(*widget, StyleState::focus);
        }

        AUIK_EXPORT void on_hover_id_updated(const ElementID &prev_hover, const ElementID &hover)
        {
            // The GPU picker is sampled whenever a frame is rendered, including frames caused by unrelated host
            // events. Re-dispatching an unchanged hover queues deferred leave/enter events, which wake the host
            // again and create a self-sustaining render loop.
            if (prev_hover == hover) return;

            auto &ctx = get_context();
            ctx.last_hover_id = prev_hover;
            const bool is_dragging = ctx.io.mouse_down && ctx.io.drag_id;
            const u32 prev_widget_id = prev_hover.widget_id;
            const u32 widget_id = hover.widget_id;
            const bool selector_changed =
                !is_dragging && detail::set_style_selector(hover, hover ? StyleState::hover : StyleState::normal);
            auto dispatch_hover = [&](u32 id, HoverState state) {
                if (id == 0) return;
                auto it = ctx.id_map.find(id);
                if (it == ctx.id_map.end()) return;
                Widget *widget = it->second;
                assert(widget && "widget is null");
                enqueue_hover_dispatch(widget, state);
                if (apply_hover_style_state(*widget, state)) enqueue_style_refresh<detail::HoverEventTraits>(widget);
            };

            if (is_dragging)
            {
                auto accepts_drag_hover = [&](const ElementID &id) -> Widget * {
                    if (!id) return nullptr;
                    auto it = ctx.id_map.find(id.widget_id);
                    if (it == ctx.id_map.end()) return nullptr;
                    Widget *widget = it->second;
                    if (!widget || !widget->accepts_drag_hover(ctx.io.drag_id, id)) return nullptr;
                    return widget;
                };

                Widget *prev_widget = accepts_drag_hover(prev_hover);
                Widget *widget = accepts_drag_hover(hover);
                const bool drag_widget_changed = prev_widget != widget;
                const bool drag_selector_changed = widget
                                                       ? detail::set_style_selector(hover, StyleState::hover)
                                                       : (prev_widget && detail::get_style_selector_id() == prev_hover
                                                              ? detail::set_style_selector({}, StyleState::normal)
                                                              : false);

                if (prev_widget) dispatch_hover(prev_hover.widget_id, HoverState::leave);
                if (widget) dispatch_hover(hover.widget_id, HoverState::enter);

                if (drag_selector_changed)
                {
                    if (prev_widget) enqueue_style_refresh<detail::HoverEventTraits>(prev_widget);
                    if (widget) enqueue_style_refresh<detail::HoverEventTraits>(widget);
                }
                if (drag_widget_changed || drag_selector_changed) mark_host_refresh_request();
                return;
            }

            if (!is_dragging)
            {
                dispatch_hover(prev_widget_id, HoverState::leave);
                dispatch_hover(widget_id, HoverState::enter);
                if (selector_changed)
                {
                    enqueue_style_refresh_by_id<detail::HoverEventTraits>(ctx, prev_widget_id);
                    if (widget_id != prev_widget_id)
                        enqueue_style_refresh_by_id<detail::HoverEventTraits>(ctx, widget_id);
                }
                if (prev_hover != hover || selector_changed) mark_host_refresh_request();
            }
        }

        static inline Widget *resolve_input_root(Context &ctx)
        {
            Widget *target = nullptr;
            if (ctx.io.active_mouse_buttons)
            {
                auto it = ctx.id_map.find(ctx.hover_id.widget_id);
                if (it != ctx.id_map.end()) target = it->second;
            }
            if (ctx.focus_id != 0)
            {
                auto it = ctx.id_map.find(ctx.focus_id);
                if (!target && it != ctx.id_map.end()) target = it->second;
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
            if (!mods && io.active_keys.empty() && !io.active_mouse_buttons) return false;
            Widget *node = resolve_input_root(ctx);
            while (node)
            {
                if (node->has_event_handler(EventFlagBits::shortcut))
                {
                    const u64 shortcut_hash =
                        detail::make_shortcut_hash(io.active_keys, io.active_mouse_buttons, mods, node->id());
                    auto binding = io.shortcuts.find(shortcut_hash);
                    if (binding != io.shortcuts.end())
                    {
                        binding->second();
                        return true;
                    }
                }
                node = node->focus_parent();
            }

            const u64 global_hash =
                detail::make_shortcut_hash(io.active_keys, io.active_mouse_buttons, mods, AUIK_TAG_GLOBAL);
            auto global_it = io.shortcuts.find(global_hash);
            if (global_it == io.shortcuts.end()) return false;
            global_it->second();
            return true;
        }

        AUIK_EXPORT void reset_event_state()
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
                    it->second->dispatch_drag({0.0f, 0.0f}, KeyPressState::release);
            }
            end_unbound_drag();

            io.mouse_down = false;
            io.clicked_id = {};
            io.drag_id = {};
            io.drag_key_flags = {};
            frame_cache.changes = FrameChangesBits::none;
            frame_cache.drag_widget_id = 0;
            frame_cache.drag_delta = {0.0f, 0.0f};
            frame_cache.scroll_delta = {0.0f, 0.0f};
            frame_cache.char_code = 0;
            frame_cache.char_repeat_count = 0;
            io.active_keys.clear();
            io.active_mouse_buttons = {};
            io.active_mods = KeyMode{};
            ctx.active_id = 0;
            ctx.last_hover_id = prev_hover;
            ctx.hover_id = {};
            detail::reset_style_selector();
            if (prev_active_id)
            {
                auto it = ctx.id_map.find(prev_active_id);
                if (it != ctx.id_map.end())
                {
                    if (it->second->id() == ctx.focus_id)
                        it->second->set_style_state(resolve_focus_visual_state(it->second));
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
                    if (widget->has_event_handler(EventFlagBits::hover)) widget->dispatch_hover(HoverState::leave);
                    if (apply_hover_style_state(*widget, HoverState::leave))
                        enqueue_style_refresh<detail::HoverEventTraits>(widget);
                }
            }
            set_window_cursor(CursorID::arrow, ctx.window_ctx);
        }

        AUIK_EXPORT void on_mouse_move(const amal::vec2 &delta)
        {
            auto &ctx = get_context();
            auto &io = ctx.io;

            // Raw mouse events carry relative movement and may arrive several times
            // before the host renders the next frame. Keep their accumulation in
            // auik so window backends only have to forward the native event.
            if (ctx.raw_mouse_mode)
            {
                if (!io.drag_id || delta == amal::vec2{0.0f}) return;
                auto &frame_cache = ctx.frame_cache;
                frame_cache.drag_widget_id = io.drag_id.widget_id;
                frame_cache.drag_delta += delta;
                frame_cache.changes |= FrameChangesBits::drag_delta;
                mark_host_refresh_request();
                return;
            }

            if (!io.mouse_down || !io.clicked_id) return;
            if (!io.active_mouse_buttons) return;

            amal::vec2 drag_delta = delta != amal::vec2{0.0f} ? delta : io.mouse_pos - io.last_drag_pos;
            if (drag_delta == amal::vec2{0.0f}) return;

            if (io.mouse_pos != io.last_drag_pos) io.last_drag_pos = io.mouse_pos;
            else io.last_drag_pos += drag_delta;

            const bool begin_drag = !io.drag_id;
            if (begin_drag)
            {
                io.drag_id = io.clicked_id;
                io.drag_key_flags = io.active_mouse_buttons;
            }
            auto it = ctx.id_map.find(io.drag_id.widget_id);
            if (it != ctx.id_map.end() && it->second->has_event_handler(EventFlagBits::drag))
            {
                it->second->dispatch_drag(drag_delta, begin_drag ? KeyPressState::press : KeyPressState::repeat);
                try_begin_unbound_drag(ctx, it->second);
            }
            mark_host_refresh_request();
        }

        AUIK_EXPORT void cancel_unbounded_mouse_drag() { end_unbound_drag(); }

        AUIK_EXPORT void on_scroll_event(const amal::vec2 &pos)
        {
            auto &ctx = get_context();
            auto &frame_cache = ctx.frame_cache;
            frame_cache.scroll_delta += pos;
            frame_cache.changes |= FrameChangesBits::scroll_delta;
            mark_host_refresh_request();
        }

        AUIK_EXPORT void on_key_event(Key key, KeyPressState state, KeyMode mods)
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
            if (target) target->dispatch_key(key, state, mods);
        }

        AUIK_EXPORT void on_char_event(u32 char_code)
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
                    if (target) target->dispatch_char(frame_cache.char_code, frame_cache.char_repeat_count);
                }
                frame_cache.char_code = char_code;
                frame_cache.char_repeat_count = 1;
            }
            frame_cache.changes |= FrameChangesBits::char_input;
            mark_host_refresh_request();
        }

        AUIK_EXPORT void deregister_widget_shortcuts(u32 widget_id)
        {
            if (!widget_id) return;
            auto &io = get_context().io;
            auto it = io.widget_shortcuts.find(widget_id);
            if (it == io.widget_shortcuts.end()) return;
            for (u64 hash : it->second) io.shortcuts.erase(hash);
            io.widget_shortcuts.erase(it);
        }

        template <class F>
        static inline void for_each_active_chain(Context &ctx, u32 leaf_id, F &&fn)
        {
            auto it = ctx.id_map.find(leaf_id);
            if (it == ctx.id_map.end()) return;
            Widget *node = it->second;
            while (node)
            {
                fn(node);
                node = node->focus_parent();
            }
        }

        static inline bool is_focus_chain_widget(Context &ctx, const Widget *widget)
        {
            if (!widget || ctx.focus_id == 0u) return false;
            auto it = ctx.id_map.find(ctx.focus_id);
            if (it == ctx.id_map.end()) return false;
            for (Widget *node = it->second; node; node = node->focus_parent())
                if (node == widget) return true;
            return false;
        }

        static inline void resolve_release_state(Widget *widget, u32 hovered_id)
        {
            assert(widget && "widget cannot be null");
            // Focus visuals are managed in focus transition path (on_focus callbacks).
            // Do not overwrite focused widgets during mouse release.
            auto &ctx = detail::get_context();
            if (is_focus_chain_widget(ctx, widget) && has_widget_state_style(*widget, StyleState::focus))
                widget->set_style_state(StyleState::focus);
            else if (widget->id() == hovered_id && has_widget_state_style(*widget, StyleState::hover))
                widget->set_style_state(StyleState::hover);
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

        static inline Widget *resolve_focus_entry(Widget *leaf)
        {
            if (!leaf) return nullptr;
            return leaf->focus_parent() ? leaf->focus_parent() : leaf;
        }

        static inline void notify_focus_leaf(Context &ctx, Widget *leaf, Widget *entry, bool focused)
        {
            if (!leaf || leaf == entry) return;
            if (leaf->has_event_handler(EventFlagBits::focus)) leaf->dispatch_focus(focused);
            if (!focused && ctx.active_id != leaf->id()) leaf->set_style_state(StyleState::normal);
            else if (focused && ctx.active_id != leaf->id()) leaf->set_style_state(resolve_focus_visual_state(leaf));
            enqueue_style_refresh<detail::FocusEventTraits>(leaf);
        }

        static void set_focus_target(Context &ctx, Widget *target)
        {
            const u32 next_focus_id = target ? target->id() : 0u;
            if (ctx.focus_id == next_focus_id) return;

            Widget *old_leaf = get_widget_by_id(ctx.focus_id);
            Widget *new_leaf = target;
            Widget *old_entry = resolve_focus_entry(old_leaf);
            Widget *new_entry = resolve_focus_entry(new_leaf);

            // If focus tree root is unchanged, keep parent focus state intact.
            if (old_entry == new_entry)
            {
                ctx.focus_id = next_focus_id;
                notify_focus_leaf(ctx, old_leaf, old_entry, false);
                notify_focus_leaf(ctx, new_leaf, new_entry, true);
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

            // Switch current focus leaf id before callbacks so blur/gain handlers can query focus state directly.
            ctx.focus_id = next_focus_id;

            // Blur: leaf-side to (but excluding) LCA.
            notify_focus_leaf(ctx, old_leaf, old_entry, false);
            for (int k = 0; k <= i; ++k)
            {
                Widget *w = path_old[k];
                assert(w && "widget cannot be null");
                if (w->has_event_handler(EventFlagBits::focus)) w->dispatch_focus(false);
                if (ctx.active_id != w->id()) w->set_style_state(StyleState::normal);
                enqueue_style_refresh<detail::FocusEventTraits>(w);
            }

            // Focus: from LCA child down to new leaf-side.
            for (int k = j; k >= 0; --k)
            {
                Widget *w = path_new[k];
                assert(w && "widget cannot be null");
                if (w->has_event_handler(EventFlagBits::focus)) w->dispatch_focus(true);
                if (ctx.active_id != w->id()) w->set_style_state(resolve_focus_visual_state(w));
                enqueue_style_refresh<detail::FocusEventTraits>(w);
            }
            notify_focus_leaf(ctx, new_leaf, new_entry, true);
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

            if (!io.drag_id) io.clicked_id = ctx.hover_id;

            auto it = ctx.id_map.find(ctx.hover_id.widget_id);
            if (it != ctx.id_map.end())
            {
                if (it->second->accepts_focus_on_mouse_press(ctx.hover_id)) set_focus_target(ctx, it->second);
                // Press transfers visual state from hover -> active immediately.
                // Hover re-enter is restored later by hover update/release path.
                if (it->second->has_event_handler(EventFlagBits::hover)) it->second->dispatch_hover(HoverState::leave);
                const u32 prev_active_id = ctx.active_id;
                const u32 next_active_id = it->second->id();
                const StyleState prev_style_state = it->second->style_state();
                if (prev_active_id != next_active_id)
                    for_each_active_chain(ctx, prev_active_id, [&](Widget *w) {
                        resolve_release_state(w, ctx.hover_id.widget_id);
                        enqueue_style_refresh<detail::ClickEventTraits>(w);
                    });
                ctx.active_id = next_active_id;
                const StyleState prev_style = it->second->style_state();
                // Pressed state has priority over focus for the duration of the press.
                // resolve_release_state() restores focus after release.
                StyleState next_style = StyleState::active;
                if (prev_style != next_style) it->second->set_style_state(next_style);
                // Pressed element (tag-based selector target) should always enter active visuals,
                // even if owning widget keeps focus state.
                detail::set_style_selector(ctx.hover_id, ctx.hover_id ? StyleState::active : StyleState::normal);
                const bool active_changed = prev_active_id != ctx.active_id;
                const bool style_changed = it->second->style_state() != prev_style_state;
                if (active_changed || style_changed)
                    for_each_active_chain(ctx, ctx.active_id,
                                          [&](Widget *w) { enqueue_style_refresh<detail::ClickEventTraits>(w); });
                if (it->second->has_event_handler(EventFlagBits::click))
                    it->second->dispatch_click(MouseKey::left, KeyPressState::press, io.click_count);
            }
            else set_focus_target(ctx, nullptr);
        }

        static void finish_drag(Context &ctx, IO &io)
        {
            auto &frame_cache = ctx.frame_cache;
            const ElementID drag_id = io.drag_id;
            const ElementID hover_id = ctx.hover_id;

            const u32 pending_drag_widget_id = frame_cache.drag_widget_id;
            const amal::vec2 pending_drag_delta = frame_cache.drag_delta;

            frame_cache.drag_widget_id = 0;
            frame_cache.drag_delta = {0.0f, 0.0f};
            frame_cache.changes &= ~FrameChangesBits::drag_delta;

            // Raw relative movement is accumulated until sync_frame(). Mouse-up may arrive first, so apply the
            // final delta before drop/release instead of losing the last input sample.
            if (pending_drag_widget_id == drag_id.widget_id && pending_drag_delta != amal::vec2{0.0f})
            {
                auto pending_it = ctx.id_map.find(pending_drag_widget_id);
                if (pending_it != ctx.id_map.end() && pending_it->second->has_event_handler(EventFlagBits::drag))
                    pending_it->second->dispatch_drag(pending_drag_delta, KeyPressState::repeat);
            }

            auto it = ctx.id_map.find(hover_id.widget_id);
            if (drag_id && hover_id && it != ctx.id_map.end() && it->second->has_event_handler(EventFlagBits::drop) &&
                it->second->accepts_drag_hover(drag_id, hover_id))
                it->second->dispatch_drop(drag_id, hover_id);

            it = ctx.id_map.find(drag_id.widget_id);
            if (drag_id && it != ctx.id_map.end() && it->second->has_event_handler(EventFlagBits::drag))
                it->second->dispatch_drag({0.0f, 0.0f}, KeyPressState::release);
            end_unbound_drag();

            io.drag_id = {};
            io.drag_key_flags = {};
            io.clicked_id = {};
        }

        static void handle_left_mouse_release(Context &ctx, IO &io, bool finish_active_drag)
        {
            const ElementID clicked_id = io.clicked_id;
            const ElementID hover_id = ctx.hover_id;
            const u32 clicked_widget_id = clicked_id.widget_id;
            auto it = ctx.id_map.find(io.clicked_id.widget_id);
            if (it != ctx.id_map.end() && it->second->has_event_handler(EventFlagBits::click))
                it->second->dispatch_click(MouseKey::left, KeyPressState::release, io.click_count);

            if (finish_active_drag) finish_drag(ctx, io);

            if (ctx.active_id != 0)
            {
                const u32 prev_active_id = ctx.active_id;
                ctx.active_id = 0;
                detail::set_style_selector(ctx.hover_id, ctx.hover_id ? StyleState::hover : StyleState::normal);
                for_each_active_chain(ctx, prev_active_id, [&](Widget *w) {
                    resolve_release_state(w, ctx.hover_id.widget_id);
                    enqueue_style_refresh<detail::HoverEventTraits>(w);
                });
                mark_host_refresh_request();
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
                            clicked_it->second->dispatch_hover(HoverState::enter);
                    }
                    else if (style_changed) enqueue_style_refresh<detail::HoverEventTraits>(clicked_it->second);
                }
            }

            if (clicked_id || hover_id) on_hover_id_updated(clicked_id, hover_id);

            if (!io.active_mouse_buttons) io.clicked_id = {};
        }

        static void handle_aux_mouse_press(Context &ctx, IO &io, MouseKey key)
        {
            io.mouse_down = true;
            io.last_drag_pos = io.mouse_pos;
            if (!io.clicked_id && !io.drag_id) io.clicked_id = ctx.hover_id;

            auto it = ctx.id_map.find(ctx.hover_id.widget_id);
            if (it == ctx.id_map.end()) return;
            if (it->second->has_event_handler(EventFlagBits::click))
                it->second->dispatch_click(key, KeyPressState::press, 1);
        }

        static void handle_aux_mouse_release(Context &ctx, IO &io, MouseKey key, bool finish_active_drag)
        {
            auto it = ctx.id_map.find(io.clicked_id.widget_id);
            if (it != ctx.id_map.end() && it->second->has_event_handler(EventFlagBits::click))
                it->second->dispatch_click(key, KeyPressState::release, 1);

            if (finish_active_drag) finish_drag(ctx, io);
            if (!io.active_mouse_buttons) io.clicked_id = {};
        }

        AUIK_EXPORT void on_mouse_click_event(MouseKey key, KeyPressState state)
        {
            auto &ctx = get_context();
            auto &io = ctx.io;
            update_window_time(ctx.window_ctx);

            const MouseKeyFlags key_flags = MouseKeyFlags(key);
            if (state == KeyPressState::release) io.active_mouse_buttons &= ~key_flags;
            else io.active_mouse_buttons |= key_flags;
            io.mouse_down = static_cast<bool>(io.active_mouse_buttons);
            const bool finish_active_drag = io.drag_id && (io.drag_key_flags & key_flags);

            if (key != MouseKey::left)
            {
                if (state == KeyPressState::press)
                {
                    dispatch_shortcut(ctx);
                    handle_aux_mouse_press(ctx, io, key);
                }
                else if (state == KeyPressState::release) handle_aux_mouse_release(ctx, io, key, finish_active_drag);
                return;
            }

            if (state == KeyPressState::press)
            {
                dispatch_shortcut(ctx);
                handle_left_mouse_press(ctx, io);
                return;
            }

            if (state == KeyPressState::release) handle_left_mouse_release(ctx, io, finish_active_drag);
        }

        AUIK_EXPORT void flush_frame_changes()
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
                    {
                        it->second->dispatch_drag(drag_delta, KeyPressState::repeat);
                        try_begin_unbound_drag(ctx, it->second);
                    }
                }
            }

            if (changes & FrameChangesBits::scroll_delta)
            {
                Widget *target = resolve_scroll_target(ctx);
                if (target) target->dispatch_scroll(frame_cache.scroll_delta);
                frame_cache.scroll_delta = {0.0f, 0.0f};
            }

            if (changes & FrameChangesBits::char_input)
            {
                if (frame_cache.char_repeat_count > 0)
                {
                    Widget *target = resolve_focus_event_target(ctx, EventFlagBits::char_input);
                    if (target) target->dispatch_char(frame_cache.char_code, frame_cache.char_repeat_count);
                }
                frame_cache.char_code = 0;
                frame_cache.char_repeat_count = 0;
            }

            frame_cache.changes = FrameChangesBits::none;
        }

    } // namespace detail

    AUIK_EXPORT bool begin_unbound_drag()
    {
        auto &ctx = detail::get_context();
        assert(ctx.window_ctx && "begin_unbound_drag: no window context");
        if (!ctx.window_ctx || !ctx.window_ctx->set_unbounded_mouse_drag) return false;
        if (ctx.window_ctx->is_unbound_mode) return true;
        const amal::vec2 cursor_position = detail::set_unbounded_mouse_drag(true, ctx.window_ctx);
        ctx.io.mouse_pos = cursor_position;
        ctx.io.last_drag_pos = cursor_position;
        ctx.raw_mouse_mode = true;
        ctx.window_ctx->is_unbound_mode = true;
        return true;
    }

    AUIK_EXPORT void end_unbound_drag()
    {
        auto &ctx = detail::get_context();
        assert(ctx.window_ctx && "end_unbound_drag: no window context");
        if (!ctx.window_ctx->is_unbound_mode) return;
        ctx.raw_mouse_mode = false;
        ctx.window_ctx->is_unbound_mode = false;
        if (!ctx.window_ctx->set_unbounded_mouse_drag) return;
        const amal::vec2 cursor_position = detail::set_unbounded_mouse_drag(false, ctx.window_ctx);
        ctx.io.mouse_pos = cursor_position;
        ctx.io.last_drag_pos = cursor_position;
    }

    AUIK_EXPORT void detail::request_style_refresh(Widget *widget)
    {
        if (!widget || !detail::g_context) return;
        auto &ctx = detail::get_context();
        if (ctx.dirty_flags & DirtyFlagBits::destroying) return;

        const auto attached = ctx.id_map.find(widget->id());
        if (attached == ctx.id_map.end() || attached->second != widget) return;

        const auto style_flags = widget->update_style_invalidated();
        if (style_flags == StyleUpdateFlagBits::none) return;

        Widget *owner = nullptr;
        if (style_flags & StyleUpdateFlagBits::parent_layout) owner = resolve_parent_layout_update_target(widget);
        else owner = widget;

        for (auto *candidate = owner; candidate; candidate = candidate->parent())
        {
            auto it = ctx.id_map.find(candidate->id());
            if (it == ctx.id_map.end() || it->second != candidate) continue;
            owner = candidate;
            break;
        }
        auto owner_it = ctx.id_map.find(owner->id());
        if (owner_it == ctx.id_map.end() || owner_it->second != owner) return;

        const u32 owner_id = owner->id();
        Widget *expected_owner = owner;
        add_render_command([owner_id, expected_owner, style_flags]() {
            auto &ctx = detail::get_context();
            auto it = ctx.id_map.find(owner_id);
            if (it == ctx.id_map.end() || it->second != expected_owner) return;

            const bool layout_dirty = style_flags & (StyleUpdateFlagBits::layout | StyleUpdateFlagBits::parent_layout);
            if (layout_dirty) expected_owner->update_layout(false);
            expected_owner->update_draw_commands(layout_dirty ? DrawReasonBits::layout : DrawReasonBits::external);
            ctx.dirty_flags |= DirtyFlagBits::redraw;
            if (layout_dirty) ctx.dirty_flags |= DirtyFlagBits::hit_rect_update;
        });
    }

    AUIK_EXPORT void detail::request_widget_refresh(Widget *widget, StyleUpdateFlags flags)
    {
        if (!widget || !detail::g_context || flags == StyleUpdateFlagBits::none) return;
        auto &ctx = detail::get_context();
        if (ctx.dirty_flags & DirtyFlagBits::destroying) return;
        const auto attached = ctx.id_map.find(widget->id());
        if (attached == ctx.id_map.end() || attached->second != widget) return;

        Widget *owner =
            flags & StyleUpdateFlagBits::parent_layout ? resolve_parent_layout_update_target(widget) : widget;
        if (!owner) return;
        if (flags & (StyleUpdateFlagBits::layout | StyleUpdateFlagBits::parent_layout))
            widget->invalidate_layout_measure();
        const u32 owner_id = owner->id();
        Widget *expected_owner = owner;
        add_render_command([owner_id, expected_owner, flags]() {
            auto &ctx = detail::get_context();
            const auto current = ctx.id_map.find(owner_id);
            if (current == ctx.id_map.end() || current->second != expected_owner) return;
            const bool layout_dirty = flags & (StyleUpdateFlagBits::layout | StyleUpdateFlagBits::parent_layout);
            if (layout_dirty) expected_owner->update_layout(false);
            expected_owner->update_draw_commands(layout_dirty ? DrawReasonBits::layout : DrawReasonBits::external);
            ctx.dirty_flags |= DirtyFlagBits::redraw;
            if (layout_dirty) ctx.dirty_flags |= DirtyFlagBits::hit_rect_update;
            mark_host_refresh_request();
        });
    }

    AUIK_EXPORT void set_cursor(CursorID::enum_type cursor)
    {
        auto &ctx = detail::get_context();
        detail::set_window_cursor(cursor, ctx.window_ctx);
    }

    AUIK_EXPORT bool is_widget_focused(const Widget *widget)
    {
        return widget && detail::get_context().focus_id == widget->id();
    }

    AUIK_EXPORT bool is_widget_hovered(const Widget *widget)
    {
        return widget && detail::get_context().hover_id.widget_id == widget->id();
    }

    AUIK_EXPORT void focus_widget(Widget *widget) { detail::set_focus_target(detail::get_context(), widget); }
} // namespace auik
