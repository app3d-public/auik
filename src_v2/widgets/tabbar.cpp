#include <auik/v2/auik.hpp>
#include <auik/v2/detail/depth.hpp>
#include <auik/v2/detail/rect.hpp>
#include <auik/v2/widgets/combobox.hpp>
#include <auik/v2/widgets/detail/selectable.hpp>
#include <auik/v2/widgets/tabbar.hpp>
#include <auik/v2/widgets/window.hpp>

#define AUIK_TAB_BAR_POPUP_ITEM_FALLBACK_HEIGHT 24.0f
#define AUIK_TAB_BAR_SCROLL_STEP                32.0f

namespace auik::v2
{
    static inline amal::vec2 get_tab_popup_depth_range()
    {
        return detail::get_root_depth_range(DepthZone::foreground, 31);
    }

    static inline f32 dominant_scroll_axis(const amal::vec2 &delta)
    {
        return amal::abs(delta.x) > amal::abs(delta.y) ? delta.x : delta.y;
    }

    static inline TabBarFlags normalize_tab_bar_flags(TabBarFlags flags)
    {
        if (flags & TabBarFlagBits::scroll) flags &= ~TabBarFlagBits::popup;
        return flags;
    }

    static inline EventFlags make_tab_bar_event_flags(TabBarFlags flags)
    {
        flags = normalize_tab_bar_flags(flags);
        EventFlags out = EventFlagBits::click | EventFlagBits::hover;
        if (flags & TabBarFlagBits::popup) out |= EventFlagBits::focus;
        if (flags & TabBarFlagBits::scroll) out |= EventFlagBits::scroll;
        if (flags & TabBarFlagBits::movable) out |= EventFlagBits::drag;
        return out;
    }

    TabBar::TabBar(u32 id, acul::vector<acul::string> items, TabBarFlags tab_flags, amal::vec2 size,
                   WidgetFlags widget_flags, Widget *parent, f32 tab_width, u32 tab_width_key, u32 item_style_tag,
                   u32 popup_item_style_tag)
        : Widget(id, widget_flags, make_tab_bar_event_flags(tab_flags), parent, {{0.0f, 0.0f}, size}, AUIK_TAG_TAB_BAR),
          _tab_flags(normalize_tab_bar_flags(tab_flags)),
          _tab_width(tab_width),
          _tab_width_key(tab_width_key)
    {
        _item_style_tag = item_style_tag;
        _popup_item_style_tag = popup_item_style_tag;
        if (popup())
        {
            _overflow_button = acul::alloc<detail::PopupTrigger>(AUIK_TAG_TABBAR_POPUP_BTN, AUIK_TAG_TABBAR_POPUP_BTN,
                                                                 AUIK_ICON_CHEVRON_DOWN, AUIK_ICON_CHEVRON_UP, true);
            _overflow_button->set_owner(this);
            ensure_overflow_icon_resources();
            _overflow_button->update_style(AUIK_TAG_TABBAR_POPUP_BTN, id, StyleState::normal);

            _popup = acul::alloc<Window>(AUIK_TAG_TAB_BAR_POPUP, "", amal::rect{{0.0f, 0.0f}, {0.0f, 0.0f}},
                                         get_popup_window_flags(),
                                         WidgetFlagBits::visible | WidgetFlagBits::hittable | WidgetFlagBits::fixed);
            _popup->get_rect().widget_id = id;
            _popup->set_window_style_tag(AUIK_TAG_TAB_BAR_POPUP);
            _popup->set_focus_parent(this);
            _popup->update_style();
            _popup->hide();
        }

        set_items(std::move(items));
        sync_selection_to_widgets();
    }

    TabBar::~TabBar()
    {
        close_popup();
        if (_popup) acul::release(_popup);
        if (_overflow_button) acul::release(_overflow_button);
        for (auto *tab : _tabs) acul::release(tab);
        for (auto *button : _close_buttons) acul::release(button);
        _tabs.clear();
        _close_buttons.clear();
    }

    void TabBar::rebuild_items()
    {
        if (_popup) _popup->clear_children();
        while (_close_buttons.size() < _tabs.size())
        {
            auto *button = acul::alloc<ImageButton>(
                AUIK_TAG_CLOSE_BUTTON, get_cached_image(AUIK_ICON_CLOSE), amal::vec2{0.0f, 0.0f},
                amal::vec2{0.0f, 0.0f}, WidgetFlagBits::visible | WidgetFlagBits::hittable | WidgetFlagBits::fixed,
                this, AUIK_TAG_CLOSE_BUTTON);
            button->get_rect().widget_id = id();
            button->set_focus_parent(this);
            button->update_style();
            _close_buttons.push_back(button);
        }
        while (_close_buttons.size() > _tabs.size())
        {
            acul::release(_close_buttons.back());
            _close_buttons.erase(_close_buttons.end() - 1);
        }

        for (u32 i = 0; i < _tabs.size(); ++i)
        {
            const u32 element_id = _element_ids[i];
            auto *tab = _tabs[i];
            tab->get_rect().element_id = element_id;
            _close_buttons[i]->get_rect().widget_id = id();
            _close_buttons[i]->get_rect().element_id = element_id;

            if (_popup)
            {
                auto *popup_item = acul::alloc<detail::Selectable>(
                    _popup_item_style_tag, _popup_item_style_tag, element_id, tab->text(), amal::vec2{0.0f, 0.0f},
                    _popup, _popup_item_style_tag, WidgetFlagBits::visible | WidgetFlagBits::hittable);
                popup_item->get_rect().widget_id = id();
                popup_item->set_focus_parent(_popup);
                _popup->add_child(popup_item, WindowChildLayout::block);
            }
        }

        if (_tabs.empty())
        {
            _selected_index = 0u;
            _selected_element_ids.clear();
            return;
        }
        if (_selected_index >= _tabs.size()) _selected_index = static_cast<u32>(_tabs.size() - 1u);
        if (_selected_element_ids.empty()) _selected_element_ids.push_back(_element_ids[_selected_index]);
        if (!multiple() && _selected_element_ids.size() > 1u)
            _selected_element_ids.erase(_selected_element_ids.begin() + 1u, _selected_element_ids.end());
    }

    void TabBar::sync_selection_to_widgets()
    {
        const auto apply = [&](detail::Selectable *widget, u32 index) {
            if (!widget) return;
            const bool selected = index < _element_ids.size() && is_selected(_element_ids[index]);
            widget->set_style_state(StyleState::normal);
            widget->set_selected(selected);
            widget->update_style();
        };
        for (u32 i = 0; i < _tabs.size(); ++i) apply(_tabs[i], i);
        if (_popup)
            for (u32 i = 0; i < _popup->children.size(); ++i)
                apply(static_cast<detail::Selectable *>(_popup->children[i]), i);
    }

    void TabBar::set_items(acul::vector<acul::string> items)
    {
        if (_popup) _popup->clear_children();
        for (auto *tab : _tabs) acul::release(tab);
        for (auto *button : _close_buttons) acul::release(button);
        _tabs.clear();
        _close_buttons.clear();
        _element_ids.clear();
        _selected_element_ids.clear();
        _next_element_id = 1u;
        _drag_element_id = 0u;
        _drag_preview_index = 0u;
        _last_selected_element_id = 0u;
        for (u32 i = 0; i < items.size(); ++i)
        {
            const u32 element_id = _next_element_id++;
            _element_ids.push_back(element_id);
            auto *tab = acul::alloc<detail::Selectable>(
                _item_style_tag, _item_style_tag, element_id, items[i], amal::vec2{0.0f, 0.0f}, this, _item_style_tag,
                WidgetFlagBits::visible | WidgetFlagBits::hittable, _item_style_tag, StyleState::focus);
            tab->get_rect().widget_id = id();
            tab->set_focus_parent(this);
            tab->update_style();
            _tabs.push_back(tab);
        }
        rebuild_items();
        sync_selection_to_widgets();
    }

    void TabBar::set_style_tag(u32 tag_id)
    {
        if (_style.tag_id == tag_id) return;
        _style = {Theme::STYLE_ID_INVALID, tag_id};
        set_rect_tag_id(tag_id);
        detail::get_context().dirty_flags |= DirtyFlagBits::layout;
    }

    u32 TabBar::selected_id() const { return _selected_element_ids.empty() ? 0u : _selected_element_ids[0]; }

    bool TabBar::is_selected(u32 element_id) const
    {
        for (u32 selected_id : _selected_element_ids)
            if (selected_id == element_id) return true;
        return false;
    }

    void TabBar::set_selected(u32 element_id)
    {
        const u32 index = find_index_by_element_id(element_id);
        if (index >= _tabs.size()) return;
        if (!_selected_element_ids.empty() && _selected_element_ids[0] != element_id)
            _last_selected_element_id = _selected_element_ids[0];
        _selected_index = index;
        _selected_element_ids.clear();
        _selected_element_ids.push_back(element_id);
        sync_selection_to_widgets();
        detail::get_context().dirty_flags |= DirtyFlagBits::redraw;
        detail::mark_host_refresh_request();
    }

    void TabBar::set_selected(const acul::vector<u32> &element_ids)
    {
        _selected_element_ids.clear();
        for (u32 element_id : element_ids)
        {
            const u32 index = find_index_by_element_id(element_id);
            if (index >= _tabs.size() || is_selected(element_id)) continue;
            _selected_element_ids.push_back(element_id);
            _selected_index = index;
            if (!multiple()) break;
        }
        if (!_selected_element_ids.empty()) _last_selected_element_id = _selected_element_ids[0];
        sync_selection_to_widgets();
        detail::get_context().dirty_flags |= DirtyFlagBits::redraw;
        detail::mark_host_refresh_request();
    }

    void TabBar::set_tab_width(f32 value)
    {
        if (_tab_width == value) return;
        _tab_width = value;
        detail::get_context().dirty_flags |= DirtyFlagBits::layout | DirtyFlagBits::redraw;
        detail::mark_host_refresh_request();
    }

    void TabBar::set_tab_width_key(u32 key)
    {
        if (_tab_width_key == key) return;
        _tab_width_key = key;
        detail::get_context().dirty_flags |= DirtyFlagBits::layout | DirtyFlagBits::redraw;
        detail::mark_host_refresh_request();
    }

    StyleUpdateFlags TabBar::update_tab_item_style(u32 index,
                                                   const detail::WidgetStyleSelectorTransition &transition)
    {
        if (index >= _tabs.size()) return StyleUpdateFlagBits::none;
        auto *tab = _tabs[index];
        if (!tab) return StyleUpdateFlagBits::none;
        const bool selected = index < _element_ids.size() && is_selected(_element_ids[index]);
        StyleState state = StyleState::normal;
        if (transition.current_id.tag_id == _item_style_tag && index < _element_ids.size() &&
            transition.current_id.element_id == _element_ids[index])
            state = transition.current_state;
        tab->set_style_state(state);
        tab->set_selected(selected);
        return tab->update_style();
    }

    StyleUpdateFlags TabBar::update_close_button_style(u32 index,
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

    StyleUpdateFlags TabBar::update_popup_item_style(u32 index,
                                                     const detail::WidgetStyleSelectorTransition &transition)
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

    StyleUpdateFlags TabBar::update_style()
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
            StyleState overflow_state = _open ? StyleState::active : StyleState::normal;
            if (transition.current_id.tag_id == AUIK_TAG_TABBAR_POPUP_BTN) overflow_state = transition.current_state;
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

    amal::vec2 TabBar::measure_overflow_size()
    {
        if (!_overflow_button) return {0.0f, 0.0f};
        ensure_overflow_icon_resources();
        _overflow_button->update_layout_min_size({0.0f, 0.0f}, true);
        return _overflow_button->required_size();
    }

    void TabBar::ensure_overflow_icon_resources()
    {
        if (!_overflow_button) return;
        _overflow_button->set_icons(AUIK_ICON_CHEVRON_DOWN, AUIK_ICON_CHEVRON_UP);
    }

    f32 TabBar::resolve_tab_width() const
    {
        if (_tab_width_key != 0u)
        {
            const f32 width = get_theme()->get_var<f32>(_tab_width_key);
            return width > 0.0f ? width : 0.0f;
        }
        return _tab_width > 0.0f ? _tab_width : 0.0f;
    }

    amal::vec2 TabBar::resolve_tab_required_size(u32 index)
    {
        if (index >= _tabs.size() || !_tabs[index]) return {0.0f, 0.0f};
        auto *tab = _tabs[index];
        const f32 fixed_tab_width = resolve_tab_width();
        tab->set_size({fixed_tab_width, 0.0f});
        tab->update_layout_min_size();
        amal::vec2 required = tab->required_size();
        if (fixed_tab_width > 0.0f) required.x = fixed_tab_width;
        if (closable() && index < _close_buttons.size())
        {
            auto *close_button = _close_buttons[index];
            if (close_button)
            {
                close_button->set_size({0.0f, 0.0f});
                close_button->update_layout_min_size();
                const amal::vec2 close_required = close_button->required_size();
                if (fixed_tab_width <= 0.0f) required.x += close_required.x;
                required.y = amal::max(required.y, close_required.y);
            }
        }
        return required;
    }

    void TabBar::update_layout_min_size()
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

        amal::vec2 min_size = is_fixed() ? size() : amal::vec2{0.0f, 0.0f};
        if (min_size.x <= 0.0f)
        {
            const f32 visual_min_width = popup() ? measure_overflow_size().x : tabs_size.x;
            min_size.x = visual_min_width + padding.x + padding.z;
        }
        if (min_size.y <= 0.0f) min_size.y = tabs_size.y + padding.y + padding.w;
        set_required_size({min_size.x + margin.x + margin.z, min_size.y + margin.y + margin.w});
    }

    void TabBar::update_layout(bool min_size_known)
    {
        if (!min_size_known) update_layout_min_size();

        const auto &style = get_theme()->get_style(_style.id);
        const amal::vec4 margin = style.margin();
        const amal::vec4 padding = style.padding();
        const f32 inline_spacing = amal::max(style.inline_spacing(), 0.0f);
        const amal::vec2 layout_origin = position();
        const amal::vec2 min_required = required_size();
        amal::vec2 widget_size = size();
        const amal::vec2 min_inner = {amal::max(min_required.x - margin.x - margin.z, 0.0f),
                                      amal::max(min_required.y - margin.y - margin.w, 0.0f)};
        if (!is_fixed()) widget_size.x = amal::max(widget_size.x - margin.x - margin.z, min_inner.x);
        else widget_size.x = amal::max(widget_size.x, min_inner.x);
        widget_size.y = amal::max(widget_size.y, min_inner.y);

        set_position({layout_origin.x + margin.x, layout_origin.y + margin.y});
        set_size(widget_size);
        Widget::update_layout(true);

        const f32 content_x = position().x + padding.x;
        const f32 content_y = position().y + padding.y;
        const f32 content_w = amal::max(size().x - padding.x - padding.z, 0.0f);
        const f32 content_h = amal::max(size().y - padding.y - padding.w, 0.0f);
        const amal::vec2 overflow_size = popup() ? measure_overflow_size() : amal::vec2{0.0f, 0.0f};
        const f32 popup_reserved = popup() ? overflow_size.x : 0.0f;

        const amal::vec4 parent_clip =
            parent() ? parent()->get_content_clip_rect() : amal::vec4{position().x, position().y, size().x, size().y};
        const amal::vec4 full_clip =
            detail::intersect_rects(parent_clip, {position().x, position().y, size().x, size().y});
        const f32 tabs_clip_right = position().x + size().x - padding.z - popup_reserved;
        const amal::vec4 tabs_clip = detail::intersect_rects(
            parent_clip, {position().x, position().y, amal::max(tabs_clip_right - position().x, 0.0f), size().y});
        if (_full_clip_id == 0xFFFFu) _full_clip_id = push_clip_rect(full_clip);
        else update_clip_rect(_full_clip_id, full_clip);
        if (_content_clip_id == 0xFFFFu) _content_clip_id = push_clip_rect(tabs_clip);
        else update_clip_rect(_content_clip_id, tabs_clip);
        set_clip_id(_full_clip_id);

        f32 cursor_x = content_x - (scroll() ? _scroll_offset : 0.0f);
        f32 total_w = 0.0f;
        _visible_count = 0u;
        _overflow_start = static_cast<u32>(_tabs.size());

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
            const bool fits_popup = cursor_x + required.x <= content_x + amal::max(content_w - popup_reserved, 0.0f);
            const bool visible = !popup() || fits_popup;
            if (!visible && _overflow_start == _tabs.size()) _overflow_start = i;
            if (visible) ++_visible_count;
            tab->set_visible(!popup() || visible);
            if (close_button) close_button->set_visible(closable() && visible);
            tab->set_size({required.x, tab->size().y});
            tab->set_position({cursor_x, content_y + amal::max((content_h - required.y) * 0.5f, 0.0f)});
            tab->update_layout(true);
            if (close_button && close_button->is_visible())
            {
                const f32 close_x = cursor_x + required.x - close_required.x;
                const f32 close_y = content_y + amal::max((content_h - close_required.y) * 0.5f, 0.0f);
                close_button->set_position({close_x, close_y});
                close_button->update_layout(true);
            }
            if (movable() && i == drag_index && (_drag_offset.x != 0.0f || _drag_offset.y != 0.0f))
            {
                tab->translate(_drag_offset);
                if (close_button && close_button->is_visible()) close_button->translate(_drag_offset);
                _drag_applied_offset = _drag_offset;
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
        else if (_popup) _popup->hide();
    }

    void TabBar::translate(const amal::vec2 &delta)
    {
        if (delta.x == 0.0f && delta.y == 0.0f) return;
        Widget::translate(delta);
        for (auto *tab : _tabs)
            if (tab) tab->translate(delta);
        for (auto *button : _close_buttons)
            if (button) button->translate(delta);
        if (_overflow_button) _overflow_button->translate(delta);
        if (_open && _popup) static_cast<Widget *>(_popup)->translate(delta);
        if (_full_clip_id != 0xFFFFu || _content_clip_id != 0xFFFFu)
        {
            const auto &style = get_theme()->get_style(_style.id);
            const amal::vec4 padding = style.padding();
            const amal::vec2 overflow_size = popup() ? measure_overflow_size() : amal::vec2{0.0f, 0.0f};
            const f32 popup_reserved = popup() ? overflow_size.x : 0.0f;
            const amal::vec4 parent_clip = parent() ? parent()->get_content_clip_rect()
                                                    : amal::vec4{position().x, position().y, size().x, size().y};
            const amal::vec4 full_clip =
                detail::intersect_rects(parent_clip, {position().x, position().y, size().x, size().y});
            const f32 tabs_clip_right = position().x + size().x - padding.z - popup_reserved;
            const amal::vec4 tabs_clip = detail::intersect_rects(
                parent_clip, {position().x, position().y, amal::max(tabs_clip_right - position().x, 0.0f), size().y});
            if (_full_clip_id != 0xFFFFu) update_clip_rect(_full_clip_id, full_clip);
            if (_content_clip_id != 0xFFFFu) update_clip_rect(_content_clip_id, tabs_clip);
        }
    }

    void TabBar::rebuild_clip_rects()
    {
        _full_clip_id = 0xFFFFu;
        _content_clip_id = 0xFFFFu;
        const auto &style = get_theme()->get_style(_style.id);
        const amal::vec4 padding = style.padding();
        const amal::vec2 overflow_size = popup() ? measure_overflow_size() : amal::vec2{0.0f, 0.0f};
        const f32 popup_reserved = popup() ? overflow_size.x : 0.0f;
        const amal::vec4 parent_clip =
            parent() ? parent()->get_content_clip_rect() : amal::vec4{position().x, position().y, size().x, size().y};
        const amal::vec4 full_clip =
            detail::intersect_rects(parent_clip, {position().x, position().y, size().x, size().y});
        const f32 tabs_clip_right = position().x + size().x - padding.z - popup_reserved;
        const amal::vec4 tabs_clip = detail::intersect_rects(
            parent_clip, {position().x, position().y, amal::max(tabs_clip_right - position().x, 0.0f), size().y});
        if (_full_clip_id == 0xFFFFu) _full_clip_id = push_clip_rect(full_clip);
        else update_clip_rect(_full_clip_id, full_clip);
        if (_content_clip_id == 0xFFFFu) _content_clip_id = push_clip_rect(tabs_clip);
        else update_clip_rect(_content_clip_id, tabs_clip);
        set_clip_id(_full_clip_id);
        for (auto *tab : _tabs)
            if (tab) tab->rebuild_clip_rects();
        for (auto *button : _close_buttons)
            if (button) button->rebuild_clip_rects();
        if (_overflow_button) _overflow_button->rebuild_clip_rects(clip_id());
        if (_popup) _popup->rebuild_clip_rects();
    }

    void TabBar::update_depth(const amal::vec2 &depth_range)
    {
        Widget::update_depth(depth_range);
        amal::vec2 next_range = this->depth_range();
        for (u32 i = 0; i < _tabs.size(); ++i)
        {
            auto *tab = _tabs[i];
            if (!tab) continue;
            amal::vec2 child_range{};
            assign_next_depth(next_range, child_range);
            tab->update_depth(child_range);
            next_range = child_range;
            if (i < _close_buttons.size() && _close_buttons[i])
            {
                amal::vec2 close_range{};
                assign_next_depth(next_range, close_range);
                _close_buttons[i]->update_depth(close_range);
                next_range = close_range;
            }
        }
        if (_overflow_button)
        {
            amal::vec2 child_range{};
            assign_next_depth(next_range, child_range);
            _overflow_button->update_depth(child_range);
        }
        if (_popup) static_cast<Widget *>(_popup)->update_depth(get_tab_popup_depth_range());
        if (_drag_element_id != 0u) update_drag_depth();
    }

    bool TabBar::draw_transition_targets(DrawCtx &ctx)
    {
        const auto transition = detail::get_widget_style_selector_transition(id());
        bool emitted = false;
        auto draw_target = [&](const detail::ElementID &element_id) {
            if (!element_id || element_id.widget_id != id()) return;
            if (element_id.tag_id == _item_style_tag)
            {
                const u32 index = find_index_by_element_id(element_id.element_id);
                if (index >= _tabs.size() || !_tabs[index] || !_tabs[index]->is_visible()) return;
                DrawCtx tab_ctx = ctx;
                tab_ctx.emit_hit_rect = _tabs[index]->is_hittable();
                _tabs[index]->draw(tab_ctx);
                emitted = true;
                return;
            }
            if (element_id.tag_id == AUIK_TAG_CLOSE_BUTTON)
            {
                const u32 index = find_index_by_element_id(element_id.element_id);
                if (index >= _close_buttons.size() || !_close_buttons[index] || !_close_buttons[index]->is_visible())
                    return;
                DrawCtx close_ctx = ctx;
                close_ctx.emit_hit_rect = _close_buttons[index]->is_hittable();
                _close_buttons[index]->draw(close_ctx);
                emitted = true;
                return;
            }
            if (element_id.tag_id == AUIK_TAG_TABBAR_POPUP_BTN && _overflow_button && _overflow_start < _tabs.size())
            {
                ensure_overflow_icon_resources();
                _overflow_button->draw(ctx, ctx.emit_hit_rect);
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
                popup_ctx.emit_hit_rect = child->is_hittable();
                child->draw(popup_ctx);
                emitted = true;
            }
        };
        draw_target(transition.prev_id);
        draw_target(transition.current_id);
        return emitted;
    }

    void TabBar::draw(DrawCtx &ctx)
    {
        if (!(widget_flags & WidgetFlagBits::visible)) return;
        if (ctx.is_updating() && ctx.reason == DrawReasonBits::none && draw_transition_targets(ctx)) return;
        for (u32 i = 0; i < _tabs.size(); ++i)
        {
            auto *tab = _tabs[i];
            if (!tab || !tab->is_visible()) continue;
            DrawCtx tab_ctx = ctx;
            tab_ctx.emit_hit_rect = tab->is_hittable();
            tab->draw(tab_ctx);
            if (i < _close_buttons.size())
            {
                auto *close_button = _close_buttons[i];
                if (close_button && close_button->is_visible())
                {
                    DrawCtx close_ctx = ctx;
                    close_ctx.emit_hit_rect = close_button->is_hittable();
                    close_button->draw(close_ctx);
                }
            }
        }
        if (_overflow_button && _overflow_start < _tabs.size())
        {
            ensure_overflow_icon_resources();
            DrawCtx overflow_ctx = ctx;
            _overflow_button->draw(overflow_ctx, ctx.emit_hit_rect);
        }
        if (_open && _popup)
        {
            DrawCtx popup_ctx = ctx;
            popup_ctx.emit_hit_rect = _popup->is_hittable();
            _popup->draw(popup_ctx);
        }
    }

    void TabBar::on_focus(bool focused)
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

    void TabBar::on_hover(HoverState state)
    {
        bool changed = apply_hover_style_state(*this, state);
        if (changed)
        {
            update_style();
            update_draw_commands();
            detail::get_context().dirty_flags |= DirtyFlagBits::redraw;
        }
    }

    void TabBar::handle_item_click(u32 element_id)
    {
        const u32 index = find_index_by_element_id(element_id);
        if (index >= _tabs.size()) return;
        const u32 prev_selected = selected_id();
        if (!multiple())
        {
            set_selected(element_id);
            return;
        }

        const auto mods = detail::get_context().io.active_mods;
        if (mods & KeyModeBits::alt)
        {
            for (u32 i = 0; i < _selected_element_ids.size(); ++i)
            {
                if (_selected_element_ids[i] != element_id) continue;
                _selected_element_ids.erase(_selected_element_ids.begin() + i);
                break;
            }
        }
        else if (mods & KeyModeBits::control)
        {
            if (is_selected(element_id))
            {
                for (u32 i = 0; i < _selected_element_ids.size(); ++i)
                {
                    if (_selected_element_ids[i] != element_id) continue;
                    _selected_element_ids.erase(_selected_element_ids.begin() + i);
                    break;
                }
            }
            else _selected_element_ids.push_back(element_id);
        }
        else
        {
            _selected_element_ids.clear();
            _selected_element_ids.push_back(element_id);
        }
        _selected_index = index;
        if (prev_selected != 0u && prev_selected != selected_id()) _last_selected_element_id = prev_selected;
        sync_selection_to_widgets();
        detail::get_context().dirty_flags |= DirtyFlagBits::redraw;
        detail::mark_host_refresh_request();
    }

    void TabBar::close_item(u32 element_id)
    {
        const u32 index = find_index_by_element_id(element_id);
        if (index >= _tabs.size()) return;

        acul::release(_tabs[index]);
        _tabs.erase(_tabs.begin() + index);
        _element_ids.erase(_element_ids.begin() + index);
        for (u32 i = 0; i < _selected_element_ids.size();)
        {
            if (_selected_element_ids[i] == element_id) _selected_element_ids.erase(_selected_element_ids.begin() + i);
            else ++i;
        }
        if (_tabs.empty()) _selected_index = 0u;
        else
        {
            if (_selected_index >= _tabs.size()) _selected_index = static_cast<u32>(_tabs.size() - 1u);
            if (_selected_element_ids.empty())
            {
                const u32 last_index = find_index_by_element_id(_last_selected_element_id);
                if (last_index < _tabs.size())
                {
                    _selected_index = last_index;
                    _selected_element_ids.push_back(_last_selected_element_id);
                }
                else
                {
                    const u32 fallback_index = index > 0u ? index - 1u : 0u;
                    _selected_index = amal::min(fallback_index, static_cast<u32>(_tabs.size() - 1u));
                    _selected_element_ids.push_back(_element_ids[_selected_index]);
                }
            }
        }
        if (_drag_element_id == element_id) end_drag();
        rebuild_items();
        sync_selection_to_widgets();
        detail::get_context().dirty_flags |= DirtyFlagBits::layout | DirtyFlagBits::redraw;
        detail::mark_host_refresh_request();
    }

    u32 TabBar::find_index_by_element_id(u32 element_id) const
    {
        for (u32 i = 0; i < _element_ids.size(); ++i)
            if (_element_ids[i] == element_id) return i;
        return static_cast<u32>(-1);
    }

    void TabBar::reorder_item(u32 from, u32 to)
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
        if (_selected_index == from) _selected_index = to;
        else if (from < _selected_index && to >= _selected_index) --_selected_index;
        else if (from > _selected_index && to <= _selected_index) ++_selected_index;
        rebuild_items();
        sync_selection_to_widgets();
        detail::get_context().dirty_flags |= DirtyFlagBits::layout | DirtyFlagBits::redraw;
        detail::mark_host_refresh_request();
    }

    void TabBar::begin_drag(u32 element_id)
    {
        if (!movable() || find_index_by_element_id(element_id) >= _tabs.size()) return;
        _drag_element_id = element_id;
        _drag_preview_index = find_index_by_element_id(element_id);
        _drag_offset = {0.0f, 0.0f};
        _drag_applied_offset = {0.0f, 0.0f};
        _drag_moved = false;
        update_drag_depth();
    }

    void TabBar::end_drag()
    {
        _drag_element_id = 0u;
        _drag_preview_index = 0u;
        _drag_offset = {0.0f, 0.0f};
        _drag_applied_offset = {0.0f, 0.0f};
        _drag_moved = false;
        update_depth(depth_range());
    }

    u32 TabBar::find_drop_index_by_x(f32 x) const
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

    u32 TabBar::find_drop_index_by_dragged_center() const
    {
        const u32 drag_index = find_index_by_element_id(_drag_element_id);
        if (drag_index >= _tabs.size()) return 0u;
        auto *drag_tab = _tabs[drag_index];
        if (!drag_tab) return 0u;
        const auto drag_bounds = drag_tab->bounds();
        const amal::vec2 pending_delta = _drag_offset - _drag_applied_offset;
        return find_drop_index_by_x(drag_bounds.offset.x + pending_delta.x + drag_bounds.size.x * 0.5f);
    }

    void TabBar::update_drag_realtime_order(f32 delta_x)
    {
        u32 drag_index = find_index_by_element_id(_drag_element_id);
        if (drag_index >= _tabs.size()) return;
        auto *drag_tab = _tabs[drag_index];
        if (!drag_tab) return;

        if (delta_x > 0.0f)
        {
            while (drag_index + 1u < _tabs.size())
            {
                auto *neighbor = _tabs[drag_index + 1u];
                if (!neighbor || !neighbor->is_visible()) break;
                const auto drag_bounds = drag_tab->bounds();
                const auto neighbor_bounds = neighbor->bounds();
                const f32 drag_edge = drag_bounds.offset.x + drag_bounds.size.x;
                const f32 neighbor_center = neighbor_bounds.offset.x + neighbor_bounds.size.x * 0.5f;
                if (drag_edge <= neighbor_center) break;
                swap_drag_with_neighbor(drag_index, drag_index + 1u);
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
                const f32 drag_edge = drag_bounds.offset.x;
                const f32 neighbor_center = neighbor_bounds.offset.x + neighbor_bounds.size.x * 0.5f;
                if (drag_edge >= neighbor_center) break;
                swap_drag_with_neighbor(drag_index, drag_index - 1u);
                --drag_index;
            }
        }
        _drag_preview_index = drag_index;
    }

    void TabBar::swap_drag_with_neighbor(u32 drag_index, u32 neighbor_index)
    {
        if (drag_index >= _tabs.size() || neighbor_index >= _tabs.size() || drag_index == neighbor_index) return;
        auto *drag_tab = _tabs[drag_index];
        auto *neighbor_tab = _tabs[neighbor_index];
        if (!drag_tab || !neighbor_tab) return;

        const bool move_right = neighbor_index > drag_index;
        const amal::vec2 drag_abs_pos = drag_tab->position();
        const amal::vec2 drag_base_pos = drag_abs_pos - _drag_applied_offset;
        const amal::vec2 neighbor_pos = neighbor_tab->position();
        const amal::vec2 drag_size = drag_tab->size();
        const f32 inline_spacing = amal::max(get_theme()->get_style(_style.id).inline_spacing(), 0.0f);

        const f32 new_drag_base_x = neighbor_pos.x;
        const f32 new_neighbor_x = move_right ? drag_base_pos.x : neighbor_pos.x + drag_size.x + inline_spacing;
        const amal::vec2 neighbor_delta = {new_neighbor_x - neighbor_pos.x, 0.0f};
        neighbor_tab->translate(neighbor_delta);
        if (neighbor_index < _close_buttons.size() && _close_buttons[neighbor_index] &&
            _close_buttons[neighbor_index]->is_visible())
            _close_buttons[neighbor_index]->translate(neighbor_delta);

        _drag_offset = {drag_abs_pos.x - new_drag_base_x, 0.0f};
        _drag_applied_offset = _drag_offset;

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

        if (_selected_index == drag_index) _selected_index = neighbor_index;
        else if (_selected_index == neighbor_index) _selected_index = drag_index;
    }

    void TabBar::update_drag_depth()
    {
        const u32 drag_index = find_index_by_element_id(_drag_element_id);
        if (drag_index >= _tabs.size()) return;

        const amal::vec2 parent_work_range = parent() ? detail::get_depth_workzone_range(parent()->depth_range())
                                                      : detail::get_depth_workzone_range(depth_range());
        amal::vec2 child_top_range{};
        assign_next_depth(parent_work_range, child_top_range);
        const amal::vec2 tab_range = detail::depth_zone_range(child_top_range, DepthZone::foreground);
        if (_tabs[drag_index]) _tabs[drag_index]->update_depth(tab_range);
        if (drag_index < _close_buttons.size() && _close_buttons[drag_index])
        {
            amal::vec2 close_range = tab_range;
            _close_buttons[drag_index]->update_depth(close_range);
        }
    }

    void TabBar::update_layout_from_current_bounds(bool min_size_known)
    {
        const auto &style = get_theme()->get_style(_style.id);
        const amal::vec4 margin = style.margin();
        set_position({position().x - margin.x, position().y - margin.y});
        if (!is_fixed()) set_size({size().x + margin.x + margin.z, size().y});
        update_layout(min_size_known);
    }

    void TabBar::on_click(MouseKey key, KeyPressState state, u32 click_count)
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
                handle_item_click(element_id);
                close_popup();
                redraw_all_commands();
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

    void TabBar::on_drag(const amal::vec2 &delta, KeyPressState state)
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
            const bool should_snap_layout = _drag_moved && find_index_by_element_id(_drag_element_id) < _tabs.size();
            end_drag();
            if (should_snap_layout)
            {
                add_render_command<detail::DragEventTraits>(this, [this]() {
                    update_layout_from_current_bounds(false);
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
        add_render_command<detail::DragEventTraits>(this, [this]() {
            const u32 drag_index = find_index_by_element_id(_drag_element_id);
            if (drag_index < _tabs.size())
            {
                const amal::vec2 pending_delta = _drag_offset - _drag_applied_offset;
                if (_tabs[drag_index]) _tabs[drag_index]->translate(pending_delta);
                if (drag_index < _close_buttons.size() && _close_buttons[drag_index] &&
                    _close_buttons[drag_index]->is_visible())
                    _close_buttons[drag_index]->translate(pending_delta);
                _drag_applied_offset = _drag_offset;
                update_drag_realtime_order(pending_delta.x);
            }
            update_draw_commands(DrawReasonBits::layout);
            detail::get_context().dirty_flags |= DirtyFlagBits::redraw;
        });
    }

    void TabBar::on_scroll(const amal::vec2 &delta)
    {
        if (!scroll()) return;
        const f32 amount = dominant_scroll_axis(delta);
        if (amount == 0.0f) return;
        _scroll_offset -= amount * AUIK_TAB_BAR_SCROLL_STEP;
        clamp_scroll_offset();
        add_render_command<detail::ScrollEventTraits>(this, [this]() {
            update_layout_from_current_bounds(true);
            update_draw_commands(DrawReasonBits::layout);
            detail::get_context().dirty_flags |= DirtyFlagBits::redraw;
        });
    }

    void TabBar::clamp_scroll_offset()
    {
        const auto &style = get_theme()->get_style(_style.id);
        const amal::vec4 padding = style.padding();
        const f32 max_offset = amal::max(_content_width - amal::max(size().x - padding.x - padding.z, 0.0f), 0.0f);
        _scroll_offset = amal::clamp(_scroll_offset, 0.0f, max_offset);
    }

    void TabBar::update_popup_layout()
    {
        if (!_popup || _overflow_start >= _tabs.size()) return;

        _popup->set_parent(parent());
        _popup->set_window_style_tag(AUIK_TAG_TAB_BAR_POPUP);
        _popup->window_flags = (get_popup_window_flags() | WindowFlagBits::docked) & ~WindowFlagBits::scrollable;
        _popup->show();
        _popup->update_style();

        const auto &popup_style = get_theme()->get_style(
            get_theme()->get_resolved_style(AUIK_TAG_TAB_BAR_POPUP, _popup->id(), 0, StyleState::normal));
        const amal::vec4 popup_padding = popup_style.padding();

        f32 content_width = 0.0f;
        f32 measured_h = popup_padding.y + popup_padding.w;
        for (u32 i = 0; i < _popup->children.size(); ++i)
        {
            auto *child = _popup->children[i];
            if (!child) continue;
            const bool visible = i >= _overflow_start;
            child->set_visible(visible);
            if (!visible) continue;
            if (!child->is_fixed()) child->set_size({0.0f, 0.0f});
            child->update_layout_min_size();
            content_width = amal::max(content_width, child->required_size().x);
            measured_h += child->required_size().y;
        }

        const f32 min_button_width = _overflow_button ? _overflow_button->required_size().x : 0.0f;
        const f32 popup_w = amal::max(content_width + popup_padding.x + popup_padding.z, min_button_width);
        const amal::vec4 viewport = get_main_viewport();
        const f32 below_space = amal::max(viewport.y + viewport.w - (position().y + size().y), 0.0f);
        const f32 desired_h = amal::max(measured_h, AUIK_TAB_BAR_POPUP_ITEM_FALLBACK_HEIGHT);
        const bool need_scroll = desired_h > below_space;
        const f32 popup_h = need_scroll ? amal::max(below_space, AUIK_TAB_BAR_POPUP_ITEM_FALLBACK_HEIGHT) : desired_h;
        if (need_scroll) _popup->window_flags = get_popup_window_flags() | WindowFlagBits::docked;
        else _popup->window_flags = (get_popup_window_flags() | WindowFlagBits::docked) & ~WindowFlagBits::scrollable;
        _popup->set_position({position().x + size().x - popup_w, position().y + size().y - 1.0f});
        _popup->set_size({popup_w, popup_h});
        static_cast<Widget *>(_popup)->update_depth(get_tab_popup_depth_range());
        static_cast<Widget *>(_popup)->update_layout(false);

        content_width = amal::max(popup_w - popup_padding.x - popup_padding.z, 0.0f);
        amal::vec2 cursor = _popup->position() + amal::vec2{popup_padding.x, popup_padding.y};
        for (u32 i = 0; i < _popup->children.size(); ++i)
        {
            auto *child = _popup->children[i];
            if (!child || !child->is_visible()) continue;
            if (!child->is_fixed()) child->set_size({0.0f, 0.0f});
            child->update_layout_min_size();
            if (!child->is_fixed()) child->set_size({content_width, 0.0f});
            else child->set_size({content_width, child->size().y});
            child->set_position(cursor);
            child->update_layout(true);
            cursor.y += child->required_size().y;
        }
    }

    void TabBar::open_popup()
    {
        if (!popup() || !_popup || _overflow_start >= _tabs.size()) return;
        if (_open) return;
        _open = true;
        if (_overflow_button)
        {
            _overflow_button->set_open(true);
            _overflow_button->start_icon_animation(true);
        }
        update_popup_layout();
    }

    void TabBar::close_popup()
    {
        const bool was_open = _open;
        _open = false;
        if (_overflow_button)
        {
            _overflow_button->set_open(false);
            if (was_open) _overflow_button->start_icon_animation(false);
        }
        if (_popup) _popup->hide();
    }

    void TabBar::toggle_popup()
    {
        if (_open) close_popup();
        else open_popup();
    }

} // namespace auik::v2
