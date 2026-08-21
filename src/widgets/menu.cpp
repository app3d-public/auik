#include <auik/auik.hpp>
#include <auik/detail/depth.hpp>
#include <auik/detail/rect.hpp>
#include <auik/pipelines.hpp>
#include <auik/widgets/menu.hpp>
#include <auik/widgets/separator.hpp>
#include <auik/widgets/titlebar.hpp>
#include <auik/widgets/window.hpp>
#include "../core/session_stream_utils.hpp"

#define AUIK_MENU_POPUP_ITEM_FALLBACK_HEIGHT 24.0f

namespace auik
{
    static acul::vector<StringView> make_string_views(std::initializer_list<const char *> items)
    {
        acul::vector<StringView> out;
        out.reserve(items.size());
        for (const char *item : items) out.push_back(StringView{item});
        return out;
    }

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
            while (target->parent() && !target->is_fixed()) target = target->parent();
            target->update_layout(false);
            target->update_draw_commands(get_draw_reason_from_style_update(style_flags));
            ctx.dirty_flags |= DirtyFlagBits::redraw | DirtyFlagBits::hit_rect_update;
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

    static bool has_menu_child_groups(const acul::vector<acul::vector<u32>> &groups)
    {
        for (const auto &group : groups)
            if (!group.empty()) return true;
        return false;
    }

    static WidgetFlags get_popup_item_flags(const MenuBar::MenuItem *data)
    {
        WidgetFlags flags = WidgetFlagBits::visible | WidgetFlagBits::hittable;
        if (!data) return flags;
        if (data->is_read_only()) flags |= WidgetFlagBits::read_only;
        if (data->is_disabled()) flags |= WidgetFlagBits::disabled;
        return flags;
    }

    class MenuBar::PopupItem final : public Widget
    {
    public:
        PopupItem(u32 owner_id, u32 item_id, u32 hit_element_id, const MenuItem *data, Widget *parent)
            : Widget(AUIK_TAG_COMBO_BOX_ITEM, get_popup_item_flags(data), EventFlagBits::none, {},
                     AUIK_TAG_COMBO_BOX_ITEM),
              _selected_options({}),
              _style({Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_COMBO_BOX_ITEM}),
              _selected_style({Theme::STYLE_ID_INVALID, _selected_options.tag_id}),
              _label(acul::alloc<Text>(AUIK_TAG_TEXT, data ? data->source_text() : StringView{}, amal::vec2{0.0f, 0.0f},
                                       WidgetFlagBits::visible, make_text_layout_flags(TextOverflowMode::ellipsis))),
              _shortcut(acul::alloc<Text>(AUIK_TAG_TEXT, data ? data->shortcut_source() : StringView{},
                                          amal::vec2{0.0f, 0.0f}, WidgetFlagBits::visible,
                                          make_text_layout_flags(TextOverflowMode::clip))),
              _item_id(item_id),
              _has_next(data && data->has_group_layer())
        {
            set_parent(parent);
            _label->set_style_tag(AUIK_STYLE_TAG_NO_PAD);
            _shortcut->set_style_tag(AUIK_STYLE_TAG_MENU_SHORTCUT);
            _label->set_parent(this);
            _shortcut->set_parent(this);
            sync_text_widget_flags();
            _rect.id.widget_id = owner_id;
            _rect.id.element_id = hit_element_id;
        }

        ~PopupItem() override
        {
            acul::release(_label);
            acul::release(_shortcut);
        }

        StyleUpdateFlags update_style() override
        {
            sync_text_widget_flags();
            const u32 parent_id = parent() ? parent()->id() : 0u;
            StyleUpdateFlags out = resolve_style_selector(_style, _item_id, parent_id, style_state());
            if (selected_style_enabled())
                out |= resolve_style_selector(_selected_style, _item_id, parent_id, StyleState::normal);
            out |= _label->update_style_invalidated();
            out |= _shortcut->update_style_invalidated();
            return out;
        }

    private:
        friend class MenuBar;

        void sync_menu_item_flags(const MenuItem *data)
        {
            if (data && data->is_disabled()) set_disabled();
            else unset_disabled();
            if (data && data->is_read_only()) set_read_only();
            else set_mutable();
            sync_widget_flags();
            sync_text_widget_flags();
        }

        void set_selected(bool value) { _selected = value; }
        void sync_selection_state(bool enabled, bool selected)
        {
            set_selected(selected);
            const bool mode_changed = _selected_enabled != enabled;
            _selected_enabled = enabled;
            _style = {Theme::STYLE_ID_INVALID,
                      enabled ? AUIK_STYLE_TAG_COMBO_BOX_ITEM_MULTI : AUIK_STYLE_TAG_COMBO_BOX_ITEM};
            _selected_options = enabled
                                    ? detail::make_selectable_icon_options(AUIK_ICON_CHECKMARK)
                                    : detail::make_selectable_highlight_options(AUIK_STYLE_TAG_COMBO_BOX_ITEM_SELECTED);
            _selected_style = {Theme::STYLE_ID_INVALID, _selected_options.tag_id};
            if (mode_changed)
            {
                _selected_icon_texture = {};
                _selected_icon_size = {0.0f, 0.0f};
                _selected_bg = {};
                _selected_icon_draw = {};
            }
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

        void update_layout_min_size_force() override
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
            if (layout_measure_required(min_size_known)) update_layout_min_size_force();
            const auto &style = get_theme()->get_style(_style.id);
            const amal::vec4 margin = style.margin();
            const amal::vec4 padding = style.padding();
            const f32 spacing = amal::max(style.inline_spacing(), 8.0f);
            const amal::vec2 layout_origin = position();
            const amal::vec2 min_inner = {required_size().x - margin.x - margin.z,
                                          required_size().y - margin.y - margin.w};
            const amal::vec2 widget_size = {amal::max(size().x - margin.x - margin.z, min_inner.x),
                                            amal::max(size().y - margin.y - margin.w, min_inner.y)};
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
            const amal::vec2 label_required = _label->required_size();
            _label->set_layout_size({amal::max(content_size.x - selected_icon_w - right_w, 0.0f), label_required.y});
            _label->set_position({content_pos.x + selected_icon_w,
                                  content_pos.y + amal::floor((content_size.y - label_required.y) * 0.5f)});
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
                    amal::max(shortcut_required.y - shortcut_margin.y - shortcut_margin.w, 0.0f)};
                right_x -= shortcut_required.x;
                _shortcut->set_position(
                    {right_x, content_pos.y + amal::floor((content_size.y - shortcut_required.y) * 0.5f)});
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
            DrawDataID *hit_ids[] = {&_bg, &_selected_bg, &_selected_icon_draw, &_next_icon_draw};
            invalidate_hit_rect_batch(hit_ids, 4);
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
            if ((ctx.reason & DrawReasonBits::record)) _draw_recorded = true;
            else if ((ctx.reason & DrawReasonBits::invalidate)) _draw_recorded = false;
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
                                    can_emit_hit(ctx));
            }
            const bool draw_state_bg = !_selected || !selected_style_enabled();
            const bool visible =
                draw_state_bg && fill_quads_instance_by_style(get_theme()->get_style(_style.id), clip_id(), bg);
            emit_quads_instance(ctx, quads_stream, _bg, bg, get_rect(), visible, can_emit_hit(ctx));
            _label->draw_local(ctx);
            if (!_shortcut->text().empty()) _shortcut->draw_local(ctx);
            draw_selected_icon(ctx);
            draw_next_icon(ctx);
        }

    private:
        bool selected_style_enabled() const { return _selected_options.tag_id != detail::AUIK_SELECTABLE_STYLE_NONE; }
        bool selected_icon_enabled() const { return _selected_options.icon_id != detail::AUIK_SELECTABLE_STYLE_NONE; }

        void sync_text_widget_flags()
        {
            sync_text_widget_flags(_label);
            sync_text_widget_flags(_shortcut);
        }

        void sync_text_widget_flags(Text *text)
        {
            if (!text) return;
            if (is_disabled()) text->set_disabled();
            else text->unset_disbled();
            if (is_read_only()) text->set_read_only();
            else text->set_mutable();
            auto **chain = text == _label ? &_label_disabled_fx : &_shortcut_disabled_fx;
            if (is_disabled())
            {
                if (!*chain) *chain = text->add_post_effect(get_disabled_post_effect());
            }
            else if (*chain && text->remove_post_effect(*chain)) *chain = nullptr;
        }

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
            icon.flags = AUIK_TEXTURE_INSTANCE_TINT_BIT;
            emit_context_draw(ctx, stream, _selected_icon_draw, &icon, _selected_icon_rect, false);
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
            icon.flags = AUIK_TEXTURE_INSTANCE_TINT_BIT;
            emit_context_draw(ctx, stream, _next_icon_draw, &icon, get_rect(), false);
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
        PostFxChain *_label_disabled_fx = nullptr;
        PostFxChain *_shortcut_disabled_fx = nullptr;
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

    MenuBar::MenuBar(u32 id, const acul::vector<StringView> &items, amal::vec2 inline_size, WidgetFlags widget_flags)
        : Tabbar(id, acul::vector<StringView>{}, TabbarFlagBits::none, widget_flags, inline_size),
          _menu_base(this, AUIK_STYLE_TAG_MENU_BAR_ITEM, AUIK_STYLE_TAG_MENU_BAR_ITEM)
    {
        add_event_flags(EventFlagBits::focus);
        set_style_tag(AUIK_STYLE_TAG_WINDOW_MENU_BAR);
        set_item_style_tag(AUIK_STYLE_TAG_MENU_BAR_ITEM);
        set_selected_item_style_tag(AUIK_STYLE_TAG_MENU_BAR_ITEM);
        _menu_style = {Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_WINDOW_MENU_BAR};
        add_items(items);
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
        _menu_base.set_item_style_tags(tag_id, tag_id);
        set_item_style_tag(tag_id);
        set_selected_item_style_tag(tag_id);
        update_item_tags();
        sync_tags();
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
                const auto style_flags = item->update_style_invalidated();
                sync_widget_after_style_update(item, style_flags);
            }
        }
        mark_host_refresh_request();
    }

    MenuBar::MenuItem *MenuBar::create_item(StringView text, Widget *parent)
    {
        return _menu_base.create_item(text, parent);
    }

    MenuBar::MenuItem *MenuBar::find_item(u32 element_id) { return _menu_base.find_item(element_id); }

    const MenuBar::MenuItem *MenuBar::find_item(u32 element_id) const { return _menu_base.find_item(element_id); }

    MenuBar::MenuItem *MenuBar::item(u32 element_id) { return find_item(element_id); }

    const MenuBar::MenuItem *MenuBar::item(u32 element_id) const { return find_item(element_id); }

    detail::Selectable *MenuBar::create_root_tab(MenuItem *item)
    {
        if (!item) return nullptr;
        auto *tab = acul::alloc<detail::Selectable>(
            make_element_id(id(), _item_style_tag, item->element_id()), item->source_text(), false,
            amal::vec2{AUIK_SIZE_X_INHERIT, AUIK_SIZE_Y_INHERIT}, this, detail::get_selectable_item_flags());
        tab->set_style_tag(_item_style_tag);
        tab->set_selected_style_tag(_selected_item_style_tag);
        tab->set_focus_parent(this);
        tab->update_style_invalidated();
        return tab;
    }

    void MenuBar::append_root_tab(MenuItem *item)
    {
        if (auto *tab = create_root_tab(item))
        {
            Item root_item{};
            root_item.element_id = item->element_id();
            root_item.tab = tab;
            _items.push_back(root_item);
        }
    }

    void MenuBar::release_root_tabs()
    {
        if (!_items.empty()) invalidate_draw_commands(DrawReasonBits::layout);
        for (auto &item : _items) release_item(item);
        _items.clear();
    }

    void MenuBar::set_item_shortcut(u32 element_id, acul::string shortcut)
    {
        if (auto *item = find_item(element_id)) item->set_shortcut(std::move(shortcut));
    }

    void MenuBar::set_popup_viewport(Viewport *viewport)
    {
        if (_popup_viewport == viewport) return;
        _popup_viewport = viewport;
        for (auto *popup : _popups)
            if (popup) popup->attach_to_viewport(get_popup_viewport());
        reposition_open_popups();
        detail::mark_layout_dirty();
    }

    void MenuBar::set_item_disabled(u32 element_id, bool disabled)
    {
        auto *item = find_item(element_id);
        if (!item || item->is_disabled() == disabled) return;

        if (disabled) item->set_disabled();
        else item->unset_disabled();

        if (auto *child = find_popup_child(element_id))
        {
            auto *popup_item = static_cast<PopupItem *>(child);
            popup_item->sync_menu_item_flags(item);
            sync_widget_after_style_update(popup_item, popup_item->update_style_invalidated());
        }
        request_redraw();
    }

    bool MenuBar::is_item_selected(u32 element_id) const
    {
        if (const auto *item = find_item(element_id)) return item->selected();
        return false;
    }

    void MenuBar::set_item_selected(u32 element_id, bool selected)
    {
        auto *item = find_item(element_id);
        if (!item || item->selected() == selected) return;
        item->set_selected(selected);
        if (mark_changed()) return;
        sync_popup_item_states();
        detail::get_context().dirty_flags |= DirtyFlagBits::redraw;
        mark_host_refresh_request();
    }

    void MenuBar::set_selected_items(const acul::vector<u32> &element_ids)
    {
        bool changed = false;
        auto contains = [&](u32 id) {
            for (u32 element_id : element_ids)
                if (element_id == id) return true;
            return false;
        };
        for (auto *item : _menu_base.owned_items())
        {
            if (!item) continue;
            const bool next_selected = contains(item->element_id());
            if (item->selected() != next_selected) changed = true;
            item->set_selected(next_selected);
        }
        if (!changed) return;
        if (mark_changed()) return;
        sync_popup_item_states();
        detail::get_context().dirty_flags |= DirtyFlagBits::redraw;
        mark_host_refresh_request();
    }

    MenuBar::MenuItem *MenuBar::append_item(const char *text, acul::unique_function<void(ClickEvent &)> callback)
    {
        return append_item(StringView{text}, std::move(callback));
    }

    MenuBar::MenuItem *MenuBar::append_item(acul::string text, acul::unique_function<void(ClickEvent &)> callback)
    {
        return append_item(StringView{text}, std::move(callback));
    }

    MenuBar::MenuItem *MenuBar::append_item(StringView text, acul::unique_function<void(ClickEvent &)> callback)
    {
        return append_root_suffix_item(text, std::move(callback));
    }

    void MenuBar::add_items(u32 count)
    {
        _menu_base.root_layer().add_groups(1u);
        auto *group = _menu_base.root_layer()[0];
        if (!group) return;
        const size_t old_size = group->size();
        group->add_items(count);
        for (size_t i = old_size; i < group->size(); ++i) append_root_tab((*group)[i]);
        rebuild_items();
    }

    void MenuBar::add_items(const acul::vector<StringView> &items)
    {
        _menu_base.root_layer().add_groups(1u);
        auto *group = _menu_base.root_layer()[0];
        if (!group) return;
        const size_t old_size = group->size();
        group->add_items(items);
        for (size_t i = old_size; i < group->size(); ++i)
        {
            auto *item = (*group)[i];
            if (!item) continue;
            append_root_tab(item);
        }
        const auto selected = selected_ids();
        const u32 prev_selected = selected.empty() ? 0u : selected[0];
        const u32 prev_selected_index = find_index_by_element_id(prev_selected);
        if (prev_selected_index < _items.size() && _items[prev_selected_index].tab)
            _items[prev_selected_index].tab->set_selected(false);
        update_root_item_state(prev_selected);
        rebuild_items();
    }

    void MenuBar::add_items(std::initializer_list<const char *> items) { add_items(make_string_views(items)); }

    acul::vector<MenuBar::SerializedItem> MenuBar::serialized_items() const
    {
        acul::vector<SerializedItem> out;
        out.reserve(_menu_base.owned_items().size());
        for (const auto *item : _menu_base.owned_items())
        {
            if (!item) continue;
            SerializedItem serialized{};
            serialized.id = item->element_id();
            const auto source = item->source_text();
            serialized.text = source.str ? source.str : "";
            serialized.translated = source.is_translated;
            serialized.shortcut = item->shortcut();
            serialized.selected = item->selected();
            serialized.next_groups = group_layer_ids(item->group_layer());

            out.push_back(std::move(serialized));
        }
        return out;
    }

    void MenuBar::restore_serialized_items(const acul::vector<u32> &root_ids, const acul::vector<SerializedItem> &items)
    {
        release_root_tabs();
        _menu_base.reset();
        _menu_base.configure(this, _item_style_tag, _selected_item_style_tag);

        for (const auto &src : items)
        {
            auto *item = create_item(StringView{src.text.c_str(), src.translated});
            item->set_element_id(src.id);
            item->set_shortcut(src.shortcut);
            item->set_selected(src.selected);
            _menu_base.reserve_next_element_id(src.id);
        }

        for (const auto &src : items)
        {
            auto *item = find_item(src.id);
            if (!item) continue;
            auto *layer = item->add_group_layer(static_cast<u32>(src.next_groups.size()));
            for (u32 group_i = 0u; group_i < src.next_groups.size(); ++group_i)
            {
                const auto &group = src.next_groups[group_i];
                if (group.empty()) continue;
                auto *dst_group = (*layer)[group_i];
                if (!dst_group) continue;
                for (u32 child_id : group)
                {
                    if (auto *child = find_item(child_id)) _menu_base.add_existing_item(dst_group, child);
                }
            }
        }

        _menu_base.root_layer().add_groups(1u);
        auto *root_group = _menu_base.root_layer()[0];
        for (u32 root_id : root_ids)
        {
            auto *item = find_item(root_id);
            if (!item) continue;
            _menu_base.add_existing_item(root_group, item);
            append_root_tab(item);
        }
        rebuild_items();
    }

    MenuBar::RuntimeSuffixGroup &MenuBar::ensure_runtime_suffix_group(u32 group_index)
    {
        while (_runtime_suffix_groups.size() <= group_index) _runtime_suffix_groups.push_back({});
        auto &group = _runtime_suffix_groups[group_index];
        if (!group.group)
        {
            _menu_base.root_layer().add_groups(static_cast<u32>(_menu_base.root_layer().size()) + 1u);
            group.group = _menu_base.root_layer()[_menu_base.root_layer().size() - 1u];
        }
        return group;
    }

    const MenuBar::RuntimeSuffixGroup *MenuBar::runtime_suffix_group(u32 group_index) const
    {
        return group_index < _runtime_suffix_groups.size() ? &_runtime_suffix_groups[group_index] : nullptr;
    }

    u32 MenuBar::push_root_suffix_group()
    {
        const u32 group_index = static_cast<u32>(_runtime_suffix_groups.size());
        _runtime_suffix_groups.push_back({});
        bool has_root_items = !_items.empty();
        if (!has_root_items)
            for (const auto &group : _runtime_suffix_groups)
                if (!group.empty())
                {
                    has_root_items = true;
                    break;
                }
        return group_index;
    }

    void MenuBar::pop_root_suffix_group()
    {
        if (_runtime_suffix_groups.empty()) return;
        erase_root_suffix_group(static_cast<u32>(_runtime_suffix_groups.size()) - 1u);
    }

    MenuBar::MenuItem *MenuBar::append_root_suffix_item(acul::string text,
                                                        acul::unique_function<void(ClickEvent &)> callback)
    {
        return append_root_suffix_item(StringView{text}, std::move(callback));
    }

    MenuBar::MenuItem *MenuBar::append_root_suffix_item(StringView text,
                                                        acul::unique_function<void(ClickEvent &)> callback)
    {
        if (_runtime_suffix_groups.empty()) push_root_suffix_group();
        return append_root_suffix_item(static_cast<u32>(_runtime_suffix_groups.size()) - 1u, text, std::move(callback));
    }

    u32 MenuBar::append_root_suffix_separator()
    {
        if (_runtime_suffix_groups.empty()) push_root_suffix_group();
        return append_root_suffix_separator(static_cast<u32>(_runtime_suffix_groups.size()) - 1u);
    }

    MenuBar::MenuItem *MenuBar::append_root_suffix_item(u32 group_index, acul::string text,
                                                        acul::unique_function<void(ClickEvent &)> callback)
    {
        return append_root_suffix_item(group_index, StringView{text}, std::move(callback));
    }

    MenuBar::MenuItem *MenuBar::append_root_suffix_item(u32 group_index, StringView text,
                                                        acul::unique_function<void(ClickEvent &)> callback)
    {
        auto &suffix = ensure_runtime_suffix_group(group_index);
        if (!suffix.group) return nullptr;
        auto *item = suffix.group->add_item(text);
        if (!item) return nullptr;
        if (callback) item->bind().on_click(std::move(callback));
        return item;
    }

    u32 MenuBar::append_root_suffix_separator(u32 group_index)
    {
        (void)group_index;
        return 0u;
    }

    bool MenuBar::has_root_suffix(u32 group_index) const
    {
        const auto *group = runtime_suffix_group(group_index);
        return group && !group->empty();
    }

    bool MenuBar::root_suffix_ends_with_separator(u32 group_index) const
    {
        (void)group_index;
        return false;
    }

    void MenuBar::erase_root_suffix_group(u32 group_index)
    {
        auto *group = group_index < _runtime_suffix_groups.size() ? &_runtime_suffix_groups[group_index] : nullptr;
        if (!group) return;
        discard_popups();
        if (group->empty())
        {
            if (group_index + 1u == _runtime_suffix_groups.size())
            {
                _runtime_suffix_groups.pop_back();
            }
            return;
        }
        _menu_base.release_group_items(group->group);
        if (group_index + 1u == _runtime_suffix_groups.size())
        {
            _runtime_suffix_groups.pop_back();
        }
    }

    acul::vector<u32> MenuBar::root_ids() const
    {
        acul::vector<u32> out;
        for (auto *group : _menu_base.root_layer().groups())
        {
            if (!group) continue;
            for (auto *item : group->items())
                if (item) out.push_back(item->element_id());
        }
        return out;
    }

    acul::vector<u32> MenuBar::element_ids() const { return root_ids(); }

    acul::vector<acul::vector<u32>> MenuBar::group_layer_ids(const MenuGroupLayer *layer) const
    {
        acul::vector<acul::vector<u32>> out;
        if (!layer) return out;
        for (auto *group : layer->groups())
        {
            if (!group || group->empty()) continue;
            acul::vector<u32> ids;
            ids.reserve(group->size());
            for (auto *item : group->items())
                if (item) ids.push_back(item->element_id());
            if (!ids.empty()) out.push_back(std::move(ids));
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
        return child->update_style_invalidated();
    }

    StyleState MenuBar::resolve_tab_item_state(u32 index, const detail::WidgetStyleSelectorTransition &transition) const
    {
        const StyleState transition_state = Tabbar::resolve_tab_item_state(index, transition);
        if (index >= _items.size() || _open_path.empty()) return transition_state;

        const u32 element_id = _items[index].element_id;
        if (_open_path[0] != element_id) return transition_state;
        return StyleState::focus;
    }

    StyleUpdateFlags MenuBar::update_style()
    {
        const auto transition = detail::get_widget_style_selector_transition(id());
        StyleUpdateFlags out = Tabbar::update_style();
        out |= resolve_style_selector(_menu_style, id(), parent() ? parent()->id() : 0u, style_state());
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
                out |= popup->update_style_invalidated();
                for (auto *child : popup->children)
                {
                    if (!child) continue;
                    if (child->get_rect().id.tag_id == AUIK_TAG_COMBO_BOX_ITEM)
                    {
                        auto *item = static_cast<PopupItem *>(child);
                        item->sync_selection_state(_selected_enabled, is_item_selected(popup_child_item_id(child)));
                    }
                    child->set_style_state(resolve_popup_item_state(child));
                    out |= child->update_style_invalidated();
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
                out |= child->update_style_invalidated();
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
                out |= child->update_style_invalidated();
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

    u16 MenuBar::get_layout_parent_clip_id() const { return parent() ? parent()->content_clip_id() : clip_id(); }

    amal::vec4 MenuBar::get_layout_parent_clip_rect() const
    {
        return parent() ? parent()->get_content_clip_rect()
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
        Tabbar::update_layout(min_size_known);
        refresh_menu_clip_rects();
        reposition_open_popups();
    }

    void MenuBar::update_depth(const amal::vec2 &depth_range)
    {
        Tabbar::update_depth(depth_range);
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
        Tabbar::back_hit_depth();
        for (auto *popup : _popups)
            if (popup) static_cast<Widget *>(popup)->back_hit_depth();
    }

    void MenuBar::restore_hit_depth()
    {
        Tabbar::restore_hit_depth();
        for (auto *popup : _popups)
            if (popup) static_cast<Widget *>(popup)->restore_hit_depth();
    }

    void MenuBar::translate(const amal::vec2 &delta)
    {
        Tabbar::translate(delta);
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
        Tabbar::reset_clip_rect_records();
        for (auto *popup : _popups)
            if (popup) popup->reset_clip_rect_records();
    }

    void MenuBar::rebuild_clip_rects()
    {
        Tabbar::rebuild_clip_rects();
        refresh_menu_clip_rects();
        invalidate_hit_rect(_bg);
        for (auto *popup : _popups)
        {
            if (!popup) continue;
            popup->rebuild_clip_rects();
            if (popup->is_visible()) refresh_popup_clip_rect(popup);
        }
    }

    void MenuBar::draw(DrawCtx &ctx)
    {
        DrawCtx menu_ctx = ctx;
        if (menu_ctx.reason == DrawReasonBits::none) menu_ctx.reason |= DrawReasonBits::external;

        auto *quads_stream = get_primary_quads_stream();
        QuadsInstanceData bg{};
        bg.rect = bounds();
        bg.z_order = get_z_order();
        const bool visible = fill_quads_instance_by_style(get_theme()->get_style(_menu_style.id), clip_id(), bg);
        emit_quads_instance(menu_ctx, quads_stream, _bg, bg, get_rect(), visible, can_emit_hit(menu_ctx));
        Tabbar::draw(menu_ctx);
        if (parent() && parent()->get_rect().id.tag_id == AUIK_TAG_WINDOW)
        {
            // Window draws menu popups after its content during full redraws.
            // A direct MenuBar update has no such parent pass, so refresh popups here.
            if ((!(menu_ctx.reason & DrawReasonBits::record) && !(menu_ctx.reason & DrawReasonBits::invalidate)) &&
                (menu_ctx.reason & (DrawReasonBits::external | DrawReasonBits::transient)))
                draw_popups(menu_ctx);
            return;
        }
        draw_popups(menu_ctx);
    }

    void MenuBar::draw_popups(DrawCtx &ctx)
    {
        for (auto *popup : _popups)
        {
            if (!popup || !popup->is_visible()) continue;
            refresh_popup_clip_rect(popup);
            DrawCtx popup_ctx = ctx;
            popup->draw_local(popup_ctx);
        }
    }

    Window *MenuBar::ensure_popup(u32 depth)
    {
        while (_popups.size() <= depth) _popups.push_back(nullptr);
        if (_popups[depth]) return _popups[depth];
        auto *popup = acul::alloc<Window>(AUIK_TAG_MENU_POPUP, "", amal::rect{{0.0f, 0.0f}, {0.0f, 0.0f}},
                                          get_popup_window_flags(), WidgetFlagBits::visible | WidgetFlagBits::hittable);
        popup->get_rect().id.widget_id = id();
        popup->set_window_style_tag(AUIK_STYLE_TAG_MENU_POPUP);
        popup->set_focus_parent(this);
        popup->unset_visible();
        popup->sync_widget_flags();
        _popups[depth] = popup;
        return popup;
    }

    void MenuBar::reposition_open_popups()
    {
        if (_open_path.empty() || _open_path[0] == 0u) return;

        for (u32 depth = 0u; depth < _popups.size(); ++depth)
        {
            auto *popup = _popups[depth];
            if (!popup || !popup->is_visible()) continue;

            amal::rect anchor{};
            if (depth == 0u)
            {
                const u32 root_index = find_index_by_element_id(_open_path[0]);
                if (root_index >= _items.size() || !_items[root_index].tab) break;
                anchor = resolve_root_popup_anchor(_items[root_index].tab->bounds());
            }
            else
            {
                if (depth >= _open_path.size() || _open_path[depth] == 0u) break;
                auto *anchor_child = find_popup_child(_open_path[depth]);
                if (!anchor_child) break;
                anchor = anchor_child->bounds();
            }

            const amal::vec2 next_pos = resolve_popup_position(depth, anchor, popup->size(), popup->id());
            const amal::vec2 delta = next_pos - popup->position();
            if (delta.x != 0.0f || delta.y != 0.0f) static_cast<Widget *>(popup)->translate(delta);
            refresh_popup_clip_rect(popup);
        }
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
        const f32 right_x = anchor_right - 1.0f;
        const f32 left_x = anchor_left - popup_size.x + 1.0f;
        const f32 popup_y = anchor.offset.y - popup_style.padding().y - item_style.margin().y;
        const bool fits_right = right_x + popup_size.x <= max_x;
        const bool fits_left = left_x >= min_x;
        if (fits_right) return clamp({right_x, popup_y});
        if (fits_left) return clamp({left_x, popup_y});

        const f32 space_right = max_x - anchor_right;
        const f32 space_left = anchor_left - min_x;
        return clamp({space_left > space_right ? left_x : right_x, popup_y});
    }

    void MenuBar::layout_popup(u32 depth, const amal::rect &anchor, const acul::vector<acul::vector<u32>> &groups)
    {
        auto *popup = ensure_popup(depth);
        popup->set_parent(parent());
        if (popup->is_visible()) static_cast<Widget *>(popup)->invalidate_draw_commands();
        if (depth == 0u && !_open_path.empty())
        {
            const u32 root_index = find_index_by_element_id(_open_path[0]);
            if (root_index < _items.size() && _items[root_index].tab) popup->set_focus_parent(_items[root_index].tab);
        }
        else if (depth > 0u && depth - 1u < _open_path.size())
        {
            if (auto *anchor = find_popup_child(_open_path[depth - 1u])) popup->set_focus_parent(anchor);
        }
        popup->clear_children();
        popup->update_style_invalidated();
        f32 content_w = 0.0f;
        f32 content_h = 0.0f;
        bool needs_separator = false;
        for (const auto &group : groups)
        {
            if (group.empty()) continue;
            if (needs_separator)
            {
                const auto &separator_style = get_theme()->get_style(get_theme()->get_resolved_style(
                    AUIK_STYLE_TAG_SEPARATOR, AUIK_TAG_SEPARATOR, popup->id(), StyleState::normal));
                const f32 separator_top_margin = separator_style.margin().y;
                if (separator_top_margin > 0.0f)
                {
                    auto *top_margin = static_cast<Widget *>(acul::alloc<Dummy>(
                        AUIK_TAG_DUMMY, amal::vec2{0.0f, separator_top_margin}, WidgetFlagBits::visible));
                    top_margin->set_size({AUIK_SIZE_X_FILL, separator_top_margin});
                    top_margin->update_layout_min_size();
                    content_w = amal::max(content_w, top_margin->required_size().x);
                    content_h += top_margin->required_size().y;
                    popup->add_child(top_margin);
                }
                auto *row = static_cast<Widget *>(acul::alloc<HSeparator>(WidgetFlagBits::visible));
                row->set_size({AUIK_SIZE_X_FILL, AUIK_SIZE_Y_FIT});
                row->get_rect().id.widget_id = id();
                row->set_focus_parent(popup);
                row->update_style_invalidated();
                row->update_layout_min_size();
                content_w = amal::max(content_w, row->required_size().x);
                content_h += row->required_size().y;
                popup->add_child(row);
            }
            for (u32 item_id : group)
            {
                auto *item = find_item(item_id);
                if (!item) continue;
                auto *row = static_cast<Widget *>(acul::alloc<PopupItem>(id(), item_id, item_id, item, popup));
                row->set_size({AUIK_SIZE_X_FILL, AUIK_SIZE_Y_FIT});
                row->set_focus_parent(popup);
                auto *popup_item = static_cast<PopupItem *>(row);
                popup_item->sync_selection_state(_selected_enabled, is_item_selected(item_id));
                row->set_style_state(is_popup_item_focused(item_id) ? StyleState::focus : StyleState::normal);
                row->update_style_invalidated();
                row->update_layout_min_size();
                content_w = amal::max(content_w, row->required_size().x);
                content_h += row->required_size().y;
                popup->add_child(row);
            }
            needs_separator = true;
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
        popup->set_visible();
        popup->sync_widget_flags();
        popup->update_style_invalidated();
        const amal::vec2 popup_size{popup_w, popup_h};
        popup->set_position(resolve_popup_position(depth, anchor, popup_size, popup->id()));
        popup->set_size(popup_size);
        popup->attach_to_viewport(get_popup_viewport());
        const amal::vec2 popup_range =
            _popup_depth_mode == PopupDepthMode::workzone_overlay
                ? get_menu_popup_depth_range(parent() ? parent()->depth_range() : this->depth_range())
                : get_root_menu_popup_depth_range(_popup_depth_mode);
        popup->update_depth(popup_range);
        refresh_popup_clip_rect(popup);
        popup->update_layout(false);
        refresh_popup_clip_rect(popup);
    }

    amal::rect MenuBar::resolve_root_popup_anchor(const amal::rect &anchor) const
    {
        amal::rect out = anchor;
        if (_popup_parent)
        {
            const amal::rect parent_bounds = _popup_parent->bounds();
            out.offset.y = parent_bounds.offset.y;
            out.size.y = parent_bounds.size.y;
        }
        else if (parent() && parent()->get_rect().id.tag_id == AUIK_TAG_TITLEBAR)
        {
            out.offset.y = parent()->position().y;
            out.size.y = parent()->size().y;
        }
        return out;
    }

    void MenuBar::open_root(u32 element_id)
    {
        const u32 index = find_index_by_element_id(element_id);
        if (index >= _items.size() || !_items[index].tab) return;
        auto *item = find_item(element_id);
        auto groups = group_layer_ids(item ? item->group_layer() : nullptr);
        if (!item || !has_menu_child_groups(groups)) return;
        close_from_depth(0);
        _open_path.clear();
        _open_path.push_back(element_id);
        layout_popup(0, resolve_root_popup_anchor(_items[index].tab->bounds()), groups);
        update_root_item_state(element_id);
        sync_popup_item_states();
        request_redraw();
    }

    bool MenuBar::open_root_at(u32 element_id, const amal::rect &anchor)
    {
        auto *item = find_item(element_id);
        auto groups = group_layer_ids(item ? item->group_layer() : nullptr);
        if (!item || !has_menu_child_groups(groups)) return false;
        close_from_depth(0);
        _open_path.clear();
        _open_path.push_back(element_id);
        layout_popup(0, anchor, groups);
        update_root_item_state(element_id);
        sync_popup_item_states();
        request_redraw();
        return true;
    }

    bool MenuBar::open_root_items_at(const amal::rect &anchor, MenuGroupLayer *layer)
    {
        auto groups = group_layer_ids(layer);
        if (!has_menu_child_groups(groups)) return false;
        close_from_depth(0);
        _open_path.clear();
        layout_popup(0, anchor, groups);
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
        auto groups = group_layer_ids(item ? item->group_layer() : nullptr);
        if (!item || !has_menu_child_groups(groups))
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
        layout_popup(depth, anchor, groups);
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
            _popups[i]->unset_visible();
            _popups[i]->sync_widget_flags();
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
            if (popup)
            {
                if (popup->is_visible() && !(detail::get_context().dirty_flags & DirtyFlagBits::destroying))
                    static_cast<Widget *>(popup)->invalidate_draw_commands(DrawReasonBits::full_redraw);
                popup->unset_visible();
                popup->sync_widget_flags();
            }
        _open_path.clear();
    }

    void MenuBar::request_redraw()
    {
        detail::get_context().dirty_flags |= DirtyFlagBits::redraw;
        mark_host_refresh_request();
    }

    StyleUpdateFlags MenuBar::update_root_item_state(u32 element_id)
    {
        if (element_id == 0u) return StyleUpdateFlagBits::none;
        const u32 index = find_index_by_element_id(element_id);
        if (index >= _items.size() || !_items[index].tab) return StyleUpdateFlagBits::none;

        const auto transition = detail::get_widget_style_selector_transition(id());
        auto *tab = _items[index].tab;
        tab->set_style_state(resolve_tab_item_state(index, transition));
        return tab->update_style_invalidated();
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
                child->update_style_invalidated();
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
            if (!hovered_child || hovered_child->is_read_only() || hovered_child->is_disabled()) return;
            const u32 clicked_item_id = popup_child_item_id(hovered_child);
            auto *item = find_item(clicked_item_id);
            if (!item || item->is_read_only() || item->is_disabled()) return;
            if (has_menu_child_groups(group_layer_ids(item->group_layer()))) return;

            const u32 owner_id = id();
            Widget *owner_parent = parent();
            item->dispatch_click(MouseKey::left, KeyPressState::press, 1u);

            const auto &ctx = detail::get_context();
            const auto it = ctx.id_map.find(owner_id);
            if (it == ctx.id_map.end() || it->second != this) return;
            auto *menu = static_cast<MenuBar *>(it->second);
            if (menu->parent() != owner_parent) return;
            menu->close_all();
            menu->update_draw_commands(DrawReasonBits::external);
            request_redraw();
        }
    }

    void MenuBar::on_hover(HoverState state)
    {
        Tabbar::on_hover(state);
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
                if (!has_menu_child_groups(group_layer_ids(item->group_layer())))
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

    PopupMenu::PopupMenu(u32 id, const acul::vector<StringView> &items, WidgetFlags widget_flags, bool selected_enabled)
        : Widget(id, widget_flags | WidgetFlagBits::hittable, EventFlagBits::click | EventFlagBits::focus,
                 {{0.0f, 0.0f}, AUIK_SIZE_INHERIT}, AUIK_TAG_POPUP_MENU),
          _button(AUIK_STYLE_TAG_DOCK_TABBAR_MENU, AUIK_TAG_POPUP_MENU_BUTTON, AUIK_ICON_MENU, AUIK_ICON_MENU, false),
          _menu(acul::alloc<MenuBar>(id + AUIK_TAG_MENU_POPUP, items))
    {
        set_button_update_target(this);
        set_button_hit_id(make_element_id(id, AUIK_TAG_POPUP_MENU_BUTTON, 0u));
        _button.set_element_id(0u);
        _menu->set_focus_parent(this);
        _menu->set_popup_depth_mode(MenuBar::PopupDepthMode::root_overlay_next);
        _menu->set_selected_enabled(selected_enabled);
        _menu->set_position({0.0f, 0.0f});
        _menu->set_layout_size({0.0f, 0.0f});
    }

    PopupMenu::PopupMenu(MenuBar *menu, WidgetFlags widget_flags, bool selected_enabled)
        : Widget(menu ? (menu->id() + AUIK_TAG_MENU_POPUP) : AUIK_TAG_POPUP_MENU,
                 widget_flags | WidgetFlagBits::hittable, EventFlagBits::click | EventFlagBits::focus,
                 {{0.0f, 0.0f}, AUIK_SIZE_INHERIT}, AUIK_TAG_POPUP_MENU),
          _button(AUIK_STYLE_TAG_DOCK_TABBAR_MENU, AUIK_TAG_POPUP_MENU_BUTTON, AUIK_ICON_MENU, AUIK_ICON_MENU, false),
          _menu(menu ? menu : acul::alloc<MenuBar>(AUIK_TAG_MENU_POPUP, acul::vector<StringView>{}))
    {
        set_button_update_target(this);
        set_button_hit_id(make_element_id(id(), AUIK_TAG_POPUP_MENU_BUTTON, 0u));
        _button.set_element_id(0u);
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

    MenuBar::MenuGroupLayer *PopupMenu::root_items() { return _menu ? &_menu->_menu_base.root_layer() : nullptr; }

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
        }
    }

    void PopupMenu::discard_popup()
    {
        _open = false;
        _button.set_open(false);
        if (_menu) _menu->discard_popups();
        if (_menu && detail::get_context().focus_id == _menu->id()) focus_widget(nullptr);
        detach_menu_from_popup();
    }

    void PopupMenu::open_menu()
    {
        if (!_menu) return;
        auto *ids = root_items();
        if (!ids || ids->empty())
        {
            set_open(false);
            return;
        }
        _menu->set_parent(parent());
        _menu->set_focus_parent(this);
        _menu->attach_to_viewport(this->viewport());
        _menu->set_position({0.0f, 0.0f});
        _menu->set_layout_size({0.0f, 0.0f});
        _menu->update_style_invalidated();
        _menu->update_layout_min_size();
        attach_menu_for_popup();
        _menu->update_depth(detail::get_global_foreground_depth_range());
        _menu->open_root_items_at(_popup_anchor_overridden ? _popup_anchor_override : _button.bounds(), ids);
        focus_widget(_menu);
    }

    StyleUpdateFlags PopupMenu::update_style()
    {
        const auto state = resolve_button_state();
        StyleUpdateFlags out = _button.update_style(id(), parent() ? parent()->id() : 0u, state);
        if (_menu) out |= _menu->update_style_invalidated();
        return out;
    }

    void PopupMenu::update_layout_min_size_force()
    {
        _button.update_layout_min_size_force({0.0f, 0.0f}, true);
        set_required_size(_button.required_size());
    }

    void PopupMenu::update_layout(bool min_size_known)
    {
        if (layout_measure_required(min_size_known)) update_layout_min_size_force();
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

    void PopupMenu::draw_button(DrawCtx &ctx)
    {
        DrawCtx button_ctx = ctx;
        button_ctx.is_hit_allowed = true;
        _button.draw(button_ctx, can_emit_hit(button_ctx));
    }

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
        mark_host_refresh_request();
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
        mark_host_refresh_request();
    }

    void PopupMenu::on_attach()
    {
        Widget::on_attach();
        if (_open) attach_menu_for_popup();
    }

    void PopupMenu::on_detach()
    {
        detach_menu_from_popup();
        Widget::on_detach();
    }

    namespace
    {
        void write_menu_items(acul::bin_stream &stream, const acul::vector<MenuBar::SerializedItem> &items)
        {
            stream.write(static_cast<u32>(items.size()));
            for (const auto &item : items)
            {
                stream.write(item.id);
                detail::write_localized_string(stream, item.text, item.translated);
                stream.write(item.shortcut);
                stream.write(static_cast<u32>(item.next_groups.size()));
                for (const auto &group : item.next_groups)
                {
                    stream.write(static_cast<u32>(group.size()));
                    if (!group.empty()) stream.write(group.data(), group.size());
                }
                stream.write(item.selected);
            }
        }

        acul::vector<MenuBar::SerializedItem> read_menu_items(acul::bin_stream &stream)
        {
            u32 count = 0u;
            stream.read(count);
            acul::vector<MenuBar::SerializedItem> out;
            out.reserve(count);
            for (u32 i = 0u; i < count; ++i)
            {
                MenuBar::SerializedItem item{};
                stream.read(item.id);
                auto text = detail::read_localized_string(stream);
                item.text = std::move(text.text);
                item.translated = text.translated;
                stream.read(item.shortcut);
                u32 group_count = 0u;
                stream.read(group_count);
                item.next_groups.reserve(group_count);
                for (u32 group_i = 0u; group_i < group_count; ++group_i)
                {
                    u32 item_count = 0u;
                    stream.read(item_count);
                    acul::vector<u32> group;
                    group.resize(item_count);
                    if (!group.empty()) stream.read(group.data(), group.size());
                    item.next_groups.push_back(std::move(group));
                }
                stream.read(item.selected);
                out.push_back(std::move(item));
            }
            return out;
        }

        void write_menu_bar(acul::bin_stream &stream, umbf::Block *block)
        {
            auto *menu = static_cast<MenuBar *>(block);
            detail::write_widget_common_data(stream, *menu);
            const auto &root_ids = menu->element_ids();
            stream.write(static_cast<u32>(root_ids.size()));
            if (!root_ids.empty()) stream.write(root_ids.data(), root_ids.size());
            stream.write(menu->menu_style_tag())
                .write(menu->menu_item_style_tag())
                .write(static_cast<u32>(menu->popup_depth_mode()))
                .write(menu->selected_enabled());
            write_menu_items(stream, menu->serialized_items());
        }

        umbf::Block *read_menu_bar(acul::bin_stream &stream)
        {
            const auto common = detail::read_widget_common_data(stream);
            u32 root_id_count = 0u;
            stream.read(root_id_count);
            acul::vector<u32> root_ids;
            root_ids.resize(root_id_count);
            if (!root_ids.empty()) stream.read(root_ids.data(), root_ids.size());

            u32 menu_style_tag = AUIK_STYLE_TAG_WINDOW_MENU_BAR;
            u32 menu_item_style_tag = AUIK_STYLE_TAG_MENU_BAR_ITEM;
            u32 popup_depth_mode = static_cast<u32>(MenuBar::PopupDepthMode::workzone_overlay);
            bool selected_enabled = false;
            stream.read(menu_style_tag).read(menu_item_style_tag).read(popup_depth_mode).read(selected_enabled);
            auto items = read_menu_items(stream);

            auto *menu = acul::alloc<MenuBar>(common.id, acul::vector<StringView>{}, common.inline_size,
                                              WidgetFlags(common.widget_flags));
            menu->set_menu_style_tag(menu_style_tag);
            menu->set_menu_item_style_tag(menu_item_style_tag);
            menu->set_popup_depth_mode(static_cast<MenuBar::PopupDepthMode>(popup_depth_mode));
            menu->set_selected_enabled(selected_enabled);
            menu->restore_serialized_items(root_ids, items);
            detail::apply_widget_common_data(menu, common);
            return menu;
        }

        void write_popup_menu(acul::bin_stream &stream, umbf::Block *block)
        {
            auto *popup = static_cast<PopupMenu *>(block);
            detail::write_widget_common_data(stream, *popup);
            acul::vector<umbf::Block *> blocks;
            if (auto *menu = popup->menu_model()) blocks.push_back(menu);
            stream.write(blocks);
        }

        umbf::Block *read_popup_menu(acul::bin_stream &stream)
        {
            const auto common = detail::read_widget_common_data(stream);
            acul::vector<umbf::Block *> blocks;
            stream.read(blocks);

            MenuBar *menu = nullptr;
            if (!blocks.empty()) menu = static_cast<MenuBar *>(blocks[0]);
            auto *popup = acul::alloc<PopupMenu>(menu, WidgetFlags(common.widget_flags));
            detail::apply_widget_common_data(popup, common);
            return popup;
        }
    } // namespace

    namespace streams
    {
        AUIK_EXPORT const umbf::streams::Stream menu_bar{read_menu_bar, write_menu_bar};
        AUIK_EXPORT const umbf::streams::Stream popup_menu{read_popup_menu, write_popup_menu};
    } // namespace streams
} // namespace auik
