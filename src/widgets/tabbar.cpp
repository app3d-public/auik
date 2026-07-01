#include <auik/auik.hpp>
#include <auik/detail/depth.hpp>
#include <auik/detail/rect.hpp>
#include <auik/widgets/combobox.hpp>
#include <auik/widgets/detail/selectable.hpp>
#include <auik/widgets/tabbar.hpp>
#include <auik/widgets/window.hpp>
#include "../core/session_stream_utils.hpp"

#define AUIK_TAB_BAR_POPUP_ITEM_FALLBACK_HEIGHT 24.0f
#define AUIK_TAB_BAR_SCROLL_STEP                32.0f

namespace auik
{
    constexpr f32 AUIK_DROPDOWN_MIN_VISIBLE_ITEMS = 4.0f;

    struct DropdownPopupPlacement
    {
        bool place_above = false;
        bool need_scroll = false;
        f32 height = 0.0f;
        f32 y = 0.0f;
    };

    static inline amal::vec2 get_tab_popup_depth_range() { return detail::get_global_foreground_depth_range(); }

    static inline DropdownPopupPlacement resolve_dropdown_popup_placement(f32 control_y, f32 control_h, f32 desired_h,
                                                                          f32 item_h, u32 item_count,
                                                                          const amal::vec4 &viewport, f32 fallback_h)
    {
        const f32 gap = 0.0f;
        const f32 below_space = amal::max(viewport.y + viewport.w - (control_y + control_h + gap), 0.0f);
        const f32 above_space = amal::max(control_y - gap - viewport.y, 0.0f);
        const bool fits_below = desired_h <= below_space;
        const f32 safe_item_h = amal::max(item_h, 1.0f);
        const f32 below_visible_items = amal::floor(below_space / safe_item_h);
        const f32 min_visible_items = amal::min(AUIK_DROPDOWN_MIN_VISIBLE_ITEMS, static_cast<f32>(item_count));
        const bool place_above = !fits_below && below_visible_items < min_visible_items && above_space > below_space;
        const f32 available_h = place_above ? above_space : below_space;
        const bool need_scroll = desired_h > available_h;
        const f32 popup_h = need_scroll ? available_h : amal::max(desired_h, fallback_h);
        const f32 popup_y = place_above ? control_y - gap - popup_h + 1.0f : control_y + control_h + gap - 1.0f;
        return {place_above, need_scroll, popup_h, popup_y};
    }

    static inline bool should_own_full_clip_rect(const Tabbar &tabbar) { return tabbar.is_fixed() || !tabbar.parent(); }

    static inline bool is_content_width_constrained(const Tabbar &tabbar)
    { return tabbar.required_size().x > 0.0f && tabbar.size().x + 0.5f < tabbar.required_size().x; }

    static inline bool should_own_content_clip_rect(const Tabbar &tabbar)
    {
        return tabbar.scroll() || tabbar.clipped() || tabbar.is_fixed() || !tabbar.parent() ||
               is_content_width_constrained(tabbar);
    }

    static inline amal::vec2 snap_drag_visual_position(const amal::vec2 &position)
    { return {amal::round(position.x), amal::round(position.y)}; }

    static inline f32 dominant_scroll_axis(const amal::vec2 &delta)
    { return amal::abs(delta.x) > amal::abs(delta.y) ? delta.x : delta.y; }

    static inline TabbarFlags normalize_tab_bar_flags(TabbarFlags flags)
    {
        if (flags & TabbarFlagBits::scroll) flags &= ~TabbarFlagBits::popup;
        return flags;
    }

    static inline EventFlags make_tab_bar_event_flags(TabbarFlags flags)
    {
        flags = normalize_tab_bar_flags(flags);
        EventFlags out = EventFlagBits::click | EventFlagBits::hover;
        if (flags & TabbarFlagBits::popup) out |= EventFlagBits::focus;
        if (flags & TabbarFlagBits::scroll) out |= EventFlagBits::scroll;
        if (flags & TabbarFlagBits::movable) out |= EventFlagBits::drag;
        return out;
    }

    static inline void refresh_selection_owner(Tabbar &tabbar, StyleUpdateFlags flags)
    {
        auto &ctx = detail::get_context();
        ctx.dirty_flags |= DirtyFlagBits::redraw | DirtyFlagBits::hit_rect_update;

        if (flags & (StyleUpdateFlagBits::layout | StyleUpdateFlagBits::parent_layout))
        {
            if (Widget *target = resolve_parent_layout_update_target(&tabbar)) target->update_layout(false);
        }

        DrawReasonFlags reason = get_draw_reason_from_style_update(flags);
        if ((flags & StyleUpdateFlagBits::redraw) && reason == DrawReasonBits::none) reason = DrawReasonBits::external;
        tabbar.update_draw_commands(reason);
        detail::mark_host_refresh_request();
    }

    static inline void refresh_layout_owner(Tabbar &tabbar)
    {
        auto &ctx = detail::get_context();
        Widget *target = resolve_parent_layout_update_target(&tabbar);
        if (!target) target = &tabbar;
        if (target->clip_id() == 0xFFFFu)
        {
            ctx.dirty_flags |= DirtyFlagBits::redraw | DirtyFlagBits::hit_rect_update;
            detail::mark_host_refresh_request();
            return;
        }
        target->update_layout(false);
        target->update_draw_commands(DrawReasonBits::layout);
        ctx.dirty_flags |= DirtyFlagBits::redraw | DirtyFlagBits::hit_rect_update;
        detail::mark_host_refresh_request();
    }

    Tabbar::Tabbar(u32 id, acul::vector<acul::string> items, TabbarFlags tab_flags, amal::vec2 size,
                   WidgetFlags widget_flags, f32 tab_width, u32 tab_width_key, u32 item_style_tag,
                   u32 selected_item_style_tag, u32 popup_item_style_tag)
        : Tabbar(
              id,
              [&items]() {
                  acul::vector<StringView> views;
                  views.reserve(items.size());
                  for (const auto &item : items) views.push_back(StringView{item});
                  return views;
              }(),
              tab_flags, size, widget_flags, tab_width, tab_width_key, item_style_tag, selected_item_style_tag,
              popup_item_style_tag)
    {
    }

    Tabbar::Tabbar(u32 id, const acul::vector<StringView> &items, TabbarFlags tab_flags, amal::vec2 size,
                   WidgetFlags widget_flags, f32 tab_width, u32 tab_width_key, u32 item_style_tag,
                   u32 selected_item_style_tag, u32 popup_item_style_tag)
        : Widget(id, widget_flags, make_tab_bar_event_flags(tab_flags), {{0.0f, 0.0f}, size}, AUIK_TAG_TAB_BAR),
          _tab_flags(normalize_tab_bar_flags(tab_flags)),
          _tab_width(tab_width),
          _tab_width_key(tab_width_key)
    {
        _item_style_tag = item_style_tag;
        _selected_item_style_tag = selected_item_style_tag;
        _popup_item_style_tag = popup_item_style_tag;
        if (popup())
        {
            _overflow_button =
                acul::alloc<detail::PopupTrigger>(AUIK_STYLE_TAG_TABBAR_POPUP_BUTTON, AUIK_TAG_TABBAR_POPUP_BTN,
                                                  AUIK_ICON_CHEVRON_DOWN, AUIK_ICON_CHEVRON_UP, true);
            _overflow_button->set_update_target(this);
            _overflow_button->set_hit_id(make_element_id(id, AUIK_TAG_TABBAR_POPUP_BTN, 0u));
            ensure_overflow_icon_resources();

            _popup = acul::alloc<Window>(AUIK_TAG_TAB_BAR_POPUP, "", amal::rect{{0.0f, 0.0f}, {0.0f, 0.0f}},
                                         get_popup_window_flags(), WidgetFlagBits::hittable);
            _popup->get_rect().id.widget_id = id;
            _popup->set_window_style_tag(AUIK_STYLE_TAG_TAB_BAR_POPUP);
            _popup->set_focus_parent(this);
        }

        set_items(items);
    }

    Tabbar::~Tabbar()
    {
        if (_model_binding)
        {
            _model_binding->on_records = nullptr;
            _model_binding->on_field_change = nullptr;
            detach_model_binding(*_model_binding);
        }
        close_popup(false);
        if (_popup) acul::release(_popup);
        if (_overflow_button) acul::release(_overflow_button);
        for (auto *tab : _tabs) acul::release(tab);
        for (auto *button : _close_buttons) acul::release(button);
        _tabs.clear();
        _close_buttons.clear();
    }

    void Tabbar::rebuild_items()
    {
        if (_popup) _popup->clear_children();
        while (_close_buttons.size() < _tabs.size())
        {
            auto *button = acul::alloc<ImageButton>(AUIK_TAG_CLOSE_BUTTON, get_cached_image(AUIK_ICON_CLOSE),
                                                    amal::vec2{0.0f, 0.0f}, amal::vec2{0.0f, 0.0f},
                                                    WidgetFlagBits::visible | WidgetFlagBits::hittable,
                                                    AUIK_TAG_CLOSE_BUTTON);
            button->set_parent(this);
            button->get_rect().id.widget_id = id();
            button->set_focus_parent(this);
            button->update_style();
            _close_buttons.push_back(button);
        }
        while (_close_buttons.size() > _tabs.size())
        {
            acul::release(_close_buttons.back());
            _close_buttons.erase(_close_buttons.end() - 1);
        }

        if (_tabs.empty())
        {
            _selected_index = 0u;
            return;
        }
        if (_selected_index >= _tabs.size()) _selected_index = static_cast<u32>(_tabs.size() - 1u);
        u32 first_selected_index = static_cast<u32>(-1);
        for (u32 i = 0; i < _tabs.size(); ++i)
        {
            auto *tab = _tabs[i];
            if (!tab || !tab->selected()) continue;
            if (first_selected_index == static_cast<u32>(-1)) first_selected_index = i;
            else if (!multiple()) tab->set_selected(false);
        }
        if (first_selected_index == static_cast<u32>(-1))
        {
            if (auto_select_first_item() && _selected_index < _tabs.size() && _tabs[_selected_index])
            {
                _tabs[_selected_index]->set_selected(true);
                first_selected_index = _selected_index;
            }
        }
        else _selected_index = first_selected_index;

        const auto transition = detail::get_widget_style_selector_transition(id());
        for (u32 i = 0; i < _tabs.size(); ++i)
        {
            const u32 element_id = _element_ids[i];
            auto *tab = _tabs[i];
            tab->get_rect().id.element_id = element_id;
            _close_buttons[i]->get_rect().id.widget_id = id();
            _close_buttons[i]->get_rect().id.element_id = element_id;

            if (_popup)
            {
                auto *popup_item = acul::alloc<detail::Selectable>(
                    make_element_id(id(), _popup_item_style_tag, element_id), tab->source_text(), false,
                    amal::vec2{0.0f, 0.0f}, _popup, detail::get_selectable_item_flags());
                popup_item->set_style_tag(_popup_item_style_tag);
                popup_item->set_selected_style_tag(_selected_item_style_tag);
                popup_item->set_focus_parent(_popup);
                _popup->add_child(popup_item);
            }
            update_item_state(i, transition);
        }
    }

    void Tabbar::set_items(acul::vector<acul::string> items)
    {
        acul::vector<StringView> views;
        views.reserve(items.size());
        for (const auto &item : items) views.push_back(StringView{item});
        set_items(views);
    }

    void Tabbar::set_model_binding(ModelBinding *binding)
    {
        if (_model_binding)
        {
            _model_binding->on_records = nullptr;
            _model_binding->on_field_change = nullptr;
            detach_model_binding(*_model_binding);
        }
        _model_binding = binding;
        if (!_model_binding) return;

        auto rebuild = [this]() {
            rebuild_from_model_binding();
            refresh_layout_owner(*this);
        };
        _model_binding->on_records = [rebuild](const ModelRecordsEvent &) { rebuild(); };
        _model_binding->on_field_change = [rebuild](ModelRecordID, ModelFieldID) { rebuild(); };
        attach_model_binding(*_model_binding);
        rebuild_model_binding_records(*_model_binding);
        rebuild_from_model_binding();
    }

    void Tabbar::rebuild_from_model_binding()
    {
        if (!_model_binding || !is_model_binding_valid(*_model_binding))
        {
            set_items(acul::vector<StringView>{});
            return;
        }

        auto *model = find_model(_model_binding->db, _model_binding->model_id);
        acul::vector<acul::string> items;
        if (model) items.reserve(_model_binding->records.size());
        if (model)
        {
            for (ModelRecordID record_id : _model_binding->records)
            {
                acul::string text{};
                read_model_binding_value(*_model_binding, record_id, 1u, text);
                items.push_back(std::move(text));
            }
        }
        set_items(std::move(items));
        update_depth(depth_range());
    }

    void Tabbar::set_items(const acul::vector<StringView> &items)
    {
        if (!_tabs.empty() || !_close_buttons.empty() || _overflow_button)
        {
            invalidate_draw_commands(DrawReasonBits::layout);
            reset_draw_records();
        }
        if (_popup) _popup->clear_children();
        for (auto *tab : _tabs) acul::release(tab);
        for (auto *button : _close_buttons) acul::release(button);
        _tabs.clear();
        _close_buttons.clear();
        _element_ids.clear();
        _selected_index = 0u;
        _next_element_id = 1u;
        _drag_element_id = 0u;
        _drag_preview_index = 0u;
        _last_selected_element_id = 0u;
        for (u32 i = 0; i < items.size(); ++i)
        {
            const u32 element_id = _next_element_id++;
            _element_ids.push_back(element_id);
            auto *tab = acul::alloc<detail::Selectable>(make_element_id(id(), _item_style_tag, element_id), items[i],
                                                        false, amal::vec2{0.0f, 0.0f}, this,
                                                        detail::get_selectable_item_flags());
            tab->set_style_tag(_item_style_tag);
            tab->set_selected_style_tag(_selected_item_style_tag);
            tab->set_focus_parent(this);
            tab->update_style();
            _tabs.push_back(tab);
        }
        if (!_tabs.empty() && auto_select_first_item()) _tabs[0]->set_selected(true);
        rebuild_items();
    }

    void Tabbar::set_style_tag(u32 tag_id)
    {
        if (tag_id == 0u) tag_id = AUIK_STYLE_TAG_GLOBAL;
        if (_style.tag_id == tag_id) return;
        _style = {Theme::STYLE_ID_INVALID, tag_id};
        if (tag_id != AUIK_STYLE_TAG_GLOBAL) set_rect_tag_id(tag_id);
    }

    u32 Tabbar::selected_index() const
    {
        for (u32 i = 0; i < _tabs.size(); ++i)
            if (_tabs[i] && _tabs[i]->selected()) return i;
        return 0u;
    }

    u32 Tabbar::selected_id() const
    {
        const u32 index = selected_index();
        if (index >= _tabs.size() || !_tabs[index] || !_tabs[index]->selected()) return 0u;
        return index < _element_ids.size() ? _element_ids[index] : 0u;
    }

    acul::vector<u32> Tabbar::selected_ids() const
    {
        acul::vector<u32> out;
        for (u32 i = 0; i < _tabs.size() && i < _element_ids.size(); ++i)
            if (_tabs[i] && _tabs[i]->selected()) out.push_back(_element_ids[i]);
        return out;
    }

    bool Tabbar::is_selected(u32 element_id) const
    {
        const u32 index = find_index_by_element_id(element_id);
        return index < _tabs.size() && _tabs[index] && _tabs[index]->selected();
    }

    void Tabbar::set_selected(u32 element_id)
    {
        const u32 index = find_index_by_element_id(element_id);
        if (index >= _tabs.size()) return;
        const u32 prev_selected = selected_id();
        const auto prev_selected_ids = selected_ids();
        if (prev_selected == element_id && prev_selected_ids.size() == 1u) return;
        if (prev_selected != 0u && prev_selected != element_id) _last_selected_element_id = prev_selected;
        _selected_index = index;
        for (u32 i = 0; i < _tabs.size(); ++i)
            if (_tabs[i]) _tabs[i]->set_selected(i == index);
        if (mark_changed()) return;
        const auto transition = detail::get_widget_style_selector_transition(id());
        StyleUpdateFlags flags = StyleUpdateFlagBits::none;
        for (u32 selected_id : prev_selected_ids)
        {
            if (selected_id == element_id) continue;
            flags |= update_item_state(find_index_by_element_id(selected_id), transition);
        }
        flags |= update_item_state(index, transition);
        refresh_selection_owner(*this, flags);
    }

    void Tabbar::set_selected_silent(u32 element_id)
    {
        const u32 index = find_index_by_element_id(element_id);
        if (index >= _tabs.size()) return;
        const u32 prev_selected = selected_id();
        const auto prev_selected_ids = selected_ids();
        if (prev_selected != 0u && prev_selected != element_id) _last_selected_element_id = prev_selected;
        _selected_index = index;
        for (u32 i = 0; i < _tabs.size(); ++i)
            if (_tabs[i]) _tabs[i]->set_selected(i == index);
        const auto transition = detail::get_widget_style_selector_transition(id());
        for (u32 selected_id : prev_selected_ids)
        {
            if (selected_id == element_id) continue;
            update_item_state(find_index_by_element_id(selected_id), transition);
        }
        update_item_state(index, transition);
    }

    void Tabbar::set_selected(const acul::vector<u32> &element_ids)
    {
        const auto prev_selected_ids = selected_ids();
        for (auto *tab : _tabs)
            if (tab) tab->set_selected(false);
        for (u32 element_id : element_ids)
        {
            const u32 index = find_index_by_element_id(element_id);
            if (index >= _tabs.size() || !_tabs[index]) continue;
            _tabs[index]->set_selected(true);
            _selected_index = index;
            if (!multiple()) break;
        }
        const auto next_selected_ids = selected_ids();
        if (next_selected_ids == prev_selected_ids) return;
        if (!next_selected_ids.empty()) _last_selected_element_id = next_selected_ids[0];
        if (mark_changed()) return;
        const auto transition = detail::get_widget_style_selector_transition(id());
        StyleUpdateFlags flags = StyleUpdateFlagBits::none;
        auto update_changed = [&](u32 element_id) {
            if (element_id == 0u) return;
            const u32 index = find_index_by_element_id(element_id);
            if (index < _tabs.size()) flags |= update_item_state(index, transition);
        };
        for (u32 element_id : prev_selected_ids) update_changed(element_id);
        for (u32 element_id : next_selected_ids)
        {
            bool already_updated = false;
            for (u32 prev_id : prev_selected_ids)
            {
                if (prev_id != element_id) continue;
                already_updated = true;
                break;
            }
            if (!already_updated) update_changed(element_id);
        }
        refresh_selection_owner(*this, flags);
    }

    void Tabbar::set_tab_width(f32 value)
    {
        if (_tab_width == value) return;
        _tab_width = value;
    }

    void Tabbar::set_tab_width_key(u32 key)
    {
        if (_tab_width_key == key) return;
        _tab_width_key = key;
    }

    StyleState Tabbar::resolve_tab_item_state(u32 index, const detail::WidgetStyleSelectorTransition &transition) const
    {
        if (index >= _element_ids.size()) return StyleState::normal;
        if (transition.current_id.widget_id == id() && transition.current_id.tag_id == _item_style_tag &&
            transition.current_id.element_id == _element_ids[index])
            return transition.current_state;
        return StyleState::normal;
    }

    StyleUpdateFlags Tabbar::update_item_state(u32 index, const detail::WidgetStyleSelectorTransition &transition)
    {
        StyleUpdateFlags out = update_tab_item_style(index, transition);
        if (_popup && index < _popup->children.size()) out |= update_popup_item_style(index, transition);
        return out;
    }

    StyleUpdateFlags Tabbar::update_tab_item_style(u32 index, const detail::WidgetStyleSelectorTransition &transition)
    {
        if (index >= _tabs.size()) return StyleUpdateFlagBits::none;
        auto *tab = _tabs[index];
        if (!tab) return StyleUpdateFlagBits::none;
        const bool selected = index < _element_ids.size() && is_selected(_element_ids[index]);
        tab->set_style_state(resolve_tab_item_state(index, transition));
        tab->set_selected(selected);
        return tab->update_style();
    }

    StyleUpdateFlags Tabbar::update_close_button_style(u32 index,
                                                       const detail::WidgetStyleSelectorTransition &transition)
    {
        if (index >= _close_buttons.size() || !_close_buttons[index]) return StyleUpdateFlagBits::none;
        StyleState close_state = StyleState::normal;
        if (transition.current_id.tag_id == AUIK_TAG_CLOSE_BUTTON && index < _element_ids.size() &&
            transition.current_id.element_id == _element_ids[index])
            close_state = transition.current_state;
        _close_buttons[index]->set_style_state(close_state);
        return _close_buttons[index]->update_style();
    }

    StyleUpdateFlags Tabbar::update_popup_item_style(u32 index, const detail::WidgetStyleSelectorTransition &transition)
    {
        if (!_popup || index >= _popup->children.size()) return StyleUpdateFlagBits::none;
        auto *child = static_cast<detail::Selectable *>(_popup->children[index]);
        if (!child) return StyleUpdateFlagBits::none;
        const bool selected = index < _element_ids.size() && is_selected(_element_ids[index]);
        StyleState state = StyleState::normal;
        if (transition.current_id.tag_id == _popup_item_style_tag && index < _element_ids.size() &&
            transition.current_id.element_id == _element_ids[index])
            state = transition.current_state;
        child->set_style_state(state);
        child->set_selected(selected);
        return child->update_style();
    }

    StyleUpdateFlags Tabbar::update_style()
    {
        const u32 parent_id = parent() ? parent()->id() : 0u;
        StyleUpdateFlags out = resolve_style_selector(_style, id(), parent_id, style_state());
        const auto transition = detail::get_widget_style_selector_transition(id());
        const bool transition_targets_self =
            transition.current_id.widget_id == id() || transition.prev_id.widget_id == id();
        const bool transition_targets_item =
            transition.current_id.tag_id == _item_style_tag || transition.prev_id.tag_id == _item_style_tag;
        const bool transition_targets_close =
            transition.current_id.tag_id == AUIK_TAG_CLOSE_BUTTON || transition.prev_id.tag_id == AUIK_TAG_CLOSE_BUTTON;
        const bool transition_targets_popup_item =
            transition.current_id.tag_id == _popup_item_style_tag || transition.prev_id.tag_id == _popup_item_style_tag;
        const bool transition_targets_overflow = transition.current_id.tag_id == AUIK_TAG_TABBAR_POPUP_BTN ||
                                                 transition.prev_id.tag_id == AUIK_TAG_TABBAR_POPUP_BTN;
        const bool local_hover_transition =
            transition_targets_self && (transition_targets_item || transition_targets_close ||
                                        transition_targets_popup_item || transition_targets_overflow);

        if (_overflow_button)
        {
            StyleState overflow_state = _open ? StyleState::focus : StyleState::normal;
            if (transition.current_id.tag_id == AUIK_TAG_TABBAR_POPUP_BTN)
                overflow_state = _open ? StyleState::focus : transition.current_state;
            _overflow_button->set_style_state(overflow_state);
            out |= _overflow_button->update_style(AUIK_TAG_TABBAR_POPUP_BTN, id(), overflow_state);
        }

        if (local_hover_transition)
        {
            const u32 prev_index = find_index_by_element_id(transition.prev_id.element_id);
            const u32 current_index = find_index_by_element_id(transition.current_id.element_id);
            if (transition_targets_item)
            {
                out |= update_tab_item_style(prev_index, transition);
                if (current_index != prev_index) out |= update_tab_item_style(current_index, transition);
            }
            if (transition_targets_close)
            {
                out |= update_close_button_style(prev_index, transition);
                if (current_index != prev_index) out |= update_close_button_style(current_index, transition);
            }
        }
        else
        {
            for (u32 i = 0; i < _tabs.size(); ++i)
            {
                out |= update_tab_item_style(i, transition);
                out |= update_close_button_style(i, transition);
            }
        }

        if (_popup)
        {
            out |= _popup->update_style();
            if (local_hover_transition && transition_targets_popup_item)
            {
                const u32 prev_index = find_index_by_element_id(transition.prev_id.element_id);
                const u32 current_index = find_index_by_element_id(transition.current_id.element_id);
                out |= update_popup_item_style(prev_index, transition);
                if (current_index != prev_index) out |= update_popup_item_style(current_index, transition);
            }
            else
            {
                for (u32 i = 0; i < _popup->children.size(); ++i) out |= update_popup_item_style(i, transition);
            }
        }
        const f32 next_tab_width = resolve_tab_width();
        if (_resolved_tab_width != next_tab_width)
        {
            _resolved_tab_width = next_tab_width;
            out |= StyleUpdateFlagBits::layout;
        }
        return out;
    }

    amal::vec2 Tabbar::measure_overflow_size()
    {
        if (!_overflow_button) return {0.0f, 0.0f};
        ensure_overflow_icon_resources();
        _overflow_button->update_layout_min_size({0.0f, 0.0f}, true);
        return _overflow_button->required_size();
    }

    void Tabbar::ensure_overflow_icon_resources()
    {
        if (!_overflow_button) return;
        _overflow_button->set_icons(AUIK_ICON_CHEVRON_DOWN, AUIK_ICON_CHEVRON_UP);
    }

    void Tabbar::update_overflow_button_style()
    {
        if (!_overflow_button) return;
        const StyleState state = _open ? StyleState::focus : StyleState::normal;
        _overflow_button->set_style_state(state);
        _overflow_button->update_style(AUIK_TAG_TABBAR_POPUP_BTN, id(), state);
    }

    f32 Tabbar::resolve_tab_width() const
    {
        if (_tab_width_key != 0u)
        {
            const f32 width = get_theme()->get_var<f32>(_tab_width_key);
            return width > 0.0f ? width : 0.0f;
        }
        return _tab_width > 0.0f ? _tab_width : 0.0f;
    }

    amal::vec2 Tabbar::resolve_tab_required_size(u32 index)
    {
        if (index >= _tabs.size() || !_tabs[index]) return {0.0f, 0.0f};
        auto *tab = _tabs[index];
        const f32 fixed_tab_width = resolve_tab_width();
        tab->set_layout_size({fixed_tab_width, 0.0f});
        tab->update_layout_min_size();
        amal::vec2 required = tab->required_size();
        if (fixed_tab_width > 0.0f) required.x = fixed_tab_width;
        if (closable() && index < _close_buttons.size())
        {
            auto *close_button = _close_buttons[index];
            if (close_button)
            {
                close_button->set_layout_size({0.0f, 0.0f});
                close_button->update_layout_min_size();
                const amal::vec2 close_required = close_button->required_size();
                if (fixed_tab_width <= 0.0f) required.x += close_required.x;
                required.y = amal::max(required.y, close_required.y);
            }
        }
        return required;
    }

    void Tabbar::update_layout_min_size()
    {
        const auto &style = get_theme()->get_style(_style.id);
        const amal::vec4 margin = style.margin();
        const amal::vec4 padding = style.padding();
        const f32 inline_spacing = amal::max(style.inline_spacing(), 0.0f);
        amal::vec2 tabs_size{0.0f, 0.0f};
        for (u32 i = 0; i < _tabs.size(); ++i)
        {
            const amal::vec2 required = resolve_tab_required_size(i);
            if (required.x <= 0.0f && required.y <= 0.0f) continue;
            if (tabs_size.x > 0.0f) tabs_size.x += inline_spacing;
            tabs_size.x += required.x;
            tabs_size.y = amal::max(tabs_size.y, required.y);
        }

        if (popup())
        {
            const amal::vec2 overflow_size = measure_overflow_size();
            tabs_size.y = amal::max(tabs_size.y, overflow_size.y);
        }

        amal::vec2 min_size = {is_width_fixed() ? size().x : 0.0f, is_height_fixed() ? size().y : 0.0f};
        if (min_size.x <= 0.0f)
        {
            const f32 visual_min_width =
                content_width_fit() ? tabs_size.x : (popup() ? measure_overflow_size().x : 0.0f);
            min_size.x = visual_min_width + padding.x + padding.z;
        }
        if (min_size.y <= 0.0f) min_size.y = tabs_size.y + padding.y + padding.w;
        set_required_size({min_size.x + margin.x + margin.z, min_size.y + margin.y + margin.w});
    }

    void Tabbar::update_layout(bool min_size_known)
    {
        if (!min_size_known) update_layout_min_size();

        const auto &style = get_theme()->get_style(_style.id);
        const amal::vec4 margin = style.margin();
        const amal::vec4 padding = style.padding();
        const f32 inline_spacing = amal::max(style.inline_spacing(), 0.0f);
        const amal::vec2 layout_origin = position();
        const amal::vec2 min_required = required_size();
        amal::vec2 widget_size = {amal::max(size().x - margin.x - margin.z, 0.0f),
                                  amal::max(size().y - margin.y - margin.w, 0.0f)};
        const amal::vec2 min_inner = {amal::max(min_required.x - margin.x - margin.z, 0.0f),
                                      amal::max(min_required.y - margin.y - margin.w, 0.0f)};
        const bool width_constrained_by_layout =
            parent() && size().x > 0.0f && size().x + 0.5f < min_required.x;
        if (!is_width_fixed())
        {
            if (!width_constrained_by_layout) widget_size.x = amal::max(widget_size.x, min_inner.x);
        }
        else widget_size.x = amal::max(widget_size.x, min_inner.x);
        widget_size.y = amal::max(widget_size.y, min_inner.y);

        set_position({layout_origin.x + margin.x, layout_origin.y + margin.y});
        set_layout_size(widget_size);
        Widget::update_layout(true);

        if (detail::is_fast_layout_update() && !popup() && !scroll() && !movable() && !closable())
        {
            const f32 content_x = position().x + padding.x;
            const f32 content_y = position().y + padding.y;
            const f32 content_h = amal::max(size().y - padding.y - padding.w, 0.0f);
            const bool own_full_clip = should_own_full_clip_rect(*this);
            const amal::vec4 parent_clip = get_layout_parent_clip_rect();
            const amal::vec4 full_clip =
                detail::intersect_rects(parent_clip, {position().x, position().y, size().x, size().y});
            const f32 tabs_clip_right = position().x + size().x - padding.z;
            const amal::vec4 tabs_clip = detail::intersect_rects(
                parent_clip, {position().x, position().y, amal::max(tabs_clip_right - position().x, 0.0f), size().y});

            if (own_full_clip)
            {
                if (_full_clip_id == 0xFFFFu) _full_clip_id = push_clip_rect(full_clip);
                else update_clip_rect(_full_clip_id, full_clip);
                set_clip_id(_full_clip_id);
            }
            else
            {
                _full_clip_id = 0xFFFFu;
                set_clip_id(get_layout_parent_clip_id());
            }

            if (should_own_content_clip_rect(*this))
            {
                if (_content_clip_id == 0xFFFFu) _content_clip_id = push_clip_rect(tabs_clip);
                else update_clip_rect(_content_clip_id, tabs_clip);
            }
            else _content_clip_id = 0xFFFFu;

            f32 cursor_x = content_x;
            _visible_count = static_cast<u32>(_tabs.size());
            _overflow_start = static_cast<u32>(_tabs.size());
            for (u32 i = 0; i < _tabs.size(); ++i)
            {
                auto *tab = _tabs[i];
                if (!tab) continue;
                if (i > 0u) cursor_x += inline_spacing;
                const amal::vec2 required = resolve_tab_required_size(i);
                tab->set_visible();
                tab->sync_widget_flags();
                tab->set_layout_size({required.x, tab->size().y});
                tab->set_position({cursor_x, content_y + amal::max((content_h - required.y) * 0.5f, 0.0f)});
                tab->update_layout(true);
                cursor_x += required.x;
            }
            for (auto *button : _close_buttons)
            {
                if (!button) continue;
                button->unset_visible();
                button->sync_widget_flags();
            }
            if (_overflow_button) _overflow_button->set_open(false);
            close_popup();
            return;
        }

        const f32 content_x = position().x + padding.x;
        const f32 content_y = position().y + padding.y;
        const f32 content_w = amal::max(size().x - padding.x - padding.z, 0.0f);
        const f32 content_h = amal::max(size().y - padding.y - padding.w, 0.0f);
        const amal::vec2 overflow_size = popup() ? measure_overflow_size() : amal::vec2{0.0f, 0.0f};
        const f32 popup_reserved = popup() ? overflow_size.x : 0.0f;

        const bool own_full_clip = should_own_full_clip_rect(*this);
        const amal::vec4 parent_clip = get_layout_parent_clip_rect();
        const amal::vec4 full_clip =
            detail::intersect_rects(parent_clip, {position().x, position().y, size().x, size().y});
        const f32 tabs_clip_right = position().x + size().x - padding.z - popup_reserved;
        const amal::vec4 tabs_clip = detail::intersect_rects(
            parent_clip, {position().x, position().y, amal::max(tabs_clip_right - position().x, 0.0f), size().y});
        if (own_full_clip)
        {
            if (_full_clip_id == 0xFFFFu) _full_clip_id = push_clip_rect(full_clip);
            else update_clip_rect(_full_clip_id, full_clip);
            set_clip_id(_full_clip_id);
        }
        else
        {
            _full_clip_id = 0xFFFFu;
            set_clip_id(get_layout_parent_clip_id());
        }
        if (should_own_content_clip_rect(*this))
        {
            if (_content_clip_id == 0xFFFFu) _content_clip_id = push_clip_rect(tabs_clip);
            else update_clip_rect(_content_clip_id, tabs_clip);
        }
        else _content_clip_id = 0xFFFFu;
        f32 cursor_x = content_x - (scroll() ? _scroll_offset : 0.0f);
        f32 total_w = 0.0f;
        _visible_count = 0u;
        _overflow_start = static_cast<u32>(_tabs.size());
        const f32 visible_tabs_right = content_x + amal::max(content_w - popup_reserved, 0.0f);

        const u32 drag_index = find_index_by_element_id(_drag_element_id);
        for (u32 i = 0; i < _tabs.size(); ++i)
        {
            auto *tab = _tabs[i];
            if (!tab) continue;
            if (i > 0u) cursor_x += inline_spacing;
            amal::vec2 required = resolve_tab_required_size(i);
            amal::vec2 close_required{0.0f, 0.0f};
            auto *close_button = i < _close_buttons.size() ? _close_buttons[i] : nullptr;
            if (close_button && closable()) { close_required = close_button->required_size(); }
            const bool fits_popup = cursor_x + required.x <= visible_tabs_right;
            const bool visible = !popup() || fits_popup;
            if (!visible && _overflow_start == _tabs.size()) _overflow_start = i;
            if (visible) ++_visible_count;
            if (!popup() || visible) tab->set_visible();
            else tab->unset_visible();
            tab->sync_widget_flags();
            if (close_button)
            {
                if (closable() && visible) close_button->set_visible();
                else close_button->unset_visible();
                close_button->sync_widget_flags();
            }
            tab->set_layout_size({required.x, tab->size().y});
            const amal::vec2 tab_base_pos{cursor_x, content_y + amal::max((content_h - required.y) * 0.5f, 0.0f)};
            amal::vec2 drag_visual_offset{0.0f, 0.0f};
            if (movable() && i == drag_index && (_drag_offset.x != 0.0f || _drag_offset.y != 0.0f))
            {
                const amal::vec2 visual_pos = snap_drag_visual_position(tab_base_pos + _drag_offset);
                drag_visual_offset = visual_pos - tab_base_pos;
                _drag_applied_offset = drag_visual_offset;
            }
            tab->set_position(tab_base_pos + drag_visual_offset);
            tab->update_layout(true);
            if (close_button && close_button->is_visible())
            {
                const f32 close_x = cursor_x + required.x - close_required.x;
                const f32 close_y = content_y + amal::max((content_h - close_required.y) * 0.5f, 0.0f);
                close_button->set_position(amal::vec2{close_x, close_y} + drag_visual_offset);
                close_button->update_layout(true);
            }
            cursor_x += required.x;
            total_w += required.x;
            if (i > 0u) total_w += inline_spacing;
        }

        _content_width = total_w;
        const f32 old_scroll_offset = _scroll_offset;
        clamp_scroll_offset();
        if (scroll() && old_scroll_offset != _scroll_offset)
        {
            const f32 dx = old_scroll_offset - _scroll_offset;
            for (auto *tab : _tabs)
                if (tab) tab->translate({dx, 0.0f});
            for (auto *button : _close_buttons)
                if (button) button->translate({dx, 0.0f});
            if (movable() && drag_index < _tabs.size())
            {
                _drag_offset.x += dx;
                _drag_applied_offset.x += dx;
            }
            detail::get_context().dirty_flags |= DirtyFlagBits::hit_rect_update;
        }

        if (popup() && _overflow_button && _overflow_start < _tabs.size())
        {
            _overflow_button->update_layout({{position().x + size().x - padding.z - overflow_size.x,
                                              content_y + amal::max((content_h - overflow_size.y) * 0.5f, 0.0f)},
                                             overflow_size},
                                            clip_id());
        }
        else
        {
            if (_overflow_button) _overflow_button->set_open(false);
            close_popup();
        }

        if (_open) update_popup_layout();
        else if (_popup)
        {
            _popup->unset_visible();
            _popup->sync_widget_flags();
        }
    }

    void Tabbar::translate(const amal::vec2 &delta)
    {
        if (delta.x == 0.0f && delta.y == 0.0f) return;
        Widget::translate(delta);
        if (_full_clip_id != 0xFFFFu || _content_clip_id != 0xFFFFu)
        {
            const auto &style = get_theme()->get_style(_style.id);
            const amal::vec4 padding = style.padding();
            const amal::vec2 overflow_size = popup() ? measure_overflow_size() : amal::vec2{0.0f, 0.0f};
            const f32 popup_reserved = popup() ? overflow_size.x : 0.0f;
            const amal::vec4 parent_clip = get_layout_parent_clip_rect();
            const amal::vec4 full_clip =
                detail::intersect_rects(parent_clip, {position().x, position().y, size().x, size().y});
            const f32 tabs_clip_right = position().x + size().x - padding.z - popup_reserved;
            const amal::vec4 tabs_clip = detail::intersect_rects(
                parent_clip, {position().x, position().y, amal::max(tabs_clip_right - position().x, 0.0f), size().y});
            if (_full_clip_id != 0xFFFFu) update_clip_rect(_full_clip_id, full_clip);
            else set_clip_id(get_layout_parent_clip_id());
            if (_content_clip_id != 0xFFFFu) update_clip_rect(_content_clip_id, tabs_clip);
        }
        else set_clip_id(get_layout_parent_clip_id());
        for (auto *tab : _tabs)
            if (tab) tab->translate(delta);
        for (auto *button : _close_buttons)
            if (button) button->translate(delta);
        if (_overflow_button) _overflow_button->translate(delta);
        if (_open && _popup) static_cast<Widget *>(_popup)->translate(delta);
    }

    void Tabbar::reset_clip_rect_records()
    {
        Widget::reset_clip_rect_records();
        _full_clip_id = 0xFFFFu;
        _content_clip_id = 0xFFFFu;
        for (auto *tab : _tabs)
            if (tab) tab->reset_clip_rect_records();
        for (auto *button : _close_buttons)
            if (button) button->reset_clip_rect_records();
        if (_popup) _popup->reset_clip_rect_records();
    }

    void Tabbar::rebuild_clip_rects()
    {
        const auto &style = get_theme()->get_style(_style.id);
        const amal::vec4 padding = style.padding();
        const amal::vec2 overflow_size = popup() ? measure_overflow_size() : amal::vec2{0.0f, 0.0f};
        const f32 popup_reserved = popup() ? overflow_size.x : 0.0f;
        const bool own_full_clip = should_own_full_clip_rect(*this);
        const amal::vec4 parent_clip = get_layout_parent_clip_rect();
        const amal::vec4 full_clip =
            detail::intersect_rects(parent_clip, {position().x, position().y, size().x, size().y});
        const f32 tabs_clip_right = position().x + size().x - padding.z - popup_reserved;
        const amal::vec4 tabs_clip = detail::intersect_rects(
            parent_clip, {position().x, position().y, amal::max(tabs_clip_right - position().x, 0.0f), size().y});
        if (own_full_clip)
        {
            if (_full_clip_id == 0xFFFFu) _full_clip_id = push_clip_rect(full_clip);
            else update_clip_rect(_full_clip_id, full_clip);
            set_clip_id(_full_clip_id);
        }
        else
        {
            _full_clip_id = 0xFFFFu;
            set_clip_id(get_layout_parent_clip_id());
        }
        if (should_own_content_clip_rect(*this))
        {
            if (_content_clip_id == 0xFFFFu) _content_clip_id = push_clip_rect(tabs_clip);
            else update_clip_rect(_content_clip_id, tabs_clip);
        }
        else _content_clip_id = 0xFFFFu;
        for (auto *tab : _tabs)
            if (tab) tab->rebuild_clip_rects();
        for (auto *button : _close_buttons)
            if (button) button->rebuild_clip_rects();
        if (_overflow_button) _overflow_button->rebuild_clip_rects(clip_id());
        if (_popup) _popup->rebuild_clip_rects();
    }

    void Tabbar::reset_draw_records()
    {
        Widget::reset_draw_records();
        for (auto *tab : _tabs)
            if (tab) tab->reset_draw_records();
        for (auto *button : _close_buttons)
            if (button) button->reset_draw_records();
        if (_overflow_button) _overflow_button->reset_draw_records();
        if (_popup) _popup->reset_draw_records();
    }

    void Tabbar::update_depth(const amal::vec2 &depth_range)
    {
        Widget::update_depth(depth_range);
        const u32 tab_layer_requirement = get_depth_requirement() > 1u ? get_depth_requirement() - 1u : 1u;
        DepthCursor cursor(this->depth_range(), tab_layer_requirement + 1u);
        const amal::vec2 tab_range = cursor.next(tab_layer_requirement);
        const amal::vec2 control_range = cursor.next(1u);
        for (u32 i = 0; i < _tabs.size(); ++i)
        {
            auto *tab = _tabs[i];
            if (tab) tab->update_depth(tab_range);
            if (i < _close_buttons.size() && _close_buttons[i]) { _close_buttons[i]->update_depth(control_range); }
        }
        if (_overflow_button) _overflow_button->update_depth(control_range);
        if (_popup) _popup->update_depth(get_tab_popup_depth_range());
        if (_drag_element_id != 0u) update_drag_depth();
    }

    u32 Tabbar::get_depth_requirement() const
    {
        u32 tab_requirement = 1u;
        for (auto *tab : _tabs)
            if (tab) tab_requirement = amal::max(tab_requirement, tab->get_depth_requirement());
        for (auto *button : _close_buttons)
            if (button) tab_requirement = amal::max(tab_requirement, button->get_depth_requirement());
        if (_overflow_button) tab_requirement = amal::max(tab_requirement, _overflow_button->get_depth_requirement());
        return tab_requirement + 1u;
    }

    void Tabbar::back_hit_depth()
    {
        Widget::back_hit_depth();
        for (auto *tab : _tabs)
            if (tab) tab->back_hit_depth();
        for (auto *button : _close_buttons)
            if (button) button->back_hit_depth();
        if (_overflow_button) _overflow_button->back_hit_depth();
        if (_popup) _popup->back_hit_depth();
    }

    void Tabbar::restore_hit_depth()
    {
        Widget::restore_hit_depth();
        for (auto *tab : _tabs)
            if (tab) tab->restore_hit_depth();
        for (auto *button : _close_buttons)
            if (button) button->restore_hit_depth();
        if (_overflow_button) _overflow_button->restore_hit_depth();
        if (_popup) _popup->restore_hit_depth();
    }

    u16 Tabbar::get_layout_parent_clip_id() const { return parent() ? parent()->content_clip_id() : clip_id(); }

    amal::vec4 Tabbar::get_layout_parent_clip_rect() const
    {
        return parent() ? parent()->get_content_clip_rect()
                        : amal::vec4{position().x, position().y, size().x, size().y};
    }

    bool Tabbar::draw_transition_targets(DrawCtx &ctx)
    {
        const auto transition = detail::get_widget_style_selector_transition(id());
        bool emitted = false;
        auto draw_target = [&](const ElementID &element_id) {
            if (!element_id || element_id.widget_id != id()) return;
            if (element_id.tag_id == _item_style_tag)
            {
                const u32 index = find_index_by_element_id(element_id.element_id);
                if (index >= _tabs.size() || !_tabs[index] || !_tabs[index]->is_visible()) return;
                DrawCtx tab_ctx = ctx;
                _tabs[index]->draw_local(tab_ctx);
                emitted = true;
                return;
            }
            if (element_id.tag_id == AUIK_TAG_CLOSE_BUTTON)
            {
                const u32 index = find_index_by_element_id(element_id.element_id);
                if (index >= _close_buttons.size() || !_close_buttons[index] || !_close_buttons[index]->is_visible())
                    return;
                DrawCtx close_ctx = ctx;
                _close_buttons[index]->draw_local(close_ctx);
                emitted = true;
                return;
            }
            if (element_id.tag_id == AUIK_TAG_TABBAR_POPUP_BTN && _overflow_button && _overflow_start < _tabs.size())
            {
                ensure_overflow_icon_resources();
                _overflow_button->draw(ctx, can_emit_hit(ctx));
                emitted = true;
                return;
            }
            if (element_id.tag_id == _popup_item_style_tag && _popup)
            {
                const u32 index = find_index_by_element_id(element_id.element_id);
                if (index >= _popup->children.size()) return;
                auto *child = _popup->children[index];
                if (!child || !child->is_visible()) return;
                DrawCtx popup_ctx = ctx;
                child->draw_local(popup_ctx);
                emitted = true;
            }
        };
        draw_target(transition.prev_id);
        draw_target(transition.current_id);
        return emitted;
    }

    void Tabbar::draw(DrawCtx &ctx)
    {
        if (!(widget_flags & WidgetFlagBits::visible)) return;
        if ((!(ctx.reason & DrawReasonBits::record) && !(ctx.reason & DrawReasonBits::invalidate)) &&
            ctx.reason == DrawReasonBits::none && draw_transition_targets(ctx))
            return;

        const auto invalidate_hidden_widget = [&](Widget *widget) {
            if (!widget) return;
            if ((ctx.reason & DrawReasonBits::record))
            {
                widget->reset_draw_records();
                return;
            }
            DrawCtx invalidate_ctx = ctx;
            invalidate_ctx.reason |= DrawReasonBits::invalidate;
            invalidate_ctx.is_hit_allowed = false;
            widget->draw_local(invalidate_ctx);
        };

        for (u32 i = 0; i < _tabs.size(); ++i)
        {
            auto *tab = _tabs[i];
            if (!tab) continue;
            auto *close_button = i < _close_buttons.size() ? _close_buttons[i] : nullptr;
            if (!tab->is_visible())
            {
                invalidate_hidden_widget(tab);
                invalidate_hidden_widget(close_button);
                continue;
            }
            DrawCtx tab_ctx = ctx;
            tab->draw_local(tab_ctx);
            if (close_button && close_button->is_visible())
            {
                DrawCtx close_ctx = ctx;
                close_button->draw_local(close_ctx);
            }
            else invalidate_hidden_widget(close_button);
        }
        if (_overflow_button && _overflow_start < _tabs.size())
        {
            ensure_overflow_icon_resources();
            DrawCtx overflow_ctx = ctx;
            _overflow_button->draw(overflow_ctx, can_emit_hit(overflow_ctx));
        }
        else if (_overflow_button && !(ctx.reason & DrawReasonBits::record))
        {
            DrawCtx invalidate_ctx = ctx;
            invalidate_ctx.reason |= DrawReasonBits::invalidate;
            invalidate_ctx.is_hit_allowed = false;
            _overflow_button->draw(invalidate_ctx, false);
        }
        else if (_overflow_button && (ctx.reason & DrawReasonBits::record)) _overflow_button->reset_draw_records();
        if (_open && _popup)
        {
            DrawCtx popup_ctx = ctx;
            _popup->draw_local(popup_ctx);
        }
    }

    void Tabbar::on_focus(bool focused)
    {
        if (!focused)
        {
            add_render_command<detail::FocusEventTraits>(this, [this]() {
                close_popup();
                redraw_all_commands();
            });
            detail::mark_host_refresh_request();
        }
    }

    void Tabbar::on_hover(HoverState state)
    {
        bool changed = apply_hover_style_state(*this, state);
        if (changed)
        {
            update_style();
            update_draw_commands();
            detail::get_context().dirty_flags |= DirtyFlagBits::redraw;
        }
    }

    void Tabbar::handle_item_click(u32 element_id)
    {
        const u32 index = find_index_by_element_id(element_id);
        if (index >= _tabs.size()) return;
        const u32 prev_selected = selected_id();
        const auto prev_selected_ids = selected_ids();
        if (!multiple())
        {
            set_selected(element_id);
            return;
        }

        const auto mods = detail::get_context().io.active_mods;
        if (mods & KeyModeBits::alt)
        {
            if (_tabs[index]) _tabs[index]->set_selected(false);
        }
        else if (mods & KeyModeBits::control)
        {
            if (_tabs[index]) _tabs[index]->set_selected(!_tabs[index]->selected());
        }
        else
        {
            for (u32 i = 0; i < _tabs.size(); ++i)
                if (_tabs[i]) _tabs[i]->set_selected(i == index);
        }
        _selected_index = index;
        if (prev_selected != 0u && prev_selected != selected_id()) _last_selected_element_id = prev_selected;
        if (_last_selected_element_id == 0u && selected_id() != 0u) _last_selected_element_id = selected_id();
        const auto next_selected_ids = selected_ids();
        const auto transition = detail::get_widget_style_selector_transition(id());
        StyleUpdateFlags flags = StyleUpdateFlagBits::none;
        auto update_changed = [&](u32 changed_id) {
            if (changed_id == 0u) return;
            const u32 changed_index = find_index_by_element_id(changed_id);
            if (changed_index < _tabs.size()) flags |= update_item_state(changed_index, transition);
        };
        for (u32 selected_id : prev_selected_ids) update_changed(selected_id);
        for (u32 selected_id : next_selected_ids)
        {
            bool already_updated = false;
            for (u32 prev_id : prev_selected_ids)
            {
                if (prev_id != selected_id) continue;
                already_updated = true;
                break;
            }
            if (!already_updated) update_changed(selected_id);
        }
        refresh_selection_owner(*this, flags);
    }

    void Tabbar::close_item(u32 element_id)
    {
        const u32 index = find_index_by_element_id(element_id);
        if (index >= _tabs.size()) return;
        const bool closed_selected = _tabs[index] && _tabs[index]->selected();

        acul::release(_tabs[index]);
        _tabs.erase(_tabs.begin() + index);
        _element_ids.erase(_element_ids.begin() + index);
        if (_tabs.empty()) _selected_index = 0u;
        else
        {
            if (_selected_index >= _tabs.size()) _selected_index = static_cast<u32>(_tabs.size() - 1u);
            if (closed_selected && selected_id() == 0u && auto_select_first_item())
            {
                const u32 last_index = find_index_by_element_id(_last_selected_element_id);
                if (last_index < _tabs.size())
                {
                    _selected_index = last_index;
                    if (_tabs[_selected_index]) _tabs[_selected_index]->set_selected(true);
                }
                else
                {
                    const u32 fallback_index = index > 0u ? index - 1u : 0u;
                    _selected_index = amal::min(fallback_index, static_cast<u32>(_tabs.size() - 1u));
                    if (_tabs[_selected_index]) _tabs[_selected_index]->set_selected(true);
                }
            }
            else _selected_index = selected_index();
        }
        if (_drag_element_id == element_id) end_drag();
        rebuild_items();
        mark_changed();
        refresh_layout_owner(*this);
    }

    u32 Tabbar::find_index_by_element_id(u32 element_id) const
    {
        for (u32 i = 0; i < _element_ids.size(); ++i)
            if (_element_ids[i] == element_id) return i;
        return static_cast<u32>(-1);
    }

    void Tabbar::reorder_item(u32 from, u32 to)
    {
        if (from >= _tabs.size()) return;
        to = amal::min(to, static_cast<u32>(_tabs.size() - 1u));
        if (from == to) return;
        auto *tab = _tabs[from];
        const u32 element_id = _element_ids[from];
        _tabs.erase(_tabs.begin() + from);
        _element_ids.erase(_element_ids.begin() + from);
        to = amal::min(to, static_cast<u32>(_tabs.size()));
        _tabs.insert(_tabs.begin() + to, tab);
        _element_ids.insert(_element_ids.begin() + to, element_id);
        _selected_index = selected_index();
        rebuild_items();
        mark_changed();
        refresh_layout_owner(*this);
    }

    void Tabbar::begin_drag(u32 element_id)
    {
        const u32 index = find_index_by_element_id(element_id);
        if (!movable() || index >= _tabs.size()) return;
        _drag_element_id = element_id;
        _drag_preview_index = index;
        if (auto *tab = _tabs[index])
        {
            const auto bounds = tab->bounds();
            _drag_grab_offset = detail::get_io().mouse_pos - bounds.offset;
            _drag_grab_offset.x = amal::clamp(_drag_grab_offset.x, 0.0f, amal::max(bounds.size.x, 0.0f));
            _drag_grab_offset.y = amal::clamp(_drag_grab_offset.y, 0.0f, amal::max(bounds.size.y, 0.0f));
            _drag_grab_offset_valid = true;
        }
        else
        {
            _drag_grab_offset = {0.0f, 0.0f};
            _drag_grab_offset_valid = false;
        }
        _drag_offset = {0.0f, 0.0f};
        _drag_applied_offset = {0.0f, 0.0f};
        _drag_moved = false;
        update_drag_depth();
    }

    void Tabbar::end_drag()
    {
        _drag_element_id = 0u;
        _drag_preview_index = 0u;
        _drag_grab_offset = {0.0f, 0.0f};
        _drag_grab_offset_valid = false;
        _drag_offset = {0.0f, 0.0f};
        _drag_applied_offset = {0.0f, 0.0f};
        _drag_moved = false;
        update_depth(depth_range());
    }

    u32 Tabbar::find_drop_index_by_x(f32 x) const
    {
        const u32 drag_index = find_index_by_element_id(_drag_element_id);
        if (drag_index >= _tabs.size()) return 0u;

        u32 drop_index = 0u;
        for (u32 i = 0; i < _tabs.size(); ++i)
        {
            if (i == drag_index) continue;
            auto *tab = _tabs[i];
            if (!tab || !tab->is_visible()) continue;
            const auto bounds = tab->bounds();
            const f32 center_x = bounds.offset.x + bounds.size.x * 0.5f;
            if (x < center_x) return drop_index;
            ++drop_index;
        }
        return drop_index;
    }

    u32 Tabbar::find_drop_index_by_dragged_center() const
    {
        const u32 drag_index = find_index_by_element_id(_drag_element_id);
        if (drag_index >= _tabs.size()) return 0u;
        auto *drag_tab = _tabs[drag_index];
        if (!drag_tab) return 0u;
        const auto drag_bounds = drag_tab->bounds();
        const f32 drag_base_x = drag_bounds.offset.x - _drag_applied_offset.x;
        return find_drop_index_by_x(drag_base_x + _drag_offset.x + drag_bounds.size.x * 0.5f);
    }

    u32 Tabbar::insertion_index_at(const amal::vec2 &point) const
    {
        u32 drop_index = 0u;
        for (u32 i = 0; i < _tabs.size(); ++i)
        {
            auto *tab = _tabs[i];
            if (!tab || !tab->is_visible()) continue;
            const auto bounds = tab->bounds();
            const f32 center_x = bounds.offset.x + bounds.size.x * 0.5f;
            if (point.x < center_x) return drop_index;
            ++drop_index;
        }
        return drop_index;
    }

    void Tabbar::begin_external_drag(u32 element_id)
    {
        begin_drag(element_id);
        const u32 index = find_index_by_element_id(element_id);
        if (index >= _tabs.size()) return;
        auto *tab = _tabs[index];
        if (!tab) return;

        const auto bounds = tab->bounds();
        if (bounds.size.x <= 0.0f || bounds.size.y <= 0.0f) return;
        const auto mouse = detail::get_io().mouse_pos;
        const amal::vec2 grab{
            amal::clamp(_drag_grab_offset.x, 0.0f, bounds.size.x),
            amal::clamp(_drag_grab_offset.y, 0.0f, bounds.size.y),
        };
        const amal::vec2 target_pos = snap_drag_visual_position({mouse.x - grab.x, bounds.offset.y});
        _drag_offset = target_pos - bounds.offset;
        _drag_applied_offset = {0.0f, 0.0f};
        _drag_moved = true;
        update_layout_from_current_bounds(true);
    }

    void Tabbar::cancel_drag() { end_drag(); }

    bool Tabbar::update_drag_realtime_order(f32 delta_x)
    {
        u32 drag_index = find_index_by_element_id(_drag_element_id);
        if (drag_index >= _tabs.size()) return false;
        auto *drag_tab = _tabs[drag_index];
        if (!drag_tab) return false;

        bool changed = false;
        if (delta_x > 0.0f)
        {
            while (drag_index + 1u < _tabs.size())
            {
                auto *neighbor = _tabs[drag_index + 1u];
                if (!neighbor || !neighbor->is_visible()) break;
                const auto drag_bounds = drag_tab->bounds();
                const auto neighbor_bounds = neighbor->bounds();
                const f32 drag_base_x = drag_bounds.offset.x - _drag_applied_offset.x;
                const f32 drag_edge = drag_base_x + _drag_offset.x + drag_bounds.size.x;
                const f32 neighbor_center = neighbor_bounds.offset.x + neighbor_bounds.size.x * 0.5f;
                if (drag_edge <= neighbor_center) break;
                if (!swap_drag_with_neighbor(drag_index, drag_index + 1u)) break;
                changed = true;
                ++drag_index;
            }
        }
        else if (delta_x < 0.0f)
        {
            while (drag_index > 0u)
            {
                auto *neighbor = _tabs[drag_index - 1u];
                if (!neighbor || !neighbor->is_visible()) break;
                const auto drag_bounds = drag_tab->bounds();
                const auto neighbor_bounds = neighbor->bounds();
                const f32 drag_base_x = drag_bounds.offset.x - _drag_applied_offset.x;
                const f32 drag_edge = drag_base_x + _drag_offset.x;
                const f32 neighbor_center = neighbor_bounds.offset.x + neighbor_bounds.size.x * 0.5f;
                if (drag_edge >= neighbor_center) break;
                if (!swap_drag_with_neighbor(drag_index, drag_index - 1u)) break;
                changed = true;
                --drag_index;
            }
        }
        _drag_preview_index = drag_index;
        return changed;
    }

    bool Tabbar::swap_drag_with_neighbor(u32 drag_index, u32 neighbor_index)
    {
        if (drag_index >= _tabs.size() || neighbor_index >= _tabs.size() || drag_index == neighbor_index) return false;
        auto *drag_tab = _tabs[drag_index];
        auto *neighbor_tab = _tabs[neighbor_index];
        if (!drag_tab || !neighbor_tab) return false;

        const bool move_right = neighbor_index > drag_index;
        const amal::vec2 drag_abs_pos = drag_tab->position();
        const amal::vec2 drag_base_pos = drag_abs_pos - _drag_applied_offset;
        const amal::vec2 neighbor_pos = neighbor_tab->position();
        const amal::vec2 drag_size = drag_tab->size();
        const amal::vec2 neighbor_size = neighbor_tab->size();
        const f32 inline_spacing = amal::max(get_theme()->get_style(_style.id).inline_spacing(), 0.0f);

        const f32 new_drag_base_x = move_right ? drag_base_pos.x + neighbor_size.x + inline_spacing : neighbor_pos.x;
        const f32 new_neighbor_x = move_right ? drag_base_pos.x : new_drag_base_x + drag_size.x + inline_spacing;
        const amal::vec2 snapped_neighbor_pos = snap_drag_visual_position({new_neighbor_x, neighbor_pos.y});
        const amal::vec2 neighbor_delta = snapped_neighbor_pos - neighbor_pos;
        neighbor_tab->translate(neighbor_delta);
        if (neighbor_index < _close_buttons.size() && _close_buttons[neighbor_index] &&
            _close_buttons[neighbor_index]->is_visible())
            _close_buttons[neighbor_index]->translate(neighbor_delta);

        const f32 logical_drag_x = drag_base_pos.x + _drag_offset.x;
        _drag_offset = {logical_drag_x - new_drag_base_x, 0.0f};
        _drag_applied_offset = {drag_abs_pos.x - new_drag_base_x, 0.0f};

        auto *tab_tmp = _tabs[drag_index];
        _tabs[drag_index] = _tabs[neighbor_index];
        _tabs[neighbor_index] = tab_tmp;

        const u32 id_tmp = _element_ids[drag_index];
        _element_ids[drag_index] = _element_ids[neighbor_index];
        _element_ids[neighbor_index] = id_tmp;

        if (drag_index < _close_buttons.size() && neighbor_index < _close_buttons.size())
        {
            auto *close_tmp = _close_buttons[drag_index];
            _close_buttons[drag_index] = _close_buttons[neighbor_index];
            _close_buttons[neighbor_index] = close_tmp;
        }

        _selected_index = selected_index();
        return true;
    }

    void Tabbar::update_drag_depth()
    {
        const u32 drag_index = find_index_by_element_id(_drag_element_id);
        if (drag_index >= _tabs.size()) return;

        const amal::vec2 parent_work_range = detail::depth_foreground_range(depth_range());
        amal::vec2 child_top_range{};
        assign_next_depth(parent_work_range, child_top_range);
        const amal::vec2 tab_range = detail::depth_foreground_range(child_top_range);
        if (_tabs[drag_index]) _tabs[drag_index]->update_depth(tab_range);
        if (drag_index < _close_buttons.size() && _close_buttons[drag_index])
        {
            amal::vec2 close_range = tab_range;
            _close_buttons[drag_index]->update_depth(close_range);
        }
    }

    void Tabbar::update_layout_from_current_bounds(bool min_size_known)
    {
        const auto &style = get_theme()->get_style(_style.id);
        const amal::vec4 margin = style.margin();
        set_position({position().x - margin.x, position().y - margin.y});
        if (!is_width_fixed()) set_layout_size({size().x + margin.x + margin.z, size().y});
        update_layout(min_size_known);
    }

    void Tabbar::on_click(MouseKey key, KeyPressState state, u32 click_count)
    {
        (void)click_count;
        if (key != MouseKey::left || state != KeyPressState::press) return;

        const auto hover_id = detail::get_context().hover_id;
        if (hover_id.widget_id != id()) return;
        if (hover_id.tag_id == AUIK_TAG_CLOSE_BUTTON)
        {
            add_render_command<detail::ClickEventTraits>(this, [this, element_id = hover_id.element_id]() {
                close_item(element_id);
                close_popup();
                redraw_all_commands();
            });
            detail::mark_host_refresh_request();
            return;
        }
        if (hover_id.tag_id == _item_style_tag)
        {
            add_render_command<detail::ClickEventTraits>(this, [this, element_id = hover_id.element_id]() {
                if (_drag_moved) return;
                const bool was_open = _open;
                handle_item_click(element_id);
                close_popup();
                if (was_open) redraw_all_commands();
            });
            detail::mark_host_refresh_request();
            return;
        }
        if (hover_id.tag_id == _popup_item_style_tag)
        {
            add_render_command<detail::ClickEventTraits>(this, [this, element_id = hover_id.element_id]() {
                handle_item_click(element_id);
                close_popup();
                redraw_all_commands();
            });
            detail::mark_host_refresh_request();
            return;
        }
        if (hover_id.tag_id == AUIK_TAG_TABBAR_POPUP_BTN)
        {
            add_render_command<detail::ClickEventTraits>(this, [this]() {
                toggle_popup();
                redraw_all_commands();
            });
            detail::mark_host_refresh_request();
        }
    }

    void Tabbar::on_drag(const amal::vec2 &delta, KeyPressState state)
    {
        if (!movable())
        {
            end_drag();
            return;
        }

        const auto &ctx = detail::get_context();
        if (state == KeyPressState::press)
        {
            if (ctx.hover_id.widget_id == id() && ctx.hover_id.tag_id == _item_style_tag)
                begin_drag(ctx.hover_id.element_id);
            return;
        }
        if (state == KeyPressState::release)
        {
            const bool had_drag = _drag_element_id != 0u;
            const bool should_snap_layout = _drag_moved && find_index_by_element_id(_drag_element_id) < _tabs.size();
            end_drag();
            if (should_snap_layout) mark_changed();
            if (should_snap_layout || had_drag)
            {
                add_render_command<detail::DragEventTraits>(this, [this, should_snap_layout]() {
                    if (should_snap_layout) update_layout_from_current_bounds(false);
                    update_draw_commands(DrawReasonBits::layout);
                    detail::get_context().dirty_flags |= DirtyFlagBits::redraw;
                });
                detail::mark_host_refresh_request();
            }
            return;
        }
        const u32 drag_index = find_index_by_element_id(_drag_element_id);
        if (state != KeyPressState::repeat || drag_index >= _tabs.size()) return;
        if (delta.x == 0.0f && delta.y == 0.0f) return;

        _drag_moved = true;
        _drag_offset.x += delta.x;
        _drag_preview_index = find_drop_index_by_dragged_center();
        add_render_command<detail::DragEventTraits>(this, [this, drag_delta_x = delta.x]() {
            if (find_index_by_element_id(_drag_element_id) >= _tabs.size()) return;
            update_layout_from_current_bounds(true);
            if (update_drag_realtime_order(drag_delta_x)) update_layout_from_current_bounds(true);
            update_draw_commands(DrawReasonBits::layout);
            detail::get_context().dirty_flags |= DirtyFlagBits::redraw;
        });
    }

    void Tabbar::on_scroll(const amal::vec2 &delta)
    {
        if (!scroll()) return;
        const f32 amount = dominant_scroll_axis(delta);
        if (amount == 0.0f) return;
        const f32 prev_scroll_offset = _scroll_offset;
        _scroll_offset -= amount * AUIK_TAB_BAR_SCROLL_STEP;
        clamp_scroll_offset();
        if (prev_scroll_offset == _scroll_offset) return;
        add_render_command<detail::ScrollEventTraits>(this, [this]() {
            update_layout_from_current_bounds(true);
            auto &ctx = detail::get_context();
            ctx.dirty_flags |= DirtyFlagBits::hit_rect_update;
            invalidate_draw_commands(DrawReasonBits::layout);
            update_draw_commands(DrawReasonBits::layout);
            ctx.dirty_flags |= DirtyFlagBits::redraw;
            detail::mark_host_refresh_request();
        });
        detail::mark_host_refresh_request();
    }

    void Tabbar::clamp_scroll_offset()
    {
        const auto &style = get_theme()->get_style(_style.id);
        const amal::vec4 padding = style.padding();
        const f32 max_offset = amal::max(_content_width - amal::max(size().x - padding.x - padding.z, 0.0f), 0.0f);
        _scroll_offset = amal::clamp(_scroll_offset, 0.0f, max_offset);
    }

    void Tabbar::update_popup_layout()
    {
        if (!_popup || _overflow_start >= _tabs.size()) return;

        _popup->set_parent(parent());
        _popup->set_window_style_tag(AUIK_STYLE_TAG_TAB_BAR_POPUP);
        _popup->window_flags = (get_popup_window_flags() | WindowFlagBits::docked) & ~WindowFlagBits::scrollable;
        _popup->set_visible();
        _popup->sync_widget_flags();
        _popup->update_style();

        const auto &popup_style = get_theme()->get_style(
            get_theme()->get_resolved_style(AUIK_STYLE_TAG_TAB_BAR_POPUP, _popup->id(), 0, StyleState::normal));
        const amal::vec4 popup_padding = popup_style.padding();

        f32 content_width = 0.0f;
        f32 measured_h = popup_padding.y + popup_padding.w;
        u32 visible_items = 0u;
        for (u32 i = 0; i < _popup->children.size(); ++i)
        {
            auto *child = _popup->children[i];
            if (!child) continue;
            const bool visible = i >= _overflow_start;
            if (visible) child->set_visible();
            else child->unset_visible();
            child->sync_widget_flags();
            if (!visible) continue;
            ++visible_items;
            if (!child->is_fixed()) child->set_layout_size({0.0f, 0.0f});
            child->update_layout_min_size();
            content_width = amal::max(content_width, child->required_size().x);
            measured_h += child->required_size().y;
        }

        const f32 min_button_width = _overflow_button ? _overflow_button->required_size().x : 0.0f;
        const f32 popup_w = amal::max(content_width + popup_padding.x + popup_padding.z, min_button_width);
        const amal::vec4 viewport = get_widget_viewport_rect(this);
        const f32 desired_h = amal::max(measured_h, AUIK_TAB_BAR_POPUP_ITEM_FALLBACK_HEIGHT);
        const f32 content_h = amal::max(measured_h - popup_padding.y - popup_padding.w, 0.0f);
        const f32 item_h =
            visible_items > 0u ? content_h / static_cast<f32>(visible_items) : AUIK_TAB_BAR_POPUP_ITEM_FALLBACK_HEIGHT;
        const auto placement =
            resolve_dropdown_popup_placement(position().y, size().y, desired_h, item_h, visible_items, viewport,
                                             AUIK_TAB_BAR_POPUP_ITEM_FALLBACK_HEIGHT);
        if (placement.need_scroll) _popup->window_flags = get_popup_window_flags() | WindowFlagBits::docked;
        else _popup->window_flags = (get_popup_window_flags() | WindowFlagBits::docked) & ~WindowFlagBits::scrollable;
        _popup->set_position({position().x + size().x - popup_w, placement.y});
        _popup->set_size({popup_w, placement.height});
        _popup->attach_to_viewport(this->viewport());
        _popup->update_depth(get_tab_popup_depth_range());
        _popup->update_layout(false);

        content_width = amal::max(popup_w - popup_padding.x - popup_padding.z, 0.0f);
        amal::vec2 cursor = _popup->position() + amal::vec2{popup_padding.x, popup_padding.y};
        for (u32 i = 0; i < _popup->children.size(); ++i)
        {
            auto *child = _popup->children[i];
            if (!child || !child->is_visible()) continue;
            if (!child->is_fixed()) child->set_layout_size({0.0f, 0.0f});
            child->update_layout_min_size();
            if (!child->is_fixed()) child->set_layout_size({content_width, 0.0f});
            else child->set_layout_size({content_width, child->size().y});
            child->set_position(cursor);
            child->update_layout(true);
            cursor.y += child->required_size().y;
        }
    }

    void Tabbar::open_popup()
    {
        if (!popup() || !_popup || _overflow_start >= _tabs.size()) return;
        if (_open) return;
        _open = true;
        if (_overflow_button)
        {
            _overflow_button->set_open(true);
            update_overflow_button_style();
            _overflow_button->start_icon_animation(true);
        }
        update_popup_layout();
    }

    void Tabbar::close_popup(bool refresh_style)
    {
        const bool was_open = _open;
        _open = false;
        if (_overflow_button)
        {
            _overflow_button->set_open(false);
            if (refresh_style) update_overflow_button_style();
            if (refresh_style && was_open) _overflow_button->start_icon_animation(false);
        }
        if (_popup)
        {
            _popup->unset_visible();
            _popup->sync_widget_flags();
        }
    }

    namespace
    {
        struct TabbarData
        {
            detail::WidgetCommonData common{};
            acul::vector<acul::string> items;
            acul::vector<bool> translated_items;
            acul::vector<u32> selected_ids;
            u32 tab_flags = 0u;
            u32 style_tag = AUIK_STYLE_TAG_GLOBAL;
            u32 item_style_tag = AUIK_STYLE_TAG_TAB_BAR_ITEM;
            u32 selected_item_style_tag = AUIK_STYLE_TAG_TAB_BAR_ITEM_SELECTED;
            u32 popup_item_style_tag = AUIK_STYLE_TAG_COMBO_BOX_ITEM;
            f32 tab_width = 0.0f;
            u32 tab_width_key = 0u;
            f32 scroll_offset = 0.0f;
        };

        void write_tab_bar(acul::bin_stream &stream, umbf::Block *block)
        {
            auto *widget = static_cast<Tabbar *>(block);
            detail::write_widget_common_data(stream, *widget);
            stream.write(static_cast<u32>(widget->child_size()));
            for (auto *tab : *widget)
            {
                const StringView text = tab ? tab->source_text() : StringView{};
                detail::write_localized_string(stream, text.str ? text.str : "", text.is_translated);
            }
            const auto &selected_ids = widget->selected_ids();
            stream.write(static_cast<u32>(selected_ids.size()));
            if (!selected_ids.empty()) stream.write(selected_ids.data(), selected_ids.size());
            stream.write(static_cast<u32>(widget->tab_flags()))
                .write(widget->style_tag())
                .write(widget->item_style_tag())
                .write(widget->selected_item_style_tag())
                .write(widget->popup_item_style_tag())
                .write(widget->tab_width())
                .write(widget->tab_width_key())
                .write(widget->scroll_offset());
        }

        TabbarData read_tab_bar_data(acul::bin_stream &stream)
        {
            TabbarData out{};
            out.common = detail::read_widget_common_data(stream);
            u32 item_count = 0u;
            stream.read(item_count);
            out.items.reserve(item_count);
            out.translated_items.reserve(item_count);
            for (u32 i = 0u; i < item_count; ++i)
            {
                auto item = detail::read_localized_string(stream);
                out.translated_items.push_back(item.translated);
                out.items.push_back(std::move(item.text));
            }
            u32 selected_ids_count = 0u;
            stream.read(selected_ids_count);
            out.selected_ids.resize(selected_ids_count);
            if (!out.selected_ids.empty()) stream.read(out.selected_ids.data(), out.selected_ids.size());
            stream.read(out.tab_flags)
                .read(out.style_tag)
                .read(out.item_style_tag)
                .read(out.selected_item_style_tag)
                .read(out.popup_item_style_tag)
                .read(out.tab_width)
                .read(out.tab_width_key)
                .read(out.scroll_offset);
            return out;
        }

        umbf::Block *read_tab_bar(acul::bin_stream &stream)
        {
            const auto data = read_tab_bar_data(stream);
            acul::vector<StringView> items;
            items.reserve(data.items.size());
            for (u32 i = 0u; i < data.items.size(); ++i)
            {
                const bool translated = i < data.translated_items.size() && data.translated_items[i];
                items.push_back(StringView{data.items[i].c_str(), translated});
            }
            auto *widget =
                acul::alloc<Tabbar>(data.common.id, items, TabbarFlags(data.tab_flags), data.common.requested_size,
                                    WidgetFlags(data.common.widget_flags), data.tab_width, data.tab_width_key,
                                    data.item_style_tag, data.selected_item_style_tag, data.popup_item_style_tag);
            widget->set_style_tag(data.style_tag);
            widget->set_selected(data.selected_ids);
            widget->set_scroll_offset(data.scroll_offset);
            detail::apply_widget_common_data(widget, data.common);
            return widget;
        }
    } // namespace

    namespace streams
    {
        AUIK_EXPORT const umbf::streams::Stream tab_bar{read_tab_bar, write_tab_bar};
    } // namespace streams
} // namespace auik
