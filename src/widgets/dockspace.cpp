#include <auik/auik.hpp>
#include <auik/detail/depth.hpp>
#include <auik/pipelines.hpp>
#include <auik/widgets/detail/popup_trigger.hpp>
#include <auik/widgets/detail/selectable.hpp>
#include <auik/widgets/detail/table_base.hpp>
#include <auik/widgets/dockspace.hpp>
#include <auik/widgets/menu.hpp>
#include "../core/session_stream_utils.hpp"

#define AUIK_DOCKSPACE_VERTICAL_DROP_HIT_SIZE      50.0f
#define AUIK_DOCKSPACE_VERTICAL_DROP_BAND_FRACTION (1.0f / 3.0f)

namespace auik
{
    DockspaceContext *get_dockspace_context()
    {
        auto &ctx = detail::get_context();
        if (!ctx.dockspace_context) ctx.dockspace_context = acul::alloc<DockspaceContext>();
        return ctx.dockspace_context;
    }

    void destroy_dockspace_context()
    {
        if (!detail::g_context || !detail::g_context->dockspace_context) return;
        acul::release(detail::g_context->dockspace_context);
        detail::g_context->dockspace_context = nullptr;
    }

    void register_dockspace(Dockspace *dockspace)
    {
        if (!dockspace) return;
        auto *ctx = get_dockspace_context();
        for (auto *item : ctx->docks)
            if (item == dockspace) return;
        ctx->docks.push_back(dockspace);
    }

    void unregister_dockspace(Dockspace *dockspace)
    {
        if (!detail::g_context || !detail::g_context->dockspace_context || !dockspace) return;
        auto &docks = detail::g_context->dockspace_context->docks;
        for (u32 i = 0; i < docks.size(); ++i)
        {
            if (docks[i] != dockspace) continue;
            docks.erase(docks.begin() + i);
            break;
        }
    }

    static DockNodeSettings normalize_dock_node_settings(DockNodeSettings settings)
    {
        if (settings.style_tag == 0u) settings.style_tag = AUIK_STYLE_TAG_DOCKSPACE_NODE;
        return settings;
    }

    static PopupMenu *window_popup_menu(Window *window)
    {
        if (!window || !window->is_popup_menu()) return nullptr;
        auto *menu = window->get_menu();
        return menu && menu->is_popup_menu() ? static_cast<PopupMenu *>(menu->get_widget()) : nullptr;
    }

    void enable_dockspace_drag_zones(Window *window)
    {
        if (!window) return;
        auto *ctx = get_dockspace_context();
        ctx->drag_window = window;
        ctx->drag_zones_enabled = true;
        ctx->drag_zones_dirty = true;
        detail::get_context().dirty_flags |= DirtyFlagBits::redraw | DirtyFlagBits::hit_rect_update;
        for (auto *dock : ctx->docks)
            if (dock) dock->update_drag_zones();
        ctx->drag_zones_dirty = false;
        mark_host_refresh_request();
    }

    void disable_dockspace_drag_zones(Window *window, const char *reason)
    {
        (void)reason;
        if (!detail::g_context || !detail::g_context->dockspace_context) return;
        auto *ctx = detail::g_context->dockspace_context;
        if (window && ctx->drag_window != window) return;
        ctx->drag_window = nullptr;
        ctx->drag_zones_enabled = false;
        ctx->drag_zones_dirty = true;
        detail::get_context().dirty_flags |= DirtyFlagBits::redraw | DirtyFlagBits::hit_rect_update;
        for (auto *dock : ctx->docks)
            if (dock) dock->update_drag_zones();
        ctx->drag_zones_dirty = false;
        mark_host_refresh_request();
    }

    enum DockMenuAction : u32
    {
        DockMenuActionUndock = 0u,
        DockMenuActionCloseWindow = 1u,
        DockMenuActionCloseGroup = 2u
    };

    static inline amal::vec2 get_dock_menu_popup_depth_range(const amal::vec2 &dock_depth_range)
    {
        amal::vec2 overlay_range = detail::depth_foreground_range(dock_depth_range);
        amal::vec2 next_range{};
        assign_next_depth(overlay_range, next_range);
        return next_range;
    }

    static f32 get_dock_drop_zone_visual_depth() { return next_depth(detail::get_global_foreground_depth_range()); }

    static amal::vec2 get_dock_drag_hit_depth_range() { return detail::get_global_foreground_depth_range(); }

    static f32 get_dock_tab_panel_hit_depth()
    {
        const amal::vec2 range = get_dock_drag_hit_depth_range();
        return (range.x + range.y) * 0.5f;
    }

    static f32 get_dock_helper_hit_depth() { return next_depth(get_dock_drag_hit_depth_range()); }

    static amal::vec2 get_tab_drag_grab_offset(Tabbar *tabbar, u32 element_id, const amal::vec2 &fallback)
    {
        if (!tabbar) return fallback;
        const auto mouse = detail::get_io().mouse_pos;
        amal::vec2 grab = fallback;
        if (tabbar->has_drag_grab_offset()) grab = tabbar->drag_grab_offset();
        else
        {
            for (auto *tab : *tabbar)
            {
                if (!tab || tab->get_rect().id.element_id != element_id) continue;
                const auto tab_bounds = tab->bounds();
                grab.x = amal::clamp(mouse.x - tab_bounds.offset.x, 0.0f, amal::max(tab_bounds.size.x, 0.0f));
                break;
            }
        }

        const auto tabbar_bounds = tabbar->bounds();
        if (tabbar_bounds.size.y > 0.0f)
            grab.y = amal::clamp(mouse.y - tabbar_bounds.offset.y, 0.0f, tabbar_bounds.size.y);
        return grab;
    }

    static bool is_tabbar_drag_escape(Tabbar *tabbar, u32 element_id)
    {
        if (!tabbar || element_id == 0u) return false;
        u32 index = static_cast<u32>(-1);
        for (u32 i = 0u; i < tabbar->child_size(); ++i)
        {
            if (tabbar->item_element_id(i) != element_id) continue;
            index = i;
            break;
        }
        if (index >= tabbar->child_size()) return false;

        const auto own_bounds = tabbar->bounds();
        const auto *tab = tabbar->item_at(index);
        const f32 escape_x = tab ? amal::max(tab->size().x, 1.0f) : own_bounds.size.y;
        const f32 escape_y = amal::max(own_bounds.size.y, 1.0f);
        const auto mouse = detail::get_io().mouse_pos;
        return mouse.x < own_bounds.offset.x - escape_x ||
               mouse.x > own_bounds.offset.x + own_bounds.size.x + escape_x ||
               mouse.y < own_bounds.offset.y - escape_y || mouse.y > own_bounds.offset.y + own_bounds.size.y + escape_y;
    }

    static f32 axis_size(const amal::vec2 &value, amal::axis axis) { return axis == amal::axis::x ? value.x : value.y; }

    static acul::string window_tab_title(Window *window)
    {
        if (!window) return {};
        if (const auto *text = window->title_text()) return text->text();
        return window->title();
    }

    static StringView window_tab_title_view(Window *window)
    {
        if (!window) return {};
        if (const auto *text = window->title_text()) return text->source_text();
        return {window->title()};
    }

    static f32 cross_size(const amal::vec2 &value, amal::axis axis)
    {
        return axis == amal::axis::x ? value.y : value.x;
    }

    static amal::vec2 make_axis_size(amal::axis axis, f32 main, f32 cross)
    {
        return axis == amal::axis::x ? amal::vec2{main, cross} : amal::vec2{cross, main};
    }

    static void set_axis_size(amal::vec2 &value, amal::axis axis, f32 size)
    {
        if (axis == amal::axis::x) value.x = size;
        else value.y = size;
    }

    static amal::rect snap_layout_rect(const amal::rect &rect)
    {
        const f32 x0 = amal::ceil(rect.offset.x);
        const f32 y0 = amal::ceil(rect.offset.y);
        const f32 x1 = amal::floor(rect.offset.x + rect.size.x);
        const f32 y1 = amal::floor(rect.offset.y + rect.size.y);
        return {{x0, y0}, {amal::max(x1 - x0, 0.0f), amal::max(y1 - y0, 0.0f)}};
    }

    static bool has_resize_helper_flag(const DockspaceResizeFlags &flags, amal::axis axis)
    {
        return axis == amal::axis::x ? (flags & DockspaceResizeFlagBits::resize_helper_x)
                                     : (flags & DockspaceResizeFlagBits::resize_helper_y);
    }

    static bool has_visible_resize_helper_flag(const DockspaceResizeFlags &flags, amal::axis axis)
    {
        return axis == amal::axis::x ? (flags & DockspaceResizeFlagBits::visible_resize_helper_x)
                                     : (flags & DockspaceResizeFlagBits::visible_resize_helper_y);
    }

    static StyleState resolve_resize_helper_state(const detail::RectData &rect)
    {
        const auto &ctx = detail::get_context();
        if (ctx.io.drag_id == rect.id) return StyleState::active;
        if (ctx.hover_id == rect.id) return StyleState::hover;
        return StyleState::normal;
    }

    static u32 resize_helper_tag_for_axis(amal::axis axis)
    {
        return axis == amal::axis::x ? AUIK_TAG_DOCKSPACE_RESIZE_HELPER_V : AUIK_TAG_DOCKSPACE_RESIZE_HELPER_H;
    }

    static bool is_resize_helper_tag(u32 tag_id)
    {
        return tag_id == AUIK_TAG_DOCKSPACE_RESIZE_HELPER_V || tag_id == AUIK_TAG_DOCKSPACE_RESIZE_HELPER_H;
    }

    static void update_existing_resize_helper_hit(DrawDataID &draw_id, const detail::RectData &rect)
    {
        if (draw_id.hit_id == AUIK_INVALID_DRAW_DATA_ID) return;
        update_hit_rect(draw_id.hit_id, rect, true);
    }

    static u32 make_dockspace_tabbar_id(u32 dockspace_id, DockNodeID node_id)
    {
        return dockspace_id + AUIK_DOCKSPACE_TABBAR_ID_BASE + node_id;
    }

    static u32 make_dockspace_menu_id(u32 dockspace_id, DockNodeID node_id)
    {
        return dockspace_id + AUIK_DOCKSPACE_MENU_ID_BASE + node_id;
    }

    static amal::vec2 make_dockspace_tabbar_depth_range(const amal::vec2 &dock_depth_range)
    {
        amal::vec2 tabbar_range{};
        assign_next_depth(detail::depth_work_range(dock_depth_range), tabbar_range);
        return tabbar_range;
    }

    static bool is_valid_depth_range(const amal::vec2 &range) { return range.y > range.x; }

    static amal::rect make_resize_helper_hit_bounds(amal::axis axis, const amal::rect &bounds, const amal::vec4 &margin)
    {
        amal::rect hit_bounds = bounds;
        if (axis == amal::axis::x)
        {
            hit_bounds.offset.x -= margin.x;
            hit_bounds.size.x += margin.x + margin.z;
        }
        else
        {
            hit_bounds.offset.y -= margin.y;
            hit_bounds.size.y += margin.y + margin.w;
        }
        return snap_layout_rect(hit_bounds);
    }

    static bool can_invalidate_dock_chrome()
    {
        auto &ctx = detail::get_context();
        const auto *stream = get_primary_quads_stream();
        return ctx.gpu_ctx && stream && stream->runtime_data;
    }

    static void invalidate_window_draw_records(Window *window)
    {
        if (!window || !can_invalidate_dock_chrome()) return;
        window->invalidate_draw_commands(DrawReasonBits::layout);
    }

    static bool remove_root_widget(Widget *widget) { return remove_widget_from_root_unsync(widget); }

    static void rerecord_root_widgets_after(Widget *anchor)
    {
        if (!anchor || anchor->parent()) return;
        auto &ctx = detail::get_context();
        bool after_anchor = false;
        for (auto *widget : ctx.widget_tree)
        {
            if (!after_anchor)
            {
                after_anchor = widget == anchor;
                continue;
            }
            if (!widget) continue;
            widget->invalidate_draw_commands(DrawReasonBits::layout);
            widget->reset_draw_records();
            widget->update_draw_commands(DrawReasonBits::full_redraw | DrawReasonBits::record);
        }
        ctx.dirty_flags |= DirtyFlagBits::redraw | DirtyFlagBits::hit_rect_update;
    }

    class Dockspace::DockMenu final : public Widget
    {
    public:
        DockMenu(Dockspace *dockspace, DockNodeID node_id)
            : Widget(make_dockspace_menu_id(dockspace->id(), node_id),
                     WidgetFlagBits::visible | WidgetFlagBits::attachable | WidgetFlagBits::hittable,
                     EventFlagBits::click | EventFlagBits::hover | EventFlagBits::focus, {},
                     AUIK_TAG_DOCKSPACE_MENU_BUTTON),
              _dockspace(dockspace),
              _node_id(node_id),
              _button(AUIK_STYLE_TAG_DOCK_TABBAR_MENU, AUIK_TAG_DOCKSPACE_MENU_BUTTON, AUIK_ICON_MENU, AUIK_ICON_MENU,
                      false)
        {
            set_parent(dockspace);
            set_focus_parent(dockspace);
            _button.set_update_target(this);
            _button.set_hit_id(make_element_id(id(), AUIK_TAG_DOCKSPACE_MENU_BUTTON, 0u));
            _button.set_element_id(0u);
        }

        ~DockMenu() override { release_menu(true); }

        void set_node(DockNodeID node_id)
        {
            if (_node_id == node_id) return;
            release_menu(true);
            _node_id = node_id;
            _id = make_dockspace_menu_id(_dockspace->id(), node_id);
            _button.set_hit_id(make_element_id(id(), AUIK_TAG_DOCKSPACE_MENU_BUTTON, 0u));
        }

        void set_open(bool value)
        {
            if (_open == value) return;
            _open = value;
            _button.set_open(value);
            if (_open) open_menu();
            else if (_menu) _menu->set_open(false);
        }

        bool is_open() const { return _open; }
        bool owns_active_window_menu() const { return _menu_owner && _menu_owner == active_window(); }

        bool sync_menu_model(bool force = false)
        {
            auto *window = active_window();
            const bool source_changed = _menu_owner ? _menu_owner != window : active_window_menu() != nullptr;
            if (!force && _model_ready && !source_changed) return _model_has_items;
            _model_has_items = rebuild_menu_model();
            _model_ready = true;
            return _model_has_items;
        }

        void update_layout_min_size_force() override
        {
            _button.update_style(id(), parent() ? parent()->id() : 0u, resolve_button_state());
            _button.update_layout_min_size_force({0.0f, 0.0f}, true);
            set_required_size(_button.required_size());
        }

        void update_layout(bool min_size_known) override
        {
            if (layout_measure_required(min_size_known)) update_layout_min_size_force();
            layout_button(bounds(), clip_id());
        }

        void layout_button(const amal::rect &bounds, u16 clip_id)
        {
            get_rect().bounds = bounds;
            set_layout_size(bounds.size);
            set_position(bounds.offset);
            set_clip_id(clip_id);
            _button.update_style(id(), parent() ? parent()->id() : 0u, resolve_button_state());
            _button.update_layout(bounds, clip_id);
            if (_open) refresh_menu_anchor();
            Widget::update_layout(true);
        }

        StyleUpdateFlags update_style() override
        {
            StyleUpdateFlags out = StyleUpdateFlagBits::none;
            out |= _button.update_style(id(), parent() ? parent()->id() : 0u, resolve_button_state());
            if (_menu) out |= _menu->update_style_invalidated();
            return out;
        }

        void update_depth(const amal::vec2 &dock_depth_range) override
        {
            update_depth(dock_depth_range, make_dockspace_tabbar_depth_range(dock_depth_range));
        }

        void update_depth(const amal::vec2 &dock_depth_range, const amal::vec2 &tabbar_depth_range)
        {
            Widget::update_depth(dock_depth_range);
            _button.update_depth(tabbar_depth_range);
            if (_menu) _menu->update_depth(get_dock_menu_popup_depth_range(dock_depth_range));
        }

        void back_hit_depth() override
        {
            Widget::back_hit_depth();
            _button.back_hit_depth();
            if (_menu) _menu->back_hit_depth();
        }

        void restore_hit_depth() override
        {
            Widget::restore_hit_depth();
            _button.restore_hit_depth();
            if (_menu) _menu->restore_hit_depth();
        }

        void rebuild_clip_rects() override
        {
            _button.rebuild_clip_rects(clip_id());
            if (_menu) _menu->rebuild_clip_rects();
        }

        void reset_clip_rect_records() override
        {
            Widget::reset_clip_rect_records();
            if (_menu) _menu->reset_clip_rect_records();
        }

        void translate(const amal::vec2 &delta) override
        {
            if (delta.x == 0.0f && delta.y == 0.0f) return;
            Widget::translate(delta);
            _button.translate(delta);
            if (_open) refresh_menu_anchor();
        }

        void reset_draw_records() override
        {
            _button.reset_draw_records();
            if (_menu) _menu->reset_draw_records();
        }

        void draw(DrawCtx &ctx) override { draw_button(ctx); }

        void draw_button(DrawCtx &ctx) { _button.draw(ctx, can_emit_hit(ctx)); }

        void draw_popups(DrawCtx &ctx)
        {
            if (!_menu) return;
            _menu->draw_popups(ctx);
        }

        void on_click(MouseKey key, KeyPressState state, u32 click_count) override
        {
            (void)click_count;
            if (key != MouseKey::left || state != KeyPressState::press) return;

            const auto hover_id = detail::get_context().hover_id;
            if (hover_id.widget_id != id()) return;
            if (hover_id.tag_id == AUIK_TAG_DOCKSPACE_MENU_BUTTON)
            {
                auto *dockspace = _dockspace;
                const DockNodeID node_id = _node_id;
                add_render_command<detail::ClickEventTraits>(
                    dockspace, [dockspace, node_id]() { dockspace->toggle_menu(node_id); });
                mark_host_refresh_request();
                return;
            }
        }

        void on_focus(bool focused) override
        {
            if (focused) return;
            const auto hover_id = detail::get_context().hover_id;
            if (hover_id.widget_id == id() && hover_id.tag_id == AUIK_TAG_DOCKSPACE_MENU_BUTTON) return;
            if (_menu && detail::get_context().focus_id == _menu->id()) return;
            add_render_command<detail::FocusEventTraits>(this, [this]() {
                if (_dockspace) _dockspace->close_menu();
                mark_host_refresh_request();
            });
        }

        void on_attach() override
        {
            Widget::on_attach();
            if (_menu && (_menu->widget_flags & WidgetFlagBits::attachable)) _menu->on_attach();
        }

        void on_detach() override
        {
            if (_menu && (_menu->widget_flags & WidgetFlagBits::attachable)) _menu->on_detach();
            Widget::on_detach();
        }

        void close_popup()
        {
            if (_dockspace && _dockspace->_open_menu_node == _node_id)
                _dockspace->_open_menu_node = AUIK_DOCK_NODE_INVALID;
            set_open(false);
        }

        void discard_popup()
        {
            if (_dockspace && _dockspace->_open_menu_node == _node_id)
                _dockspace->_open_menu_node = AUIK_DOCK_NODE_INVALID;
            _open = false;
            _button.set_open(false);
            if (_menu) _menu->discard_popup();
            release_menu(true);
        }

        void detach_menu_for_action()
        {
            if (_dockspace && _dockspace->_open_menu_node == _node_id)
                _dockspace->_open_menu_node = AUIK_DOCK_NODE_INVALID;
            _open = false;
            _button.set_open(false);
            if (!_menu) return;
            _menu->set_open(false);
            _menu->pop_suffix_group();
            _menu->clear_popup_anchor_override();
            detach_menu_from_popup();
            if (_menu_owner)
            {
                auto *owner = _menu_owner;
                auto *menu = _menu;
                _menu = nullptr;
                _menu_owner = nullptr;
                owner->set_menu_widget(menu);
            }
            else
            {
                acul::release(_menu);
                _menu = nullptr;
            }
            _model_ready = false;
            _model_has_items = false;
        }

    private:
        bool menu_attached() const
        {
            if (!_menu) return false;
            const auto &map = detail::get_context().id_map;
            const auto it = map.find(_menu->id());
            return it != map.end() && it->second == _menu;
        }

        void attach_menu_for_popup()
        {
            if (!_menu || !(_menu->widget_flags & WidgetFlagBits::attachable) || menu_attached()) return;
            _menu->on_attach();
        }

        void detach_menu_from_popup()
        {
            if (!_menu || !(_menu->widget_flags & WidgetFlagBits::attachable) || !menu_attached()) return;
            _menu->on_detach();
        }

        Window *active_window() const
        {
            if (!_dockspace) return nullptr;
            auto *node = _dockspace->get_node(_node_id);
            return node ? _dockspace->active_window(*node) : nullptr;
        }

        PopupMenu *active_window_menu() const
        {
            auto *window = active_window();
            return window_popup_menu(window);
        }

        void release_menu(bool discard)
        {
            if (!_menu) return;
            if (discard) _menu->discard_popup();
            else _menu->set_open(false);
            _menu->pop_suffix_group();
            _menu->clear_popup_anchor_override();
            detach_menu_from_popup();
            if (_menu_owner)
            {
                auto *owner = _menu_owner;
                auto *menu = _menu;
                _menu = nullptr;
                _menu_owner = nullptr;
                owner->set_menu_widget(menu);
                _model_ready = false;
                _model_has_items = false;
                return;
            }
            acul::release(_menu);
            _menu = nullptr;
            _model_ready = false;
            _model_has_items = false;
        }

        StyleState resolve_button_state() const
        {
            const auto &ctx = detail::get_context();
            const auto button_id = make_element_id(id(), AUIK_TAG_DOCKSPACE_MENU_BUTTON, 0u);
            if (ctx.io.drag_id == button_id) return StyleState::active;
            if (ctx.hover_id == button_id) return StyleState::hover;
            return StyleState::normal;
        }

        void ensure_menu()
        {
            auto *window = active_window();
            const bool active_has_window_menu = window_popup_menu(window) != nullptr;
            if (_menu)
            {
                const bool owner_changed = _menu_owner && _menu_owner != window;
                const bool should_use_window_menu = !_menu_owner && active_has_window_menu;
                if (!owner_changed && !should_use_window_menu) return;
                release_menu(false);
            }
            _menu_owner = nullptr;
            if (window && window_popup_menu(window))
            {
                auto *menu = window->take_menu_widget();
                _menu = menu && menu->get_rect().id.tag_id == AUIK_TAG_POPUP_MENU ? static_cast<PopupMenu *>(menu)
                                                                                  : nullptr;
                if (menu && !_menu)
                {
                    window->set_menu_widget(menu);
                }
                if (_menu)
                {
                    _menu_owner = window;
                    _menu->set_visible();
                    _menu->sync_widget_flags();
                }
            }
            if (!_menu)
            {
                constexpr WidgetFlags widget_flags = WidgetFlagBits::visible | WidgetFlagBits::attachable |
                                                     WidgetFlagBits::configurable | WidgetFlagBits::hittable;
                _menu = acul::alloc<PopupMenu>(make_dockspace_menu_id(_dockspace->id(), _node_id) + AUIK_TAG_POPUP_MENU,
                                               acul::vector<StringView>{}, widget_flags, false);
            }
            _menu->set_parent(parent());
            _menu->set_focus_parent(this);
            _menu->attach_to_viewport(this->viewport());
            _menu->set_position({0.0f, 0.0f});
            _menu->set_layout_size({0.0f, 0.0f});
            _menu->update_style_invalidated();
            _menu->update_layout_min_size();
            attach_menu_for_popup();
        }

        bool rebuild_menu_model()
        {
            ensure_menu();
            _menu->set_parent(parent());
            _menu->set_focus_parent(this);
            _menu->pop_suffix_group();
            _menu->set_selected_enabled(true);
            _menu->update_style_invalidated();
            _menu->update_layout_min_size();
            attach_menu_for_popup();

            bool has_any = false;
            auto *model = _menu->menu_model();
            const u32 group = _menu->push_suffix_group();
            const u32 suffix_count_before = _menu->suffix_item_count(group);
            const auto &items = _dockspace->menu_group();
            if (!items.empty())
            {
                for (u32 i = 0u; i < items.size(); ++i)
                {
                    model->append_item(items[i],
                                       [dockspace = _dockspace, node_id = _node_id, action = i](ClickEvent &) {
                                           if (!dockspace) return;
                                           dockspace->queue_menu_action(node_id, action);
                                       });
                }
            }

            const u32 suffix_count = _menu->suffix_item_count(group);
            if (suffix_count == suffix_count_before) _menu->erase_suffix_group(group);
            else has_any = true;

            if (_menu->suffix_group_count() > 0u || !_menu->element_ids().empty()) has_any = true;
            return has_any;
        }

        amal::rect menu_anchor() const
        {
            amal::rect anchor = _button.bounds();
            if (const auto *node = _dockspace ? _dockspace->get_node(_node_id) : nullptr;
                node && node->tab_panel_rect.bounds.size.y > 0.0f)
            {
                anchor.offset.y = node->tab_panel_rect.bounds.offset.y;
                anchor.size.y = node->tab_panel_rect.bounds.size.y;
            }
            return anchor;
        }

        void refresh_menu_anchor()
        {
            if (!_menu || !_dockspace) return;
            _menu->update_depth(get_dock_menu_popup_depth_range(_dockspace->depth_range()));
            _menu->set_popup_anchor_override(menu_anchor());
        }

        void open_menu()
        {
            if (!_open) return;
            if (!_model_ready || !_model_has_items) return;
            refresh_menu_anchor();
            _menu->set_open(true);
        }

        Dockspace *_dockspace = nullptr;
        DockNodeID _node_id = AUIK_DOCK_NODE_INVALID;
        detail::PopupTrigger _button;
        PopupMenu *_menu = nullptr;
        Window *_menu_owner = nullptr;
        bool _model_ready = false;
        bool _model_has_items = false;
        bool _open = false;
    };

    Dockspace::Dockspace(u32 id, WidgetFlags widget_flags)
        : Widget(id, widget_flags,
                 EventFlagBits::click | EventFlagBits::hover | EventFlagBits::drag | EventFlagBits::drop,
                 {{0.0f, 0.0f}, {0.0f, 0.0f}}, AUIK_TAG_DOCKSPACE)
    {
        set_size(AUIK_SIZE_FILL);
        _nodes.push_back({});
        _nodes[0].settings.min_size = {0.0f, 0.0f};
        update_node_style_cache(0u, _nodes[0]);
    }

    Dockspace::~Dockspace() { clear(); }

    Dockspace::Node *Dockspace::get_node(DockNodeID node)
    {
        if (node >= _nodes.size()) return nullptr;
        return &_nodes[node];
    }

    const Dockspace::Node *Dockspace::get_node(DockNodeID node) const
    {
        if (node >= _nodes.size()) return nullptr;
        return &_nodes[node];
    }

    void Dockspace::update_node_style_cache(DockNodeID node_id, Node &node)
    {
        node.settings = normalize_dock_node_settings(node.settings);
        if (node_id == 0u)
        {
            node.style_size = AUIK_SIZE_FILL;
            node.min_size = node.settings.min_size;
            return;
        }
        const StyleID style_id = get_theme()->get_resolved_style(node.settings.style_tag, 0u, 0u, StyleState::normal);
        const Style &style = get_theme()->get_style(style_id);
        node.style_size = style.size();
        node.min_size = {amal::max(node.settings.min_size.x, style.min_width()),
                         amal::max(node.settings.min_size.y, style.min_height())};
    }

    DockNodeID Dockspace::create_node(DockNodeID parent, bool split, DockNodeSettings settings)
    {
        settings = normalize_dock_node_settings(settings);
        assert(parent < _nodes.size() && "parent dock node is invalid");
        DockNodeID id = static_cast<DockNodeID>(_nodes.size());
        Node node{};
        node.parent = parent;
        node.settings = settings;
        update_node_style_cache(id, node);
        _nodes.push_back(std::move(node));
        auto &parent_node = _nodes[parent];
        parent_node.children.push_back(id);
        for (DockNodeID child_id : parent_node.children)
        {
            auto &child = _nodes[child_id];
            if (is_size_fill(axis_size(child.style_size, parent_node.axis)))
                set_axis_size(child.settings.size, parent_node.axis, 0.0f);
        }
        if (split) _nodes[id].axis = parent_node.axis;
        return id;
    }

    DockNodeID Dockspace::create_split(DockNodeID parent, amal::axis axis, DockNodeSettings settings)
    {
        DockNodeID id = create_node(parent, true, settings);
        _nodes[id].axis = axis;
        return id;
    }

    DockNodeID Dockspace::create_leaf(DockNodeID parent, DockNodeSettings settings)
    {
        return create_node(parent, false, settings);
    }

    void Dockspace::set_split_axis(DockNodeID node, amal::axis axis)
    {
        if (auto *n = get_node(node)) n->axis = axis;
    }

    void Dockspace::set_node_settings(DockNodeID node, DockNodeSettings settings)
    {
        settings = normalize_dock_node_settings(settings);
        if (auto *n = get_node(node))
        {
            const bool recreate_tabbar = n->tabbar && (n->settings.tabbar_flags != settings.tabbar_flags ||
                                                       n->settings.tabpanel != settings.tabpanel);
            n->settings = settings;
            update_node_style_cache(node, *n);
            if (recreate_tabbar) clear_node_chrome(*n);
        }
    }

    void Dockspace::set_node_tabbar_flags(DockNodeID node, TabbarFlags flags)
    {
        auto *n = get_node(node);
        if (!n || n->settings.tabbar_flags == flags) return;

        const bool recreate_tabbar = n->tabbar != nullptr;
        n->settings.tabbar_flags = flags;
        if (!recreate_tabbar) return;

        clear_node_chrome(*n);
        auto &ctx = detail::get_context();
        ctx.dirty_flags |= DirtyFlagBits::redraw | DirtyFlagBits::hit_rect_update;
        update_layout(false);
        invalidate_draw_commands(DrawReasonBits::layout);
        update_draw_commands(DrawReasonBits::layout | DrawReasonBits::record);
        if (ctx.dirty_flags & DirtyFlagBits::clip_rect) sync_clip_rect_cache();
        mark_host_refresh_request();
    }

    void Dockspace::set_new_node_settings(DockNodeSettings settings)
    {
        _new_node_settings = normalize_dock_node_settings(settings);
    }

    void Dockspace::set_policy_flags(DockspaceFlags flags)
    {
        if (_policy_flags == flags) return;
        _policy_flags = flags;
        _drag_zones_dirty = true;
        auto &ctx = detail::get_context();
        ctx.dirty_flags |= DirtyFlagBits::redraw | DirtyFlagBits::hit_rect_update;
        update_layout_sync(true);
        mark_host_refresh_request();
    }

    void Dockspace::set_docking_enabled(bool enabled)
    {
        DockspaceFlags flags = _policy_flags;
        if (enabled) flags |= DockspaceFlagBits::docking;
        else flags &= ~DockspaceFlagBits::docking;
        set_policy_flags(flags);
    }

    void Dockspace::set_tabpanel_enabled(bool enabled)
    {
        DockspaceFlags flags = _policy_flags;
        if (enabled) flags |= DockspaceFlagBits::tabpanel;
        else flags &= ~DockspaceFlagBits::tabpanel;
        set_policy_flags(flags);
    }

    void Dockspace::set_menu_group(MenuGroup group)
    {
        _menu_group = std::move(group);
        close_menu();
        for (auto &node : _nodes)
        {
            if (node.menu) node.menu->close_popup();
            if (node.menu)
            {
                if (node.menu->sync_menu_model(true)) continue;
                if (node.menu->widget_flags & WidgetFlagBits::attachable) node.menu->on_detach();
                if (can_invalidate_dock_chrome()) node.menu->invalidate_draw_commands(DrawReasonBits::layout);
                acul::release(node.menu);
                node.menu = nullptr;
                detail::get_context().dirty_flags |= DirtyFlagBits::redraw;
            }
        }
        update_layout_sync(true);
    }

    bool Dockspace::needs_node_tab_panel(const Node &node) const
    {
        return tabpanel_enabled() && node.settings.tabpanel && !node.windows.empty();
    }

    void Dockspace::attach_window(Window *window)
    {
        if (!window) return;
        window->set_parent(this);
        window->set_focus_parent(this);
        window->window_flags |= WindowFlagBits::docked;
        window->window_flags &= ~(WindowFlagBits::movable | WindowFlagBits::resizable);
        window->reset_clip_rect_records();
        if (auto *menu = window->take_menu_widget()) window->set_menu_widget(menu);
        window->update_style_invalidated();
        auto &map = detail::get_context().id_map;
        const bool dockspace_attached = map.find(id()) != map.end();
        if (dockspace_attached && (window->widget_flags & WidgetFlagBits::attachable))
            static_cast<Widget *>(window)->on_attach();
    }

    void Dockspace::detach_window(Window *window, const amal::rect *undocked_bounds)
    {
        if (!window) return;
        auto &map = detail::get_context().id_map;
        const bool window_attached = map.find(window->id()) != map.end();
        if (window_attached && (window->widget_flags & WidgetFlagBits::attachable))
            static_cast<Widget *>(window)->on_detach();
        if (undocked_bounds) window->get_rect().bounds = *undocked_bounds;
        window->window_flags &= ~WindowFlagBits::docked;
        window->set_parent(nullptr);
        window->set_focus_parent(nullptr);
    }

    Window *Dockspace::active_window(Node &node)
    {
        if (node.windows.empty()) return nullptr;
        const size_t index = selected_window_index(node);
        return index < node.windows.size() ? node.windows[index] : nullptr;
    }

    Window *Dockspace::extract_window(Node &node, Window *window, amal::rect *out_undocked_bounds)
    {
        if (!window) return nullptr;
        for (size_t i = 0; i < node.windows.size(); ++i)
        {
            if (node.windows[i] != window) continue;
            if (out_undocked_bounds)
                *out_undocked_bounds = i < node.undocked_bounds.size() ? node.undocked_bounds[i] : window->bounds();
            node.windows.erase(node.windows.begin() + i);
            if (i < node.undocked_bounds.size()) node.undocked_bounds.erase(node.undocked_bounds.begin() + i);
            if (i < node.tab_titles.size()) node.tab_titles.erase(node.tab_titles.begin() + i);
            node.active_window_index = static_cast<size_t>(-1);
            node.record_active_window = true;
            clear_node_chrome(node);
            return window;
        }
        return nullptr;
    }

    void Dockspace::remove_empty_node(DockNodeID node_id)
    {
        auto *node = get_node(node_id);
        if (!node || !node->children.empty() || !node->windows.empty()) return;
        close_menu();
        clear_node_chrome(*node);
        if (node->parent != AUIK_DOCK_NODE_INVALID)
        {
            auto *parent_node = get_node(node->parent);
            if (parent_node)
            {
                for (size_t i = 0; i < parent_node->children.size(); ++i)
                {
                    if (parent_node->children[i] != node_id) continue;
                    parent_node->children.erase(parent_node->children.begin() + i);
                    break;
                }
            }
        }
        node->parent = AUIK_DOCK_NODE_INVALID;
        node->settings.flags = DockspaceResizeFlagBits::none;
    }

    void Dockspace::undock_window(DockNodeID node_id, Window *window)
    {
        auto *node = get_node(node_id);
        if (!node || !window)
        {
            return;
        }

        amal::rect undocked_bounds{};
        invalidate_window_draw_records(window);
        Window *extracted = extract_window(*node, window, &undocked_bounds);
        if (!extracted) return;
        detach_window(extracted, &undocked_bounds);
        extracted->window_flags |= WindowFlagBits::decorated | WindowFlagBits::movable | WindowFlagBits::resizable;
        extracted->window_flags &= ~WindowFlagBits::docked;
        extracted->reset_draw_records();
        extracted->set_size(amal::max(extracted->size(), extracted->min_size()));
        extracted->set_auto_size(false, false);
        extracted->set_auto_position(false, false);
        if (node->windows.empty()) remove_empty_node(node_id);
        else fit_node_to_required_width(node_id, true);
        update_layout_sync(true);
        add_widget_to_root(extracted);
        const auto viewport = get_widget_viewport_rect(extracted);
        extracted->set_root_viewport_origin({viewport.x, viewport.y});
        extracted->update_style_invalidated();
        extracted->update_layout(false);
        extracted->rebuild_clip_rects();
        focus_widget(extracted);
        extracted->update_draw_commands(DrawReasonBits::record);
        auto &ctx = detail::get_context();
        ctx.dirty_flags |= DirtyFlagBits::redraw | DirtyFlagBits::hit_rect_update;
        mark_host_refresh_request();
    }

    void Dockspace::close_window(DockNodeID node_id, Window *window)
    {
        auto *node = get_node(node_id);
        if (!node || !window) return;
        if (node->tabbar && node->tabbar->child_size() == node->windows.size())
        {
            for (size_t i = 0; i < node->windows.size(); ++i)
            {
                if (node->windows[i] != window) continue;
                const u32 element_id = node->tabbar->item_element_id(static_cast<u32>(i));
                invalidate_window_draw_records(window);
                detach_window(window, i < node->undocked_bounds.size() ? &node->undocked_bounds[i] : nullptr);
                acul::release(window);
                node->windows.erase(node->windows.begin() + i);
                if (i < node->undocked_bounds.size()) node->undocked_bounds.erase(node->undocked_bounds.begin() + i);
                if (i < node->tab_titles.size()) node->tab_titles.erase(node->tab_titles.begin() + i);
                node->active_window_index = static_cast<size_t>(-1);
                node->record_active_window = true;
                node->tabbar->close_item(element_id);
                if (node->windows.empty()) remove_empty_node(node_id);
                else
                {
                    node->active_window_index = selected_window_index(*node);
                    fit_node_to_required_width(node_id, true);
                }
                update_layout_sync(true);
                mark_host_refresh_request();
                return;
            }
        }

        amal::rect undocked_bounds{};
        invalidate_window_draw_records(window);
        Window *extracted = extract_window(*node, window, &undocked_bounds);
        if (!extracted) return;
        detach_window(extracted, &undocked_bounds);
        acul::release(extracted);
        if (node->windows.empty()) remove_empty_node(node_id);
        else fit_node_to_required_width(node_id, true);
        update_layout_sync(true);
    }

    void Dockspace::close_group(DockNodeID node_id)
    {
        auto *node = get_node(node_id);
        if (!node) return;
        clear_node_windows(*node);
        remove_empty_node(node_id);
        update_layout_sync(true);
    }

    void Dockspace::add_window(DockNodeID node, Window *window)
    {
        auto *n = get_node(node);
        assert(n && "dock node is invalid");
        assert(window && "window is null");
        n->undocked_bounds.push_back(window->bounds());
        attach_window(window);
        n->windows.push_back(window);
        n->active_window_index = n->windows.size() - 1u;
        n->record_active_window = true;
        if (needs_node_tab_panel(*n))
        {
            sync_node_tabbar(node, *n);
            if (has_node_menu(*n)) sync_node_menu(node, *n);
        }
        auto &map = detail::get_context().id_map;
        if (map.find(id()) != map.end()) update_depth(this->depth_range());
    }

    bool Dockspace::dock_drag_window_to_tab_panel(Window *window, DockNodeID node_id)
    {
        auto *node = get_node(node_id);
        if (!node || !window) return false;
        if (!docking_enabled()) return false;
        if (!node->tabbar || window->parent() || (window->window_flags & WindowFlagBits::docked)) return false;

        const u32 insert_index = amal::min<u32>(node->tabbar->insertion_index_at(detail::get_io().mouse_pos),
                                                static_cast<u32>(node->windows.size()));
        window->restore_hit_depth();
        disable_dockspace_drag_zones(window, "tab-panel");

        invalidate_window_draw_records(window);
        window->reset_draw_records();
        if (!remove_root_widget(window)) return false;

        node = get_node(node_id);
        if (!node) return false;
        node->undocked_bounds.insert(node->undocked_bounds.begin() + insert_index, window->bounds());
        attach_window(window);
        node->windows.insert(node->windows.begin() + insert_index, window);
        node->active_window_index = insert_index;
        node->record_active_window = true;

        sync_node_tabbar(node_id, *node);
        if (!node->tabbar || insert_index >= node->tabbar->child_size()) return true;
        const u32 element_id = node->tabbar->item_element_id(insert_index);
        if (element_id == 0u) return true;
        node->tabbar->set_selected_silent(element_id);
        fit_node_to_required_width(node_id, false);
        update_depth(this->depth_range());
        update_layout_sync(true);

        node = get_node(node_id);
        if (!node || !node->tabbar) return true;
        auto &ctx = detail::get_context();
        ctx.io.drag_id = make_element_id(node->tabbar->id(), node->tabbar->item_style_tag(), element_id);
        ctx.io.drag_key_flags = ctx.io.active_mouse_buttons;
        ctx.frame_cache.drag_widget_id = node->tabbar->id();
        node->tabbar->begin_external_drag(element_id);
        node->tabbar->invalidate_draw_commands(DrawReasonBits::layout);
        node->tabbar->reset_draw_records();
        node->tabbar->update_draw_commands(DrawReasonBits::layout | DrawReasonBits::record);
        ctx.dirty_flags |= DirtyFlagBits::redraw | DirtyFlagBits::hit_rect_update;
        mark_host_refresh_request();
        return true;
    }

    void Dockspace::clear_node_windows(Node &node)
    {
        if (node.menu) node.menu->discard_popup();
        for (size_t i = 0; i < node.windows.size(); ++i)
        {
            auto *window = node.windows[i];
            if (!window) continue;
            invalidate_window_draw_records(window);
            const amal::rect *undocked_bounds = i < node.undocked_bounds.size() ? &node.undocked_bounds[i] : nullptr;
            detach_window(window, undocked_bounds);
            acul::release(window);
        }
        node.windows.clear();
        node.undocked_bounds.clear();
        node.active_window_index = static_cast<size_t>(-1);
        node.record_active_window = false;
        clear_node_chrome(node);
    }

    void Dockspace::clear_node_chrome(Node &node)
    {
        if (node.tabbar)
        {
            if (node.tabbar->widget_flags & WidgetFlagBits::attachable) node.tabbar->on_detach();
            if (can_invalidate_dock_chrome()) node.tabbar->invalidate_draw_commands(DrawReasonBits::layout);
            acul::release(node.tabbar);
            node.tabbar = nullptr;
        }
        if (node.menu)
        {
            node.menu->discard_popup();
            if (node.menu->widget_flags & WidgetFlagBits::attachable) node.menu->on_detach();
            if (can_invalidate_dock_chrome()) node.menu->invalidate_draw_commands(DrawReasonBits::layout);
            acul::release(node.menu);
            node.menu = nullptr;
        }
        if ((node.tab_panel_draw.render_id != AUIK_INVALID_DRAW_DATA_ID ||
             node.tab_panel_draw.hit_id != AUIK_INVALID_DRAW_DATA_ID) &&
            can_invalidate_dock_chrome())
        {
            auto *quads_stream = get_primary_quads_stream();
            DrawCtx ctx{DrawReasonBits::layout | DrawReasonBits::invalidate};
            ctx.is_hit_allowed = false;
            if (node.tab_panel_draw.render_id != AUIK_INVALID_DRAW_DATA_ID)
                emit_quads_instance(ctx, quads_stream, node.tab_panel_draw, {}, node.tab_panel_rect, false, false);
            if (node.tab_panel_draw.hit_id != AUIK_INVALID_DRAW_DATA_ID)
            {
                detail::RectData hidden_hit = node.tab_panel_rect;
                hidden_hit.bounds.size = {0.0f, 0.0f};
                update_hit_rect(node.tab_panel_draw.hit_id, hidden_hit, true);
            }
        }
        detail::get_context().dirty_flags |= DirtyFlagBits::redraw | DirtyFlagBits::hit_rect_update;
        node.tab_titles.clear();
        node.tab_panel_rect.bounds = {};
        node.tab_panel_draw = {};
    }

    void Dockspace::clear_windows(DockNodeID node)
    {
        if (auto *n = get_node(node)) clear_node_windows(*n);
    }

    void Dockspace::clear()
    {
        for (auto &node : _nodes)
        {
            clear_node_windows(node);
            clear_node_chrome(node);
        }
        _nodes.clear();
        _nodes.push_back({});
        _nodes[0].settings.min_size = {0.0f, 0.0f};
        update_node_style_cache(0u, _nodes[0]);
    }

    StyleUpdateFlags Dockspace::update_style()
    {
        StyleUpdateFlags out = StyleUpdateFlagBits::none;
        out |= resolve_style_selector(_resize_helper_style, id(), parent() ? parent()->id() : 0u, style_state());
        out |= resolve_style_selector(_resize_helper_drag_style, id(), parent() ? parent()->id() : 0u, style_state());
        out |= resolve_style_selector(_tab_panel_style, id(), parent() ? parent()->id() : 0u, style_state());
        const auto transition = detail::get_widget_style_selector_transition(id());
        if (is_resize_helper_tag(transition.prev_id.tag_id) || is_resize_helper_tag(transition.current_id.tag_id))
        {
            out |= StyleUpdateFlagBits::redraw;
            auto &ctx = detail::get_context();
            if (ctx.hover_id.widget_id == id() && is_resize_helper_tag(ctx.hover_id.tag_id))
            {
                if (const auto *helper = resize_helper_from_element(ctx.hover_id.element_id);
                    helper && helper->interactive)
                {
                    detail::set_window_cursor(helper->axis == amal::axis::x ? detail::CursorID::resize_ew
                                                                            : detail::CursorID::resize_ns,
                                              ctx.window_ctx);
                }
            }
        }
        for (DockNodeID node_id = 0u; node_id < _nodes.size(); ++node_id)
        {
            auto &node = _nodes[node_id];
            update_node_style_cache(node_id, node);
            if (node.tabbar) out |= node.tabbar->update_style_invalidated();
            if (node.menu) out |= node.menu->update_style_invalidated();
            for (auto *window : node.windows)
                if (window) out |= window->update_style_invalidated();
        }
        return out;
    }

    void Dockspace::sync_node_tabbar(DockNodeID node_id, Node &node)
    {
        if (!needs_node_tab_panel(node))
        {
            clear_node_chrome(node);
            return;
        }

        if (!node.tabbar)
        {
            node.tabbar = acul::alloc<Tabbar>(make_dockspace_tabbar_id(id(), node_id), acul::vector<acul::string>{},
                                              node.settings.tabbar_flags, detail::get_tabbar_widget_flags(),
                                              amal::vec2{AUIK_SIZE_X_INHERIT, AUIK_SIZE_Y_INHERIT});
            node.tabbar->set_parent(this);
            node.tabbar->set_style_tag(AUIK_STYLE_TAG_DOCK_TABBAR);
            node.tabbar->set_item_style_tag(AUIK_STYLE_TAG_DOCK_TAB_ITEM);
            node.tabbar->set_selected_item_style_tag(AUIK_STYLE_TAG_DOCK_TAB_ITEM_SELECTED);
            node.tabbar->set_focus_parent(this);
            node.tabbar->bind()
                .on_change([this, node_id](ChangeEvent &) {
                    auto *node = get_node(node_id);
                    if (!node || !node->tabbar) return;
                    handle_tabbar_changed(node_id, node->tabbar->change_reason());
                })
                .on_drag([this, node_id](DragEvent &e) {
                    if (e.state != KeyPressState::repeat) return;
                    auto *node = get_node(node_id);
                    if (!node || !node->tabbar) return;
                    const auto drag_id = detail::get_context().io.drag_id;
                    if (drag_id.widget_id != node->tabbar->id() || drag_id.tag_id != node->tabbar->item_style_tag())
                        return;
                    if (!is_tabbar_drag_escape(node->tabbar, drag_id.element_id)) return;
                    if (handle_tabbar_drag_escape(node_id, drag_id.element_id)) e.prevent_default();
                });
            node.tabbar->update_style_invalidated();
            node.tabbar->update_depth(is_valid_depth_range(_tabbar_depth_range)
                                          ? _tabbar_depth_range
                                          : make_dockspace_tabbar_depth_range(this->depth_range()));
            auto &map = detail::get_context().id_map;
            if (map.find(id()) != map.end() && (node.tabbar->widget_flags & WidgetFlagBits::attachable))
                node.tabbar->on_attach();
        }

        bool same_titles = node.tab_titles.size() == node.windows.size();
        if (same_titles)
        {
            for (size_t i = 0; i < node.windows.size(); ++i)
            {
                const acul::string title = window_tab_title(node.windows[i]);
                if (node.tab_titles[i] != title)
                {
                    same_titles = false;
                    break;
                }
            }
        }
        if (same_titles)
        {
            for (size_t i = 0; i < node.windows.size() && i < node.tabbar->child_size(); ++i)
                node.tabbar->set_item_user_data(static_cast<u32>(i), node.windows[i]);
            if (node.record_active_window && node.active_window_index < node.tabbar->child_size())
                node.tabbar->set_selected_silent(
                    node.tabbar->item_element_id(static_cast<u32>(node.active_window_index)));
            return;
        }

        acul::vector<acul::string> titles;
        acul::vector<StringView> title_views;
        titles.reserve(node.windows.size());
        title_views.reserve(node.windows.size());
        for (auto *window : node.windows)
        {
            titles.push_back(window_tab_title(window));
            title_views.push_back(window_tab_title_view(window));
        }
        node.tab_titles = titles;
        node.tabbar->set_items(title_views);
        for (size_t i = 0; i < node.windows.size() && i < node.tabbar->child_size(); ++i)
            node.tabbar->set_item_user_data(static_cast<u32>(i), node.windows[i]);
        if (node.record_active_window && node.active_window_index < node.tabbar->child_size())
            node.tabbar->set_selected_silent(node.tabbar->item_element_id(static_cast<u32>(node.active_window_index)));
        node.tabbar->update_style_invalidated();
        node.tabbar->update_depth(is_valid_depth_range(_tabbar_depth_range)
                                      ? _tabbar_depth_range
                                      : make_dockspace_tabbar_depth_range(this->depth_range()));
    }

    bool Dockspace::fit_node_to_required_width(DockNodeID node_id, bool allow_shrink)
    {
        if (!get_node(node_id)) return false;
        measure_node(root_node());

        DockNodeID current_id = node_id;
        while (current_id != AUIK_DOCK_NODE_INVALID)
        {
            auto *current = get_node(current_id);
            if (!current || current->parent == AUIK_DOCK_NODE_INVALID) return false;
            auto *parent = get_node(current->parent);
            if (!parent) return false;

            if (parent->axis == amal::axis::x)
            {
                const f32 required_width = amal::ceil(amal::max(current->required_size.x, 0.0f));
                if (required_width <= 0.0f) return false;
                const f32 current_width = axis_size(current->settings.size, amal::axis::x);
                if (!allow_shrink)
                {
                    if (current->bounds.size.x + 0.5f >= required_width) return false;
                    if (current_width + 0.5f >= required_width) return false;
                }
                else if (std::abs(current_width - required_width) <= 0.5f) return false;

                const f32 next_width = allow_shrink ? required_width : amal::max(current_width, required_width);
                set_axis_size(current->settings.size, amal::axis::x, next_width);
                return true;
            }

            current_id = current->parent;
        }
        return false;
    }

    void Dockspace::close_menu()
    {
        if (_open_menu_node != AUIK_DOCK_NODE_INVALID)
        {
            if (auto *node = get_node(_open_menu_node))
                if (node->menu) node->menu->close_popup();
        }
        _open_menu_node = AUIK_DOCK_NODE_INVALID;
    }

    void Dockspace::update_drag_zones()
    {
        if (auto *parent_widget = parent(); parent_widget && parent_widget->signature() == AUIK_TAG_WIDGET_STACK &&
                                            !static_cast<WidgetStack *>(parent_widget)->is_active_child(this))
            return;

        const auto *dock_ctx = detail::g_context ? detail::g_context->dockspace_context : nullptr;
        const bool enabled = dock_ctx && dock_ctx->drag_zones_enabled && dock_ctx->drag_window;
        const bool dirty = _drag_zones_dirty || (dock_ctx && dock_ctx->drag_zones_dirty);
        sync_drag_zone_helpers(enabled, dirty);
        update_draw_commands(DrawReasonBits::layout);
    }

    void Dockspace::sync_drag_zone_helpers(bool enabled, bool dirty)
    {
        auto *overlay_stream = get_overlay_quads_stream();
        if (dirty || !enabled)
        {
            while (_resize_helper_count > 0u && _resize_helpers[_resize_helper_count - 1u].drop_zone)
            {
                --_resize_helper_count;
                auto &helper = _resize_helpers[_resize_helper_count];
                if (helper.draw_in_overlay && helper.draw.render_id != AUIK_INVALID_DRAW_DATA_ID && overlay_stream &&
                    overlay_stream->invalidate_data_in_stream)
                {
                    overlay_stream->invalidate_data_in_stream(overlay_stream, helper.draw);
                    helper.draw.render_id = AUIK_INVALID_DRAW_DATA_ID;
                }
                helper.visible = false;
                helper.interactive = false;
                helper.drop_zone = false;
                helper.draw_in_overlay = false;
                helper.rect.bounds.size = {0.0f, 0.0f};
                helper.hit_rect.bounds.size = {0.0f, 0.0f};
                update_existing_resize_helper_hit(helper.hit_draw, helper.hit_rect);
            }
        }
        if (enabled)
        {
            if (dirty)
            {
                add_root_drop_helpers();
                add_vertical_drop_helpers(root_node());
            }
            _drag_zones_dirty = false;
        }
        else _drag_zones_dirty = true;

        for (size_t i = 0; i < _resize_helper_count; ++i)
        {
            auto &helper = _resize_helpers[i];
            if (!helper.drop_zone) continue;
            helper.interactive = enabled;
        }
    }

    void Dockspace::toggle_menu(DockNodeID node_id)
    {
        auto *node = get_node(node_id);
        if (!node || !node->menu || !has_node_menu(*node)) return;

        if (_open_menu_node == node_id)
        {
            close_menu();
        }
        else
        {
            close_menu();
            _open_menu_node = node_id;
            node->menu->set_open(true);
        }

        auto &ctx = detail::get_context();
        ctx.dirty_flags |= DirtyFlagBits::redraw | DirtyFlagBits::hit_rect_update;
        update_layout(false);
        update_draw_commands(DrawReasonBits::layout);
        if (ctx.dirty_flags & DirtyFlagBits::clip_rect) sync_clip_rect_cache();
        mark_host_refresh_request();
    }

    void Dockspace::queue_menu_action(DockNodeID node_id, u32 action)
    {
        const u32 dockspace_id = id();
        _open_menu_node = AUIK_DOCK_NODE_INVALID;
        add_render_command([dockspace_id, node_id, action]() {
            auto &ctx = detail::get_context();
            const auto it = ctx.id_map.find(dockspace_id);
            if (it == ctx.id_map.end() || !it->second) return;
            if (it->second->get_rect().id.tag_id != AUIK_TAG_DOCKSPACE) return;
            auto *dockspace = static_cast<Dockspace *>(it->second);
            dockspace->execute_menu_action(node_id, action);
        });
    }

    void Dockspace::request_undock_window(DockNodeID node) { queue_menu_action(node, DockMenuActionUndock); }

    void Dockspace::request_close_window(DockNodeID node) { queue_menu_action(node, DockMenuActionCloseWindow); }

    void Dockspace::request_close_group(DockNodeID node) { queue_menu_action(node, DockMenuActionCloseGroup); }

    void Dockspace::execute_menu_action(DockNodeID node_id, u32 action)
    {
        auto *node = get_node(node_id);
        Window *window = node ? active_window(*node) : nullptr;
        if (!node) return;
        if (node->menu) node->menu->detach_menu_for_action();
        _open_menu_node = AUIK_DOCK_NODE_INVALID;
        if (action == DockMenuActionUndock) undock_window(node_id, window);
        else if (action == DockMenuActionCloseWindow) close_window(node_id, window);
        else if (action == DockMenuActionCloseGroup) close_group(node_id);
    }

    bool Dockspace::has_node_menu(const Node &node) const
    {
        if (!tabpanel_enabled()) return false;
        if (node.windows.empty()) return false;
        if (!_menu_group.empty()) return true;
        if (node.menu && node.menu->owns_active_window_menu()) return true;
        const size_t active_index = selected_window_index(node);
        auto *window = active_index < node.windows.size() ? node.windows[active_index] : nullptr;
        auto *menu = window_popup_menu(window);
        return menu && !menu->element_ids().empty();
    }

    void Dockspace::sync_node_menu(DockNodeID node_id, Node &node)
    {
        if (!has_node_menu(node))
        {
            if (node.menu) node.menu->discard_popup();
            return;
        }
        if (!node.menu)
        {
            node.menu = acul::alloc<DockMenu>(this, node_id);
            auto &map = detail::get_context().id_map;
            if (map.find(id()) != map.end() && (node.menu->widget_flags & WidgetFlagBits::attachable))
                node.menu->on_attach();
        }
        node.menu->sync_menu_model();
        node.menu->update_depth(this->depth_range(), is_valid_depth_range(_tabbar_depth_range)
                                                         ? _tabbar_depth_range
                                                         : make_dockspace_tabbar_depth_range(this->depth_range()));
        node.menu->set_clip_id(clip_id());
        node.menu->set_open(_open_menu_node == node_id);
        node.menu->update_layout_min_size();
    }

    size_t Dockspace::selected_window_index(const Node &node) const
    {
        if (!node.tabbar || node.windows.empty()) return 0u;
        return amal::min(static_cast<size_t>(node.tabbar->selected_index()), node.windows.size() - 1u);
    }

    amal::vec2 Dockspace::measure_node(DockNodeID node_id)
    {
        auto *node = get_node(node_id);
        if (!node) return {0.0f, 0.0f};

        amal::vec2 required = node->min_size;
        if (!node->children.empty())
        {
            f32 main = 0.0f;
            f32 cross = 0.0f;
            for (DockNodeID child_id : node->children)
            {
                const amal::vec2 child_required = measure_node(child_id);
                main += axis_size(child_required, node->axis);
                cross = amal::max(cross, cross_size(child_required, node->axis));
            }
            if (_resize_helper_style.id == Theme::STYLE_ID_INVALID) update_style();
            const amal::vec4 helper_padding = get_theme()->get_style(_resize_helper_style.id).padding();
            const f32 helper_main_size = node->axis == amal::axis::x
                                             ? amal::max(helper_padding.x + helper_padding.z, 1.0f)
                                             : amal::max(helper_padding.y + helper_padding.w, 1.0f);
            for (size_t i = 0; i < node->children.size(); ++i)
            {
                if (node->axis == amal::axis::y)
                {
                    const auto *current = get_node(node->children[i]);
                    const auto *prev = i > 0u ? get_node(node->children[i - 1u]) : nullptr;
                    const bool helper = i > 0u && current && prev &&
                                        has_visible_resize_helper_flag(current->settings.flags, amal::axis::y) &&
                                        has_visible_resize_helper_flag(prev->settings.flags, amal::axis::y);
                    if (helper) main += helper_main_size;
                }
                else if (i + 1u < node->children.size())
                {
                    const auto *current = get_node(node->children[i]);
                    const auto *next = get_node(node->children[i + 1u]);
                    const bool helper = current && next &&
                                        has_visible_resize_helper_flag(current->settings.flags, amal::axis::x) &&
                                        has_visible_resize_helper_flag(next->settings.flags, amal::axis::x);
                    if (helper) main += helper_main_size;
                }
            }
            required = amal::max(required, make_axis_size(node->axis, main, cross));
        }
        else
        {
            if (needs_node_tab_panel(*node)) sync_node_tabbar(node_id, *node);
            const size_t selected_index = selected_window_index(*node);
            for (size_t i = 0; i < node->windows.size(); ++i)
            {
                auto *window = node->windows[i];
                if (!window) continue;
                const bool active = i == selected_index;
                if (!active) continue;
                window->update_layout_min_size();
                required = amal::max(required, window->required_size());
            }
            if (needs_node_tab_panel(*node) && node->tabbar)
            {
                if (_tab_panel_style.id == Theme::STYLE_ID_INVALID) update_style();
                node->tabbar->update_layout_min_size();
                f32 menu_w = 0.0f;
                f32 menu_h = 0.0f;
                if (has_node_menu(*node))
                {
                    sync_node_menu(node_id, *node);
                    if (node->menu)
                    {
                        menu_w = node->menu->required_size().x;
                        menu_h = node->menu->required_size().y;
                    }
                }
                const amal::vec4 padding = get_theme()->get_style(_tab_panel_style.id).padding();
                const amal::vec2 chrome_required = node->tabbar->required_size();
                required.x = amal::max(required.x, chrome_required.x + menu_w + padding.x + padding.z);
                required.y += amal::max(chrome_required.y, menu_h) + padding.y + padding.w;
            }
        }

        if (node_id != 0u)
        {
            if (node->parent != AUIK_DOCK_NODE_INVALID)
            {
                const auto *parent_node = get_node(node->parent);
                if (parent_node)
                {
                    const amal::axis axis = parent_node->axis;
                    const f32 style_size = axis_size(node->style_size, axis);
                    if (is_size_concrete(style_size))
                        set_axis_size(required, axis, amal::max(axis_size(required, axis), style_size));
                }
            }
        }
        node->required_size = required;
        return required;
    }

    void Dockspace::update_layout_min_size_force()
    {
        const amal::vec2 root_required = measure_node(root_node());
        set_required_size(root_required);
    }

    void Dockspace::layout_leaf_window(Node &node, Window *window)
    {
        if (!window) return;
        window->set_position(node.content_bounds.offset);
        window->set_layout_size(node.content_bounds.size);
        window->update_layout(false);
        window->rebuild_clip_rects();
    }

    void Dockspace::layout_leaf_window_fast(Node &node, Window *window)
    {
        if (!window) return;
        window->set_position(node.content_bounds.offset);
        window->set_layout_size(node.content_bounds.size);
        window->update_layout(true);
        window->rebuild_clip_rects();
    }

    bool Dockspace::prepare_active_window(Node &node, bool force_record, bool record_pass)
    {
        if (node.windows.empty())
        {
            node.active_window_index = static_cast<size_t>(-1);
            node.record_active_window = false;
            return false;
        }
        const size_t selected_index = selected_window_index(node);
        bool changed = false;

        if (node.active_window_index != selected_index)
        {
            if (!record_pass && node.active_window_index < node.windows.size())
            {
                if (auto *prev_window = node.windows[node.active_window_index])
                    prev_window->invalidate_draw_commands(DrawReasonBits::layout);
            }
            node.active_window_index = selected_index;
            if (!record_pass)
            {
                force_record = true;
                node.record_active_window = true;
            }
            changed = true;
        }

        for (size_t i = 0; i < node.windows.size(); ++i)
        {
            auto *window = node.windows[i];
            if (!window) continue;
            const bool active = i == selected_index;
            if (!active) continue;

            layout_leaf_window(node, window);
            if (force_record)
            {
                window->invalidate_draw_commands(DrawReasonBits::layout);
                window->reset_draw_records();
                node.record_active_window = true;
                changed = true;
            }
        }
        return changed;
    }

    bool Dockspace::sync_active_windows(DockNodeID node_id, bool record_pass)
    {
        auto *node = get_node(node_id);
        if (!node) return false;
        bool changed = false;
        for (DockNodeID child_id : node->children) changed |= sync_active_windows(child_id, record_pass);
        if (!node->children.empty()) return changed;
        return changed | prepare_active_window(*node, false, record_pass);
    }

    size_t Dockspace::begin_resize_helpers()
    {
        const size_t previous_count = _resize_helper_count;
        _resize_helper_count = 0u;
        auto *overlay_stream = get_overlay_quads_stream();
        for (auto &helper : _resize_helpers)
        {
            if (helper.draw_in_overlay && helper.draw.render_id != AUIK_INVALID_DRAW_DATA_ID && overlay_stream &&
                overlay_stream->invalidate_data_in_stream)
            {
                overlay_stream->invalidate_data_in_stream(overlay_stream, helper.draw);
                helper.draw.render_id = AUIK_INVALID_DRAW_DATA_ID;
            }
            helper.visible = false;
            helper.interactive = false;
            helper.drop_zone = false;
            helper.draw_in_overlay = false;
            helper.rect.bounds = {};
        }
        return previous_count;
    }

    amal::rect Dockspace::add_resize_helper(DockNodeID parent, size_t before_child, size_t after_child, amal::axis axis,
                                            const amal::rect &bounds, bool visible, bool interactive, bool drop_zone,
                                            const amal::rect *hit_bounds)
    {
        const amal::rect snapped_bounds = snap_layout_rect(bounds);
        if (!visible && !interactive && !drop_zone) return snapped_bounds;
        if (_resize_helper_count >= _resize_helpers.size()) _resize_helpers.push_back({});
        auto &helper = _resize_helpers[_resize_helper_count];
        helper.parent = parent;
        helper.before_child = static_cast<u32>(before_child);
        helper.after_child = static_cast<u32>(after_child);
        helper.axis = axis;
        helper.visible = visible;
        helper.interactive = interactive;
        helper.drop_zone = drop_zone;
        const bool next_draw_in_overlay = drop_zone;
        if (helper.draw_in_overlay != next_draw_in_overlay && helper.draw.render_id != AUIK_INVALID_DRAW_DATA_ID)
        {
            auto *previous_stream = helper.draw_in_overlay ? get_overlay_quads_stream() : get_primary_quads_stream();
            if (previous_stream && previous_stream->invalidate_data_in_stream)
                previous_stream->invalidate_data_in_stream(previous_stream, helper.draw);
            helper.draw.render_id = AUIK_INVALID_DRAW_DATA_ID;
        }
        helper.draw_in_overlay = drop_zone;
        helper.rect = detail::make_rect_data(id(), resize_helper_tag_for_axis(axis), snapped_bounds, clip_id(),
                                             _resize_helper_depth, 0, static_cast<u32>(_resize_helper_count));
        if (drop_zone)
        {
            helper.rect.depth = get_dock_drop_zone_visual_depth();
            helper.rect.hit_depth = get_dock_helper_hit_depth();
        }
        if (_resize_helper_style.id == Theme::STYLE_ID_INVALID) update_style();
        const amal::vec4 hit_margin = get_theme()->get_style(_resize_helper_style.id).margin();
        helper.hit_rect = helper.rect;
        helper.hit_rect.bounds = hit_bounds ? snap_layout_rect(*hit_bounds)
                                            : make_resize_helper_hit_bounds(axis, snapped_bounds, hit_margin);
        if (drop_zone)
        {
            helper.hit_rect.depth = helper.rect.depth;
            helper.hit_rect.hit_depth = helper.rect.hit_depth;
        }
        if (helper.interactive) update_existing_resize_helper_hit(helper.hit_draw, helper.hit_rect);
        else
        {
            detail::RectData hidden_hit = helper.hit_rect;
            hidden_hit.bounds.size = {0.0f, 0.0f};
            update_existing_resize_helper_hit(helper.hit_draw, hidden_hit);
        }
        ++_resize_helper_count;
        return snapped_bounds;
    }

    bool Dockspace::resize_helper_accepts_drop(const ResizeHelperVisual &helper) const
    {
        if (!docking_enabled()) return false;
        return helper.drop_zone || helper.axis == amal::axis::x;
    }

    void Dockspace::add_root_drop_helpers()
    {
        const auto *dock_ctx = detail::g_context ? detail::g_context->dockspace_context : nullptr;
        const bool enabled = dock_ctx && dock_ctx->drag_zones_enabled && dock_ctx->drag_window;
        if (!enabled || !docking_enabled()) return;
        auto *root = get_node(root_node());
        if (!root || root->axis != amal::axis::x || root->children.empty()) return;
        if (_resize_helper_style.id == Theme::STYLE_ID_INVALID) update_style();
        const amal::vec4 helper_padding = get_theme()->get_style(_resize_helper_style.id).padding();
        const f32 helper_w = amal::max(helper_padding.x + helper_padding.z, 1.0f);
        const amal::rect root_bounds = root->bounds;
        if (root_bounds.size.x <= 0.0f || root_bounds.size.y <= 0.0f) return;

        const amal::rect left_helper_bounds{{root_bounds.offset.x, root_bounds.offset.y},
                                            {helper_w, root_bounds.size.y}};
        add_resize_helper(root_node(), 0u, 0u, amal::axis::x, left_helper_bounds, false, enabled, true);

        const amal::rect right_helper_bounds{
            {root_bounds.offset.x + root_bounds.size.x - helper_w, root_bounds.offset.y},
            {helper_w, root_bounds.size.y}};
        add_resize_helper(root_node(), root->children.size() - 1u, root->children.size(), amal::axis::x,
                          right_helper_bounds, false, enabled, true);
    }

    void Dockspace::add_vertical_drop_helpers(DockNodeID node_id)
    {
        auto *node = get_node(node_id);
        if (!node) return;
        for (DockNodeID child_id : node->children) add_vertical_drop_helpers(child_id);
        const auto *dock_ctx = detail::g_context ? detail::g_context->dockspace_context : nullptr;
        const bool enabled = dock_ctx && dock_ctx->drag_zones_enabled && dock_ctx->drag_window;
        if (!enabled || !docking_enabled() || node->axis != amal::axis::y || node->children.empty()) return;

        bool top_zone_added = false;
        for (size_t i = 0; i < node->children.size(); ++i)
        {
            auto *child = get_node(node->children[i]);
            if (!child) continue;
            const amal::rect child_bounds = child->content_bounds;
            if (child_bounds.size.x <= 0.0f || child_bounds.size.y <= 0.0f) continue;

            const f32 band_h =
                amal::max(amal::floor(child_bounds.size.y * AUIK_DOCKSPACE_VERTICAL_DROP_BAND_FRACTION), 1.0f);
            const f32 hit_h = amal::min(AUIK_DOCKSPACE_VERTICAL_DROP_HIT_SIZE, child_bounds.size.y);
            if (!top_zone_added)
            {
                const amal::rect band{{child_bounds.offset.x, child_bounds.offset.y}, {child_bounds.size.x, band_h}};
                const amal::rect hit{{child_bounds.offset.x, child_bounds.offset.y}, {child_bounds.size.x, hit_h}};
                add_resize_helper(node_id, i, i, amal::axis::y, band, false, true, true, &hit);
                top_zone_added = true;
            }

            const amal::rect band{{child_bounds.offset.x, child_bounds.offset.y + child_bounds.size.y - band_h},
                                  {child_bounds.size.x, band_h}};
            const amal::rect hit{{child_bounds.offset.x, child_bounds.offset.y + child_bounds.size.y - hit_h},
                                 {child_bounds.size.x, hit_h}};
            add_resize_helper(node_id, i, i + 1u, amal::axis::y, band, false, true, true, &hit);
        }
    }

    void Dockspace::layout_node(DockNodeID node_id, const amal::rect &bounds)
    {
        auto *node = get_node(node_id);
        if (!node) return;
        const bool fast_update = detail::get_context().dirty_flags & DirtyFlagBits::fast_update;

        node->bounds = snap_layout_rect(bounds);
        node->content_bounds = node->bounds;
        if (node->children.empty())
        {
            const bool needs_tab_panel = needs_node_tab_panel(*node);
            const bool can_fast_layout_chrome = fast_update && needs_tab_panel && node->tabbar;
            if (can_fast_layout_chrome)
            {
                if (_tab_panel_style.id == Theme::STYLE_ID_INVALID) update_style();
                const amal::vec4 panel_padding = get_theme()->get_style(_tab_panel_style.id).padding();
                const amal::vec2 chrome_required = node->tabbar->required_size();
                f32 menu_h = 0.0f;
                f32 menu_reserved = 0.0f;
                if (has_node_menu(*node) && node->menu)
                {
                    const amal::vec2 menu_size = node->menu->required_size();
                    menu_reserved = menu_size.x;
                    menu_h = menu_size.y;
                }
                const f32 panel_h = amal::min(node->bounds.size.y,
                                              amal::max(chrome_required.y, menu_h) + panel_padding.y + panel_padding.w);
                const amal::rect panel_bounds{{node->bounds.offset.x, node->bounds.offset.y},
                                              {node->bounds.size.x, panel_h}};
                node->tab_panel_rect.bounds = panel_bounds;
                node->tab_panel_rect.clip_id = clip_id();

                const f32 panel_inner_w = amal::max(panel_bounds.size.x - panel_padding.x - panel_padding.z, 0.0f);
                const f32 panel_inner_h = amal::max(panel_bounds.size.y - panel_padding.y - panel_padding.w, 0.0f);
                if (has_node_menu(*node) && node->menu)
                {
                    const amal::vec2 menu_size = node->menu->required_size();
                    const f32 menu_x = panel_bounds.offset.x + panel_bounds.size.x - panel_padding.z - menu_size.x;
                    const f32 menu_y =
                        panel_bounds.offset.y + panel_padding.y + amal::max((panel_inner_h - menu_size.y) * 0.5f, 0.0f);
                    node->menu->layout_button({{menu_x, menu_y}, menu_size}, clip_id());
                }

                const amal::vec2 tabbar_pos{panel_bounds.offset.x + panel_padding.x,
                                            panel_bounds.offset.y + panel_padding.y};
                const amal::vec2 tabbar_size{amal::max(panel_inner_w - menu_reserved, 0.0f), panel_inner_h};
                node->tabbar->set_position(tabbar_pos);
                node->tabbar->set_layout_size(tabbar_size);
                node->tabbar->update_layout(true);
                const f32 panel_bottom = panel_bounds.offset.y + panel_bounds.size.y;
                const f32 node_bottom = node->bounds.offset.y + node->bounds.size.y;
                node->content_bounds.offset.y = panel_bottom;
                node->content_bounds.size.y = amal::max(node_bottom - panel_bottom, 0.0f);
            }
            else if (needs_tab_panel)
            {
                sync_node_tabbar(node_id, *node);
                if (node->tabbar)
                {
                    if (_tab_panel_style.id == Theme::STYLE_ID_INVALID) update_style();
                    const amal::vec4 panel_padding = get_theme()->get_style(_tab_panel_style.id).padding();
                    node->tabbar->set_layout_size(
                        {amal::max(node->bounds.size.x - panel_padding.x - panel_padding.z, 0.0f), 0.0f});
                    node->tabbar->update_layout_min_size();
                    f32 menu_h = 0.0f;
                    if (has_node_menu(*node))
                    {
                        if (!node->menu)
                        {
                            node->menu = acul::alloc<DockMenu>(this, node_id);
                            auto &map = detail::get_context().id_map;
                            if (map.find(id()) != map.end() && (node->menu->widget_flags & WidgetFlagBits::attachable))
                                node->menu->on_attach();
                        }
                        node->menu->update_layout_min_size();
                        menu_h = node->menu->required_size().y;
                    }
                    const amal::vec2 chrome_required = node->tabbar->required_size();
                    const f32 panel_h = amal::min(node->bounds.size.y, amal::max(chrome_required.y, menu_h) +
                                                                           panel_padding.y + panel_padding.w);
                    const amal::rect panel_bounds{{node->bounds.offset.x, node->bounds.offset.y},
                                                  {node->bounds.size.x, panel_h}};
                    node->tab_panel_rect = detail::make_rect_data(id(), AUIK_TAG_DOCKSPACE_TAB_PANEL, panel_bounds,
                                                                  clip_id(), _tab_panel_depth, 0, node_id);
                    sync_node_menu(node_id, *node);

                    const f32 panel_inner_w = amal::max(panel_bounds.size.x - panel_padding.x - panel_padding.z, 0.0f);
                    const f32 panel_inner_h = amal::max(panel_bounds.size.y - panel_padding.y - panel_padding.w, 0.0f);
                    f32 menu_reserved = 0.0f;
                    if (has_node_menu(*node) && node->menu)
                    {
                        const amal::vec2 menu_size = node->menu->required_size();
                        menu_reserved = menu_size.x;
                        const f32 menu_x = panel_bounds.offset.x + panel_bounds.size.x - panel_padding.z - menu_size.x;
                        const f32 menu_y = panel_bounds.offset.y + panel_padding.y +
                                           amal::max((panel_inner_h - menu_size.y) * 0.5f, 0.0f);
                        node->menu->layout_button({{menu_x, menu_y}, menu_size}, clip_id());
                    }

                    const amal::vec2 tabbar_pos{panel_bounds.offset.x + panel_padding.x,
                                                panel_bounds.offset.y + panel_padding.y};
                    const amal::vec2 tabbar_size{amal::max(panel_inner_w - menu_reserved, 0.0f), panel_inner_h};
                    node->tabbar->set_position(tabbar_pos);
                    node->tabbar->set_layout_size(tabbar_size);
                    node->tabbar->update_layout(true);
                    const f32 panel_bottom = panel_bounds.offset.y + panel_bounds.size.y;
                    const f32 node_bottom = node->bounds.offset.y + node->bounds.size.y;
                    node->content_bounds.offset.y = panel_bottom;
                    node->content_bounds.size.y = amal::max(node_bottom - panel_bottom, 0.0f);
                }
            }
            else clear_node_chrome(*node);

            const size_t selected_index = selected_window_index(*node);
            for (size_t i = 0; i < node->windows.size(); ++i)
            {
                auto *window = node->windows[i];
                if (!window) continue;
                const bool active = i == selected_index;
                if (active)
                {
                    if (fast_update) layout_leaf_window_fast(*node, window);
                    else layout_leaf_window(*node, window);
                }
            }
            if (!node->windows.empty() && node->active_window_index == static_cast<size_t>(-1))
                node->active_window_index = selected_index;
            return;
        }

        const f32 main_available = amal::max(axis_size(node->bounds.size, node->axis), 0.0f);
        const f32 cross_available = amal::max(cross_size(node->bounds.size, node->axis), 0.0f);
        if (_resize_helper_style.id == Theme::STYLE_ID_INVALID) update_style();
        const auto &helper_style = get_theme()->get_style(_resize_helper_style.id);
        const amal::vec4 helper_padding = helper_style.padding();
        const f32 helper_main_size = node->axis == amal::axis::x ? amal::max(helper_padding.x + helper_padding.z, 1.0f)
                                                                 : amal::max(helper_padding.y + helper_padding.w, 1.0f);
        auto helper_traits = [&](size_t before_child, size_t after_child, bool leading, bool &visible,
                                 bool &interactive) {
            visible = false;
            interactive = false;
            auto *before = before_child < node->children.size() ? get_node(node->children[before_child]) : nullptr;
            auto *after = after_child < node->children.size() ? get_node(node->children[after_child]) : nullptr;
            if (leading)
            {
                return;
            }
            visible = before && after && has_visible_resize_helper_flag(before->settings.flags, node->axis) &&
                      has_visible_resize_helper_flag(after->settings.flags, node->axis);
            interactive = before && after && has_resize_helper_flag(before->settings.flags, node->axis) &&
                          has_resize_helper_flag(after->settings.flags, node->axis);
        };
        auto helper_slot_size = [&](size_t before_child, size_t after_child, bool leading) {
            bool visible = false;
            bool interactive = false;
            helper_traits(before_child, after_child, leading, visible, interactive);
            return visible ? helper_main_size : 0.0f;
        };
        auto emit_helper_slot = [&](f32 slot_cursor, f32 slot_size, size_t before_child, size_t after_child,
                                    bool leading) {
            bool visible = false;
            bool interactive = false;
            helper_traits(before_child, after_child, leading, visible, interactive);
            if (!visible && !interactive) return slot_cursor;
            const f32 helper_size = visible ? slot_size : helper_main_size;
            if (helper_size <= 0.0f) return slot_cursor;
            const f32 helper_cursor = slot_cursor;
            amal::rect helper_bounds{};
            if (node->axis == amal::axis::x)
                helper_bounds = {{helper_cursor, node->bounds.offset.y}, {helper_size, cross_available}};
            else helper_bounds = {{node->bounds.offset.x, helper_cursor}, {cross_available, helper_size}};
            const amal::rect snapped =
                add_resize_helper(node_id, before_child, after_child, node->axis, helper_bounds, visible, interactive);
            if (!visible) return slot_cursor;
            return axis_size(snapped.offset, node->axis) + axis_size(snapped.size, node->axis);
        };

        f32 helpers_sum = 0.0f;
        for (size_t i = 0; i < node->children.size(); ++i)
        {
            if (node->axis == amal::axis::y) helpers_sum += helper_slot_size(i == 0u ? i : i - 1u, i, i == 0u);
            else if (i + 1u < node->children.size()) helpers_sum += helper_slot_size(i, i + 1u, false);
        }
        auto child_axis_min_size = [axis = node->axis](const Node &child) {
            const f32 settings_min = axis_size(child.min_size, axis);
            const f32 style_size = axis_size(child.style_size, axis);
            if (is_size_fill(style_size)) return amal::ceil(settings_min);
            if (is_size_fit(style_size))
                return amal::ceil(amal::max(settings_min, axis_size(child.required_size, axis)));
            if (is_size_concrete(style_size)) return amal::ceil(amal::max(settings_min, style_size));
            return amal::ceil(amal::max(settings_min, axis_size(child.required_size, axis)));
        };
        const f32 children_available = amal::max(main_available - helpers_sum, 0.0f);
        acul::vector<f32> child_main_sizes;
        child_main_sizes.resize(node->children.size());
        acul::vector<f32> child_min_sizes;
        child_min_sizes.resize(node->children.size());
        acul::vector<f32> child_basis_sizes;
        child_basis_sizes.resize(node->children.size());
        acul::vector<bool> child_fill;
        child_fill.resize(node->children.size());

        f32 fixed_size = 0.0f;
        f32 fill_min_sum = 0.0f;
        u32 fill_count = 0u;
        for (size_t i = 0; i < node->children.size(); ++i)
        {
            auto *child = get_node(node->children[i]);
            if (!child) continue;
            const f32 minimum = child_axis_min_size(*child);
            const f32 agreed_size = axis_size(child->settings.size, node->axis);
            const f32 style_size = axis_size(child->style_size, node->axis);
            if (agreed_size > 0.0f) child_basis_sizes[i] = agreed_size;
            else if (is_size_concrete(style_size)) child_basis_sizes[i] = style_size;
            else child_basis_sizes[i] = minimum;

            child_min_sizes[i] = minimum;
            child_fill[i] = agreed_size <= 0.0f && is_size_fill(style_size);
            child_main_sizes[i] = amal::ceil(amal::max(child_basis_sizes[i], minimum));
            if (child_fill[i])
            {
                ++fill_count;
                fill_min_sum += child_main_sizes[i];
            }
            else fixed_size += child_main_sizes[i];
        }

        const f32 clamped_size = fixed_size + fill_min_sum;
        if (clamped_size > children_available && clamped_size > 0.0f)
        {
            f32 slack_sum = 0.0f;
            for (size_t i = 0; i < node->children.size(); ++i)
            {
                auto *child = get_node(node->children[i]);
                if (!child) continue;
                slack_sum += amal::max(child_main_sizes[i] - child_min_sizes[i], 0.0f);
            }

            if (slack_sum > 0.0f)
            {
                const f32 overflow = clamped_size - children_available;
                for (size_t i = 0; i < node->children.size(); ++i)
                {
                    auto *child = get_node(node->children[i]);
                    if (!child) continue;
                    const f32 slack = amal::max(child_main_sizes[i] - child_min_sizes[i], 0.0f);
                    if (slack <= 0.0f) continue;
                    const f32 shrink = amal::min(slack, overflow * slack / slack_sum);
                    child_main_sizes[i] -= shrink;
                }

                f32 total = 0.0f;
                for (f32 size : child_main_sizes) total += amal::max(size, 0.0f);
                f32 extra = total - children_available;
                for (size_t rev = node->children.size(); rev > 0u && extra > 0.0f; --rev)
                {
                    const size_t i = rev - 1u;
                    auto *child = get_node(node->children[i]);
                    if (!child) continue;
                    const f32 slack = amal::max(child_main_sizes[i] - child_min_sizes[i], 0.0f);
                    if (slack <= 0.0f) continue;
                    const f32 shrink = amal::min(slack, extra);
                    child_main_sizes[i] -= shrink;
                    extra -= shrink;
                }
            }
        }
        else if (fill_count > 0u)
        {
            const f32 fill_extra = amal::max(children_available - clamped_size, 0.0f);
            const f32 fill_share = fill_extra / static_cast<f32>(fill_count);
            for (size_t i = 0; i < node->children.size(); ++i)
            {
                auto *child = get_node(node->children[i]);
                if (!child) continue;
                if (child_fill[i]) child_main_sizes[i] += fill_share;
            }
        }

        f32 cursor = axis_size(node->bounds.offset, node->axis);
        for (size_t i = 0; i < node->children.size(); ++i)
        {
            auto *child = get_node(node->children[i]);
            if (!child) continue;
            if (node->axis == amal::axis::y)
            {
                const f32 helper_size = helper_slot_size(i == 0u ? i : i - 1u, i, i == 0u);
                cursor = emit_helper_slot(cursor, helper_size, i == 0u ? i : i - 1u, i, i == 0u);
            }

            f32 child_main = child_main_sizes[i];
            if (i + 1u == node->children.size())
            {
                const f32 remaining_to_edge = axis_size(node->bounds.offset, node->axis) + main_available - cursor;
                if (remaining_to_edge >= 0.0f) child_main = remaining_to_edge;
            }

            amal::rect child_bounds{};
            if (node->axis == amal::axis::x)
                child_bounds = {{cursor, node->bounds.offset.y}, {child_main, cross_available}};
            else child_bounds = {{node->bounds.offset.x, cursor}, {cross_available, child_main}};
            layout_node(node->children[i], child_bounds);
            cursor = axis_size(_nodes[node->children[i]].bounds.offset, node->axis) +
                     axis_size(_nodes[node->children[i]].bounds.size, node->axis);

            if (node->axis == amal::axis::x)
            {
                if (i + 1u < node->children.size())
                {
                    const f32 helper_size = helper_slot_size(i, i + 1u, false);
                    cursor = emit_helper_slot(cursor, helper_size, i, i + 1u, false);
                }
                continue;
            }
        }
    }

    void Dockspace::update_layout(bool min_size_known)
    {
        if (layout_measure_required(min_size_known)) update_layout_min_size_force();

        if (parent())
        {
            set_layout_size(amal::max(size(), required_size()));
        }
        Widget::update_layout(true);
        ensure_own_clip_rect({position().x, position().y, size().x, size().y});

        const amal::rect content_bounds{position(), size()};
        const size_t previous_resize_helper_count = begin_resize_helpers();
        layout_node(root_node(), content_bounds);
        _drag_zones_dirty = true;
        const auto *dock_ctx = detail::g_context ? detail::g_context->dockspace_context : nullptr;
        if (dock_ctx && dock_ctx->drag_zones_enabled && dock_ctx->drag_window) sync_drag_zone_helpers(true, true);
        const size_t inactive_end = amal::min(previous_resize_helper_count, _resize_helpers.size());
        for (size_t i = _resize_helper_count; i < inactive_end; ++i)
        {
            detail::RectData hidden_hit = _resize_helpers[i].hit_rect;
            hidden_hit.bounds.size = {0.0f, 0.0f};
            update_existing_resize_helper_hit(_resize_helpers[i].hit_draw, hidden_hit);
        }
    }

    void Dockspace::translate(const amal::vec2 &delta)
    {
        if (delta.x == 0.0f && delta.y == 0.0f) return;
        Widget::translate(delta);
        for (auto &node : _nodes)
        {
            if (node.tabbar) node.tabbar->translate(delta);
            if (node.menu) node.menu->translate(delta);
            node.tab_panel_rect.bounds.offset += delta;
            node.bounds.offset += delta;
            node.content_bounds.offset += delta;
            for (auto *window : node.windows)
                if (window) window->translate(delta);
        }
        for (size_t i = 0; i < _resize_helper_count && i < _resize_helpers.size(); ++i)
        {
            auto &helper = _resize_helpers[i];
            helper.rect.bounds.offset += delta;
            helper.hit_rect.bounds.offset += delta;
            if (helper.interactive) update_existing_resize_helper_hit(helper.hit_draw, helper.hit_rect);
        }
        if (clip_id() != 0xFFFFu) update_clip_rect(clip_id(), {position().x, position().y, size().x, size().y});
    }

    void Dockspace::reset_clip_rect_records()
    {
        Widget::reset_clip_rect_records();
        for (auto &node : _nodes)
        {
            if (node.tabbar) node.tabbar->reset_clip_rect_records();
            if (node.menu) node.menu->reset_clip_rect_records();
            for (auto *window : node.windows)
                if (window) window->reset_clip_rect_records();
        }
    }

    void Dockspace::rebuild_clip_rects()
    {
        ensure_own_clip_rect({position().x, position().y, size().x, size().y});
        for (auto &node : _nodes)
        {
            if (node.tabbar) node.tabbar->rebuild_clip_rects();
            if (node.menu) node.menu->rebuild_clip_rects();
            for (auto *window : node.windows)
                if (window) window->rebuild_clip_rects();
        }
    }

    void Dockspace::reset_draw_records()
    {
        for (auto &node : _nodes)
        {
            if (node.tabbar) node.tabbar->reset_draw_records();
            if (node.menu) node.menu->reset_draw_records();
            node.tab_panel_draw = {};
            for (auto *window : node.windows)
                if (window) window->reset_draw_records();
        }
        for (auto &helper : _resize_helpers) helper.draw = {};
        for (auto &helper : _resize_helpers) helper.hit_draw = {};
    }

    void Dockspace::handle_tabbar_changed(DockNodeID node_id, TabbarChangeReason reason)
    {
        auto *node_ptr = get_node(node_id);
        if (!node_ptr || !node_ptr->tabbar) return;
        auto &node = *node_ptr;
        Window *prev_active_window =
            node.active_window_index < node.windows.size() ? node.windows[node.active_window_index] : nullptr;

        if (node.tabbar->child_size() == node.windows.size())
        {
            bool order_valid = true;
            bool order_changed = false;
            acul::vector<Window *> windows;
            acul::vector<amal::rect> undocked_bounds;
            acul::vector<acul::string> tab_titles;
            windows.reserve(node.windows.size());
            undocked_bounds.reserve(node.undocked_bounds.size());
            tab_titles.reserve(node.tab_titles.size());

            for (u32 i = 0u; i < node.tabbar->child_size(); ++i)
            {
                auto *window = static_cast<Window *>(node.tabbar->item_user_data(i));
                size_t old_index = static_cast<size_t>(-1);
                for (size_t window_i = 0; window_i < node.windows.size(); ++window_i)
                {
                    if (node.windows[window_i] != window) continue;
                    old_index = window_i;
                    break;
                }
                if (old_index >= node.windows.size())
                {
                    order_valid = false;
                    break;
                }
                if (old_index != i) order_changed = true;
                windows.push_back(node.windows[old_index]);
                if (old_index < node.undocked_bounds.size()) undocked_bounds.push_back(node.undocked_bounds[old_index]);
                if (old_index < node.tab_titles.size()) tab_titles.push_back(node.tab_titles[old_index]);
            }

            if (order_valid && order_changed)
            {
                node.windows = std::move(windows);
                node.undocked_bounds = std::move(undocked_bounds);
                node.tab_titles = std::move(tab_titles);
                for (size_t i = 0; i < node.windows.size(); ++i)
                    node.tabbar->set_item_user_data(static_cast<u32>(i), node.windows[i]);
            }

            if (reason == TabbarChangeReason::selection)
            {
                node.active_window_index = selected_window_index(node);
                node.record_active_window = true;
            }
            else
            {
                node.active_window_index = static_cast<size_t>(-1);
                if (prev_active_window)
                {
                    for (size_t i = 0; i < node.windows.size(); ++i)
                    {
                        if (node.windows[i] != prev_active_window) continue;
                        node.active_window_index = i;
                        break;
                    }
                }
            }
        }
        else
        {
            node.active_window_index = static_cast<size_t>(-1);
            if (prev_active_window)
            {
                for (size_t i = 0; i < node.windows.size(); ++i)
                {
                    if (node.windows[i] != prev_active_window) continue;
                    node.active_window_index = i;
                    break;
                }
            }
        }

        auto &ctx = detail::get_context();
        ctx.dirty_flags |= DirtyFlagBits::redraw | DirtyFlagBits::hit_rect_update;
        update_layout(false);
        invalidate_draw_commands(DrawReasonBits::layout);
        update_draw_commands(DrawReasonBits::layout | DrawReasonBits::record);
        rerecord_root_widgets_after(this);
        if (ctx.dirty_flags & DirtyFlagBits::clip_rect) sync_clip_rect_cache();
    }

    bool Dockspace::handle_tabbar_drag_escape(DockNodeID node_id, u32 element_id)
    {
        if (element_id == 0u) return false;
        auto *node_ptr = get_node(node_id);
        if (!node_ptr) return false;
        auto &node = *node_ptr;
        auto *window =
            node.tabbar ? static_cast<Window *>(node.tabbar->item_user_data_by_element_id(element_id)) : nullptr;

        size_t window_index = static_cast<size_t>(-1);
        for (size_t i = 0; i < node.windows.size(); ++i)
        {
            if (node.windows[i] != window) continue;
            window_index = i;
            break;
        }
        if (window_index >= node.windows.size()) return false;

        if (!window) return false;

        Tabbar *tabbar = node.tabbar;
        const amal::vec2 mouse = detail::get_io().mouse_pos;
        const auto drag_grab =
            get_tab_drag_grab_offset(tabbar, element_id, {amal::min(window->size().x * 0.5f, 80.0f), 12.0f});
        if (tabbar) tabbar->cancel_drag();
        amal::rect previous_undocked_bounds{};
        invalidate_window_draw_records(window);
        Window *extracted = extract_window(node, window, &previous_undocked_bounds);
        if (!extracted) return false;

        amal::rect drag_bounds = extracted->bounds();
        drag_bounds.size = amal::max(previous_undocked_bounds.size, extracted->min_size());
        drag_bounds.offset = {mouse.x - amal::clamp(drag_grab.x, 0.0f, drag_bounds.size.x),
                              mouse.y - amal::clamp(drag_grab.y, 0.0f, drag_bounds.size.y)};

        detach_window(extracted, &drag_bounds);
        extracted->window_flags |= WindowFlagBits::decorated | WindowFlagBits::movable | WindowFlagBits::resizable;
        extracted->window_flags &= ~WindowFlagBits::docked;
        extracted->set_auto_size(false, false);
        extracted->set_auto_position(false, false);
        extracted->set_position(drag_bounds.offset);
        extracted->set_size(drag_bounds.size);
        extracted->reset_draw_records();
        if (node.windows.empty()) remove_empty_node(node_id);
        else fit_node_to_required_width(node_id, true);
        update_layout_sync(true);
        add_widget_to_root(extracted);
        const auto viewport = get_widget_viewport_rect(extracted);
        extracted->set_root_viewport_origin({viewport.x, viewport.y});
        extracted->set_position(drag_bounds.offset);
        extracted->set_size(drag_bounds.size);

        auto &ctx = detail::get_context();
        const auto header_id = make_element_id(extracted->id(), AUIK_TAG_WINDOW_HEADER);
        ctx.io.clicked_id = header_id;
        ctx.io.drag_id = header_id;
        if (!ctx.io.drag_key_flags) ctx.io.drag_key_flags = ctx.io.active_mouse_buttons;
        ctx.io.last_drag_pos = mouse;
        const bool was_already_focused = ctx.focus_id == extracted->id();
        focus_widget(extracted);
        if (was_already_focused) extracted->set_style_state(StyleState::focus);
        ctx.active_id = extracted->id();
        detail::set_style_selector(header_id, StyleState::active);
        ctx.frame_cache.drag_widget_id = extracted->id();
        extracted->begin_external_move_drag();
        extracted->update_style_invalidated();
        extracted->update_layout(false);
        extracted->rebuild_clip_rects();
        extracted->back_hit_depth();
        extracted->update_draw_commands(DrawReasonBits::record);
        enable_dockspace_drag_zones(extracted);
        ctx.dirty_flags |= DirtyFlagBits::redraw | DirtyFlagBits::hit_rect_update;
        mark_host_refresh_request();
        return true;
    }

    void Dockspace::update_depth(const amal::vec2 &depth_range)
    {
        Widget::update_depth(depth_range);
        const amal::vec2 dock_work_range = detail::depth_work_range(this->depth_range());
        u32 content_requirement = 1u;
        u32 chrome_requirement = 1u;
        for (const auto &node : _nodes)
        {
            for (auto *window : node.windows)
                if (window) content_requirement = amal::max(content_requirement, window->get_depth_requirement());
            if (node.tabbar) chrome_requirement = amal::max(chrome_requirement, node.tabbar->get_depth_requirement());
            if (node.menu) chrome_requirement = amal::max(chrome_requirement, node.menu->get_depth_requirement());
        }
        DepthCursor cursor(dock_work_range, content_requirement + chrome_requirement);
        const amal::vec2 content_range = cursor.next(content_requirement);
        _tabbar_depth_range = cursor.next(chrome_requirement);
        for (auto &node : _nodes)
        {
            for (auto *window : node.windows)
            {
                if (!window) continue;
                window->update_depth(content_range);
            }
        }
        get_rect().depth = next_depth(this->depth_range());
        get_rect().hit_depth = get_rect().depth;
        const amal::vec2 dock_bg_range = detail::depth_background_range(this->depth_range());
        _tab_panel_depth = dock_bg_range.x;
        const amal::vec2 dock_fg_range = detail::depth_foreground_range(this->depth_range());
        const amal::vec2 helper_range = detail::depth_foreground_range(dock_fg_range);
        _resize_helper_depth = next_depth(helper_range);
        for (auto &node : _nodes)
        {
            node.tab_panel_rect.depth = _tab_panel_depth;
            node.tab_panel_rect.hit_depth = node.tab_panel_rect.depth;
            if (node.tabbar) node.tabbar->update_depth(_tabbar_depth_range);
            if (node.menu) node.menu->update_depth(this->depth_range(), _tabbar_depth_range);
        }
        for (size_t i = 0; i < _resize_helper_count; ++i)
        {
            const f32 helper_depth =
                _resize_helpers[i].drop_zone ? get_dock_drop_zone_visual_depth() : _resize_helper_depth;
            _resize_helpers[i].rect.depth = helper_depth;
            _resize_helpers[i].rect.hit_depth =
                _resize_helpers[i].drop_zone ? get_dock_helper_hit_depth() : _resize_helpers[i].rect.depth;
            _resize_helpers[i].hit_rect.depth = helper_depth;
            _resize_helpers[i].hit_rect.hit_depth = _resize_helpers[i].hit_rect.depth;
            if (_resize_helpers[i].drop_zone) _resize_helpers[i].hit_rect.hit_depth = _resize_helpers[i].rect.hit_depth;
        }
    }

    u32 Dockspace::get_depth_requirement() const
    {
        u32 content_requirement = 1u;
        u32 chrome_requirement = 1u;
        for (const auto &node : _nodes)
        {
            for (auto *window : node.windows)
                if (window) content_requirement = amal::max(content_requirement, window->get_depth_requirement());
            if (node.tabbar) chrome_requirement = amal::max(chrome_requirement, node.tabbar->get_depth_requirement());
            if (node.menu) chrome_requirement = amal::max(chrome_requirement, node.menu->get_depth_requirement());
        }
        return content_requirement + chrome_requirement;
    }

    void Dockspace::back_hit_depth()
    {
        Widget::back_hit_depth();
        for (auto &node : _nodes)
        {
            node.tab_panel_rect.hit_depth = get_rect().hit_depth;
            if (node.tabbar) node.tabbar->back_hit_depth();
            if (node.menu) node.menu->back_hit_depth();
            for (auto *window : node.windows)
                if (window) window->back_hit_depth();
        }
        for (size_t i = 0; i < _resize_helper_count; ++i)
        {
            _resize_helpers[i].rect.hit_depth = get_rect().hit_depth;
            _resize_helpers[i].hit_rect.hit_depth = get_rect().hit_depth;
        }
        detail::get_context().dirty_flags |= DirtyFlagBits::hit_rect_update;
    }

    void Dockspace::restore_hit_depth()
    {
        Widget::restore_hit_depth();
        for (auto &node : _nodes)
        {
            node.tab_panel_rect.hit_depth = node.tab_panel_rect.depth;
            if (node.tabbar) node.tabbar->restore_hit_depth();
            if (node.menu) node.menu->restore_hit_depth();
            for (auto *window : node.windows)
                if (window) window->restore_hit_depth();
        }
        for (size_t i = 0; i < _resize_helper_count; ++i)
        {
            _resize_helpers[i].rect.hit_depth = _resize_helpers[i].rect.depth;
            _resize_helpers[i].hit_rect.hit_depth = _resize_helpers[i].hit_rect.depth;
        }
        detail::get_context().dirty_flags |= DirtyFlagBits::hit_rect_update;
    }

    void Dockspace::draw_node_tab_panel(DrawCtx &ctx, DockNodeID node_id, Node &node)
    {
        auto *quads_stream = get_primary_quads_stream();
        if (!quads_stream) return;
        const bool has_panel =
            node.tabbar && node.tab_panel_rect.bounds.size.x > 0.0f && node.tab_panel_rect.bounds.size.y > 0.0f;
        if (!has_panel)
        {
            emit_quads_instance(ctx, quads_stream, node.tab_panel_draw, {}, node.tab_panel_rect, false, false);
            if (node.tab_panel_draw.hit_id != AUIK_INVALID_DRAW_DATA_ID)
            {
                detail::RectData hidden_hit = node.tab_panel_rect;
                hidden_hit.bounds.size = {0.0f, 0.0f};
                update_hit_rect(node.tab_panel_draw.hit_id, hidden_hit, true);
            }
            return;
        }

        const auto &style = get_theme()->get_style(_tab_panel_style.id);
        QuadsInstanceData data{};
        data.rect = node.tab_panel_rect.bounds;
        data.z_order = node.tab_panel_rect.depth;
        const bool visible = fill_quads_instance_by_style(style, clip_id(), data);
        const auto *dock_ctx = detail::g_context ? detail::g_context->dockspace_context : nullptr;
        const bool emit_panel_hit =
            dock_ctx && dock_ctx->drag_zones_enabled && dock_ctx->drag_window && docking_enabled();
        emit_quads_instance(ctx, quads_stream, node.tab_panel_draw, data, node.tab_panel_rect, visible, false);
        detail::RectData panel_hit = node.tab_panel_rect;
        if (emit_panel_hit) panel_hit.hit_depth = get_dock_tab_panel_hit_depth();
        else panel_hit.bounds.size = {0.0f, 0.0f};
        detail::emit_table_service_hit_rect(ctx, node.tab_panel_draw, panel_hit, can_emit_hit(ctx));

        DrawCtx tab_ctx = ctx;
        node.tabbar->draw_local(tab_ctx);
        draw_node_menu(ctx, node_id, node);
    }

    void Dockspace::draw_node_menu(DrawCtx &ctx, DockNodeID node_id, Node &node)
    {
        (void)node_id;
        if (!has_node_menu(node) || !node.menu) return;
        DrawCtx button_ctx = ctx;
        node.menu->draw_button(button_ctx);
        if (node.menu->is_open()) _deferred_menu_popup_draw = node.menu;
    }

    void Dockspace::draw_resize_helpers(DrawCtx &ctx)
    {
        auto *quads_stream = get_primary_quads_stream();
        auto *overlay_quads_stream = get_overlay_quads_stream();
        if (!quads_stream || !overlay_quads_stream) return;
        auto *theme = get_theme();
        const auto *dock_ctx = detail::g_context ? detail::g_context->dockspace_context : nullptr;
        const bool drag_zones_enabled = dock_ctx && dock_ctx->drag_zones_enabled && dock_ctx->drag_window;
        auto hide_helper = [&](ResizeHelperVisual &helper, DrawStream *helper_stream) {
            helper.rect.bounds.size = {0.0f, 0.0f};
            helper.hit_rect.bounds.size = {0.0f, 0.0f};
            emit_quads_instance(ctx, helper_stream, helper.draw, {}, helper.rect, false, false);
            detail::emit_table_service_hit_rect(ctx, helper.hit_draw, helper.hit_rect, can_emit_hit(ctx));
        };
        for (size_t i = 0; i < _resize_helpers.size(); ++i)
        {
            auto &helper = _resize_helpers[i];
            auto *helper_stream = helper.draw_in_overlay ? overlay_quads_stream : quads_stream;
            if (i >= _resize_helper_count)
            {
                hide_helper(helper, helper_stream);
                continue;
            }
            if (!helper.visible && !helper.interactive)
            {
                if (!helper.drop_zone)
                {
                    hide_helper(helper, helper_stream);
                    continue;
                }
                QuadsInstanceData hidden_data{};
                hidden_data.rect = {};
                hidden_data.z_order = helper.rect.depth;
                emit_quads_instance(ctx, helper_stream, helper.draw, hidden_data, helper.rect, true, false);
                detail::RectData hidden_hit = helper.hit_rect;
                hidden_hit.bounds.size = {0.0f, 0.0f};
                detail::emit_table_service_hit_rect(ctx, helper.hit_draw, hidden_hit, can_emit_hit(ctx));
                continue;
            }

            StyleState state = helper.interactive ? resolve_resize_helper_state(helper.rect) : StyleState::normal;
            u32 style_tag = _resize_helper_style.tag_id;
            if (helper.drop_zone && helper.axis == amal::axis::y) style_tag = AUIK_STYLE_TAG_DOCKSPACE_DROP_ZONE;
            else if (drag_zones_enabled && helper.axis == amal::axis::x) style_tag = _resize_helper_drag_style.tag_id;
            const u32 widget_id = style_tag;
            const StyleID style_id = theme->get_resolved_style(style_tag, widget_id, id(), state);
            const Style &style = theme->get_style(style_id);
            QuadsInstanceData data{};
            data.rect = helper.rect.bounds;
            data.z_order = helper.rect.depth;
            const bool drag_zone_helper = drag_zones_enabled && (helper.drop_zone || helper.axis == amal::axis::x);
            bool visible =
                helper.visible || state == StyleState::active || (drag_zone_helper && state == StyleState::hover);
            const bool style_visible = fill_quads_instance_by_style(style, clip_id(), data);
            visible = style_visible && visible;
            emit_quads_instance(ctx, helper_stream, helper.draw, data, helper.rect, visible, false);
            if (helper.interactive)
            {
                detail::RectData helper_hit = helper.hit_rect;
                if (drag_zone_helper) helper_hit.hit_depth = get_dock_helper_hit_depth();
                detail::emit_table_service_hit_rect(ctx, helper.hit_draw, helper_hit, can_emit_hit(ctx));
            }
        }
    }

    void Dockspace::draw_node(DrawCtx &ctx, DockNodeID node_id)
    {
        auto *node = get_node(node_id);
        if (!node) return;
        for (DockNodeID child_id : node->children) draw_node(ctx, child_id);
        const size_t selected_index = selected_window_index(*node);
        auto draw_window = [&](size_t i, Window *window, bool respect_visibility, bool force_record) {
            if (!window) return;
            if (respect_visibility && !window->is_visible()) return;
            DrawCtx child_ctx = ctx;
            if (force_record)
            {
                window->invalidate_draw_commands(DrawReasonBits::layout);
                window->reset_draw_records();
                child_ctx.reason |= DrawReasonBits::record;
                window->draw_local(child_ctx);
                return;
            }
            window->draw_local(child_ctx);
        };
        if (ctx.reason & DrawReasonBits::invalidate)
        {
            for (size_t i = 0; i < node->windows.size(); ++i) draw_window(i, node->windows[i], false, false);
            if (needs_node_tab_panel(*node)) draw_node_tab_panel(ctx, node_id, *node);
            return;
        }
        if (selected_index < node->windows.size())
        {
            const bool force_record = node->record_active_window && !(ctx.reason & DrawReasonBits::invalidate);
            draw_window(selected_index, node->windows[selected_index], false, force_record);
            if (force_record) node->record_active_window = false;
        }
        if (needs_node_tab_panel(*node)) draw_node_tab_panel(ctx, node_id, *node);
    }

    void Dockspace::draw(DrawCtx &ctx)
    {
        if (!is_visible() && !(ctx.reason & DrawReasonBits::invalidate)) return;
        _deferred_menu_popup_draw = nullptr;
        const bool fast_update = detail::get_context().dirty_flags & DirtyFlagBits::fast_update;
        if (!fast_update && sync_active_windows(root_node(), (ctx.reason & DrawReasonBits::record)))
            sync_clip_rect_cache();
        draw_node(ctx, root_node());
        draw_resize_helpers(ctx);
        if (_deferred_menu_popup_draw && _deferred_menu_popup_draw->is_open())
        {
            // Popup belongs to the dockspace draw order and is emitted once after all dock content.
            DrawCtx menu_ctx = ctx;
            _deferred_menu_popup_draw->draw_popups(menu_ctx);
        }
        _deferred_menu_popup_draw = nullptr;
    }

    Dockspace::ResizeHelperVisual *Dockspace::resize_helper_from_element(u32 element_id)
    {
        if (element_id >= _resize_helper_count) return nullptr;
        return &_resize_helpers[element_id];
    }

    const Dockspace::ResizeHelperVisual *Dockspace::resize_helper_from_element(u32 element_id) const
    {
        if (element_id >= _resize_helper_count) return nullptr;
        return &_resize_helpers[element_id];
    }

    void Dockspace::on_hover(HoverState state)
    {
        auto &ctx = detail::get_context();
        detail::CursorID::enum_type cursor = detail::CursorID::arrow;
        bool helper_state = state == HoverState::leave;
        if (state != HoverState::leave && ctx.hover_id.widget_id == id() && is_resize_helper_tag(ctx.hover_id.tag_id))
        {
            if (const auto *helper = resize_helper_from_element(ctx.hover_id.element_id); helper && helper->interactive)
            {
                if (!helper->drop_zone)
                    cursor = helper->axis == amal::axis::x ? detail::CursorID::resize_ew : detail::CursorID::resize_ns;
                helper_state = true;
            }
        }
        detail::set_window_cursor(cursor, ctx.window_ctx);
        if (helper_state)
        {
            ctx.dirty_flags |= DirtyFlagBits::redraw;
            mark_host_refresh_request();
        }
    }

    void Dockspace::on_click(MouseKey key, KeyPressState state, u32 click_count)
    {
        (void)click_count;
        (void)key;
        (void)state;
    }

    void Dockspace::update_layout_sync(bool force_record, bool min_size_known)
    {
        auto &ctx = detail::get_context();
        ctx.dirty_flags |= DirtyFlagBits::redraw | DirtyFlagBits::hit_rect_update;
        update_layout(min_size_known);
        if (force_record)
        {
            invalidate_draw_commands(DrawReasonBits::layout);
            reset_draw_records();
            update_draw_commands(DrawReasonBits::layout | DrawReasonBits::record);
        }
        else update_draw_commands(DrawReasonBits::layout);
        mark_host_refresh_request();
    }

    void Dockspace::on_drag(const amal::vec2 &delta, KeyPressState state)
    {
        auto &ctx = detail::get_context();
        const auto drag_id = ctx.io.drag_id;
        if (state == KeyPressState::press)
        {
            _resizing_helper = static_cast<size_t>(-1);
            _resize_drag_accum = {0.0f, 0.0f};
            _resize_basis.clear();
            if (drag_id.widget_id != id() || !is_resize_helper_tag(drag_id.tag_id)) return;
            auto *helper = resize_helper_from_element(drag_id.element_id);
            if (!helper || !helper->interactive || helper->drop_zone) return;
            auto *parent_node = get_node(helper->parent);
            if (!parent_node) return;
            _resizing_helper = drag_id.element_id;
            _resize_basis.resize(parent_node->children.size());
            for (size_t i = 0; i < parent_node->children.size(); ++i)
            {
                const auto *child = get_node(parent_node->children[i]);
                _resize_basis[i] = child ? axis_size(child->bounds.size, parent_node->axis) : 0.0f;
            }
            detail::set_window_cursor(helper->axis == amal::axis::x ? detail::CursorID::resize_ew
                                                                    : detail::CursorID::resize_ns,
                                      ctx.window_ctx);
            return;
        }

        if (state == KeyPressState::release)
        {
            const bool was_resizing = _resizing_helper != static_cast<size_t>(-1);
            _resizing_helper = static_cast<size_t>(-1);
            _resize_drag_accum = {0.0f, 0.0f};
            _resize_basis.clear();
            detail::set_window_cursor(detail::CursorID::arrow, ctx.window_ctx);
            if (was_resizing) update_layout_sync(false, false);
            return;
        }

        auto *helper = resize_helper_from_element(static_cast<u32>(_resizing_helper));
        if (!helper || !helper->interactive) return;
        auto *parent_node = get_node(helper->parent);
        if (!parent_node || helper->after_child >= parent_node->children.size() ||
            helper->before_child >= parent_node->children.size())
            return;
        if (_resize_basis.size() != parent_node->children.size()) return;

        _resize_drag_accum += delta;
        const f32 requested_delta = helper->axis == amal::axis::x ? _resize_drag_accum.x : _resize_drag_accum.y;
        if (requested_delta == 0.0f) return;

        auto child_min_size = [&](size_t index) {
            auto *child = get_node(parent_node->children[index]);
            if (!child) return 0.0f;
            const f32 settings_min = axis_size(child->min_size, helper->axis);
            const f32 style_size = axis_size(child->style_size, helper->axis);
            if (is_size_fill(style_size)) return amal::ceil(settings_min);
            if (helper->axis == amal::axis::x && is_size_fit(style_size)) return amal::ceil(settings_min);
            if (is_size_concrete(style_size)) return amal::ceil(amal::max(settings_min, style_size));
            const f32 required_min = axis_size(child->required_size, helper->axis);
            return amal::ceil(amal::max(settings_min, required_min));
        };

        acul::vector<f32> next_sizes;
        next_sizes.resize(parent_node->children.size());
        for (size_t i = 0; i < parent_node->children.size(); ++i) next_sizes[i] = _resize_basis[i];

        auto is_fill_child = [&](size_t index) {
            if (index >= parent_node->children.size()) return false;
            auto *child = get_node(parent_node->children[index]);
            return child && is_size_fill(axis_size(child->style_size, helper->axis));
        };
        auto find_prev_fill_child = [&](size_t index) {
            if (parent_node->children.empty()) return static_cast<size_t>(-1);
            size_t i = amal::min(index, parent_node->children.size() - 1u);
            for (;;)
            {
                if (is_fill_child(i)) return i;
                if (i == 0u) break;
                --i;
            }
            return static_cast<size_t>(-1);
        };
        auto find_next_fill_child = [&](size_t index) {
            for (size_t i = index; i < parent_node->children.size(); ++i)
                if (is_fill_child(i)) return i;
            return static_cast<size_t>(-1);
        };
        const size_t before_resize_child =
            is_fill_child(helper->before_child) ? helper->before_child : find_prev_fill_child(helper->before_child);
        const size_t after_resize_child =
            is_fill_child(helper->after_child) ? helper->after_child : find_next_fill_child(helper->after_child);
        const size_t grow_before = before_resize_child != static_cast<size_t>(-1)
                                       ? before_resize_child
                                       : static_cast<size_t>(helper->before_child);
        const size_t grow_after = after_resize_child != static_cast<size_t>(-1)
                                      ? after_resize_child
                                      : static_cast<size_t>(helper->after_child);
        if (grow_before >= parent_node->children.size() || grow_after >= parent_node->children.size() ||
            grow_before == grow_after)
            return;

        auto shrink_child = [&](size_t index, f32 amount) {
            if (index >= parent_node->children.size()) return 0.0f;
            auto *child = get_node(parent_node->children[index]);
            if (!child) return 0.0f;
            const f32 slack = next_sizes[index] - child_min_size(index);
            if (slack <= 0.0f) return 0.0f;
            const f32 consumed = amal::min(amount, slack);
            next_sizes[index] -= consumed;
            return consumed;
        };

        if (requested_delta > 0.0f)
        {
            const f32 applied = shrink_child(grow_after, requested_delta);
            if (applied <= 0.0f) return;
            next_sizes[grow_before] += applied;
        }
        else
        {
            const f32 applied = shrink_child(grow_before, -requested_delta);
            if (applied <= 0.0f) return;
            next_sizes[grow_after] += applied;
        }

        bool changed = false;
        for (size_t i = 0; i < parent_node->children.size(); ++i)
        {
            auto *child = get_node(parent_node->children[i]);
            if (!child) continue;
            if (next_sizes[i] == _resize_basis[i]) continue;
            f32 next_size = amal::max(next_sizes[i], child_min_size(i));
            next_size = amal::max(next_size, 0.0001f);

            if (axis_size(child->settings.size, helper->axis) == next_size) continue;
            set_axis_size(child->settings.size, helper->axis, next_size);
            changed = true;
        }
        if (!changed) return;
        detail::mark_fast_update_dirty();
        update_layout_sync(false, true);
    }

    void Dockspace::on_drop(ElementID drag_id, ElementID drop_id)
    {
        if (drag_id.tag_id != AUIK_TAG_WINDOW_HEADER) return;
        if (drop_id.widget_id != id() || !is_resize_helper_tag(drop_id.tag_id)) return;

        auto *dock_ctx = detail::g_context ? detail::g_context->dockspace_context : nullptr;
        Window *window = dock_ctx ? dock_ctx->drag_window : nullptr;
        if (!window || window->parent() || (window->window_flags & WindowFlagBits::docked)) return;

        const auto *helper = resize_helper_from_element(drop_id.element_id);
        if (!helper || !helper->interactive) return;
        if (!resize_helper_accepts_drop(*helper)) return;

        const DockNodeID parent_id = helper->parent;
        const amal::axis axis = helper->axis;
        const bool drop_zone = helper->drop_zone;
        const size_t after_child = helper->after_child;

        auto *parent = get_node(parent_id);
        if (!parent || parent->axis != axis) return;
        if (!drop_zone && axis != amal::axis::x) return;
        if (after_child > parent->children.size()) return;

        window->restore_hit_depth();
        disable_dockspace_drag_zones(window, "helper-drop");

        invalidate_window_draw_records(window);
        window->reset_draw_records();
        if (!remove_root_widget(window)) return;

        const size_t insert_index = amal::min<size_t>(after_child, parent->children.size());
        DockNodeSettings settings = normalize_dock_node_settings(_new_node_settings);

        DockNodeID leaf_id = static_cast<DockNodeID>(_nodes.size());
        Node leaf{};
        leaf.parent = parent_id;
        leaf.settings = settings;
        _nodes.push_back(std::move(leaf));

        parent = get_node(parent_id);
        if (!parent) return;
        parent->children.insert(parent->children.begin() + insert_index, leaf_id);
        for (DockNodeID child_id : parent->children)
        {
            auto *child = get_node(child_id);
            if (child && is_size_fill(axis_size(child->style_size, axis)))
                set_axis_size(child->settings.size, axis, 0.0f);
        }

        add_window(leaf_id, window);
        fit_node_to_required_width(leaf_id, false);
        update_layout_sync(true);
    }

    void Dockspace::on_attach()
    {
        Widget::on_attach();
        register_dockspace(this);
        for (auto &node : _nodes)
        {
            if (node.tabbar && (node.tabbar->widget_flags & WidgetFlagBits::attachable)) node.tabbar->on_attach();
            if (node.menu && (node.menu->widget_flags & WidgetFlagBits::attachable)) node.menu->on_attach();
            for (auto *window : node.windows)
                if (window && (window->widget_flags & WidgetFlagBits::attachable))
                    static_cast<Widget *>(window)->on_attach();
        }
    }

    void Dockspace::on_detach()
    {
        unregister_dockspace(this);
        for (auto &node : _nodes)
        {
            if (node.menu && (node.menu->widget_flags & WidgetFlagBits::attachable)) node.menu->on_detach();
            if (node.tabbar && (node.tabbar->widget_flags & WidgetFlagBits::attachable)) node.tabbar->on_detach();
            for (auto *window : node.windows)
                if (window && (window->widget_flags & WidgetFlagBits::attachable))
                    static_cast<Widget *>(window)->on_detach();
        }
        Widget::on_detach();
    }

    bool Dockspace::accepts_drag_hover(ElementID drag_id, ElementID hover_id) const
    {
        const auto *dock_ctx = detail::g_context ? detail::g_context->dockspace_context : nullptr;
        if (!dock_ctx || !dock_ctx->drag_zones_enabled || !dock_ctx->drag_window) return false;
        if (drag_id.tag_id != AUIK_TAG_WINDOW_HEADER) return false;

        if (hover_id.widget_id == id() && hover_id.tag_id == AUIK_TAG_DOCKSPACE_TAB_PANEL)
        {
            const auto *node = get_node(hover_id.element_id);
            return docking_enabled() && node && node->tabbar;
        }

        if (hover_id.widget_id != id() || !is_resize_helper_tag(hover_id.tag_id)) return false;
        const auto *helper = resize_helper_from_element(hover_id.element_id);
        if (!helper || !helper->interactive) return false;
        return resize_helper_accepts_drop(*helper);
    }

    struct DockspaceStreamAccess
    {
        static void write_node_settings(acul::bin_stream &stream, const DockNodeSettings &settings)
        {
            stream.write(settings.style_tag)
                .write(settings.size)
                .write(settings.min_size)
                .write(static_cast<u32>(settings.flags))
                .write(static_cast<u32>(settings.tabbar_flags))
                .write(settings.tabpanel);
        }

        static DockNodeSettings read_node_settings(acul::bin_stream &stream)
        {
            DockNodeSettings settings{};
            u32 flags = 0u;
            u32 tabbar_flags = 0u;
            stream.read(settings.style_tag)
                .read(settings.size)
                .read(settings.min_size)
                .read(flags)
                .read(tabbar_flags)
                .read(settings.tabpanel);
            settings.flags = DockspaceResizeFlags(flags);
            settings.tabbar_flags = TabbarFlags(tabbar_flags);
            return normalize_dock_node_settings(settings);
        }

        static void write_menu_group(acul::bin_stream &stream, const Dockspace::MenuGroup &group)
        {
            stream.write(static_cast<u32>(group.size()));
            for (const auto &item : group) stream.write(item);
        }

        static Dockspace::MenuGroup read_menu_group(acul::bin_stream &stream)
        {
            u32 item_count = 0u;
            stream.read(item_count);
            Dockspace::MenuGroup group;
            group.reserve(item_count);
            for (u32 item_i = 0u; item_i < item_count; ++item_i)
            {
                acul::string item;
                stream.read(item);
                group.push_back(std::move(item));
            }
            return group;
        }

        static void write_node(acul::bin_stream &stream, const Dockspace::Node &node)
        {
            write_node_settings(stream, node.settings);
            stream.write(static_cast<u8>(node.axis)).write(node.parent).write(static_cast<u32>(node.children.size()));
            if (!node.children.empty()) stream.write(node.children.data(), node.children.size());

            stream.write(node.bounds)
                .write(node.content_bounds)
                .write(node.required_size)
                .write(static_cast<u64>(node.active_window_index))
                .write(node.record_active_window);

            acul::vector<umbf::Block *> windows;
            acul::vector<amal::rect> undocked_bounds;
            windows.reserve(node.windows.size());
            undocked_bounds.reserve(node.windows.size());
            for (size_t window_i = 0u; window_i < node.windows.size(); ++window_i)
            {
                auto *window = node.windows[window_i];
                if (!window || !(window->widget_flags & WidgetFlagBits::configurable)) continue;
                windows.push_back(window);
                undocked_bounds.push_back(window_i < node.undocked_bounds.size() ? node.undocked_bounds[window_i]
                                                                                 : window->bounds());
            }

            stream.write(static_cast<u32>(undocked_bounds.size()));
            if (!undocked_bounds.empty()) stream.write(undocked_bounds.data(), undocked_bounds.size());
            stream.write(windows);
        }

        static void read_node(acul::bin_stream &stream, Dockspace::Node &node)
        {
            node.settings = read_node_settings(stream);
            u8 axis = static_cast<u8>(amal::axis::x);
            stream.read(axis).read(node.parent);
            node.axis = static_cast<amal::axis>(axis);

            u32 child_count = 0u;
            stream.read(child_count);
            node.children.resize(child_count);
            if (!node.children.empty()) stream.read(node.children.data(), node.children.size());

            u64 active_window_index = static_cast<u64>(-1);
            stream.read(node.bounds)
                .read(node.content_bounds)
                .read(node.required_size)
                .read(active_window_index)
                .read(node.record_active_window);
            node.active_window_index = static_cast<size_t>(active_window_index);

            u32 bounds_count = 0u;
            stream.read(bounds_count);
            node.undocked_bounds.resize(bounds_count);
            if (!node.undocked_bounds.empty()) stream.read(node.undocked_bounds.data(), node.undocked_bounds.size());

            acul::vector<umbf::Block *> windows;
            stream.read(windows);
            node.windows.reserve(windows.size());
            for (auto *block : windows) node.windows.push_back(static_cast<Window *>(block));
            if (node.active_window_index < node.windows.size()) node.record_active_window = true;
        }

        static void write(acul::bin_stream &stream, umbf::Block *block)
        {
            auto *dockspace = static_cast<Dockspace *>(block);
            detail::write_widget_common_data(stream, *dockspace);
            stream.write(static_cast<u32>(dockspace->_policy_flags));
            write_node_settings(stream, dockspace->_new_node_settings);
            write_menu_group(stream, dockspace->_menu_group);
            stream.write(dockspace->_open_menu_node);

            stream.write(static_cast<u32>(dockspace->_nodes.size()));
            for (const auto &node : dockspace->_nodes) write_node(stream, node);
        }

        static umbf::Block *read(acul::bin_stream &stream)
        {
            const auto common = detail::read_widget_common_data(stream);
            u32 policy_flags = 0u;
            stream.read(policy_flags);
            auto new_node_settings = read_node_settings(stream);
            auto menu_group = read_menu_group(stream);
            DockNodeID open_menu_node = AUIK_DOCK_NODE_INVALID;
            stream.read(open_menu_node);

            auto *dockspace = acul::alloc<Dockspace>(common.id, WidgetFlags(common.widget_flags));
            detail::apply_widget_common_data(dockspace, common);
            dockspace->_policy_flags = DockspaceFlags(policy_flags);
            dockspace->_new_node_settings = new_node_settings;
            dockspace->_menu_group = std::move(menu_group);
            dockspace->_open_menu_node = open_menu_node;

            for (auto &node : dockspace->_nodes) dockspace->clear_node_chrome(node);
            dockspace->_nodes.clear();

            u32 node_count = 0u;
            stream.read(node_count);
            dockspace->_nodes.resize(node_count);
            for (u32 node_i = 0u; node_i < node_count; ++node_i)
            {
                read_node(stream, dockspace->_nodes[node_i]);
                dockspace->update_node_style_cache(node_i, dockspace->_nodes[node_i]);
                for (auto *window : dockspace->_nodes[node_i].windows) dockspace->attach_window(window);
            }

            if (dockspace->_nodes.empty())
            {
                dockspace->_nodes.push_back({});
                dockspace->_nodes[0].settings.min_size = {0.0f, 0.0f};
                dockspace->update_node_style_cache(0u, dockspace->_nodes[0]);
            }
            return dockspace;
        }
    };

    namespace streams
    {
        AUIK_EXPORT const umbf::streams::Stream dockspace{DockspaceStreamAccess::read, DockspaceStreamAccess::write};
    } // namespace streams
} // namespace auik
