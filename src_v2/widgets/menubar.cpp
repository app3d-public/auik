#include <auik/v2/auik.hpp>
#include <auik/v2/detail/depth.hpp>
#include <auik/v2/detail/rect.hpp>
#include <auik/v2/pipelines.hpp>
#include <auik/v2/widgets/menubar.hpp>
#include <auik/v2/widgets/separator.hpp>
#include <auik/v2/widgets/titlebar.hpp>
#include <auik/v2/widgets/window.hpp>

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

    static inline amal::vec2 get_root_menu_popup_depth_range(MenuBar::PopupDepthMode mode)
    {
        amal::vec2 overlay_range = detail::get_global_foreground_depth_range();
        if (mode != MenuBar::PopupDepthMode::root_overlay_next) return overlay_range;
        amal::vec2 next_range{};
        assign_next_depth(overlay_range, next_range);
        return next_range;
    }

    static void sync_widget_after_style_update(Widget *widget, StyleUpdateFlags style_flags)
    {
        if (!widget || style_flags == StyleUpdateFlagBits::none) return;

        auto &ctx = detail::get_context();
        if (style_flags & StyleUpdateFlagBits::parent_layout)
        {
            Widget *target = widget->parent() ? widget->parent() : widget;
            while (target->parent() && !target->is_fixed_bounds() &&
                   target->viewport_layout().mode == ViewportLayoutMode::none)
                target = target->parent();

            const bool needs_root_layout = !target->parent() && target->viewport_layout().mode != ViewportLayoutMode::none;
            target->update_layout(false);
            if (needs_root_layout) detail::mark_layout_dirty();
            else
            {
                target->update_draw_commands(get_draw_reason_from_style_update(style_flags));
                ctx.dirty_flags |= DirtyFlagBits::redraw | DirtyFlagBits::hit_rect_update;
            }
            return;
        }

        if (style_flags & StyleUpdateFlagBits::layout)
        {
            widget->update_layout(false);
            ctx.dirty_flags |= DirtyFlagBits::hit_rect_update;
        }
        if (style_flags & StyleUpdateFlagBits::redraw)
        {
            widget->update_draw_commands(get_draw_reason_from_style_update(style_flags));
            ctx.dirty_flags |= DirtyFlagBits::redraw;
        }
    }

    class MenuBar::PopupItem final : public Widget
    {
    public:
        PopupItem(u32 owner_id, u32 item_id, u32 hit_element_id, const ItemData *data, Widget *parent)
            : Widget(AUIK_TAG_COMBO_BOX_ITEM, WidgetFlagBits::visible | WidgetFlagBits::hittable, EventFlagBits::none,
                     parent, {}, AUIK_TAG_COMBO_BOX_ITEM),
              _selected_options({}),
              _style({Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_COMBO_BOX_ITEM}),
              _selected_style({Theme::STYLE_ID_INVALID, _selected_options.tag_id}),
              _label(acul::alloc<Text>(AUIK_TAG_TEXT, data ? data->text : "", amal::vec2{0.0f, 0.0f},
                                       WidgetFlagBits::visible, this, AUIK_STYLE_TAG_NO_PAD,
                                       detail::TextOverflowMode::ellipsis, detail::TextVerticalAlign::center)),
              _shortcut(acul::alloc<Text>(AUIK_TAG_TEXT, data ? data->shortcut : "", amal::vec2{0.0f, 0.0f},
                                          WidgetFlagBits::visible, this, AUIK_TAG_MENU_SHORTCUT,
                                          detail::TextOverflowMode::ellipsis, detail::TextVerticalAlign::center)),
              _item_id(item_id),
              _has_next(data && !data->next.empty())
        {
            _rect.id.widget_id = owner_id;
            _rect.id.element_id = hit_element_id;
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
            StyleUpdateFlags out = resolve_style_selector(_style, _item_id, parent_id, style_state());
            if (selected_style_enabled())
                out |= resolve_style_selector(_selected_style, _item_id, parent_id, StyleState::normal);
            out |= _label->update_style();
            out |= _shortcut->update_style();
            return out;
        }

    private:
        friend class MenuBar;

        void set_selected(bool value) { _selected = value; }
        void sync_selection_state(bool enabled, bool selected)
        {
            set_selected(enabled && selected);
            if (_selected_enabled == enabled) return;
            _selected_enabled = enabled;
            _style = {Theme::STYLE_ID_INVALID,
                      enabled ? AUIK_STYLE_TAG_COMBO_BOX_ITEM_MULTI : AUIK_STYLE_TAG_COMBO_BOX_ITEM};
            _selected_options =
                enabled ? detail::make_selectable_icon_options(AUIK_ICON_CHECKMARK) : detail::SelectableStyleOptions{};
            _selected_style = {Theme::STYLE_ID_INVALID, _selected_options.tag_id};
        }

    public:
        u32 item_id() const { return _item_id; }
        bool has_draw_record() const { return _draw_recorded; }

        void reset_draw_records() override
        {
            _bg = {};
            _selected_bg = {};
            _selected_icon_draw = {};
            _next_icon_draw = {};
            _label->reset_draw_records();
            _shortcut->reset_draw_records();
            _draw_recorded = false;
        }

        void update_layout_min_size() override
        {
            _label->set_layout_size({0.0f, 0.0f});
            _shortcut->set_layout_size({0.0f, 0.0f});
            _label->update_layout_min_size();
            _shortcut->update_layout_min_size();
            ensure_next_icon_resources();
            ensure_selected_icon_resources();
            const auto &style = get_theme()->get_style(_style.id);
            const amal::vec4 margin = style.margin();
            const amal::vec4 padding = style.padding();
            const f32 spacing = amal::max(style.inline_spacing(), 8.0f);
            const bool has_shortcut = !_shortcut->text().empty();
            const f32 selected_icon_w = selected_icon_slot_width(style);
            f32 w = selected_icon_w + _label->required_size().x;
            if (has_shortcut) w += spacing + _shortcut->required_size().x;
            if (_has_next) w += spacing + next_icon_size(style).x;
            const f32 h =
                amal::max(selected_icon_size(style).y,
                          amal::max(_label->required_size().y, amal::max(_shortcut->required_size().y,
                                                                         _has_next ? next_icon_size(style).y : 0.0f)));
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
            set_layout_size(widget_size);
            Widget::update_layout(true);
            set_clip_id(parent()->content_clip_id());

            const amal::vec2 content_pos = position() + amal::vec2{padding.x, padding.y};
            const amal::vec2 content_size = {amal::max(size().x - padding.x - padding.z, 0.0f),
                                             amal::max(size().y - padding.y - padding.w, 0.0f)};
            const f32 selected_icon_w = selected_icon_slot_width(style);
            const amal::vec2 icon_size = next_icon_size(style);
            const amal::vec2 next_size = _has_next ? icon_size : amal::vec2{0.0f, 0.0f};
            const bool has_shortcut = !_shortcut->text().empty();
            const amal::vec2 shortcut_required = has_shortcut ? _shortcut->required_size() : amal::vec2{0.0f, 0.0f};
            const f32 right_w = next_size.x + (next_size.x > 0.0f ? spacing : 0.0f) + shortcut_required.x +
                                (shortcut_required.x > 0.0f ? spacing : 0.0f);
            rebuild_selected_icon_layout(style, content_pos, content_size);
            _label->set_layout_size({amal::max(content_size.x - selected_icon_w - right_w, 0.0f), content_size.y});
            _label->set_position({content_pos.x + selected_icon_w, content_pos.y});
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
                const auto shortcut_style_id = get_theme()->get_resolved_style(
                    AUIK_STYLE_TAG_MENU_SHORTCUT, _shortcut->id(), id(), _shortcut->style_state());
                const auto &shortcut_style = get_theme()->get_style(shortcut_style_id);
                const amal::vec4 shortcut_margin = shortcut_style.margin();
                const amal::vec2 shortcut_size = {
                    amal::max(shortcut_required.x - shortcut_margin.x - shortcut_margin.z, 0.0f),
                    amal::max(content_size.y - shortcut_margin.y - shortcut_margin.w, 0.0f)};
                right_x -= shortcut_required.x;
                _shortcut->set_position({right_x, content_pos.y});
                _shortcut->set_layout_size(shortcut_size);
                _shortcut->update_layout(true);
            }
        }

        void translate(const amal::vec2 &delta) override
        {
            Widget::translate(delta);
            _label->translate(delta);
            _shortcut->translate(delta);
            _next_icon_rect.offset += delta;
            _selected_icon_rect.bounds.offset += delta;
        }

        void rebuild_clip_rects() override
        {
            set_clip_id(parent()->content_clip_id());
            _bg.hit_id = AUIK_INVALID_DRAW_DATA_ID;
            _selected_bg.hit_id = AUIK_INVALID_DRAW_DATA_ID;
            _selected_icon_draw.hit_id = AUIK_INVALID_DRAW_DATA_ID;
            _next_icon_draw.hit_id = AUIK_INVALID_DRAW_DATA_ID;
            _selected_icon_rect.clip_id = clip_id();
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
            _selected_icon_rect.depth = next_depth(_content_depth_range);
            _selected_icon_rect.hit_depth = _selected_icon_rect.depth;
        }

        void back_hit_depth() override
        {
            Widget::back_hit_depth();
            _label->back_hit_depth();
            _shortcut->back_hit_depth();
        }

        void restore_hit_depth() override
        {
            Widget::restore_hit_depth();
            _label->restore_hit_depth();
            _shortcut->restore_hit_depth();
        }

        void draw(DrawCtx &ctx) override
        {
            if (ctx.is_recording()) _draw_recorded = true;
            else if (ctx.is_invalidating()) _draw_recorded = false;
            auto *quads_stream = get_primary_quads_stream();
            QuadsInstanceData bg{};
            bg.rect = bounds();
            bg.z_order = get_z_order();
            if (_selected && selected_style_enabled())
            {
                QuadsInstanceData selected_bg = bg;
                const bool selected_visible =
                    fill_quads_instance_by_style(get_theme()->get_style(_selected_style.id), clip_id(), selected_bg);
                emit_quads_instance(ctx, quads_stream, _selected_bg, selected_bg, get_rect(), selected_visible,
                                    ctx.emit_hit_rect);
            }
            const bool visible = fill_quads_instance_by_style(get_theme()->get_style(_style.id), clip_id(), bg);
            emit_quads_instance(ctx, quads_stream, _bg, bg, get_rect(), visible, ctx.emit_hit_rect);
            _label->draw(ctx);
            if (!_shortcut->text().empty()) _shortcut->draw(ctx);
            draw_selected_icon(ctx);
            draw_next_icon(ctx);
        }

    private:
        bool selected_style_enabled() const { return _selected_options.tag_id != detail::AUIK_SELECTABLE_STYLE_NONE; }
        bool selected_icon_enabled() const { return _selected_options.icon_id != detail::AUIK_SELECTABLE_STYLE_NONE; }

        void ensure_selected_icon_resources()
        {
            if (!selected_icon_enabled()) return;
            if (_selected_icon_texture.handle != 0) return;
            if (auto *cached = get_cached_image(_selected_options.icon_id))
            {
                _selected_icon_texture = cached->texture_id();
                _selected_icon_size = cached->size();
                _selected_icon_uv_rect = {cached->uv_offset(), cached->uv_size()};
                return;
            }
            _selected_icon_texture = {};
            _selected_icon_size = {0.0f, 0.0f};
            _selected_icon_uv_rect = {{0.0f, 0.0f}, {1.0f, 1.0f}};
        }

        amal::vec2 selected_icon_size(const Style &style) const
        {
            if (!selected_icon_enabled()) return {0.0f, 0.0f};
            if (_selected_icon_size.x > 0.0f && _selected_icon_size.y > 0.0f) return _selected_icon_size;
            return {style.text_size(), style.text_size()};
        }

        f32 selected_icon_slot_width(const Style &style)
        {
            if (!selected_icon_enabled()) return 0.0f;
            ensure_selected_icon_resources();
            return selected_icon_size(style).x + amal::max(style.inline_spacing(), 8.0f);
        }

        void rebuild_selected_icon_layout(const Style &style, const amal::vec2 &content_pos,
                                          const amal::vec2 &content_size)
        {
            if (!selected_icon_enabled()) return;
            ensure_selected_icon_resources();
            const amal::vec2 icon_size = selected_icon_size(style);
            _selected_icon_rect.bounds = {
                {content_pos.x, content_pos.y + amal::max((content_size.y - icon_size.y) * 0.5f, 0.0f)}, icon_size};
            _selected_icon_rect.clip_id = clip_id();
            _selected_icon_rect.depth = next_depth(_content_depth_range);
            _selected_icon_rect.hit_depth = _selected_icon_rect.depth;
        }

        void draw_selected_icon(DrawCtx &ctx)
        {
            if (!_selected || !selected_icon_enabled()) return;
            ensure_selected_icon_resources();
            auto *stream = get_primary_textured_quads_stream();
            TextureID texture = _selected_icon_texture;
            if (!stream || texture.handle == 0) return;
            if ((detail::get_context().dirty_flags & DirtyFlagBits::textures) ||
                texture.bind_slot == AUIK_INVALID_DRAW_DATA_ID)
                texture.bind_slot = get_texture_bind_slot(texture.handle);
            if (texture.bind_slot == AUIK_INVALID_DRAW_DATA_ID) return;

            TexturesInstanceData icon{};
            icon.rect = _selected_icon_rect.bounds;
            icon.uv_rect = _selected_icon_uv_rect;
            icon.tint_color = get_theme()->get_style(_style.id).text_color();
            icon.z_order = _selected_icon_rect.depth;
            icon.texture_id = static_cast<u16>(texture.bind_slot);
            icon.clip_id = clip_id();
            icon.flags = AUIK_TEXTURE_INSTANCE_TEXT_BIT;
            ctx.emit(stream, _selected_icon_draw, &icon, _selected_icon_rect, false);
        }

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
        DrawDataID _selected_icon_draw{};
        DrawDataID _next_icon_draw{};
        detail::SelectableStyleOptions _selected_options{};
        StyleSelector _style;
        StyleSelector _selected_style;
        Text *_label = nullptr;
        Text *_shortcut = nullptr;
        TextureID _selected_icon_texture{};
        amal::rect _selected_icon_uv_rect{{0.0f, 0.0f}, {1.0f, 1.0f}};
        amal::vec2 _selected_icon_size{0.0f, 0.0f};
        detail::RectData _selected_icon_rect{};
        TextureID _next_icon_texture{};
        amal::rect _next_icon_uv_rect{{0.0f, 0.0f}, {1.0f, 1.0f}};
        amal::vec2 _next_icon_size{0.0f, 0.0f};
        amal::rect _next_icon_rect{};
        amal::vec2 _content_depth_range{0.0f, 1.0f};
        u32 _item_id = 0u;
        bool _has_next = false;
        bool _selected = false;
        bool _selected_enabled = false;
        bool _draw_recorded = false;
    };

    MenuBar::MenuBar(u32 id, acul::vector<acul::string> items, amal::vec2 size, WidgetFlags widget_flags,
                     Widget *parent)
        : TabBar(id, std::move(items), TabBarFlagBits::none, size, widget_flags, parent, 0.0f, 0u,
                 AUIK_STYLE_TAG_MENU_BAR_ITEM, AUIK_STYLE_TAG_MENU_BAR_ITEM, AUIK_STYLE_TAG_COMBO_BOX_ITEM)
    {
        add_event_flags(EventFlagBits::focus);
        set_style_tag(AUIK_STYLE_TAG_WINDOW_MENU_BAR);
        _menu_style = {Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_WINDOW_MENU_BAR};
        for (u32 i = 0; i < _tabs.size(); ++i)
        {
            if (i < _element_ids.size() && _tabs[i]) ensure_item(_element_ids[i], _tabs[i]->text());
        }
        const u32 prev_selected = selected_id();
        const u32 prev_selected_index = find_index_by_element_id(prev_selected);
        if (prev_selected_index < _tabs.size() && _tabs[prev_selected_index])
            _tabs[prev_selected_index]->set_selected(false);
        update_root_item_state(prev_selected);
    }

    MenuBar::~MenuBar()
    {
        _open_path.clear();
        for (auto *popup : _popups) acul::release(popup);
        _popups.clear();
    }

    void MenuBar::set_menu_style_tag(u32 tag_id)
    {
        if (_menu_style.tag_id == tag_id) return;
        _style = {Theme::STYLE_ID_INVALID, tag_id};
        set_rect_tag_id(tag_id);
        _menu_style = {Theme::STYLE_ID_INVALID, tag_id};
    }

    void MenuBar::set_menu_item_style_tag(u32 tag_id)
    {
        if (_item_style_tag == tag_id) return;
        _item_style_tag = tag_id;
        for (auto *tab : _tabs)
        {
            if (!tab) continue;
            tab->set_style_tag(tag_id);
            tab->set_selected_style_tag(tag_id);
        }
    }

    void MenuBar::set_popup_depth_mode(PopupDepthMode mode)
    {
        if (_popup_depth_mode == mode) return;
        _popup_depth_mode = mode;
    }

    void MenuBar::set_selected_enabled(bool value)
    {
        if (_selected_enabled == value) return;
        _selected_enabled = value;
    }

    void MenuBar::sync_selection_mode_changes()
    {
        for (auto *popup : _popups)
        {
            if (!popup) continue;
            for (auto *child : popup->children)
            {
                if (!child || child->get_rect().id.tag_id != AUIK_TAG_COMBO_BOX_ITEM) continue;
                auto *item = static_cast<PopupItem *>(child);
                item->sync_selection_state(_selected_enabled, is_item_selected(popup_child_item_id(child)));
                const auto style_flags = item->update_style();
                sync_widget_after_style_update(item, style_flags);
            }
        }
        detail::mark_host_refresh_request();
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
        if (const auto *item = find_item(element_id)) return item->selected;
        return false;
    }

    void MenuBar::set_item_selected(u32 element_id, bool selected)
    {
        auto *item = find_item(element_id);
        if (!item || item->selected == selected) return;
        item->selected = selected;
        if (mark_changed()) return;
        sync_popup_item_states();
        detail::get_context().dirty_flags |= DirtyFlagBits::redraw;
        detail::mark_host_refresh_request();
    }

    void MenuBar::set_selected_items(const acul::vector<u32> &element_ids)
    {
        bool changed = false;
        auto contains = [&](u32 id) {
            for (u32 element_id : element_ids)
                if (element_id == id) return true;
            return false;
        };
        for (auto &item : _items)
        {
            const bool next_selected = contains(item.id);
            if (item.selected != next_selected) changed = true;
            item.selected = next_selected;
        }
        if (!changed) return;
        if (mark_changed()) return;
        sync_popup_item_states();
        detail::get_context().dirty_flags |= DirtyFlagBits::redraw;
        detail::mark_host_refresh_request();
    }

    u32 MenuBar::append_item(acul::string text, acul::unique_function<void(ClickEvent &)> callback)
    {
        return append_root_suffix_item(std::move(text), std::move(callback));
    }

    MenuBar::RuntimeSuffixGroup &MenuBar::ensure_runtime_suffix_group(u32 group_index)
    {
        while (_runtime_suffix_groups.size() <= group_index) _runtime_suffix_groups.push_back({});
        return _runtime_suffix_groups[group_index];
    }

    const MenuBar::RuntimeSuffixGroup *MenuBar::runtime_suffix_group(u32 group_index) const
    {
        return group_index < _runtime_suffix_groups.size() ? &_runtime_suffix_groups[group_index] : nullptr;
    }

    void MenuBar::increment_runtime_suffix_group(u32 group_index) { ++ensure_runtime_suffix_group(group_index).count; }

    u32 MenuBar::push_root_suffix_group()
    {
        const u32 group_index = static_cast<u32>(_runtime_suffix_groups.size());
        _runtime_suffix_groups.push_back({});
        bool has_root_items = !_element_ids.empty();
        if (!has_root_items)
        {
            for (const auto &item : _items)
            {
                if (!item.runtime_suffix_root) continue;
                has_root_items = true;
                break;
            }
        }
        if (has_root_items && (_items.empty() || !_items.back().separator))
        {
            append_root_suffix_separator(group_index);
        }
        return group_index;
    }

    void MenuBar::pop_root_suffix_group()
    {
        if (_runtime_suffix_groups.empty()) return;
        erase_root_suffix_group(static_cast<u32>(_runtime_suffix_groups.size()) - 1u);
    }

    u32 MenuBar::append_root_suffix_item(acul::string text, acul::unique_function<void(ClickEvent &)> callback)
    {
        if (_runtime_suffix_groups.empty()) push_root_suffix_group();
        return append_root_suffix_item(static_cast<u32>(_runtime_suffix_groups.size()) - 1u, std::move(text),
                                       std::move(callback));
    }

    u32 MenuBar::append_root_suffix_separator()
    {
        if (_runtime_suffix_groups.empty()) push_root_suffix_group();
        return append_root_suffix_separator(static_cast<u32>(_runtime_suffix_groups.size()) - 1u);
    }

    u32 MenuBar::append_root_suffix_item(u32 group_index, acul::string text,
                                         acul::unique_function<void(ClickEvent &)> callback)
    {
        const u32 element_id = _next_element_id++;
        ItemData item{};
        item.id = element_id;
        item.text = std::move(text);
        item.callback = std::move(callback);
        item.runtime_suffix = true;
        item.runtime_suffix_root = true;
        item.runtime_suffix_group = group_index;
        _items.push_back(std::move(item));
        increment_runtime_suffix_group(group_index);
        return element_id;
    }

    u32 MenuBar::append_root_suffix_separator(u32 group_index)
    {
        if (root_suffix_ends_with_separator(group_index)) { return _items.empty() ? 0u : _items.back().id; }
        const u32 element_id = _next_element_id++;
        ItemData item{};
        item.id = element_id;
        item.separator = true;
        item.runtime_suffix = true;
        item.runtime_suffix_root = true;
        item.runtime_suffix_group = group_index;
        _items.push_back(std::move(item));
        increment_runtime_suffix_group(group_index);
        return element_id;
    }

    bool MenuBar::has_root_suffix(u32 group_index) const
    {
        const auto *group = runtime_suffix_group(group_index);
        return group && !group->empty();
    }

    bool MenuBar::root_suffix_ends_with_separator(u32 group_index) const
    {
        return !_items.empty() && _items.back().runtime_suffix && _items.back().runtime_suffix_group == group_index &&
               _items.back().separator;
    }

    void MenuBar::erase_root_suffix_group(u32 group_index)
    {
        auto *group = group_index < _runtime_suffix_groups.size() ? &_runtime_suffix_groups[group_index] : nullptr;
        if (!group) return;
        if (group->empty())
        {
            if (group_index + 1u == _runtime_suffix_groups.size()) { _runtime_suffix_groups.pop_back(); }
            return;
        }
        const u32 suffix_count = amal::min(group->count, static_cast<u32>(_items.size()));
        const u32 first_suffix_index = static_cast<u32>(_items.size()) - suffix_count;
        bool tail_match = true;
        for (u32 i = first_suffix_index; i < _items.size(); ++i)
        {
            if (!_items[i].runtime_suffix || _items[i].runtime_suffix_group != group_index)
            {
                tail_match = false;
                break;
            }
        }
        if (tail_match)
        {
            _next_element_id = _items[first_suffix_index].id;
            _items.erase(_items.begin() + first_suffix_index, _items.end());
        }
        else
        {
            acul::vector<ItemData> kept;
            kept.reserve(_items.size() - suffix_count);
            for (auto &item : _items)
            {
                if (item.runtime_suffix && item.runtime_suffix_group == group_index) continue;
                kept.push_back(std::move(item));
            }
            _items = std::move(kept);
        }
        group->count = 0u;
        if (group_index + 1u == _runtime_suffix_groups.size()) { _runtime_suffix_groups.pop_back(); }
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
        const bool runtime_suffix = item->runtime_suffix;
        const u32 runtime_suffix_group = item->runtime_suffix_group;
        if (!item->next.empty())
        {
            const u32 separator_id = _next_element_id++;
            item->next.push_back(separator_id);
            auto &separator = ensure_separator(separator_id);
            separator.runtime_suffix = runtime_suffix;
            separator.runtime_suffix_group = runtime_suffix_group;
            if (runtime_suffix) increment_runtime_suffix_group(runtime_suffix_group);
        }
        for (auto &text : items)
        {
            const u32 child_id = _next_element_id++;
            item = find_item(element_id);
            if (!item) item = &ensure_item(element_id);
            item->next.push_back(child_id);
            group_ids.push_back(child_id);
            auto &child = ensure_item(child_id, text);
            child.runtime_suffix = runtime_suffix;
            child.runtime_suffix_group = runtime_suffix_group;
            if (runtime_suffix) increment_runtime_suffix_group(runtime_suffix_group);
        }
        return group_ids;
    }

    acul::vector<acul::vector<u32>> MenuBar::set_item_next_groups(u32 element_id, MenuGroups groups)
    {
        auto *item = &ensure_item(element_id);
        const bool runtime_suffix = item->runtime_suffix;
        const u32 runtime_suffix_group = item->runtime_suffix_group;
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
                auto &separator = ensure_separator(separator_id);
                separator.runtime_suffix = runtime_suffix;
                separator.runtime_suffix_group = runtime_suffix_group;
                if (runtime_suffix) increment_runtime_suffix_group(runtime_suffix_group);
            }

            acul::vector<u32> group_ids;
            for (auto &text : group)
            {
                const u32 child_id = _next_element_id++;
                item = find_item(element_id);
                if (!item) item = &ensure_item(element_id);
                item->next.push_back(child_id);
                group_ids.push_back(child_id);
                auto &child = ensure_item(child_id, text);
                child.runtime_suffix = runtime_suffix;
                child.runtime_suffix_group = runtime_suffix_group;
                if (runtime_suffix) increment_runtime_suffix_group(runtime_suffix_group);
            }
            out.push_back(std::move(group_ids));
            needs_separator = true;
        }
        return out;
    }

    Widget *MenuBar::find_popup_child_by_transition_id(ElementID id)
    {
        if (id.tag_id != AUIK_TAG_COMBO_BOX_ITEM) return nullptr;

        Widget *fallback = nullptr;
        for (u32 i = static_cast<u32>(_popups.size()); i > 0u; --i)
        {
            auto *popup = _popups[i - 1u];
            if (!popup || !popup->is_visible()) continue;
            for (auto *popup_child : popup->children)
            {
                if (!popup_child) continue;
                const auto &rect = popup_child->get_rect();
                if (rect.id != id) continue;
                if (!fallback) fallback = popup_child;
                const StyleState state = popup_child->style_state();
                if (state == StyleState::hover || state == StyleState::active) return popup_child;
            }
        }
        return fallback;
    }

    StyleUpdateFlags MenuBar::update_popup_transition(ElementID id)
    {
        auto *child = find_popup_child_by_transition_id(id);
        if (!child) return {};
        const u32 item_id = popup_child_item_id(child);
        const StyleState fallback = is_popup_item_focused(item_id) ? StyleState::focus : StyleState::normal;
        child->set_style_state(fallback);
        return child->update_style();
    }

    StyleState MenuBar::resolve_tab_item_state(u32 index, const detail::WidgetStyleSelectorTransition &transition) const
    {
        const StyleState transition_state = TabBar::resolve_tab_item_state(index, transition);
        if (index >= _element_ids.size() || _open_path.empty()) return transition_state;

        const u32 element_id = _element_ids[index];
        if (_open_path[0] != element_id) return transition_state;
        return StyleState::focus;
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
                    if (child->get_rect().id.tag_id == AUIK_TAG_COMBO_BOX_ITEM)
                    {
                        auto *item = static_cast<PopupItem *>(child);
                        item->sync_selection_state(_selected_enabled, is_item_selected(popup_child_item_id(child)));
                    }
                    child->set_style_state(resolve_popup_item_state(child));
                    out |= child->update_style();
                }
            }
        }

        out |= update_popup_transition(transition.prev_id);
        Widget *current_transition_child = nullptr;
        if (transition.current_id.tag_id == AUIK_TAG_COMBO_BOX_ITEM)
        {
            auto *child = find_popup_child_by_transition_id(transition.current_id);
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
                if (child->get_rect().id.tag_id != AUIK_TAG_COMBO_BOX_ITEM) continue;
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
        const amal::vec4 parent_clip = get_layout_parent_clip_rect();
        const amal::vec4 own_clip =
            detail::intersect_rects(parent_clip, {position().x, position().y, size().x, size().y});
        if (_full_clip_id != 0xFFFFu) update_clip_rect(_full_clip_id, own_clip);
        if (_content_clip_id != 0xFFFFu) update_clip_rect(_content_clip_id, own_clip);
        set_clip_id(_full_clip_id != 0xFFFFu ? _full_clip_id : get_layout_parent_clip_id());
    }

    u16 MenuBar::get_layout_parent_clip_id() const { return parent() ? parent()->clip_id() : clip_id(); }

    amal::vec4 MenuBar::get_layout_parent_clip_rect() const
    {
        return parent() ? get_clip_rect(parent()->clip_id())
                        : amal::vec4{position().x, position().y, size().x, size().y};
    }

    amal::vec4 MenuBar::get_popup_clip_rect(Window *popup) const
    {
        if (!popup) return {};
        const amal::vec4 viewport_clip = get_popup_bounds_rect();
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
        const amal::vec2 popup_range =
            _popup_depth_mode == PopupDepthMode::workzone_overlay
                ? get_menu_popup_depth_range(parent() ? parent()->depth_range() : this->depth_range())
                : get_root_menu_popup_depth_range(_popup_depth_mode);
        for (u32 i = 0; i < _popups.size(); ++i)
        {
            if (_popups[i]) static_cast<Widget *>(_popups[i])->update_depth(popup_range);
        }
    }

    void MenuBar::back_hit_depth()
    {
        TabBar::back_hit_depth();
        for (auto *popup : _popups)
            if (popup) static_cast<Widget *>(popup)->back_hit_depth();
    }

    void MenuBar::restore_hit_depth()
    {
        TabBar::restore_hit_depth();
        for (auto *popup : _popups)
            if (popup) static_cast<Widget *>(popup)->restore_hit_depth();
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

    void MenuBar::reset_clip_rect_records()
    {
        TabBar::reset_clip_rect_records();
        for (auto *popup : _popups)
            if (popup) popup->reset_clip_rect_records();
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

    bool MenuBar::draw_popup_child(const ElementID &element_id, DrawCtx &ctx)
    {
        if (!element_id || element_id.widget_id != id() || element_id.tag_id != AUIK_TAG_COMBO_BOX_ITEM) return false;

        Widget *target = find_popup_child_with_depth(element_id.element_id);
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
                    if (rect.id != element_id) continue;
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
        if (!has_record)
        {
            child_ctx.emit_fn = &emit_draw_record;
            child_ctx.reason |= DrawReasonBits::record;
        }
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
            emitted |= draw_popup_child(transition.prev_id, ctx);
            emitted |= draw_popup_child(transition.current_id, ctx);
            if (emitted) return;
        }

        auto *quads_stream = get_primary_quads_stream();
        QuadsInstanceData bg{};
        bg.rect = bounds();
        bg.z_order = get_z_order();
        const bool visible = fill_quads_instance_by_style(get_theme()->get_style(_menu_style.id), clip_id(), bg);
        emit_quads_instance(ctx, quads_stream, _bg, bg, get_rect(), visible, ctx.emit_hit_rect);
        TabBar::draw(ctx);
        if (parent() && parent()->get_rect().id.tag_id == AUIK_TAG_WINDOW)
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
        auto *popup = acul::alloc<Window>(
            AUIK_TAG_MENU_POPUP, "", amal::rect{{0.0f, 0.0f}, {0.0f, 0.0f}}, get_popup_window_flags(),
            WidgetFlagBits::visible | WidgetFlagBits::hittable | WidgetFlagBits::fixed_layout);
        popup->get_rect().id.widget_id = id();
        popup->set_window_style_tag(AUIK_STYLE_TAG_MENU_POPUP);
        popup->set_focus_parent(this);
        popup->hide();
        _popups[depth] = popup;
        return popup;
    }

    amal::vec2 MenuBar::resolve_popup_position(u32 depth, const amal::rect &anchor, const amal::vec2 &popup_size,
                                               u32 popup_id) const
    {
        const amal::vec4 bounds = get_popup_bounds_rect();
        const f32 min_x = bounds.x;
        const f32 min_y = bounds.y;
        const f32 max_x = bounds.x + bounds.z;
        const f32 max_y = bounds.y + bounds.w;
        const f32 anchor_left = anchor.offset.x;
        const f32 anchor_right = anchor.offset.x + anchor.size.x;
        const f32 anchor_bottom = anchor.offset.y + anchor.size.y;

        auto clamp = [&](amal::vec2 pos) {
            const f32 clamped_max_x = amal::max(max_x - popup_size.x, min_x);
            const f32 clamped_max_y = amal::max(max_y - popup_size.y, min_y);
            pos.x = amal::clamp(pos.x, min_x, clamped_max_x);
            pos.y = amal::clamp(pos.y, min_y, clamped_max_y);
            return pos;
        };

        if (depth == 0u) return clamp({anchor_left, anchor_bottom});

        const auto &popup_style = get_theme()->get_style(
            get_theme()->get_resolved_style(AUIK_STYLE_TAG_MENU_POPUP, popup_id, 0, StyleState::normal));
        const auto &item_style = get_theme()->get_style(get_theme()->get_resolved_style(
            AUIK_STYLE_TAG_COMBO_BOX_ITEM, AUIK_TAG_COMBO_BOX_ITEM, popup_id, StyleState::normal));
        return clamp({anchor_right - 1.0f, anchor.offset.y - popup_style.padding().y - item_style.margin().y});
    }

    void MenuBar::layout_popup(u32 depth, const amal::rect &anchor, const acul::vector<u32> &ids)
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
        for (u32 item_id : ids)
        {
            auto *item = find_item(item_id);
            if (!item) continue;
            Widget *row = item->separator
                              ? static_cast<Widget *>(acul::alloc<HSeparator>(WidgetFlagBits::visible, popup))
                              : static_cast<Widget *>(acul::alloc<PopupItem>(id(), item_id, item_id, item, popup));
            if (item->separator)
            {
                row->get_rect().id.widget_id = id();
                row->get_rect().id.element_id = item_id;
                row->set_size({AUIK_F32_STRETCH, AUIK_F32_IGNORE});
            }
            row->set_focus_parent(popup);
            if (!item->separator)
            {
                auto *popup_item = static_cast<PopupItem *>(row);
                popup_item->sync_selection_state(_selected_enabled, is_item_selected(item_id));
            }
            row->set_style_state(is_popup_item_focused(item_id) ? StyleState::focus : StyleState::normal);
            row->update_style();
            row->update_layout_min_size();
            content_w = amal::max(content_w, row->required_size().x);
            content_h += row->required_size().y;
            popup->add_child(row);
        }
        const auto &popup_style = get_theme()->get_style(
            get_theme()->get_resolved_style(AUIK_STYLE_TAG_MENU_POPUP, popup->id(), 0, StyleState::normal));
        const amal::vec4 padding = popup_style.padding();
        const f32 popup_w = content_w + padding.x + padding.z;
        const f32 desired_popup_h = amal::max(content_h + padding.y + padding.w, AUIK_MENU_POPUP_ITEM_FALLBACK_HEIGHT);
        const amal::vec4 popup_bounds = get_popup_bounds_rect();
        const f32 popup_y = depth == 0u ? anchor.offset.y + anchor.size.y : anchor.offset.y;
        const f32 available_h = amal::max(popup_bounds.y + popup_bounds.w - popup_y, 0.0f);
        const bool need_scroll = desired_popup_h > available_h;
        const f32 popup_h = need_scroll ? available_h : desired_popup_h;
        popup->window_flags = get_popup_window_flags() | WindowFlagBits::docked;
        if (!need_scroll) popup->window_flags &= ~WindowFlagBits::scrollable;
        popup->show();
        popup->update_style();
        const amal::vec2 popup_size{popup_w, popup_h};
        popup->set_position(resolve_popup_position(depth, anchor, popup_size, popup->id()));
        popup->set_size(popup_size);
        popup->attach_to_viewport(this->viewport());
        const amal::vec2 popup_range =
            _popup_depth_mode == PopupDepthMode::workzone_overlay
                ? get_menu_popup_depth_range(parent() ? parent()->depth_range() : this->depth_range())
                : get_root_menu_popup_depth_range(_popup_depth_mode);
        popup->update_depth(popup_range);
        refresh_popup_clip_rect(popup);
        popup->update_layout(false);
        refresh_popup_clip_rect(popup);
        static_cast<Widget *>(popup)->update_draw_commands(DrawReasonBits::record);
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
        const auto anchor = _tabs[index]->bounds();
        amal::rect popup_anchor = anchor;
        if (parent() && parent()->get_rect().id.tag_id == AUIK_TAG_TITLEBAR)
            popup_anchor = {{anchor.offset.x, parent()->position().y}, {anchor.size.x, parent()->size().y}};
        layout_popup(0, popup_anchor, item->next);
        update_root_item_state(element_id);
        sync_popup_item_states();
        request_redraw();
    }

    bool MenuBar::open_root_at(u32 element_id, const amal::rect &anchor)
    {
        auto *item = find_item(element_id);
        if (!item || item->next.empty()) return false;
        close_from_depth(0);
        _open_path.clear();
        _open_path.push_back(element_id);
        layout_popup(0, anchor, item->next);
        update_root_item_state(element_id);
        sync_popup_item_states();
        request_redraw();
        return true;
    }

    bool MenuBar::open_root_items_at(const amal::rect &anchor, const acul::vector<u32> &ids)
    {
        if (ids.empty()) return false;
        close_from_depth(0);
        _open_path.clear();
        layout_popup(0, anchor, ids);
        _open_path.push_back(0u);
        sync_popup_item_states();
        request_redraw();
        return true;
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
        layout_popup(depth, anchor, item->next);
        sync_popup_item_states();
        request_redraw();
        return true;
    }

    void MenuBar::close_from_depth(u32 depth)
    {
        const u32 prev_root_id = !_open_path.empty() ? _open_path[0] : 0u;
        for (u32 i = depth; i < _popups.size(); ++i)
        {
            if (!_popups[i]) continue;
            if (_popups[i]->is_visible())
            {
                static_cast<Widget *>(_popups[i])->invalidate_draw_commands(DrawReasonBits::full_redraw);
            }
            _popups[i]->hide();
        }
        if (_open_path.size() > depth) _open_path.erase(_open_path.begin() + depth, _open_path.end());
        if (depth == 0u) update_root_item_state(prev_root_id);
        sync_popup_item_states();
        request_redraw();
    }

    void MenuBar::close_all()
    {
        close_from_depth(0);
        _open_path.clear();
        sync_popup_item_states();
        request_redraw();
    }

    void MenuBar::discard_popups()
    {
        for (auto *popup : _popups)
            if (popup) popup->hide();
        _open_path.clear();
    }

    void MenuBar::request_redraw()
    {
        detail::get_context().dirty_flags |= DirtyFlagBits::redraw;
        detail::mark_host_refresh_request();
    }

    StyleUpdateFlags MenuBar::update_root_item_state(u32 element_id)
    {
        if (element_id == 0u) return StyleUpdateFlagBits::none;
        const u32 index = find_index_by_element_id(element_id);
        if (index >= _tabs.size() || !_tabs[index]) return StyleUpdateFlagBits::none;

        const auto transition = detail::get_widget_style_selector_transition(id());
        auto *tab = _tabs[index];
        tab->set_style_state(resolve_tab_item_state(index, transition));
        return tab->update_style();
    }

    bool MenuBar::is_item_open(u32 element_id) const
    {
        for (u32 open_id : _open_path)
            if (open_id == element_id) return true;
        return false;
    }

    bool MenuBar::is_popup_item_focused(u32 element_id) const { return is_item_open(element_id); }

    StyleState MenuBar::resolve_popup_item_state(Widget *child) const
    {
        if (!child) return StyleState::normal;
        const auto transition = detail::get_widget_style_selector_transition(id());
        if (transition.current_id.tag_id == AUIK_TAG_COMBO_BOX_ITEM &&
            transition.current_id.element_id == child->get_rect().id.element_id)
            return transition.current_state;
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
                if (child->get_rect().id.tag_id == AUIK_TAG_COMBO_BOX_ITEM)
                {
                    auto *item = static_cast<PopupItem *>(child);
                    item->sync_selection_state(_selected_enabled, is_item_selected(popup_child_item_id(child)));
                }
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

    Widget *MenuBar::find_popup_child_with_depth(u32 element_id, u32 *out_depth) const
    {
        for (u32 i = static_cast<u32>(_popups.size()); i > 0u; --i)
        {
            const u32 depth = i - 1u;
            auto *popup = _popups[depth];
            if (!popup || !popup->is_visible()) continue;
            for (auto *child : popup->children)
            {
                if (!child || !child->is_visible()) continue;
                if (child->get_rect().id.element_id != element_id) continue;
                if (out_depth) *out_depth = depth;
                return child;
            }
        }
        return nullptr;
    }

    u32 MenuBar::popup_child_item_id(const Widget *child) const
    {
        if (!child || child->get_rect().id.tag_id != AUIK_TAG_COMBO_BOX_ITEM) return 0u;
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
                update_draw_commands(DrawReasonBits::external);
            });
            request_redraw();
            return;
        }
        if (hover_id.tag_id == AUIK_TAG_COMBO_BOX_ITEM)
        {
            auto *hovered_child = find_popup_child_with_depth(hover_id.element_id);
            if (!hovered_child) return;
            const u32 clicked_item_id = popup_child_item_id(hovered_child);
            auto *item = find_item(clicked_item_id);
            if (!item || item->separator) return;
            if (!item->next.empty()) return;

            const u32 owner_id = id();
            Widget *owner_parent = parent();
            ClickEvent e{};
            e.key = MouseKey::left;
            e.state = KeyPressState::press;
            e.click_count = 1u;
            if (item->callback) item->callback(e);

            const auto &ctx = detail::get_context();
            const auto it = ctx.id_map.find(owner_id);
            if (it == ctx.id_map.end() || it->second != this) return;
            auto *menu = static_cast<MenuBar *>(it->second);
            if (menu->parent() != owner_parent) return;
            if (e.is_prevented_default()) return;
            menu->close_all();
            menu->update_draw_commands(DrawReasonBits::external);
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
                update_draw_commands(DrawReasonBits::external);
            });
            request_redraw();
            return;
        }
        if (hover_id.tag_id == AUIK_TAG_COMBO_BOX_ITEM)
        {
            if (!find_popup_child_with_depth(hover_id.element_id)) return;
            add_render_command<detail::HoverEventTraits>(this, [this, element_id = hover_id.element_id]() {
                u32 depth = 0u;
                auto *child = find_popup_child_with_depth(element_id, &depth);
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
                        update_draw_commands(DrawReasonBits::external);
                    }
                    return;
                }
                if (open_next(item_id, depth + 1u, child->bounds())) update_draw_commands(DrawReasonBits::external);
            });
            request_redraw();
        }
    }

    void MenuBar::on_focus(bool focused)
    {
        if (!focused)
        {
            const auto hover_id = detail::get_context().hover_id;
            if (auto *owner = focus_parent(); owner && hover_id.widget_id == owner->id()) return;
            add_render_command<detail::FocusEventTraits>(this, [this]() {
                close_all();
                update_draw_commands(DrawReasonBits::external);
                redraw_all_commands();
            });
            request_redraw();
        }
    }

    PopupMenu::PopupMenu(u32 id, acul::vector<acul::string> items, WidgetFlags widget_flags, Widget *parent,
                         bool selected_enabled)
        : Widget(id, widget_flags | WidgetFlagBits::hittable, EventFlagBits::click | EventFlagBits::focus, parent, {},
                 AUIK_TAG_POPUP_MENU),
          _button(AUIK_STYLE_TAG_DOCK_TABBAR_MENU, AUIK_TAG_POPUP_MENU_BUTTON, AUIK_ICON_MENU, AUIK_ICON_MENU, false),
          _menu(acul::alloc<MenuBar>(id ^ AUIK_TAG_MENU_POPUP, std::move(items)))
    {
        set_button_update_target(this);
        set_button_hit_id(make_element_id(id, AUIK_TAG_POPUP_MENU_BUTTON, 0u));
        _button.set_element_id(0u);
        _menu->set_parent(parent);
        _menu->set_focus_parent(this);
        _menu->set_popup_depth_mode(MenuBar::PopupDepthMode::root_overlay_next);
        _menu->set_selected_enabled(selected_enabled);
        _menu->set_position({0.0f, 0.0f});
        _menu->set_layout_size({0.0f, 0.0f});
    }

    PopupMenu::PopupMenu(MenuBar *menu, WidgetFlags widget_flags, Widget *parent, bool selected_enabled)
        : Widget(menu ? (menu->id() ^ AUIK_TAG_MENU_POPUP) : AUIK_TAG_POPUP_MENU,
                 widget_flags | WidgetFlagBits::hittable, EventFlagBits::click | EventFlagBits::focus, parent, {},
                 AUIK_TAG_POPUP_MENU),
          _button(AUIK_STYLE_TAG_DOCK_TABBAR_MENU, AUIK_TAG_POPUP_MENU_BUTTON, AUIK_ICON_MENU, AUIK_ICON_MENU, false),
          _menu(menu ? menu : acul::alloc<MenuBar>(AUIK_TAG_MENU_POPUP, acul::vector<acul::string>{}))
    {
        set_button_update_target(this);
        set_button_hit_id(make_element_id(id(), AUIK_TAG_POPUP_MENU_BUTTON, 0u));
        _button.set_element_id(0u);
        _menu->set_parent(parent);
        _menu->set_focus_parent(this);
        _menu->set_popup_depth_mode(MenuBar::PopupDepthMode::root_overlay_next);
        _menu->set_selected_enabled(selected_enabled || _menu->selected_enabled());
        _menu->set_position({0.0f, 0.0f});
        _menu->set_layout_size({0.0f, 0.0f});
    }

    PopupMenu::~PopupMenu()
    {
        if (_menu) acul::release(_menu);
    }

    acul::vector<u32> PopupMenu::root_items()
    {
        acul::vector<u32> ids;
        if (!_menu) return ids;

        for (u32 item_id : _menu->element_ids()) ids.push_back(item_id);
        for (const auto &item : _menu->_items)
        {
            if (item.runtime_suffix && item.runtime_suffix_root) ids.push_back(item.id);
        }
        return ids;
    }

    void PopupMenu::set_open(bool value)
    {
        if (_open == value) return;
        _open = value;
        _button.set_open(value);
        if (_open) open_menu();
        else
        {
            if (_menu) _menu->close_all();
            if (_menu && detail::get_context().focus_id == _menu->id()) focus_widget(nullptr);
            detach_menu_from_popup();
            erase_widget_from_transient_cache(this);
        }
    }

    void PopupMenu::discard_popup()
    {
        _open = false;
        _button.set_open(false);
        if (_menu) _menu->discard_popups();
        if (_menu && detail::get_context().focus_id == _menu->id()) focus_widget(nullptr);
        detach_menu_from_popup();
        erase_widget_from_transient_cache(this);
    }

    void PopupMenu::open_menu()
    {
        if (!_menu) return;
        auto ids = root_items();
        if (ids.empty())
        {
            set_open(false);
            return;
        }
        _menu->set_parent(parent());
        _menu->set_focus_parent(this);
        _menu->attach_to_viewport(this->viewport());
        _menu->set_position({0.0f, 0.0f});
        _menu->set_layout_size({0.0f, 0.0f});
        _menu->update_style();
        _menu->update_layout_min_size();
        attach_menu_for_popup();
        _menu->update_depth(detail::get_global_foreground_depth_range());
        _menu->open_root_items_at(_popup_anchor_overridden ? _popup_anchor_override : _button.bounds(), ids);
        focus_widget(_menu);
        push_widget_to_transient_cache(this);
    }

    StyleUpdateFlags PopupMenu::update_style()
    {
        const auto state = resolve_button_state();
        StyleUpdateFlags out = _button.update_style(id(), parent() ? parent()->id() : 0u, state);
        if (_menu) out |= _menu->update_style();
        return out;
    }

    void PopupMenu::update_layout_min_size()
    {
        _button.update_layout_min_size({0.0f, 0.0f}, true);
        set_required_size(_button.required_size());
    }

    void PopupMenu::update_layout(bool min_size_known)
    {
        if (!min_size_known) update_layout_min_size();
        Widget::update_layout(true);
        _button.update_layout(bounds(), clip_id());
        if (_open) open_menu();
    }

    u32 PopupMenu::get_depth_requirement() const { return _button.get_depth_requirement(); }

    void PopupMenu::update_depth(const amal::vec2 &depth_range)
    {
        Widget::update_depth(depth_range);
        _button.update_depth(this->depth_range());
        if (_menu) _menu->update_depth(detail::get_global_foreground_depth_range());
    }

    void PopupMenu::back_hit_depth()
    {
        Widget::back_hit_depth();
        _button.back_hit_depth();
        if (_menu) _menu->back_hit_depth();
    }

    void PopupMenu::restore_hit_depth()
    {
        Widget::restore_hit_depth();
        _button.restore_hit_depth();
        if (_menu) _menu->restore_hit_depth();
    }

    void PopupMenu::translate(const amal::vec2 &delta)
    {
        if (delta.x == 0.0f && delta.y == 0.0f) return;
        Widget::translate(delta);
        _button.translate(delta);
        if (_open) open_menu();
    }

    void PopupMenu::reset_clip_rect_records()
    {
        Widget::reset_clip_rect_records();
        if (_menu) _menu->reset_clip_rect_records();
    }

    void PopupMenu::rebuild_clip_rects()
    {
        _button.rebuild_clip_rects(clip_id());
        if (_menu) _menu->rebuild_clip_rects();
    }

    void PopupMenu::reset_draw_records()
    {
        _button.reset_draw_records();
        if (_menu) _menu->reset_draw_records();
    }

    void PopupMenu::draw_button(DrawCtx &ctx) { _button.draw(ctx, ctx.emit_hit_rect); }

    void PopupMenu::draw_popups(DrawCtx &ctx)
    {
        if (!_menu) return;
        _menu->draw_popups(ctx);
    }

    void PopupMenu::draw(DrawCtx &ctx)
    {
        draw_button(ctx);
        draw_popups(ctx);
    }

    void PopupMenu::on_click(MouseKey key, KeyPressState state, u32 click_count)
    {
        (void)click_count;
        if (key != MouseKey::left || state != KeyPressState::press) return;
        const auto hover_id = detail::get_context().hover_id;
        if (!is_button_hit(hover_id)) return;
        add_render_command<detail::ClickEventTraits>(this, [this]() { set_open(!_open); });
        detail::mark_host_refresh_request();
    }

    void PopupMenu::on_focus(bool focused)
    {
        if (focused) return;
        if (is_button_hit(detail::get_context().hover_id)) return;
        if (_menu && detail::get_context().focus_id == _menu->id()) return;
        add_render_command<detail::FocusEventTraits>(this, [this]() {
            set_open(false);
            redraw_all_commands();
        });
        detail::mark_host_refresh_request();
    }

    void PopupMenu::on_attach()
    {
        detail::get_context().id_map[id()] = this;
        if (_open) attach_menu_for_popup();
    }

    void PopupMenu::on_detach()
    {
        detail::get_context().id_map.erase(id());
        detach_menu_from_popup();
        erase_widget_from_transient_cache(this);
    }
} // namespace auik::v2
