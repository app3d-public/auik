#include <auik/v2/auik.hpp>
#include <auik/v2/detail/depth.hpp>
#include <auik/v2/detail/rect.hpp>
#include <auik/v2/pipelines.hpp>
#include <auik/v2/widgets/menubar.hpp>
#include <auik/v2/widgets/separator.hpp>

#define AUIK_MENU_POPUP_ITEM_FALLBACK_HEIGHT 24.0f

namespace auik::v2
{
    static inline amal::vec2 get_menu_popup_depth_range(const amal::vec2 &parent_range)
    {
        amal::vec2 overlay_range{};
        assign_next_depth(parent_range, overlay_range);
        amal::vec2 popup_range{};
        assign_next_depth(overlay_range, popup_range);
        return popup_range;
    }

    class MenuBar::PopupItem final : public Widget
    {
    public:
        PopupItem(u32 owner_id, u32 item_id, u32 hit_element_id, const ItemData *data, Widget *parent)
            : Widget(AUIK_TAG_COMBO_BOX_ITEM, WidgetFlagBits::visible | WidgetFlagBits::hittable, EventFlagBits::none,
                     parent, {}, AUIK_TAG_COMBO_BOX_ITEM),
              _style({Theme::STYLE_ID_INVALID, AUIK_TAG_COMBO_BOX_ITEM}),
              _selected_style({Theme::STYLE_ID_INVALID, AUIK_TAG_COMBO_BOX_ITEM_SELECTED}),
              _label(acul::alloc<Text>(AUIK_TAG_TEXT, data ? data->text : "", amal::vec2{0.0f, 0.0f},
                                       WidgetFlagBits::visible, this, AUIK_TAG_NO_PAD,
                                       detail::TextOverflowMode::ellipsis, detail::TextVerticalAlign::center)),
              _shortcut(acul::alloc<Text>(AUIK_TAG_TEXT, data ? data->shortcut : "", amal::vec2{0.0f, 0.0f},
                                          WidgetFlagBits::visible, this, AUIK_TAG_MENU_SHORTCUT,
                                          detail::TextOverflowMode::ellipsis, detail::TextVerticalAlign::center)),
              _item_id(item_id),
              _has_next(data && !data->next.empty())
        {
            _rect.widget_id = owner_id;
            _rect.element_id = hit_element_id;
            _label->set_horizontal_align(detail::TextHorizontalAlign::left);
            _shortcut->set_horizontal_align(detail::TextHorizontalAlign::right);
        }

        ~PopupItem() override
        {
            acul::release(_label);
            acul::release(_shortcut);
        }

        StyleUpdateFlags update_style() override
        {
            const u32 parent_id = parent() ? parent()->id() : 0u;
            StyleUpdateFlags out = resolve_style_selector(_style, id(), parent_id, style_state());
            out |= resolve_style_selector(_selected_style, id(), parent_id, StyleState::normal);
            out |= _label->update_style();
            out |= _shortcut->update_style();
            return out;
        }

        void set_selected(bool value) { _selected = value; }
        u32 item_id() const { return _item_id; }
        bool has_draw_record() const { return _bg.render_id != AUIK_INVALID_DRAW_DATA_ID; }

        void update_layout_min_size() override
        {
            _label->set_size({0.0f, 0.0f});
            _shortcut->set_size({0.0f, 0.0f});
            _label->update_layout_min_size();
            _shortcut->update_layout_min_size();
            ensure_next_icon_resources();
            const auto &style = get_theme()->get_style(_style.id);
            const amal::vec4 margin = style.margin();
            const amal::vec4 padding = style.padding();
            const f32 spacing = amal::max(style.inline_spacing(), 8.0f);
            const bool has_shortcut = !_shortcut->text().empty();
            f32 w = _label->required_size().x;
            if (has_shortcut) w += spacing + _shortcut->required_size().x;
            if (_has_next) w += spacing + next_icon_size(style).x;
            const f32 h = amal::max(_label->required_size().y, amal::max(_shortcut->required_size().y,
                                                                         _has_next ? next_icon_size(style).y : 0.0f));
            set_required_size(
                {w + padding.x + padding.z + margin.x + margin.z, h + padding.y + padding.w + margin.y + margin.w});
        }

        void update_layout(bool min_size_known) override
        {
            if (!min_size_known) update_layout_min_size();
            const auto &style = get_theme()->get_style(_style.id);
            const amal::vec4 margin = style.margin();
            const amal::vec4 padding = style.padding();
            const f32 spacing = amal::max(style.inline_spacing(), 8.0f);
            const amal::vec2 layout_origin = position();
            const amal::vec2 min_inner = {required_size().x - margin.x - margin.z,
                                          required_size().y - margin.y - margin.w};
            const amal::vec2 widget_size = {amal::max(size().x - margin.x - margin.z, min_inner.x),
                                            amal::max(size().y, min_inner.y)};
            set_position({layout_origin.x + margin.x, layout_origin.y + margin.y});
            set_size(widget_size);
            Widget::update_layout(true);
            set_clip_id(parent()->content_clip_id());

            const amal::vec2 content_pos = position() + amal::vec2{padding.x, padding.y};
            const amal::vec2 content_size = {amal::max(size().x - padding.x - padding.z, 0.0f),
                                             amal::max(size().y - padding.y - padding.w, 0.0f)};
            const amal::vec2 icon_size = next_icon_size(style);
            const amal::vec2 next_size = _has_next ? icon_size : amal::vec2{0.0f, 0.0f};
            const bool has_shortcut = !_shortcut->text().empty();
            const amal::vec2 shortcut_required = has_shortcut ? _shortcut->required_size() : amal::vec2{0.0f, 0.0f};
            const f32 right_w = next_size.x + (next_size.x > 0.0f ? spacing : 0.0f) + shortcut_required.x +
                                (shortcut_required.x > 0.0f ? spacing : 0.0f);
            _label->set_size({amal::max(content_size.x - right_w, 0.0f), content_size.y});
            _label->set_position(content_pos);
            _label->update_layout(true);
            f32 right_x = content_pos.x + content_size.x;
            if (_has_next)
            {
                right_x -= next_size.x;
                _next_icon_rect = {{right_x, content_pos.y + amal::max((content_size.y - next_size.y) * 0.5f, 0.0f)},
                                   next_size};
                right_x -= spacing;
            }
            if (has_shortcut)
            {
                const auto shortcut_style_id = get_theme()->get_resolved_style(AUIK_TAG_MENU_SHORTCUT, _shortcut->id(),
                                                                               id(), _shortcut->style_state());
                const auto &shortcut_style = get_theme()->get_style(shortcut_style_id);
                const amal::vec4 shortcut_margin = shortcut_style.margin();
                const amal::vec2 shortcut_size = {
                    amal::max(shortcut_required.x - shortcut_margin.x - shortcut_margin.z, 0.0f),
                    amal::max(content_size.y - shortcut_margin.y - shortcut_margin.w, 0.0f)};
                right_x -= shortcut_required.x;
                _shortcut->set_position({right_x, content_pos.y});
                _shortcut->set_size(shortcut_size);
                _shortcut->update_layout(true);
            }
        }

        void translate(const amal::vec2 &delta) override
        {
            Widget::translate(delta);
            _label->translate(delta);
            _shortcut->translate(delta);
            _next_icon_rect.offset += delta;
        }

        void rebuild_clip_rects() override
        {
            set_clip_id(parent()->content_clip_id());
            _bg.hit_id = AUIK_INVALID_DRAW_DATA_ID;
            _selected_bg.hit_id = AUIK_INVALID_DRAW_DATA_ID;
            _next_icon_draw.hit_id = AUIK_INVALID_DRAW_DATA_ID;
            _label->rebuild_clip_rects();
            _shortcut->rebuild_clip_rects();
        }

        void update_depth(const amal::vec2 &depth_range) override
        {
            Widget::update_depth(depth_range);
            amal::vec2 text_range{};
            assign_next_depth(this->depth_range(), text_range);
            _label->update_depth(text_range);
            _shortcut->update_depth(text_range);
            _content_depth_range = text_range;
        }

        void draw(DrawCtx &ctx) override
        {
            auto *quads_stream = get_primary_quads_stream();
            QuadsInstanceData bg{};
            bg.rect = bounds();
            bg.z_order = get_z_order();
            if (_selected)
            {
                QuadsInstanceData selected_bg = bg;
                const bool selected_visible =
                    fill_quads_instance_by_style(get_theme()->get_style(_selected_style.id), clip_id(), selected_bg);
                if (should_emit_quads_instance(selected_visible, _selected_bg, ctx.emit_hit_rect))
                    ctx.emit(quads_stream, _selected_bg, &selected_bg, get_rect(), ctx.emit_hit_rect);
            }
            const bool visible = fill_quads_instance_by_style(get_theme()->get_style(_style.id), clip_id(), bg);
            if (should_emit_quads_instance(visible, _bg, ctx.emit_hit_rect))
                ctx.emit(quads_stream, _bg, &bg, get_rect(), ctx.emit_hit_rect);
            _label->draw(ctx);
            if (!_shortcut->text().empty()) _shortcut->draw(ctx);
            draw_next_icon(ctx);
        }

    private:
        void ensure_next_icon_resources()
        {
            if (!_has_next) return;
            if (auto *cached = get_cached_image(AUIK_ICON_CHEVRON_RIGHT))
            {
                _next_icon_texture = cached->texture_id();
                _next_icon_size = cached->size();
                _next_icon_uv_rect = {cached->uv_offset(), cached->uv_size()};
                return;
            }
            _next_icon_texture = {};
            _next_icon_size = {0.0f, 0.0f};
            _next_icon_uv_rect = {{0.0f, 0.0f}, {1.0f, 1.0f}};
        }

        amal::vec2 next_icon_size(const Style &style) const
        {
            if (_next_icon_size.x > 0.0f && _next_icon_size.y > 0.0f) return _next_icon_size;
            return {style.text_size(), style.text_size()};
        }

        void draw_next_icon(DrawCtx &ctx)
        {
            if (!_has_next) return;
            ensure_next_icon_resources();
            auto *stream = get_primary_textured_quads_stream();
            TextureID texture = _next_icon_texture;
            if (!stream || texture.handle == 0) return;
            if ((detail::get_context().dirty_flags & DirtyFlagBits::textures) ||
                texture.bind_slot == AUIK_INVALID_DRAW_DATA_ID)
                texture.bind_slot = get_texture_bind_slot(texture.handle);
            if (texture.bind_slot == AUIK_INVALID_DRAW_DATA_ID) return;

            TexturesInstanceData icon{};
            icon.rect = _next_icon_rect;
            icon.uv_rect = _next_icon_uv_rect;
            icon.tint_color = get_theme()->get_style(_style.id).text_color();
            icon.z_order = next_depth(_content_depth_range);
            icon.texture_id = static_cast<u16>(texture.bind_slot);
            icon.clip_id = clip_id();
            icon.flags = AUIK_TEXTURE_INSTANCE_TEXT_BIT;
            ctx.emit(stream, _next_icon_draw, &icon, get_rect(), false);
        }

        DrawDataID _bg{};
        DrawDataID _selected_bg{};
        DrawDataID _next_icon_draw{};
        StyleSelector _style;
        StyleSelector _selected_style;
        Text *_label = nullptr;
        Text *_shortcut = nullptr;
        TextureID _next_icon_texture{};
        amal::rect _next_icon_uv_rect{{0.0f, 0.0f}, {1.0f, 1.0f}};
        amal::vec2 _next_icon_size{0.0f, 0.0f};
        amal::rect _next_icon_rect{};
        amal::vec2 _content_depth_range{0.0f, 1.0f};
        u32 _item_id = 0u;
        bool _has_next = false;
        bool _selected = false;
    };

    MenuBar::MenuBar(u32 id, acul::vector<acul::string> items, amal::vec2 size, WidgetFlags widget_flags,
                     Widget *parent)
        : TabBar(id, std::move(items), TabBarFlagBits::none, size, widget_flags, parent, 0.0f, 0u,
                 AUIK_TAG_MENU_BAR_ITEM, AUIK_TAG_COMBO_BOX_ITEM)
    {
        add_event_flags(EventFlagBits::focus);
        set_style_tag(AUIK_TAG_WINDOW_MENU_BAR);
        _menu_style = {Theme::STYLE_ID_INVALID, AUIK_TAG_WINDOW_MENU_BAR};
        for (u32 i = 0; i < _tabs.size(); ++i)
        {
            if (i < _element_ids.size() && _tabs[i]) ensure_item(_element_ids[i], _tabs[i]->text());
        }
        _selected_element_ids.clear();
        sync_selection_to_widgets();
    }

    MenuBar::~MenuBar()
    {
        _open_path.clear();
        for (auto *popup : _popups) acul::release(popup);
        _popups.clear();
    }

    MenuBar::ItemData *MenuBar::find_item(u32 element_id)
    {
        for (auto &item : _items)
            if (item.id == element_id) return &item;
        return nullptr;
    }

    const MenuBar::ItemData *MenuBar::find_item(u32 element_id) const
    {
        for (const auto &item : _items)
            if (item.id == element_id) return &item;
        return nullptr;
    }

    MenuBar::ItemData &MenuBar::ensure_item(u32 element_id, const acul::string &text)
    {
        if (auto *item = find_item(element_id))
        {
            item->separator = false;
            if (!text.empty()) item->text = text;
            return *item;
        }
        ItemData item{};
        item.id = element_id;
        item.text = text;
        _items.push_back(std::move(item));
        return _items.back();
    }

    MenuBar::ItemData &MenuBar::ensure_separator(u32 element_id)
    {
        if (auto *item = find_item(element_id))
        {
            item->separator = true;
            item->text.clear();
            item->shortcut.clear();
            item->next.clear();
            item->callback = nullptr;
            return *item;
        }
        ItemData item{};
        item.id = element_id;
        item.separator = true;
        _items.push_back(std::move(item));
        return _items.back();
    }

    void MenuBar::set_item_shortcut(u32 element_id, acul::string shortcut)
    {
        ensure_item(element_id).shortcut = std::move(shortcut);
    }

    bool MenuBar::is_item_selected(u32 element_id) const
    {
        for (u32 selected_id : _selected_item_ids)
            if (selected_id == element_id) return true;
        return false;
    }

    void MenuBar::set_item_selected(u32 element_id, bool selected)
    {
        if (!find_item(element_id)) return;
        const bool was_selected = is_item_selected(element_id);
        if (selected == was_selected) return;
        if (selected) _selected_item_ids.push_back(element_id);
        else
        {
            for (u32 i = 0; i < _selected_item_ids.size(); ++i)
            {
                if (_selected_item_ids[i] != element_id) continue;
                _selected_item_ids.erase(_selected_item_ids.begin() + i);
                break;
            }
        }
        sync_popup_item_states();
        detail::get_context().dirty_flags |= DirtyFlagBits::redraw;
        detail::mark_host_refresh_request();
    }

    void MenuBar::set_selected_items(const acul::vector<u32> &element_ids)
    {
        _selected_item_ids.clear();
        for (u32 element_id : element_ids)
        {
            if (!find_item(element_id) || is_item_selected(element_id)) continue;
            _selected_item_ids.push_back(element_id);
        }
        sync_popup_item_states();
        detail::get_context().dirty_flags |= DirtyFlagBits::redraw;
        detail::mark_host_refresh_request();
    }

    acul::vector<u32> MenuBar::set_item_next(u32 element_id, acul::vector<acul::string> items)
    {
        acul::vector<MenuGroup> groups;
        groups.push_back(std::move(items));
        auto grouped_ids = set_item_next_groups(element_id, std::move(groups));
        return grouped_ids.empty() ? acul::vector<u32>{} : std::move(grouped_ids[0]);
    }

    acul::vector<u32> MenuBar::append_item_group(u32 element_id, acul::vector<acul::string> items)
    {
        acul::vector<u32> group_ids;
        if (items.empty()) return group_ids;
        auto *item = &ensure_item(element_id);
        if (!item->next.empty())
        {
            const u32 separator_id = _next_element_id++;
            item->next.push_back(separator_id);
            ensure_separator(separator_id);
        }
        for (auto &text : items)
        {
            const u32 child_id = _next_element_id++;
            item = find_item(element_id);
            if (!item) item = &ensure_item(element_id);
            item->next.push_back(child_id);
            group_ids.push_back(child_id);
            ensure_item(child_id, text);
        }
        return group_ids;
    }

    acul::vector<acul::vector<u32>> MenuBar::set_item_next_groups(u32 element_id, MenuGroups groups)
    {
        auto *item = &ensure_item(element_id);
        item->next.clear();
        acul::vector<acul::vector<u32>> out;
        bool needs_separator = false;
        for (auto &group : groups)
        {
            if (group.empty()) continue;
            if (needs_separator)
            {
                const u32 separator_id = _next_element_id++;
                item = find_item(element_id);
                if (!item) item = &ensure_item(element_id);
                item->next.push_back(separator_id);
                ensure_separator(separator_id);
            }

            acul::vector<u32> group_ids;
            for (auto &text : group)
            {
                const u32 child_id = _next_element_id++;
                item = find_item(element_id);
                if (!item) item = &ensure_item(element_id);
                item->next.push_back(child_id);
                group_ids.push_back(child_id);
                ensure_item(child_id, text);
            }
            out.push_back(std::move(group_ids));
            needs_separator = true;
        }
        return out;
    }

    Widget *MenuBar::find_popup_child_by_transition_id(detail::ElementID id, bool prefer_hovered_at_cursor)
    {
        if (id.tag_id != AUIK_TAG_COMBO_BOX_ITEM) return nullptr;

        if (prefer_hovered_at_cursor)
        {
            if (auto *hovered = find_popup_child_at(id.element_id, detail::get_context().io.mouse_pos)) return hovered;
        }

        Widget *fallback = nullptr;
        for (u32 i = static_cast<u32>(_popups.size()); i > 0u; --i)
        {
            auto *popup = _popups[i - 1u];
            if (!popup || !popup->is_visible()) continue;
            for (auto *popup_child : popup->children)
            {
                if (!popup_child) continue;
                const auto &rect = popup_child->get_rect();
                if (rect.tag_id != id.tag_id || rect.element_id != id.element_id) continue;
                if (!fallback) fallback = popup_child;
                const StyleState state = popup_child->style_state();
                if (state == StyleState::hover || state == StyleState::active) return popup_child;
            }
        }
        return fallback;
    }

    StyleUpdateFlags MenuBar::update_popup_transition(detail::ElementID id)
    {
        auto *child = find_popup_child_by_transition_id(id, false);
        if (!child) return {};
        const u32 item_id = popup_child_item_id(child);
        const StyleState fallback = is_popup_item_focused(item_id) ? StyleState::focus : StyleState::normal;
        child->set_style_state(fallback);
        return child->update_style();
    }

    StyleUpdateFlags MenuBar::update_style()
    {
        StyleUpdateFlags out = TabBar::update_style();
        out |= resolve_style_selector(_menu_style, id(), parent() ? parent()->id() : 0u, style_state());
        const auto transition = detail::get_widget_style_selector_transition(id());
        const bool local_popup_transition =
            transition.prev_id.widget_id == id() || transition.current_id.widget_id == id();
        const bool touches_popup_item = transition.prev_id.tag_id == AUIK_TAG_COMBO_BOX_ITEM ||
                                        transition.current_id.tag_id == AUIK_TAG_COMBO_BOX_ITEM;
        const bool touches_popup_window =
            transition.prev_id.tag_id == AUIK_TAG_MENU_POPUP || transition.current_id.tag_id == AUIK_TAG_MENU_POPUP;

        if (!(local_popup_transition && (touches_popup_item || touches_popup_window)))
        {
            for (auto *popup : _popups)
            {
                if (!popup) continue;
                out |= popup->update_style();
                for (auto *child : popup->children)
                {
                    if (!child) continue;
                    if (child->get_rect().tag_id == AUIK_TAG_COMBO_BOX_ITEM)
                        static_cast<PopupItem *>(child)->set_selected(is_item_selected(popup_child_item_id(child)));
                    child->set_style_state(resolve_popup_item_state(child));
                    out |= child->update_style();
                }
            }
        }

        out |= update_popup_transition(transition.prev_id);
        Widget *current_transition_child = nullptr;
        if (transition.current_id.tag_id == AUIK_TAG_COMBO_BOX_ITEM)
        {
            auto *child = find_popup_child_by_transition_id(transition.current_id, true);
            if (child)
            {
                child->set_style_state(transition.current_state);
                out |= child->update_style();
                current_transition_child = child;
            }
        }
        // Transition prev_id may point to non-combo tag (e.g. popup background) while
        // a previous combo row still has hover/active visual state. Normalize stale rows.
        for (u32 i = static_cast<u32>(_popups.size()); i > 0u; --i)
        {
            auto *popup = _popups[i - 1u];
            if (!popup || !popup->is_visible()) continue;
            for (auto *child : popup->children)
            {
                if (!child || child == current_transition_child) continue;
                if (child->get_rect().tag_id != AUIK_TAG_COMBO_BOX_ITEM) continue;
                const StyleState state = child->style_state();
                if (state != StyleState::hover && state != StyleState::active) continue;
                const u32 item_id = popup_child_item_id(child);
                const StyleState fallback = is_popup_item_focused(item_id) ? StyleState::focus : StyleState::normal;
                child->set_style_state(fallback);
                out |= child->update_style();
            }
        }
        return out;
    }

    void MenuBar::refresh_menu_clip_rects()
    {
        const amal::vec4 parent_clip =
            parent() ? get_clip_rect(parent()->clip_id()) : amal::vec4{position().x, position().y, size().x, size().y};
        const amal::vec4 own_clip =
            detail::intersect_rects(parent_clip, {position().x, position().y, size().x, size().y});
        if (_full_clip_id != 0xFFFFu) update_clip_rect(_full_clip_id, own_clip);
        if (_content_clip_id != 0xFFFFu) update_clip_rect(_content_clip_id, own_clip);
        if (_full_clip_id != 0xFFFFu) set_clip_id(_full_clip_id);
    }

    amal::vec4 MenuBar::get_popup_clip_rect(Window *popup) const
    {
        if (!popup) return {};
        const amal::vec4 viewport_clip = get_main_viewport();
        return detail::intersect_rects(viewport_clip,
                                       {popup->position().x, popup->position().y, popup->size().x, popup->size().y});
    }

    void MenuBar::refresh_popup_clip_rect(Window *popup)
    {
        if (!popup) return;
        const amal::vec4 popup_clip = get_popup_clip_rect(popup);
        popup->ensure_own_clip_rect(popup_clip);
        popup->override_content_clip_rect(popup_clip);
    }

    void MenuBar::update_layout(bool min_size_known)
    {
        TabBar::update_layout(min_size_known);
        refresh_menu_clip_rects();
    }

    void MenuBar::update_depth(const amal::vec2 &depth_range)
    {
        TabBar::update_depth(depth_range);
        const amal::vec2 parent_range = parent() ? parent()->depth_range() : this->depth_range();
        for (u32 i = 0; i < _popups.size(); ++i)
        {
            if (_popups[i]) static_cast<Widget *>(_popups[i])->update_depth(get_menu_popup_depth_range(parent_range));
        }
    }

    void MenuBar::translate(const amal::vec2 &delta)
    {
        TabBar::translate(delta);
        refresh_menu_clip_rects();
        for (auto *popup : _popups)
        {
            if (!popup || !popup->is_visible()) continue;
            static_cast<Widget *>(popup)->translate(delta);
            refresh_popup_clip_rect(popup);
        }
    }

    void MenuBar::rebuild_clip_rects()
    {
        TabBar::rebuild_clip_rects();
        refresh_menu_clip_rects();
        _bg.hit_id = AUIK_INVALID_DRAW_DATA_ID;
        for (auto *popup : _popups)
        {
            if (!popup) continue;
            popup->rebuild_clip_rects();
            if (popup->is_visible()) refresh_popup_clip_rect(popup);
        }
    }

    bool MenuBar::draw_popup_child(const detail::ElementID &element_id, DrawCtx &ctx, bool prefer_cursor_hit)
    {
        if (!element_id || element_id.widget_id != id() || element_id.tag_id != AUIK_TAG_COMBO_BOX_ITEM) return false;

        Widget *target = nullptr;
        if (prefer_cursor_hit) target = find_popup_child_at(element_id.element_id, detail::get_context().io.mouse_pos);

        if (!target)
        {
            Widget *fallback = nullptr;
            for (u32 i = static_cast<u32>(_popups.size()); i > 0u; --i)
            {
                auto *popup = _popups[i - 1u];
                if (!popup || !popup->is_visible()) continue;
                for (auto *child : popup->children)
                {
                    if (!child) continue;
                    const auto &rect = child->get_rect();
                    if (rect.tag_id != element_id.tag_id || rect.element_id != element_id.element_id) continue;
                    if (!fallback) fallback = child;
                    const StyleState state = child->style_state();
                    if (state != StyleState::hover && state != StyleState::active) continue;
                    target = child;
                    break;
                }
                if (target) break;
            }
            if (!target) target = fallback;
        }

        if (!target) return false;
        DrawCtx child_ctx = ctx;
        const bool has_record = static_cast<PopupItem *>(target)->has_draw_record();
        child_ctx.emit_fn = has_record ? &emit_draw_update : &emit_draw_record;
        child_ctx.emit_hit_rect = target->is_hittable();
        target->draw(child_ctx);
        return true;
    }

    void MenuBar::draw(DrawCtx &ctx)
    {
        if (ctx.is_updating() && ctx.reason == DrawReasonBits::none)
        {
            const auto transition = detail::get_widget_style_selector_transition(id());
            bool emitted = draw_transition_targets(ctx);
            emitted |= draw_popup_child(transition.prev_id, ctx, false);
            emitted |= draw_popup_child(transition.current_id, ctx, true);
            if (emitted) return;
        }

        auto *quads_stream = get_primary_quads_stream();
        QuadsInstanceData bg{};
        bg.rect = bounds();
        bg.z_order = get_z_order();
        const bool visible = fill_quads_instance_by_style(get_theme()->get_style(_menu_style.id), clip_id(), bg);
        if (should_emit_quads_instance(visible, _bg, ctx.emit_hit_rect))
            ctx.emit(quads_stream, _bg, &bg, get_rect(), ctx.emit_hit_rect);
        TabBar::draw(ctx);
        if (parent() && parent()->get_rect().tag_id == AUIK_TAG_WINDOW)
        {
            // Window draws menu popups after its content during full redraws.
            // A direct MenuBar update has no such parent pass, so refresh popups here.
            if (ctx.is_updating() && (ctx.reason & (DrawReasonBits::external | DrawReasonBits::transient)))
                draw_popups(ctx);
            return;
        }
        draw_popups(ctx);
    }

    void MenuBar::draw_popups(DrawCtx &ctx)
    {
        for (auto *popup : _popups)
        {
            if (!popup || !popup->is_visible()) continue;
            refresh_popup_clip_rect(popup);
            DrawCtx popup_ctx = ctx;
            popup_ctx.emit_hit_rect = popup->is_hittable();
            popup->draw(popup_ctx);
        }
    }

    Window *MenuBar::ensure_popup(u32 depth)
    {
        while (_popups.size() <= depth) _popups.push_back(nullptr);
        if (_popups[depth]) return _popups[depth];
        auto *popup = acul::alloc<Window>(AUIK_TAG_MENU_POPUP, "", amal::rect{{0.0f, 0.0f}, {0.0f, 0.0f}},
                                          get_popup_window_flags(),
                                          WidgetFlagBits::visible | WidgetFlagBits::hittable | WidgetFlagBits::fixed);
        popup->set_depth_zone(DepthZone::foreground);
        popup->get_rect().widget_id = id();
        popup->set_window_style_tag(AUIK_TAG_MENU_POPUP);
        popup->set_focus_parent(this);
        popup->hide();
        _popups[depth] = popup;
        return popup;
    }

    void MenuBar::layout_popup(u32 depth, const amal::vec2 &pos, const acul::vector<u32> &ids)
    {
        auto *popup = ensure_popup(depth);
        popup->set_parent(parent());
        if (popup->is_visible()) static_cast<Widget *>(popup)->invalidate_draw_commands();
        if (depth == 0u && !_open_path.empty())
        {
            const u32 root_index = find_index_by_element_id(_open_path[0]);
            if (root_index < _tabs.size()) popup->set_focus_parent(_tabs[root_index]);
        }
        else if (depth > 0u && depth - 1u < _open_path.size())
        {
            if (auto *anchor = find_popup_child(_open_path[depth - 1u])) popup->set_focus_parent(anchor);
        }
        popup->clear_children();
        popup->update_style();
        f32 content_w = 0.0f;
        f32 content_h = 0.0f;
        u32 row_index = 0u;
        for (u32 item_id : ids)
        {
            auto *item = find_item(item_id);
            if (!item) continue;
            Widget *row = item->separator
                              ? static_cast<Widget *>(acul::alloc<HSeparator>(WidgetFlagBits::visible, popup))
                              : static_cast<Widget *>(acul::alloc<PopupItem>(id(), item_id, row_index, item, popup));
            if (item->separator)
            {
                row->get_rect().widget_id = id();
                row->get_rect().element_id = row_index;
            }
            row->set_focus_parent(popup);
            if (!item->separator) static_cast<PopupItem *>(row)->set_selected(is_item_selected(item_id));
            row->set_style_state(is_popup_item_focused(item_id) ? StyleState::focus : StyleState::normal);
            row->update_style();
            row->update_layout_min_size();
            content_w = amal::max(content_w, row->required_size().x);
            content_h += row->required_size().y;
            popup->add_child(row, WindowChildLayout::block);
            ++row_index;
        }
        const auto &popup_style = get_theme()->get_style(
            get_theme()->get_resolved_style(AUIK_TAG_MENU_POPUP, popup->id(), 0, StyleState::normal));
        const amal::vec4 padding = popup_style.padding();
        const f32 popup_w = content_w + padding.x + padding.z;
        const f32 popup_h = amal::max(content_h + padding.y + padding.w, AUIK_MENU_POPUP_ITEM_FALLBACK_HEIGHT);
        popup->window_flags = (get_popup_window_flags() | WindowFlagBits::docked) & ~WindowFlagBits::scrollable;
        popup->show();
        popup->update_style();
        popup->set_position(pos);
        popup->set_size({popup_w, popup_h});
        const amal::vec2 parent_range = parent() ? parent()->depth_range() : this->depth_range();
        static_cast<Widget *>(popup)->update_depth(get_menu_popup_depth_range(parent_range));
        static_cast<Widget *>(popup)->update_layout(false);
        refresh_popup_clip_rect(popup);
        f32 cursor_y = popup->position().y + padding.y;
        for (auto *child : popup->children)
        {
            if (!child) continue;
            child->set_position({popup->position().x + padding.x, cursor_y});
            child->set_size({content_w, 0.0f});
            child->update_layout(false);
            cursor_y += child->required_size().y;
        }
        static_cast<Widget *>(popup)->record_draw_commands();
    }

    void MenuBar::open_root(u32 element_id)
    {
        const u32 index = find_index_by_element_id(element_id);
        if (index >= _tabs.size()) return;
        auto *item = find_item(element_id);
        if (!item || item->next.empty()) return;
        close_from_depth(0);
        _open_path.clear();
        _open_path.push_back(element_id);
        set_active_root(element_id);
        const auto anchor = _tabs[index]->bounds();
        layout_popup(0, {anchor.offset.x, position().y + size().y}, item->next);
        sync_popup_item_states();
        request_redraw();
    }

    bool MenuBar::open_next(u32 element_id, u32 depth, const amal::rect &anchor)
    {
        const bool had_open_path_tail = _open_path.size() > depth;
        bool had_visible_tail = false;
        for (u32 i = depth; i < _popups.size(); ++i)
        {
            if (_popups[i] && _popups[i]->is_visible())
            {
                had_visible_tail = true;
                break;
            }
        }

        auto *item = find_item(element_id);
        if (!item || item->next.empty())
        {
            if (!had_open_path_tail && !had_visible_tail) return false;
            close_from_depth(depth);
            return true;
        }

        bool has_visible_deeper = false;
        for (u32 i = depth + 1u; i < _popups.size(); ++i)
        {
            if (_popups[i] && _popups[i]->is_visible())
            {
                has_visible_deeper = true;
                break;
            }
        }
        const bool same_item_open = depth < _open_path.size() && _open_path[depth] == element_id;
        const bool popup_visible = depth < _popups.size() && _popups[depth] && _popups[depth]->is_visible();
        const bool has_deeper_path = _open_path.size() > depth + 1u;
        if (same_item_open && popup_visible && !has_deeper_path && !has_visible_deeper) return false;

        close_from_depth(depth);
        while (_open_path.size() <= depth) _open_path.push_back(0u);
        _open_path[depth] = element_id;
        layout_popup(depth, {anchor.offset.x + anchor.size.x - 1.0f, anchor.offset.y}, item->next);
        sync_popup_item_states();
        request_redraw();
        return true;
    }

    void MenuBar::close_from_depth(u32 depth)
    {
        for (u32 i = depth; i < _popups.size(); ++i)
        {
            if (!_popups[i]) continue;
            if (_popups[i]->is_visible()) static_cast<Widget *>(_popups[i])->invalidate_draw_commands();
            _popups[i]->hide();
        }
        if (_open_path.size() > depth) _open_path.erase(_open_path.begin() + depth, _open_path.end());
        sync_popup_item_states();
        request_redraw();
    }

    void MenuBar::close_all()
    {
        close_from_depth(0);
        _open_path.clear();
        clear_active_root();
        sync_popup_item_states();
        request_redraw();
    }

    void MenuBar::request_redraw()
    {
        detail::get_context().dirty_flags |= DirtyFlagBits::redraw;
        detail::mark_host_refresh_request();
    }

    void MenuBar::set_active_root(u32 element_id)
    {
        _selected_element_ids.clear();
        const u32 index = find_index_by_element_id(element_id);
        if (index < _tabs.size())
        {
            _selected_index = index;
            _selected_element_ids.push_back(element_id);
        }
        sync_selection_to_widgets();
    }

    void MenuBar::clear_active_root()
    {
        _selected_element_ids.clear();
        sync_selection_to_widgets();
    }

    bool MenuBar::is_item_open(u32 element_id) const
    {
        for (u32 open_id : _open_path)
            if (open_id == element_id) return true;
        return false;
    }

    bool MenuBar::is_popup_item_focused(u32 element_id) const
    {
        (void)element_id;
        return false;
    }

    StyleState MenuBar::resolve_popup_item_state(Widget *child) const
    {
        if (!child) return StyleState::normal;
        const auto &ctx = detail::get_context();
        const auto transition = detail::get_widget_style_selector_transition(id());
        if (transition.current_id.tag_id == AUIK_TAG_COMBO_BOX_ITEM &&
            transition.current_id.element_id == child->get_rect().element_id)
        {
            auto *hovered_child = find_popup_child_at(transition.current_id.element_id, ctx.io.mouse_pos);
            if (hovered_child == child) return transition.current_state;
        }
        return is_popup_item_focused(popup_child_item_id(child)) ? StyleState::focus : StyleState::normal;
    }

    void MenuBar::sync_popup_item_states()
    {
        for (auto *popup : _popups)
        {
            if (!popup) continue;
            for (auto *child : popup->children)
            {
                if (!child) continue;
                if (child->get_rect().tag_id == AUIK_TAG_COMBO_BOX_ITEM)
                    static_cast<PopupItem *>(child)->set_selected(is_item_selected(popup_child_item_id(child)));
                child->set_style_state(resolve_popup_item_state(child));
                child->update_style();
            }
        }
    }

    Widget *MenuBar::find_popup_child(u32 element_id) const
    {
        for (u32 i = static_cast<u32>(_popups.size()); i > 0u; --i)
        {
            auto *popup = _popups[i - 1u];
            if (!popup || !popup->is_visible()) continue;
            for (auto *child : popup->children)
            {
                if (!child || !child->is_visible()) continue;
                if (popup_child_item_id(child) == element_id) return child;
            }
        }
        return nullptr;
    }

    Widget *MenuBar::find_popup_child_at(u32 element_id, const amal::vec2 &pos, u32 *out_depth) const
    {
        for (u32 i = static_cast<u32>(_popups.size()); i > 0u; --i)
        {
            const u32 depth = i - 1u;
            auto *popup = _popups[depth];
            if (!popup || !popup->is_visible()) continue;
            for (auto *child : popup->children)
            {
                if (!child || !child->is_visible()) continue;
                if (child->get_rect().element_id != element_id) continue;
                const auto rect = child->bounds();
                const bool inside_x = pos.x >= rect.offset.x && pos.x <= rect.offset.x + rect.size.x;
                const bool inside_y = pos.y >= rect.offset.y && pos.y <= rect.offset.y + rect.size.y;
                if (!inside_x || !inside_y) continue;
                if (out_depth) *out_depth = depth;
                return child;
            }
        }
        return nullptr;
    }

    u32 MenuBar::popup_child_item_id(const Widget *child) const
    {
        if (!child || child->get_rect().tag_id != AUIK_TAG_COMBO_BOX_ITEM) return 0u;
        return static_cast<const PopupItem *>(child)->item_id();
    }

    void MenuBar::on_click(MouseKey key, KeyPressState state, u32 click_count)
    {
        (void)click_count;
        if (key != MouseKey::left || state != KeyPressState::press) return;
        const auto hover_id = detail::get_context().hover_id;
        if (hover_id.widget_id != id()) return;
        if (hover_id.tag_id == _item_style_tag)
        {
            const u32 root_id = hover_id.element_id;
            if (root_id == 0u) return;
            add_render_command<detail::ClickEventTraits>(this, [this, element_id = root_id]() {
                if (!_open_path.empty() && _open_path[0] == element_id) close_all();
                else open_root(element_id);
                update_draw_commands(DrawReasonBits::transient);
            });
            request_redraw();
            return;
        }
        if (hover_id.tag_id == AUIK_TAG_COMBO_BOX_ITEM)
        {
            const amal::vec2 mouse_pos = detail::get_context().io.mouse_pos;
            auto *hovered_child = find_popup_child_at(hover_id.element_id, mouse_pos);
            if (!hovered_child) return;
            add_render_command<detail::ClickEventTraits>(this, [this, element_id = hover_id.element_id, mouse_pos]() {
                auto *child = find_popup_child_at(element_id, mouse_pos);
                if (!child) return;
                auto *item = find_item(popup_child_item_id(child));
                if (!item || item->separator) return;
                if (!item->next.empty()) return;
                if (item->callback)
                {
                    ClickEvent e{};
                    e.key = MouseKey::left;
                    e.state = KeyPressState::press;
                    e.click_count = 1u;
                    item->callback(e);
                    if (e.is_prevented_default()) return;
                }
                close_all();
                update_draw_commands(DrawReasonBits::transient);
            });
            request_redraw();
        }
    }

    void MenuBar::on_hover(HoverState state)
    {
        TabBar::on_hover(state);
        if (_open_path.empty() || state == HoverState::leave) return;
        const auto hover_id = detail::get_context().hover_id;
        if (hover_id.widget_id != id()) return;
        if (hover_id.tag_id == _item_style_tag && hover_id.element_id != 0u && hover_id.element_id != _open_path[0])
        {
            add_render_command<detail::HoverEventTraits>(this, [this, element_id = hover_id.element_id]() {
                open_root(element_id);
                update_draw_commands(DrawReasonBits::transient);
            });
            request_redraw();
            return;
        }
        if (hover_id.tag_id == AUIK_TAG_COMBO_BOX_ITEM)
        {
            const amal::vec2 mouse_pos = detail::get_context().io.mouse_pos;
            if (!find_popup_child_at(hover_id.element_id, mouse_pos)) return;
            add_render_command<detail::HoverEventTraits>(this, [this, element_id = hover_id.element_id, mouse_pos]() {
                u32 depth = 0u;
                auto *child = find_popup_child_at(element_id, mouse_pos, &depth);
                if (!child) return;
                const u32 item_id = popup_child_item_id(child);
                auto *item = find_item(item_id);
                if (!item) return;
                if (item->separator || item->next.empty())
                {
                    bool has_deeper_open = _open_path.size() > depth + 1u;
                    if (!has_deeper_open)
                    {
                        for (u32 i = depth + 1u; i < _popups.size(); ++i)
                        {
                            if (_popups[i] && _popups[i]->is_visible())
                            {
                                has_deeper_open = true;
                                break;
                            }
                        }
                    }
                    if (has_deeper_open)
                    {
                        close_from_depth(depth + 1u);
                        update_draw_commands(DrawReasonBits::transient);
                    }
                    return;
                }
                if (open_next(item_id, depth + 1u, child->bounds())) update_draw_commands(DrawReasonBits::transient);
            });
            request_redraw();
        }
    }

    void MenuBar::on_focus(bool focused)
    {
        if (!focused)
        {
            add_render_command<detail::FocusEventTraits>(this, [this]() {
                close_all();
                update_draw_commands(DrawReasonBits::transient);
            });
            request_redraw();
        }
    }
} // namespace auik::v2
