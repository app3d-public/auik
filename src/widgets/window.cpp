#include <auik/auik.hpp>
#include <auik/detail/depth.hpp>
#include <auik/detail/rect.hpp>
#include <auik/pipelines.hpp>
#include <auik/widgets/containers.hpp>
#include <auik/widgets/dockspace.hpp>
#include <auik/widgets/menu.hpp>
#include <auik/widgets/rubber_band.hpp>
#include <auik/widgets/text.hpp>
#include <auik/widgets/window.hpp>
#include <utility>
#include "../core/session_stream_utils.hpp"

namespace auik
{
    static constexpr detail::StylePropertyFlags g_layout_style_mask =
        detail::StylePropertiesBits::padding | detail::StylePropertiesBits::margin |
        detail::StylePropertiesBits::text_size | detail::StylePropertiesBits::border_thickness |
        detail::StylePropertiesBits::border_radius | detail::StylePropertiesBits::inline_spacing;

    static inline bool has_layout_style_delta(const Style &normal, const Style &active)
    {
        const auto normal_mask = normal.mask() & g_layout_style_mask;
        const auto active_mask = active.mask() & g_layout_style_mask;
        return normal_mask != active_mask;
    }

    static inline bool needs_layout_on_active(Window &window)
    {
        auto *theme = get_theme();
        const Style &window_normal =
            theme->get_style(theme->get_resolved_style(AUIK_STYLE_TAG_WINDOW, window.id(), 0, StyleState::normal));
        const Style &window_active =
            theme->get_style(theme->get_resolved_style(AUIK_STYLE_TAG_WINDOW, window.id(), 0, StyleState::active));
        if (has_layout_style_delta(window_normal, window_active)) return true;

        if (!(window.window_flags & WindowFlagBits::decorated)) return false;
        const Style &header_normal = theme->get_style(theme->get_resolved_style(
            AUIK_STYLE_TAG_WINDOW_HEADER, AUIK_TAG_WINDOW_HEADER, window.id(), StyleState::normal));
        const Style &header_active = theme->get_style(theme->get_resolved_style(
            AUIK_STYLE_TAG_WINDOW_HEADER, AUIK_TAG_WINDOW_HEADER, window.id(), StyleState::active));
        return has_layout_style_delta(header_normal, header_active);
    }

    static inline Window *as_root_window(Widget *widget)
    {
        if (!widget) return nullptr;
        if (widget->parent()) return nullptr;
        if (widget->get_rect().id.tag_id != AUIK_TAG_WINDOW) return nullptr;
        return static_cast<Window *>(widget);
    }

    static inline bool is_widget_in_focus_chain(u32 widget_id)
    {
        if (widget_id == 0u) return false;
        auto &ctx = detail::get_context();
        auto it = ctx.id_map.find(ctx.focus_id);
        Widget *node = it != ctx.id_map.end() ? it->second : nullptr;
        while (node)
        {
            if (node->id() == widget_id) return true;
            node = node->focus_parent();
        }
        return false;
    }

    static inline f32 snap_layout_start(f32 value) { return amal::ceil(value); }

    static inline f32 snap_layout_end(f32 value) { return amal::floor(value); }

    static inline bool is_docked_window(const Window &window) { return window.window_flags & WindowFlagBits::docked; }
    static inline Widget *window_menu_widget(const Window &window)
    {
        auto *menu = window.get_menu();
        return menu ? menu->get_widget() : nullptr;
    }

    static inline bool owns_window_menu_tree(const Window &window)
    {
        return window.get_menu() && !is_docked_window(window);
    }
    static inline MenuBar *window_menu_bar(const Window &window)
    {
        auto *menu = window.get_menu();
        return menu && menu->is_menu_bar() ? menu->menu_bar() : nullptr;
    }
    static inline bool owns_classic_menu_bar_tree(const Window &window)
    {
        return owns_window_menu_tree(window) && window_menu_bar(window);
    }
    static inline bool owns_popup_menu_tree(const Window &window)
    {
        return !is_docked_window(window) && window.header_popup_menu();
    }

    static inline bool should_use_docked_window_style(const Window &window, u32 base_style_tag)
    {
        return is_docked_window(window) && base_style_tag == AUIK_STYLE_TAG_WINDOW;
    }

    static inline void get_window_resize_direction(const detail::RectData &rect, const amal::vec2 &mouse_pos,
                                                   int &resize_x, int &resize_y)
    {
        resize_x = 0;
        resize_y = 0;
        if (!(rect.flags & detail::RectBits::hitbox)) return;

        const auto &bounds = rect.bounds;
        const f32 left = amal::get_rect_left(bounds);
        const f32 right = amal::get_rect_right(bounds);
        const f32 top = amal::get_rect_top(bounds);
        const f32 bottom = amal::get_rect_bottom(bounds);

        if (amal::abs(mouse_pos.x - left) <= AUIK_HITBOX_PAD) resize_x = -1;
        else if (amal::abs(mouse_pos.x - right) <= AUIK_HITBOX_PAD) resize_x = 1;

        if (amal::abs(mouse_pos.y - top) <= AUIK_HITBOX_PAD) resize_y = -1;
        else if (amal::abs(mouse_pos.y - bottom) <= AUIK_HITBOX_PAD) resize_y = 1;
    }

    static inline detail::CursorID::enum_type get_window_resize_cursor(int resize_x, int resize_y)
    {
        if ((resize_x < 0 && resize_y < 0) || (resize_x > 0 && resize_y > 0)) return detail::CursorID::resize_nwse;
        if ((resize_x > 0 && resize_y < 0) || (resize_x < 0 && resize_y > 0)) return detail::CursorID::resize_nesw;
        if (resize_x != 0) return detail::CursorID::resize_ew;
        if (resize_y != 0) return detail::CursorID::resize_ns;
        return detail::CursorID::arrow;
    }

    static inline void clear_window_frame_hitbox_flag(Window &window)
    {
        auto &rect = window.get_rect();
        if (!(rect.flags & detail::RectBits::hitbox)) return;
        rect.flags &= ~detail::RectBits::hitbox;
        if (detail::g_context) detail::get_context().dirty_flags |= DirtyFlagBits::hit_rect_update;
    }

    static inline void fill_window_resize_hit_rect(u32 window_id, const amal::rect &bounds, f32 depth, u16 clip_id,
                                                   detail::RectData &rect)
    {
        rect.id = make_element_id(window_id, AUIK_TAG_HITBOX);
        rect.bounds = bounds;
        rect.depth = depth;
        rect.hit_depth = depth;
        rect.clip_id = clip_id;
        rect.flags = detail::RectBits::hitbox;
    }

    static inline bool can_start_dock_drag(const Window &window, ElementID drag_id)
    {
        if (!get_dockspace_context()) return false;
        if (drag_id.widget_id != window.id() || drag_id.tag_id != AUIK_TAG_WINDOW_HEADER) return false;
        if (!(window.window_flags & WindowFlagBits::dockable)) return false;
        return !(window.window_flags & WindowFlagBits::docked);
    }

    static inline amal::vec4 get_window_clip_bounds(Window *window)
    {
        if (window && window->parent())
        {
            auto *parent = window->parent();
            if (parent->clip_id() != 0xFFFFu) return get_clip_rect(parent->clip_id());
            return parent->get_content_clip_rect();
        }
        return get_widget_viewport_rect(window);
    }

    static inline bool is_root_viewport_managed(const Widget *widget)
    {
        return widget && !widget->parent() && widget->get_rect().id.tag_id == AUIK_TAG_VIEWPORT_WINDOW;
    }

    static inline amal::vec2 get_min_pos(const Widget *widget)
    {
        if (!widget || widget->parent() || is_root_viewport_managed(widget)) return {0.0f, 0.0f};

        const amal::vec4 viewport = get_widget_viewport_rect(widget);
        return {viewport.x, viewport.y};
    }

    static inline amal::vec2 clamp_root_widget_position(const Widget *widget, const amal::vec2 &local_pos,
                                                        const amal::vec2 &size)
    {
        if (!widget || widget->parent() || is_root_viewport_managed(widget)) return local_pos;

        const amal::vec4 viewport = get_widget_viewport_rect(widget);
        const f32 max_x = amal::max(viewport.z - size.x, 0.0f);
        const f32 max_y = amal::max(viewport.w - size.y, 0.0f);
        return {amal::clamp(local_pos.x, 0.0f, max_x), amal::clamp(local_pos.y, 0.0f, max_y)};
    }

    static inline amal::vec2 clamp_root_widget_drag_delta(const Widget *widget, const amal::vec2 &delta)
    {
        if (!widget || widget->parent() || is_root_viewport_managed(widget)) return delta;

        const auto &io = detail::get_context().io;
        const amal::vec4 viewport = get_widget_viewport_rect(widget);
        const f32 min_x = viewport.x;
        const f32 min_y = viewport.y;
        const f32 max_x = viewport.x + viewport.z;
        const f32 max_y = viewport.y + viewport.w;
        const amal::vec2 prev_mouse = io.mouse_pos - delta;
        const amal::vec2 clamped_prev_mouse{
            amal::clamp(prev_mouse.x, min_x, max_x),
            amal::clamp(prev_mouse.y, min_y, max_y),
        };
        const amal::vec2 clamped_mouse{
            amal::clamp(io.mouse_pos.x, min_x, max_x),
            amal::clamp(io.mouse_pos.y, min_y, max_y),
        };
        return clamped_mouse - clamped_prev_mouse;
    }

    static inline amal::vec2 resolve_root_widget_position(Widget *widget, const amal::vec2 &size, bool clamp_position)
    {
        if (!widget || widget->parent()) return widget ? widget->position() : amal::vec2{0.0f, 0.0f};

        const amal::vec2 min_pos = get_min_pos(widget);
        amal::vec2 local_pos = widget->position() - widget->root_viewport_origin();
        if (is_root_viewport_managed(widget))
        {
            widget->set_root_viewport_origin({0.0f, 0.0f});
            return local_pos;
        }

        if (clamp_position) local_pos = clamp_root_widget_position(widget, local_pos, size);
        widget->set_root_viewport_origin(min_pos);
        return min_pos + local_pos;
    }

    static inline bool is_fit_axis(f32 value) { return is_size_fit(value); }

    static inline amal::vec2 resolve_root_window_size(const Window &window)
    {
        const amal::vec4 viewport = get_widget_viewport_rect(&window);
        const amal::vec2 min_size = window.min_size();
        amal::vec2 next_size = window.size();
        const amal::vec2 requested = window.style_size();
        const amal::vec2 required = amal::max(window.required_size(), min_size);
        if (is_size_fill(requested.x)) next_size.x = viewport.z;
        else if (is_size_fit(requested.x) || next_size.x <= 0.0f) next_size.x = required.x;
        if (is_size_fill(requested.y)) next_size.y = viewport.w;
        else if (is_size_fit(requested.y) || next_size.y <= 0.0f) next_size.y = required.y;
        next_size = amal::max(next_size, min_size);
        next_size.x = amal::clamp(next_size.x, amal::min(min_size.x, viewport.z), viewport.z);
        next_size.y = amal::clamp(next_size.y, amal::min(min_size.y, viewport.w), viewport.w);
        return next_size;
    }

    static inline amal::vec2 resolve_auto_root_window_position(Window &window)
    {
        auto &ctx = detail::get_context();
        const amal::vec4 viewport = get_widget_viewport_rect(&window);
        const u32 index = ctx.root_window_count > 0u ? ctx.root_window_count - 1u : 0u;
        const f32 step = 28.0f;
        const f32 x_space = amal::max(viewport.z - window.size().x, 0.0f);
        const f32 y_space = amal::max(viewport.w - window.size().y, 0.0f);
        const amal::vec2 centered{viewport.x + x_space * 0.5f, viewport.y + y_space * 0.5f};
        const f32 cascade_limit_x = amal::max(x_space * 0.5f, step);
        const f32 cascade_limit_y = amal::max(y_space * 0.5f, step);
        const f32 cascade_x = std::fmod(step * static_cast<f32>(index), cascade_limit_x);
        const f32 cascade_y = std::fmod(step * static_cast<f32>(index), cascade_limit_y);
        const amal::vec2 local{amal::clamp(centered.x + cascade_x, viewport.x, viewport.x + x_space),
                               amal::clamp(centered.y + cascade_y, viewport.y, viewport.y + y_space)};
        return local;
    }

    class WindowHeader final : public Widget
    {
    public:
        explicit WindowHeader(Widget *parent, StringView text, bool hittable)
            : Widget(AUIK_TAG_WINDOW_HEADER,
                     WidgetFlagBits::visible | (hittable ? WidgetFlagBits::hittable : WidgetFlagBits::none),
                     EventFlagBits::none, {}, AUIK_TAG_WINDOW_HEADER),
              _style({Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_WINDOW_HEADER}),
              _title_text(text.str ? text.str : ""),
              _title(acul::alloc<Text>(AUIK_TAG_WINDOW_HEADER, _title_text, amal::vec2{0.0f, 0.0f},
                                       WidgetFlagBits::visible))
        {
            assert(parent);
            set_parent(parent);
            _title->set_style_tag(AUIK_STYLE_TAG_NO_PAD);
            _title->set_parent(this);
            _rect.id.widget_id = parent->id();
            _rect.clip_id = parent->clip_id();
            if (text.is_translated) _title->set_translated_text_literal(text.str);
        }
        ~WindowHeader() override { acul::release(_title); }

        void set_menu(PopupMenu *menu)
        {
            if (_menu == menu) return;
            _menu = menu;
            if (_menu)
            {
                _menu->set_button_update_target(parent());
                _menu->set_button_hit_id(make_element_id(parent()->id(), AUIK_TAG_POPUP_MENU_BUTTON, 0u));
                _menu->set_button_style_tag(AUIK_STYLE_TAG_WINDOW_HEADER_MENU);
            }
            if (_title) _title->reset_draw_records();
            _title_draw_dirty = true;
        }
        void set_title(StringView title)
        {
            const acul::string next = title.str ? title.str : "";
            if (_title_text == next && (!title.is_translated || (_title && _title->is_translated_text()))) return;
            _title_text = next;
            if (_title)
            {
                _title->set_text(title);
                _title->reset_draw_records();
            }
            _title_draw_dirty = true;
        }

        const Text *title_text() const { return _title; }

        f32 compute_height() const
        {
            auto *theme = get_theme();
            const auto &style = theme->get_style(_style.id);
            return style.text_size() + style.padding().y * 2.0f;
        }

        StyleUpdateFlags update_style() override
        {
            const u32 parent_id = parent() ? parent()->id() : 0u;
            StyleState state = StyleState::normal;
            if (parent())
            {
                const auto transition = detail::get_widget_style_selector_transition(parent_id);
                if (transition.current_id.tag_id == AUIK_TAG_WINDOW_HEADER) state = transition.current_state;
                const auto ps = parent()->style_state();
                if (ps == StyleState::active || ps == StyleState::focus) state = ps;
                if (is_widget_in_focus_chain(parent_id)) state = StyleState::focus;
            }
            set_style_state(state);
            StyleUpdateFlags flags = resolve_style_selector(_style, id(), parent_id, style_state());
            const auto &style = get_theme()->get_style(_style.id);
            flags |= _title->update_style_invalidated();
            if (_menu) flags |= _menu->update_style_invalidated();
            if (_resolved_text_color != style.text_color())
            {
                _resolved_text_color = style.text_color();
                _title_draw_dirty = true;
            }
            return flags;
        }
        void update_layout(bool min_size_known) override
        {
            (void)min_size_known;
            Widget::update_layout(true);
            const auto &style = get_theme()->get_style(_style.id);
            const amal::vec4 padding = style.padding();
            const amal::vec2 content_pos = {position().x + padding.x, position().y + padding.y};
            const amal::vec2 content_size = {amal::max(size().x - padding.x - padding.z, 0.0f),
                                             amal::max(size().y - padding.y - padding.w, 0.0f)};
            amal::vec2 title_size = content_size;
            if (_menu)
            {
                _menu->update_layout_min_size();
                const amal::vec2 menu_size = _menu->required_size();
                const f32 spacing = menu_size.x > 0.0f ? style.inline_spacing() : 0.0f;
                const amal::vec2 menu_pos = {content_pos.x + amal::max(content_size.x - menu_size.x, 0.0f),
                                             position().y + amal::max(size().y - menu_size.y, 0.0f) * 0.5f};
                _menu->set_position(menu_pos);
                _menu->set_layout_size(menu_size);
                _menu->set_clip_id(clip_id());
                _menu->update_layout(true);
                title_size.x = amal::max(title_size.x - menu_size.x - spacing, 0.0f);
            }
            _title->update_layout_min_size();
            const f32 title_h = _title->required_size().y;
            _title->set_position({content_pos.x, content_pos.y + amal::floor((content_size.y - title_h) * 0.5f)});
            _title->set_layout_size({title_size.x, title_h});
            _title->update_layout(true);
            _title->set_clip_id(clip_id());
        }

        void translate(const amal::vec2 &delta) override
        {
            if (delta.x == 0.0f && delta.y == 0.0f) return;
            Widget::translate(delta);
            _title->translate(delta);
            if (_menu) _menu->translate(delta);
        }

        void update_depth(const amal::vec2 &depth_range) override
        {
            Widget::update_depth(depth_range);
            const u32 menu_requirement = _menu ? _menu->get_depth_requirement() : 0u;
            DepthCursor cursor(this->depth_range(), 2u + menu_requirement);
            const amal::vec2 bg_range = cursor.next(1u);
            const amal::vec2 text_range = cursor.next(1u);
            get_rect().depth = next_depth(bg_range);
            get_rect().hit_depth = get_rect().depth;
            _title->update_depth(text_range);
            if (_menu) _menu->update_depth(cursor.next(menu_requirement));
        }

        u32 get_depth_requirement() const override { return 2u + (_menu ? _menu->get_depth_requirement() : 0u); }

        void back_hit_depth() override
        {
            Widget::back_hit_depth();
            _title->back_hit_depth();
            if (_menu) _menu->back_hit_depth();
        }

        void restore_hit_depth() override
        {
            Widget::restore_hit_depth();
            _title->restore_hit_depth();
            if (_menu) _menu->restore_hit_depth();
        }

        void rebuild_clip_rects() override
        {
            _rect.clip_id = _parent->clip_id();
            invalidate_hit_rect(_bg);
            _title->set_clip_id(clip_id());
            _title->rebuild_clip_rects();
            if (_menu)
            {
                _menu->set_clip_id(clip_id());
                _menu->rebuild_clip_rects();
            }
            _title_draw_dirty = true;
        }

        void reset_clip_rect_records() override
        {
            Widget::reset_clip_rect_records();
            if (_title) _title->reset_clip_rect_records();
            if (_menu) _menu->reset_clip_rect_records();
        }

        void reset_draw_records() override
        {
            _bg = {};
            _title->reset_draw_records();
            if (_menu) _menu->reset_draw_records();
            _title_draw_dirty = true;
        }

        void draw(DrawCtx &ctx) override
        {
            auto *theme = get_theme();
            auto *quads_stream = get_primary_quads_stream();

            QuadsInstanceData data{};
            data.rect = bounds();
            data.z_order = get_z_order();
            const bool bg_visible = fill_quads_instance_by_style(theme->get_style(_style.id), clip_id(), data);
            emit_quads_instance(ctx, quads_stream, _bg, data, get_rect(), bg_visible, can_emit_hit(ctx));

            DrawCtx title_ctx = ctx;
            _title->draw_local(title_ctx);
            if (_menu)
            {
                DrawCtx menu_ctx = ctx;
                _menu->draw_button(menu_ctx);
            }
            _title_draw_dirty = false;
        }

    private:
        DrawDataID _bg;
        StyleSelector _style;
        acul::string _title_text;
        Text *_title = nullptr;
        PopupMenu *_menu = nullptr;
        u32 _resolved_text_color{0};
        bool _title_draw_dirty = true;
    };

    static inline f32 get_window_header_height(const Widget *header, const Window &window)
    {
        if (!header || is_docked_window(window)) return 0.0f;
        return static_cast<const WindowHeader *>(header)->compute_height();
    }

    Window::Window(u32 id, StringView title, const amal::rect &bounds, WindowFlags in_window_flags,
                   WidgetFlags in_widget_flags)
        : Widget(id, in_widget_flags, EventFlagBits::click | EventFlagBits::drag | EventFlagBits::focus, bounds,
                 AUIK_TAG_WINDOW),
          _content_block(
              acul::alloc<DrawBlock>(id, WidgetFlagBits::visible | WidgetFlagBits::hittable, AUIK_TAG_WINDOW_CONTENT)),
          children(_content_block->children),
          window_flags(in_window_flags),
          _title(title.str ? title.str : "")
    {
        set_position(bounds.offset);
        set_size(bounds.size);
        widget_flags |= WidgetFlagBits::hittable;
        _content_block->set_parent(this);
        _content_block->set_focus_parent(this);
        _content_block->add_event_flags(EventFlagBits::click);
        _content_block->set_scrollbar_style_tag(AUIK_STYLE_TAG_SCROLLBAR_TRACK);
        _content_block->set_draw_block_flags(_content_block->draw_block_flags() |
                                             DrawBlockFlagBits::clip_ignores_padding_x);
        _auto_size = {is_fit_axis(bounds.size.x), is_fit_axis(bounds.size.y)};
        _auto_position = {is_fit_axis(bounds.offset.x), is_fit_axis(bounds.offset.y)};
        clear_window_frame_hitbox_flag(*this);
        _resize_hit_rect = detail::make_rect_data(id, AUIK_TAG_HITBOX);
        if (window_flags & WindowFlagBits::decorated)
            _header = acul::alloc<WindowHeader>(this, title, window_flags & WindowFlagBits::movable);
        if (window_flags & WindowFlagBits::rubber_band)
        {
            _rubber_band = acul::alloc<RubberBand>(id, WidgetFlagBits::none);
            _rubber_band->set_parent(this);
        }
    }

    u32 Window::effective_window_style_tag() const
    {
        if (should_use_docked_window_style(*this, _window_style_tag)) return AUIK_STYLE_TAG_DOCKED_WINDOW;
        return _window_style_tag;
    }

    const Style &Window::resolved_window_style() const { return get_theme()->get_style(_window_style.id); }

    void Window::set_title(StringView title)
    {
        _title = title.str ? title.str : "";
        if (_header) _header->set_title(title);
    }

    const Text *Window::title_text() const
    {
        return _header ? static_cast<const WindowHeader *>(_header)->title_text() : nullptr;
    }

    Window::~Window()
    {
        clear_children();

        if (_default_header_menu) acul::release(_default_header_menu);
        if (_rubber_band) acul::release(_rubber_band);
        if (_header) acul::release(_header);
        if (_content_block) acul::release(_content_block);
    }

    void Window::clear_children()
    {
        if (!_content_block) return;
        _content_block->clear_children();
        _content_block->reset_scroll_offset();
    }

    void Window::erase_children(size_t first, size_t count)
    {
        if (!_content_block) return;
        _content_block->erase_children(first, count);
    }

    void Window::add_child(Widget *child, ChildLayoutFlags layout)
    {
        assert(child && "child is null");
        _content_block->add_child(child, layout);
    }

    void Window::add_children(const acul::vector<Widget *> &new_children)
    {
        for (auto *child : new_children)
        {
            if (!child) continue;
            add_child(child);
        }
    }

    void Window::set_menu(MenuProxy &&menu, PFN_window_menu_suffix_create window_menu_suffix_create)
    {
        _window_menu_suffix_create = window_menu_suffix_create;
        set_menu_widget(menu.release());
    }

    void Window::set_menu(detail::MenuBase *menu, PFN_window_menu_suffix_create window_menu_suffix_create)
    {
        MenuProxy proxy{};
        if (menu)
        {
            auto *widget = menu->owner_widget();
            if (widget && widget->signature() == AUIK_TAG_MENU_BAR)
            {
                auto *menu_bar = static_cast<MenuBar *>(widget);
                if (detail::get_default_window_menu_type() == AUIK_WINDOW_MENU_TYPE_POPUP)
                    proxy.set_popup_menu(acul::alloc<PopupMenu>(menu_bar));
                else proxy.set_menu_bar(menu_bar);
            }
            else proxy.reset(widget);
        }
        set_menu(std::move(proxy), window_menu_suffix_create);
    }

    void Window::set_menu_widget(Widget *menu)
    {
        if (_menu.get_widget() == menu) return;
        _menu.reset(menu);

        auto *widget = window_menu_widget(*this);
        if (widget)
        {
            widget->set_parent(this);
            widget->set_focus_parent(this);
            widget->attach_to_viewport(viewport());
        }
        sync_header_popup_menu();
    }

    Widget *Window::take_menu_widget()
    {
        auto *widget = _menu.get_widget();
        if (!widget) return nullptr;
        remove_header_menu_suffix();
        _menu.release();
        sync_header_popup_menu();
        return widget;
    }

    bool Window::is_popup_menu() const { return _menu.is_popup_menu(); }

    PopupMenu *Window::header_popup_menu() const
    {
        if (is_popup_menu()) return static_cast<PopupMenu *>(_menu.get_widget());
        return _default_header_menu;
    }

    PopupMenu *Window::ensure_header_popup_menu()
    {
        if (is_popup_menu()) return static_cast<PopupMenu *>(_menu.get_widget());
        if (!_default_header_menu)
        {
            _default_header_menu = acul::alloc<PopupMenu>(id() + AUIK_TAG_POPUP_MENU, acul::vector<StringView>{},
                                                          detail::get_tabbar_widget_flags(), false);
        }
        _default_header_menu->set_parent(this);
        _default_header_menu->set_focus_parent(this);
        _default_header_menu->attach_to_viewport(viewport());
        return _default_header_menu;
    }

    void Window::remove_header_menu_suffix()
    {
        if (_header_menu_suffix_group == 0xFFFFu) return;
        if (auto *menu = header_popup_menu())
        {
            menu->erase_suffix_group(_header_menu_suffix_group);
        }
        _header_menu_suffix_group = 0xFFFFu;
    }

    void Window::install_header_menu_suffix()
    {
        if ((window_flags & WindowFlagBits::docked) || !(window_flags & WindowFlagBits::decorated)) return;
        if (_header_menu_suffix_group != 0xFFFFu) return;
        const auto suffix_create =
            _window_menu_suffix_create ? _window_menu_suffix_create : detail::get_default_menu_suffix_create_cb();
        if (!suffix_create) return;
        auto *menu = ensure_header_popup_menu();
        if (!menu) return;

        const u32 group = menu->push_suffix_group();
        const u32 suffix_count_before = menu->suffix_item_count(group);
        suffix_create(this, menu->menu_model());
        const u32 suffix_count = menu->suffix_item_count(group);
        if (suffix_count == suffix_count_before) menu->erase_suffix_group(group);
        else _header_menu_suffix_group = group;
    }

    void Window::sync_header_popup_menu()
    {
        PopupMenu *menu = is_popup_menu() ? static_cast<PopupMenu *>(_menu.get_widget()) : _default_header_menu;
        if (_header)
        {
            _header->set_title(_title);
            const bool has_window_header_menu = is_popup_menu();
            const bool show_menu = (window_flags & WindowFlagBits::decorated) && !is_docked_window(*this) && menu &&
                                   (_header_menu_suffix_group != 0xFFFFu || has_window_header_menu);
            _header->set_menu(show_menu ? menu : nullptr);
        }
    }

    void Window::override_content_clip_rect(const amal::vec4 &rect)
    {
        if (_content_block) _content_block->override_content_clip_rect(rect);
        sync_rubber_band();
    }

    void Window::sync_rubber_band()
    {
        if (!_rubber_band) return;
        _rubber_band->set_parent(this);
        _rubber_band->set_focus_parent(this);
        _rubber_band->update_layout(true);
    }

    void Window::commit_rubber_band()
    {
        if (!_rubber_band || !_rubber_band->committed()) return;
        if (_on_rubber_band_commit)
        {
            RubberBandCommitEvent event{_rubber_band, detail::get_context().io.active_mods};
            _on_rubber_band_commit(event);
        }
        _rubber_band->clear_commit();
    }

    void Window::on_attach()
    {
        if (!parent()) detail::setup_root_window(this);
        Widget::on_attach();
        if (_content_block && !_content_block->is_attached()) _content_block->on_attach();
        if (auto *menu = window_menu_widget(*this); menu && (menu->widget_flags & WidgetFlagBits::attachable))
            menu->on_attach();
        install_header_menu_suffix();
        sync_header_popup_menu();
        if (header_popup_menu() == _default_header_menu && owns_popup_menu_tree(*this) && _default_header_menu &&
            (_default_header_menu->widget_flags & WidgetFlagBits::attachable))
            _default_header_menu->on_attach();
        if (_rubber_band && (_rubber_band->widget_flags & WidgetFlagBits::attachable)) _rubber_band->on_attach();
    }

    void Window::sync_window_event_flags(bool scroll, bool hover)
    {
        _window_event_flags = EventFlagBits::none;
        if (scroll) _window_event_flags |= EventFlagBits::scroll;
        if (hover) _window_event_flags |= EventFlagBits::hover;
        sync_widget_flags();
    }

    void Window::on_detach()
    {
        if (!parent()) detail::teardown_root_window(this);
        remove_header_menu_suffix();
        sync_header_popup_menu();
        if (auto *menu = window_menu_widget(*this); menu && (menu->widget_flags & WidgetFlagBits::attachable))
            menu->on_detach();
        if (_default_header_menu && owns_popup_menu_tree(*this) &&
            (_default_header_menu->widget_flags & WidgetFlagBits::attachable))
            _default_header_menu->on_detach();
        if (_content_block && _content_block->is_attached()) _content_block->on_detach();
        Widget::on_detach();
    }

    void Window::on_change(ChangeEvent &event)
    {
        if (!_content_block || event.target != id()) return;
        _content_block->on_change(event);
    }

    void Window::draw(DrawCtx &ctx)
    {
        if (!is_visible() && !(ctx.reason & DrawReasonBits::invalidate)) return;

        auto *quads_stream = get_primary_quads_stream();
        QuadsInstanceData bg_data{};
        bg_data.rect = bounds();
        bg_data.z_order = get_z_order();
        const bool bg_visible = fill_quads_instance_by_style(resolved_window_style(), clip_id(), bg_data);
        emit_quads_instance(ctx, quads_stream, _bg_draw_id, bg_data, get_rect(), bg_visible, can_emit_hit(ctx));

        if (_header && !is_docked_window(*this))
        {
            DrawCtx header_ctx = ctx;
            _header->draw_local(header_ctx);
        }
        auto *classic_menu = window_menu_bar(*this);
        auto *header_menu = header_popup_menu();
        const bool draw_menu_bar = classic_menu && owns_classic_menu_bar_tree(*this) && classic_menu->is_visible();
        if (draw_menu_bar)
        {
            DrawCtx menu_ctx = ctx;
            classic_menu->draw_local(menu_ctx);
        }
        if (_content_block && _content_block->is_visible())
        {
            DrawCtx content_ctx = ctx;
            _content_block->draw_local(content_ctx);
        }

        if (_rubber_band && _rubber_band->is_visible())
        {
            DrawCtx rubber_band_ctx = ctx;
            rubber_band_ctx.is_hit_allowed = false;
            _rubber_band->draw_local(rubber_band_ctx);
        }

        if (draw_menu_bar)
        {
            DrawCtx menu_popup_ctx = ctx;
            classic_menu->draw_popups(menu_popup_ctx);
        }
        if (owns_popup_menu_tree(*this) && header_menu)
        {
            DrawCtx menu_popup_ctx = ctx;
            header_menu->draw_popups(menu_popup_ctx);
        }

        if ((window_flags & WindowFlagBits::resizable) && !(window_flags & WindowFlagBits::docked))
        {
            emit_quads_hit_rect_only(ctx, _resize_hit_draw_id, _resize_hit_rect, can_emit_hit(ctx));
        }
        else
        {
            if (_resize_hit_draw_id.hit_id != AUIK_INVALID_DRAW_DATA_ID)
            {
                detail::RectData hidden_rect = _resize_hit_rect;
                hidden_rect.bounds.size = {0.0f, 0.0f};
                emit_quads_hit_rect_only(ctx, _resize_hit_draw_id, hidden_rect, can_emit_hit(ctx));
            }
        }
    }

    void Window::redraw_decorations(DrawReasonFlags reason)
    {
        DrawCtx ctx{reason};
        if (is_disabled()) ctx.is_hit_allowed = false;

        DrawCtx own_ctx(ctx);
        if (!is_hittable()) own_ctx.is_hit_allowed = false;

        auto *quads_stream = get_primary_quads_stream();
        QuadsInstanceData bg_data{};
        bg_data.rect = bounds();
        bg_data.z_order = get_z_order();
        const bool bg_visible = fill_quads_instance_by_style(resolved_window_style(), clip_id(), bg_data);
        emit_quads_instance(own_ctx, quads_stream, _bg_draw_id, bg_data, get_rect(), bg_visible, can_emit_hit(own_ctx));

        if (_header && !is_docked_window(*this))
        {
            DrawCtx header_ctx = ctx;
            _header->draw_local(header_ctx);
        }

        const auto transition = detail::get_widget_style_selector_transition(id());
        const bool scrollbar_transition = detail::is_scrollbar_tag(transition.current_id.tag_id) ||
                                          detail::is_scrollbar_tag(transition.prev_id.tag_id);
        if (scrollbar_transition)
        {
            if (_content_block)
            {
                DrawCtx content_ctx = ctx;
                _content_block->draw_local(content_ctx);
            }
            if ((window_flags & WindowFlagBits::resizable) && !(window_flags & WindowFlagBits::docked))
            {
                emit_quads_hit_rect_only(own_ctx, _resize_hit_draw_id, _resize_hit_rect, can_emit_hit(own_ctx));
            }
        }
    }

    bool Window::accepts_focus_on_mouse_press(ElementID hit_id) const
    {
        if (hit_id.tag_id == AUIK_TAG_WINDOW_HEADER) return true;
        if (hit_id.tag_id == AUIK_TAG_HITBOX) return true;
        if (detail::is_scrollbar_tag(hit_id.tag_id)) return false;
        return true;
    }

    void Window::update_depth(const amal::vec2 &depth_range)
    {
        clear_window_frame_hitbox_flag(*this);
        if ((window_flags & WindowFlagBits::decorated) && !_header)
            _header = acul::alloc<WindowHeader>(this, _title, window_flags & WindowFlagBits::movable);
        sync_header_popup_menu();
        const f32 prev_resize_hit_depth = _resize_hit_depth;
        Widget::update_depth(depth_range);

        u32 content_requirement = _content_block ? _content_block->get_depth_requirement() : 1u;

        u32 content_chrome_requirement = content_requirement;
        if (auto *classic_menu = window_menu_bar(*this))
            content_chrome_requirement = amal::max(content_chrome_requirement, classic_menu->get_depth_requirement());
        if (_header && !is_docked_window(*this))
            content_chrome_requirement = amal::max(content_chrome_requirement, _header->get_depth_requirement());

        u32 foreground_requirement = 1u;
        if (_rubber_band)
            foreground_requirement = amal::max(foreground_requirement, _rubber_band->get_depth_requirement());

        DepthCursor cursor(this->depth_range(), 1u + content_chrome_requirement + foreground_requirement);
        const amal::vec2 bg_range = cursor.next(1u);
        const amal::vec2 content_chrome_range = cursor.next(content_chrome_requirement);
        const amal::vec2 foreground_range = cursor.next(foreground_requirement);
        get_rect().depth = next_depth(bg_range);
        get_rect().hit_depth = get_rect().depth;
        _resize_hit_depth = next_depth(foreground_range);
        if (prev_resize_hit_depth != _resize_hit_depth)
            detail::get_context().dirty_flags |= DirtyFlagBits::hit_rect_update;
        fill_window_resize_hit_rect(id(), bounds(), _resize_hit_depth, clip_id(), _resize_hit_rect);

        const amal::vec2 child_work_range = detail::depth_work_range(content_chrome_range);
        if (_content_block) _content_block->update_depth(child_work_range);

        if (auto *classic_menu = window_menu_bar(*this)) classic_menu->update_depth(child_work_range);
        if (_header && !is_docked_window(*this)) _header->update_depth(child_work_range);

        if (_rubber_band) _rubber_band->update_depth(foreground_range);
    }

    u32 Window::get_depth_requirement() const
    {
        u32 content_requirement = _content_block ? _content_block->get_depth_requirement() : 1u;

        u32 content_chrome_requirement = content_requirement;
        if (auto *classic_menu = window_menu_bar(*this))
            content_chrome_requirement = amal::max(content_chrome_requirement, classic_menu->get_depth_requirement());
        if ((window_flags & WindowFlagBits::decorated) && !(window_flags & WindowFlagBits::docked))
            content_chrome_requirement =
                amal::max(content_chrome_requirement, _header ? _header->get_depth_requirement() : 1u);
        u32 foreground_requirement = 1u;
        if (_rubber_band)
            foreground_requirement = amal::max(foreground_requirement, _rubber_band->get_depth_requirement());
        return 1u + content_chrome_requirement + foreground_requirement;
    }

    void Window::back_hit_depth()
    {
        Widget::back_hit_depth();
        _resize_hit_rect.hit_depth = get_rect().hit_depth;
        if (_header) _header->back_hit_depth();
        if (_menu) _menu->back_hit_depth();
        if (_rubber_band) _rubber_band->back_hit_depth();
        if (_content_block) _content_block->back_hit_depth();
    }

    void Window::restore_hit_depth()
    {
        get_rect().hit_depth = get_rect().depth;
        _resize_hit_rect.hit_depth = _resize_hit_depth;
        detail::get_context().dirty_flags |= DirtyFlagBits::hit_rect_update;
        if (_header) _header->restore_hit_depth();
        if (_menu) _menu->restore_hit_depth();
        if (_rubber_band) _rubber_band->restore_hit_depth();
        if (_content_block) _content_block->restore_hit_depth();
    }

    StyleUpdateFlags Window::update_style()
    {
        StyleUpdateFlags out = StyleUpdateFlagBits::none;
        const u32 frame_style_tag = effective_window_style_tag();
        if (_window_style.tag_id != frame_style_tag) _window_style = {Theme::STYLE_ID_INVALID, frame_style_tag};
        out |= resolve_style_selector(_window_style, id(), 0u, style_state());
        const Style &window_style = resolved_window_style();
        _min_size = {window_style.min_width(), window_style.min_height()};
        if (_content_block && _content_block->content_padding() != window_style.padding())
        {
            _content_block->set_content_padding(window_style.padding());
            _content_block->invalidate_layout_measure();
        }
        if (_content_block) out |= _content_block->update_style_invalidated();
        if (_menu) out |= _menu->update_style_invalidated();

        if ((window_flags & WindowFlagBits::decorated) && !_header)
            _header = acul::alloc<WindowHeader>(this, _title, window_flags & WindowFlagBits::movable);
        if (window_flags & WindowFlagBits::decorated)
        {
            if (window_flags & WindowFlagBits::movable) _header->widget_flags |= WidgetFlagBits::hittable;
            else _header->widget_flags &= ~WidgetFlagBits::hittable;
            out |= _header->update_style_invalidated();
        }
        else
        {
            if (_header)
            {
                acul::release(_header);
                _header = nullptr;
            }
        }
        sync_header_popup_menu();

        if (_rubber_band) out |= _rubber_band->update_style_invalidated();
        return out;
    }

    void Window::reset_clip_rect_records()
    {
        Widget::reset_clip_rect_records();
        _resize_hit_rect.clip_id = 0xFFFFu;
        if (_header) _header->reset_clip_rect_records();
        if (_menu) _menu->reset_clip_rect_records();
        if (_default_header_menu && _default_header_menu != window_menu_widget(*this))
            _default_header_menu->reset_clip_rect_records();
        if (_content_block) _content_block->reset_clip_rect_records();
        if (_rubber_band) _rubber_band->reset_clip_rect_records();
    }

    void Window::rebuild_clip_rects()
    {
        amal::vec4 self_clip = {position().x, position().y, size().x, size().y};
        self_clip = detail::intersect_rects(self_clip, get_window_clip_bounds(this));
        ensure_own_clip_rect(self_clip);

        if (_header && !is_docked_window(*this))
        {
            _header->rebuild_clip_rects();
            _header->set_clip_id(clip_id());
        }

        if (_menu && owns_window_menu_tree(*this))
        {
            _menu->set_clip_id(clip_id());
            _menu->rebuild_clip_rects();
        }
        if (_default_header_menu && owns_popup_menu_tree(*this) && header_popup_menu() == _default_header_menu)
        {
            _default_header_menu->set_clip_id(clip_id());
            _default_header_menu->rebuild_clip_rects();
        }

        if (_content_block) _content_block->rebuild_clip_rects();

        if (_rubber_band) _rubber_band->rebuild_clip_rects();
    }

    void Window::reset_draw_records()
    {
        Widget::reset_draw_records();
        _bg_draw_id = {};
        _resize_hit_draw_id = {};
        if (_header) _header->reset_draw_records();
        if (_menu) _menu->reset_draw_records();
        if (_default_header_menu) _default_header_menu->reset_draw_records();
        if (_rubber_band) _rubber_band->reset_draw_records();
        if (_content_block) _content_block->reset_draw_records();
    }

    void Window::update_layout_min_size_force()
    {
        f32 menu_height = 0.0f;
        if (auto *classic_menu = window_menu_bar(*this);
            classic_menu && owns_classic_menu_bar_tree(*this) && classic_menu->is_visible())
        {
            classic_menu->set_layout_size({size().x, 0.0f});
            classic_menu->update_layout_min_size();
            menu_height = classic_menu->required_size().y;
        }
        if (_content_block) _content_block->update_layout_min_size();
        const amal::vec2 children_min_size = _content_block ? _content_block->required_size() : amal::vec2{0.0f, 0.0f};

        const f32 header_height = get_window_header_height(_header, *this);
        set_required_size(
            amal::max(_min_size, amal::vec2{children_min_size.x, header_height + menu_height + children_min_size.y}));
    }

    void Window::update_layout(bool min_size_known)
    {
        clear_window_frame_hitbox_flag(*this);
        if (layout_measure_required(min_size_known)) update_layout_min_size_force();
        if (!parent() && !(window_flags & WindowFlagBits::docked) && !is_root_viewport_managed(this))
        {
            if (_auto_size.x || _auto_size.y)
            {
                auto auto_size_probe = size();
                if (_auto_size.x) auto_size_probe.x = 0.0f;
                if (_auto_size.y) auto_size_probe.y = 0.0f;
                set_layout_size(auto_size_probe);
            }
            const auto resolved_size = resolve_root_window_size(*this);
            set_layout_size(resolved_size);
            if ((_auto_position.x || _auto_position.y) && !_auto_position_resolved)
            {
                const auto auto_pos = resolve_auto_root_window_position(*this);
                amal::vec2 next_pos = position();
                if (_auto_position.x) next_pos.x = auto_pos.x;
                if (_auto_position.y) next_pos.y = auto_pos.y;
                set_position(next_pos);
                _auto_position_resolved = true;
            }
        }
        if (!parent() && !(window_flags & WindowFlagBits::docked))
            set_position(resolve_root_widget_position(this, size(), !_move_drag_active));

        if (detail::is_fast_layout_update())
        {
            Widget::update_layout(true);
            const amal::vec4 parent_bounds = get_window_clip_bounds(this);
            const amal::vec4 self_clip_rect =
                detail::intersect_rects({position().x, position().y, size().x, size().y}, parent_bounds);
            ensure_own_clip_rect(self_clip_rect);
            fill_window_resize_hit_rect(id(), bounds(), _resize_hit_depth, clip_id(), _resize_hit_rect);

            auto *classic_menu = window_menu_bar(*this);
            const bool layout_menu_bar =
                classic_menu && owns_classic_menu_bar_tree(*this) && classic_menu->is_visible();
            const f32 menu_height = layout_menu_bar ? classic_menu->required_size().y : 0.0f;
            const amal::vec2 children_min_size =
                _content_block ? _content_block->required_size() : amal::vec2{0.0f, 0.0f};
            const f32 header_height = get_window_header_height(_header, *this);
            set_required_size(amal::max(
                _min_size, amal::vec2{children_min_size.x, header_height + menu_height + children_min_size.y}));

            const f32 header_top_y = position().y;
            const f32 header_bottom_y =
                (_header && !is_docked_window(*this)) ? snap_layout_start(header_top_y + header_height) : header_top_y;
            const f32 menu_top_y = header_bottom_y;
            sync_header_popup_menu();
            const f32 menu_bottom_y = layout_menu_bar ? snap_layout_start(menu_top_y + menu_height) : menu_top_y;
            const f32 body_top_y = snap_layout_start(menu_bottom_y);
            const f32 body_bottom_y = snap_layout_end(position().y + size().y);
            const f32 body_height = amal::max(body_bottom_y - body_top_y, 0.0f);
            const f32 header_layout_height = amal::max(header_bottom_y - header_top_y, 0.0f);
            const f32 menu_layout_height = amal::max(menu_bottom_y - menu_top_y, 0.0f);

            if (_header && !is_docked_window(*this))
            {
                _header->set_clip_id(clip_id());
                _header->set_position(position());
                _header->set_layout_size({size().x, header_layout_height});
                _header->update_layout(true);
            }
            if (classic_menu)
            {
                classic_menu->set_popup_parent(classic_menu);
                classic_menu->set_clip_id(clip_id());
                classic_menu->set_position({position().x, menu_top_y});
                classic_menu->set_layout_size(layout_menu_bar ? amal::vec2{size().x, menu_layout_height}
                                                              : amal::vec2{0.0f, 0.0f});
                classic_menu->update_layout(true);
            }

            if (_content_block)
            {
                const bool can_scroll_y =
                    (window_flags & WindowFlagBits::scrollable) && !(window_flags & WindowFlagBits::no_scrollbar_y);
                const bool can_scroll_x =
                    (window_flags & WindowFlagBits::scrollable) && !(window_flags & WindowFlagBits::no_scrollbar_x);
                _content_block->set_scrollbars_enabled(can_scroll_x, can_scroll_y);
                _content_block->set_clip_id(clip_id());
                _content_block->set_position({position().x, body_top_y});
                _content_block->set_layout_size({size().x, body_height});
                _content_block->update_layout(true);
            }

            if (window_flags & WindowFlagBits::scrollable)
            {
                const bool is_scrollbar_y_visible = _content_block && _content_block->has_visible_scrollbar_y();
                const bool is_scrollbar_x_visible = _content_block && _content_block->has_visible_scrollbar_x();
                const bool needs_scroll_events = is_scrollbar_y_visible || is_scrollbar_x_visible;
                const bool needs_hover_events = needs_scroll_events || ((window_flags & WindowFlagBits::resizable) &&
                                                                        !(window_flags & WindowFlagBits::docked));
                sync_window_event_flags(needs_scroll_events, needs_hover_events);
            }

            sync_rubber_band();
            return;
        }

        Widget::update_layout(true);
        const amal::vec4 parent_bounds = get_window_clip_bounds(this);
        const amal::vec4 self_clip_rect =
            detail::intersect_rects({position().x, position().y, size().x, size().y}, parent_bounds);
        ensure_own_clip_rect(self_clip_rect);
        fill_window_resize_hit_rect(id(), bounds(), _resize_hit_depth, clip_id(), _resize_hit_rect);

        f32 menu_height = 0.0f;
        if (auto *classic_menu = window_menu_bar(*this);
            classic_menu && owns_classic_menu_bar_tree(*this) && classic_menu->is_visible())
        {
            const bool menu_width_changed = classic_menu->size().x != size().x;
            classic_menu->set_layout_size({size().x, 0.0f});
            // The normal measure pass already populated required_size(). Only a changed
            // wrapping width can invalidate that result between measure and arrange.
            if (menu_width_changed)
            {
                classic_menu->invalidate_layout_measure();
                classic_menu->update_layout_min_size();
            }
            menu_height = classic_menu->required_size().y;
        }

        const amal::vec2 children_min_size = _content_block ? _content_block->required_size() : amal::vec2{0.0f, 0.0f};
        const f32 header_height = get_window_header_height(_header, *this);
        set_required_size(
            amal::max(_min_size, amal::vec2{children_min_size.x, header_height + menu_height + children_min_size.y}));

        const f32 header_top_y = position().y;
        const f32 header_bottom_y =
            (_header && !is_docked_window(*this)) ? snap_layout_start(header_top_y + header_height) : header_top_y;
        const f32 menu_top_y = header_bottom_y;
        auto *classic_menu = window_menu_bar(*this);
        if (!(window_flags & WindowFlagBits::decorated) && (is_popup_menu() || _header_menu_suffix_group != 0xFFFFu))
            assert(false && "PopupMenu requires window decoration");
        sync_header_popup_menu();
        const bool layout_menu_bar = classic_menu && owns_classic_menu_bar_tree(*this) && classic_menu->is_visible();
        const f32 menu_bottom_y = layout_menu_bar ? snap_layout_start(menu_top_y + menu_height) : menu_top_y;
        const f32 body_top_y = snap_layout_start(menu_bottom_y);
        const f32 body_bottom_y = snap_layout_end(position().y + size().y);
        const f32 body_height = amal::max(body_bottom_y - body_top_y, 0.0f);
        const f32 header_layout_height = amal::max(header_bottom_y - header_top_y, 0.0f);
        const f32 menu_layout_height = amal::max(menu_bottom_y - menu_top_y, 0.0f);

        if (_header && !is_docked_window(*this))
        {
            _header->set_clip_id(clip_id());
            _header->set_position(position());
            _header->set_layout_size({size().x, header_layout_height});
            _header->update_layout(true);
        }
        if (_menu)
        {
            if (classic_menu)
            {
                classic_menu->set_popup_parent(classic_menu);
                classic_menu->set_clip_id(clip_id());
                classic_menu->set_position({position().x, menu_top_y});
                classic_menu->set_layout_size(layout_menu_bar ? amal::vec2{size().x, menu_layout_height}
                                                              : amal::vec2{0.0f, 0.0f});
                classic_menu->update_layout(true);
            }
        }

        const bool was_scrollbar_y_visible = _content_block && _content_block->has_visible_scrollbar_y();
        const bool was_scrollbar_x_visible = _content_block && _content_block->has_visible_scrollbar_x();
        if (_content_block)
        {
            const bool can_scroll_y =
                (window_flags & WindowFlagBits::scrollable) && !(window_flags & WindowFlagBits::no_scrollbar_y);
            const bool can_scroll_x =
                (window_flags & WindowFlagBits::scrollable) && !(window_flags & WindowFlagBits::no_scrollbar_x);
            _content_block->set_scrollbars_enabled(can_scroll_x, can_scroll_y);
            _content_block->set_clip_id(clip_id());
            _content_block->set_position({position().x, body_top_y});
            _content_block->set_layout_size({size().x, body_height});
            _content_block->update_layout(true);
        }

        if (window_flags & WindowFlagBits::scrollable)
        {
            const bool is_scrollbar_y_visible = _content_block && _content_block->has_visible_scrollbar_y();
            const bool is_scrollbar_x_visible = _content_block && _content_block->has_visible_scrollbar_x();
            const bool needs_scroll_events = is_scrollbar_y_visible || is_scrollbar_x_visible;
            const bool needs_hover_events = needs_scroll_events || ((window_flags & WindowFlagBits::resizable) &&
                                                                    !(window_flags & WindowFlagBits::docked));
            sync_window_event_flags(needs_scroll_events, needs_hover_events);
            if (was_scrollbar_y_visible != is_scrollbar_y_visible || was_scrollbar_x_visible != is_scrollbar_x_visible)
            {
                auto &ctx = detail::get_context();
                ctx.dirty_flags |= DirtyFlagBits::redraw | DirtyFlagBits::hit_rect_update;
                mark_host_refresh_request();
            }
        }
        sync_rubber_band();
    }

    void Window::translate(const amal::vec2 &delta)
    {
        if (delta.x == 0.0f && delta.y == 0.0f) return;

        amal::vec2 next_position = position() + delta;
        if (!parent() && !(window_flags & WindowFlagBits::docked))
        {
            next_position = position() + clamp_root_widget_drag_delta(this, delta);
        }
        const amal::vec2 applied_delta = next_position - position();
        if (applied_delta.x == 0.0f && applied_delta.y == 0.0f) return;

        Widget::translate(applied_delta);
        _resize_hit_rect.bounds.offset += applied_delta;
        auto &ctx = detail::get_context();
        ctx.dirty_flags |= DirtyFlagBits::hit_rect_update;

        const amal::vec4 parent_bounds = get_window_clip_bounds(this);
        const amal::vec4 self_clip_rect =
            detail::intersect_rects({position().x, position().y, size().x, size().y}, parent_bounds);
        if (clip_id() != 0xFFFFu) update_clip_rect(clip_id(), self_clip_rect);

        if (_header && !is_docked_window(*this)) _header->translate(applied_delta);
        if (auto *classic_menu = window_menu_bar(*this)) classic_menu->translate(applied_delta);
        if (_rubber_band) _rubber_band->translate(applied_delta);
        if (_content_block) _content_block->translate(applied_delta);
    }

    void Window::on_scroll(const amal::vec2 &delta)
    {
        if (_content_block) _content_block->on_scroll(delta);
    }

    void Window::on_hover(HoverState state)
    {
        auto &ctx = detail::get_context();
        const bool is_own_hitbox = ctx.hover_id.widget_id == id() && ctx.hover_id.tag_id == AUIK_TAG_HITBOX;
        const bool can_resize = (window_flags & WindowFlagBits::resizable) && !(window_flags & WindowFlagBits::docked);
        if (state != HoverState::leave && is_own_hitbox && can_resize)
        {
            int resize_x = 0;
            int resize_y = 0;
            get_window_resize_direction(_resize_hit_rect, ctx.io.mouse_pos, resize_x, resize_y);
            detail::set_window_cursor(get_window_resize_cursor(resize_x, resize_y), ctx.window_ctx);
            return;
        }

        detail::set_window_cursor(detail::CursorID::arrow, ctx.window_ctx);
    }

    void Window::on_click(MouseKey key, KeyPressState state, u32 click_count)
    {
        if (key != MouseKey::left) return;
        if (state != KeyPressState::press) return;
        (void)click_count;

        auto &ctx = detail::get_context();
        if (ctx.hover_id.widget_id == id() && ctx.hover_id.tag_id == AUIK_TAG_POPUP_MENU_BUTTON)
        {
            _move_drag_active = false;
            _resize_dir = {0, 0};
            if (auto *menu = header_popup_menu()) menu->on_click(key, state, click_count);
            return;
        }

        _move_drag_active =
            ((window_flags & WindowFlagBits::decorated) && ctx.hover_id.tag_id == AUIK_TAG_WINDOW_HEADER) ||
            (!(window_flags & WindowFlagBits::decorated) && ctx.hover_id.widget_id == id() &&
             ctx.hover_id.tag_id == get_rect().id.tag_id);
        _resize_dir = {0, 0};
        if ((window_flags & WindowFlagBits::resizable) && !(window_flags & WindowFlagBits::docked) &&
            ctx.hover_id.tag_id == AUIK_TAG_HITBOX)
        {
            // Capture exact direction at press time from current geometry/mouse.
            // This prevents losing diagonal resize on tiny hover-zone jitter.
            get_window_resize_direction(_resize_hit_rect, ctx.io.mouse_pos, _resize_dir.x, _resize_dir.y);
            if (_resize_dir.x != 0 || _resize_dir.y != 0)
            {
                _move_drag_active = false;
                detail::set_window_cursor(get_window_resize_cursor(_resize_dir.x, _resize_dir.y), ctx.window_ctx);
            }
        }

        if (_content_block && detail::is_scrollbar_tag(ctx.hover_id.tag_id))
        {
            _move_drag_active = false;
            _content_block->on_click(key, state, click_count);
            return;
        }

        if (!_rubber_band) return;
        if (_move_drag_active || _resize_dir.x != 0 || _resize_dir.y != 0) return;
        if (ctx.hover_id.widget_id != id() || ctx.hover_id.tag_id != AUIK_TAG_WINDOW) return;
        ctx.io.clicked_id = make_element_id(id(), AUIK_TAG_RUBBER_BAND);
    }

    void Window::on_drag(const amal::vec2 &delta, KeyPressState state)
    {
        auto &ctx = detail::get_context();
        const auto drag_id = ctx.io.drag_id;
        const bool own_rubber_band = drag_id.widget_id == id() && drag_id.tag_id == AUIK_TAG_RUBBER_BAND;
        const bool descendant_rubber_band = drag_id.widget_id != id();
        if (_rubber_band && (own_rubber_band || descendant_rubber_band))
        {
            _rubber_band->dispatch_drag(delta, state);
            if (state == KeyPressState::release) commit_rubber_band();
            return;
        }

        if (state == KeyPressState::release)
        {
            const bool was_resizing = _resize_dir.x != 0 || _resize_dir.y != 0;
            if (auto *dock_ctx = detail::g_context ? detail::g_context->dockspace_context : nullptr;
                dock_ctx && dock_ctx->drag_window == this)
            {
                restore_hit_depth();
                update_draw_commands(DrawReasonBits::full_redraw);
                disable_dockspace_drag_zones(this, "window-release");
            }
            _move_drag_active = false;
            if (ctx.hover_id.tag_id == AUIK_TAG_HITBOX)
            {
                int resize_x = 0;
                int resize_y = 0;
                get_window_resize_direction(_resize_hit_rect, ctx.io.mouse_pos, resize_x, resize_y);
                detail::set_window_cursor(get_window_resize_cursor(resize_x, resize_y), ctx.window_ctx);
            }
            else detail::set_window_cursor(detail::CursorID::arrow, ctx.window_ctx);
            _resize_dir = {0, 0};
            if (was_resizing)
            {
                update_layout(false);
                update_draw_commands(DrawReasonBits::layout);
                ctx.dirty_flags |= DirtyFlagBits::redraw | DirtyFlagBits::hit_rect_update;
                mark_host_refresh_request();
            }
            return;
        }

        if (state == KeyPressState::press && can_start_dock_drag(*this, drag_id))
        {
            back_hit_depth();
            update_draw_commands(DrawReasonBits::full_redraw);
            enable_dockspace_drag_zones(this);
            return;
        }

        const bool drag_scrollbar =
            _content_block && detail::is_scrollbar_tag(drag_id.tag_id) && drag_id.widget_id == _content_block->id();
        if (drag_scrollbar)
        {
            _content_block->on_drag(delta, state);
            return;
        }

        if (_resize_dir.x != 0 || _resize_dir.y != 0)
        {
            detail::set_window_cursor(get_window_resize_cursor(_resize_dir.x, _resize_dir.y), ctx.window_ctx);
            if (!(window_flags & WindowFlagBits::resizable)) return;
            if (window_flags & WindowFlagBits::docked) return;

            const amal::vec2 old_pos = position();
            const amal::vec2 old_size = size();
            amal::vec2 new_pos = old_pos;
            amal::vec2 new_size = old_size;

            const bool hit_left = _resize_dir.x < 0;
            const bool hit_right = _resize_dir.x > 0;
            const bool hit_top = _resize_dir.y < 0;
            const bool hit_bottom = _resize_dir.y > 0;

            if (hit_left)
            {
                new_pos.x += delta.x;
                new_size.x -= delta.x;
            }
            else if (hit_right) new_size.x += delta.x;

            if (hit_top)
            {
                new_pos.y += delta.y;
                new_size.y -= delta.y;
            }
            else if (hit_bottom) new_size.y += delta.y;

            const f32 min_width = _min_size.x > 0.0f ? _min_size.x : 0.0f;
            const f32 min_height = _min_size.y > 0.0f ? _min_size.y : 0.0f;
            if (new_size.x < min_width)
            {
                if (hit_left) new_pos.x -= (min_width - new_size.x);
                new_size.x = min_width;
            }
            if (new_size.y < min_height)
            {
                if (hit_top) new_pos.y -= (min_height - new_size.y);
                new_size.y = min_height;
            }

            if (new_pos == old_pos && new_size == old_size) return;
            if (_resize_dir.x != 0) _auto_size.x = false;
            if (_resize_dir.y != 0) _auto_size.y = false;
            set_position(new_pos);
            set_size(new_size);
            detail::mark_fast_update_dirty();
            add_render_command<detail::DragEventTraits>(this, [this]() {
                update_layout(true);
                update_draw_commands(DrawReasonBits::layout);
            });
            ctx.dirty_flags |= DirtyFlagBits::redraw;
            return;
        }

        if (!(window_flags & WindowFlagBits::movable)) return;
        if (window_flags & WindowFlagBits::docked) return;
        if (!_move_drag_active) return;

        if ((window_flags & WindowFlagBits::dockable) && ctx.hover_id.tag_id == AUIK_TAG_DOCKSPACE_TAB_PANEL)
        {
            auto it = ctx.id_map.find(ctx.hover_id.widget_id);
            if (it != ctx.id_map.end())
            {
                auto *dockspace = static_cast<Dockspace *>(it->second);
                if (dockspace && dockspace->dock_drag_window_to_tab_panel(this, ctx.hover_id.element_id)) return;
            }
        }

        translate(delta);
        add_render_command<detail::DragEventTraits>(this, [this]() {
            update_draw_commands(DrawReasonBits::external);
            detail::get_context().dirty_flags |= DirtyFlagBits::redraw;
        });
    }

    void Window::on_focus(bool focused)
    {
        set_style_state(focused ? StyleState::focus : StyleState::normal);
        if (!focused) return;
        if ((window_flags & WindowFlagBits::docked) || parent()) return;

        auto &ctx = detail::get_context();
        auto self_it = ctx.id_map.find(id());
        if (self_it == ctx.id_map.end()) return;
        auto *self_window = as_root_window(self_it->second);
        if (!self_window) return;

        auto &tree = ctx.widget_tree;
        const u32 count = static_cast<u32>(tree.size());
        if (count <= 1) return;

        u32 index = count;
        for (u32 i = 0; i < count; ++i)
        {
            if (tree[i] == self_window)
            {
                index = i;
                break;
            }
        }
        if (index >= count) return;

        u32 top_index = count;
        for (u32 i = count; i > 0u; --i)
        {
            if (!as_root_window(tree[i - 1u])) continue;
            top_index = i - 1u;
            break;
        }
        if (top_index >= count || index == top_index) return;

        Widget *top = tree[top_index];
        Window *top_window = as_root_window(top);
        const amal::vec2 self_range = self_window->depth_range();
        const amal::vec2 top_range = top->depth_range();

        top->update_depth(self_range);
        self_window->update_depth(top_range);
        tree[index] = top;
        tree[top_index] = self_window;

        const bool self_needs_layout = needs_layout_on_active(*self_window);
        add_render_command<detail::FocusEventTraits>(self_window, [self_window, self_needs_layout]() {
            auto flags = self_window->update_style_invalidated();
            if (self_needs_layout)
            {
                self_window->update_layout(true);
                flags |= StyleUpdateFlagBits::layout | StyleUpdateFlagBits::redraw;
            }
            const auto reason = get_draw_reason_from_style_update(flags);
            self_window->update_draw_commands(reason == DrawReasonBits::none ? DrawReasonBits::external : reason);
            detail::get_context().dirty_flags |= DirtyFlagBits::redraw;
        });

        if (top_window)
        {
            const bool top_needs_layout = needs_layout_on_active(*top_window);
            add_render_command<detail::FocusEventTraits>(top_window, [top_window, top_needs_layout]() {
                auto flags = top_window->update_style_invalidated();
                if (top_needs_layout)
                {
                    top_window->update_layout(true);
                    flags |= StyleUpdateFlagBits::layout | StyleUpdateFlagBits::redraw;
                }
                const auto reason = get_draw_reason_from_style_update(flags);
                top_window->update_draw_commands(reason == DrawReasonBits::none ? DrawReasonBits::external : reason);
                detail::get_context().dirty_flags |= DirtyFlagBits::redraw;
            });
        }
        else
            add_render_command<detail::FocusEventTraits>(top, [top]() {
                top->update_draw_commands();
                detail::get_context().dirty_flags |= DirtyFlagBits::redraw;
            });
    }

    namespace
    {
        void restore_window_content(DrawBlock *dst, DrawBlock *src)
        {
            if (!dst || !src) return;

            detail::WidgetCommonData common{};
            common.id = src->id();
            common.widget_flags = src->widget_flags;
            common.bounds = src->bounds();
            common.inline_size = src->inline_size();
            detail::apply_widget_common_data(dst, common);

            dst->set_size(src->explicit_size());

            dst->set_style_tag(src->style_tag());
            dst->set_draw_block_flags(src->draw_block_flags());
            dst->set_content_padding(src->content_padding());
            dst->set_scrollbar_style_tags(src->scrollbar_track_style_tag(), src->scrollbar_thumb_style_tag());

            dst->clear_children();
            const auto &layouts = src->child_layouts();
            for (size_t child_i = 0u; child_i < src->children.size(); ++child_i)
            {
                const ChildLayoutFlags layout =
                    child_i < layouts.size() ? layouts[child_i] : default_child_layout_flags();
                dst->add_child(src->children[child_i], layout);
            }
            src->children.clear();
        }

        void write_window_menu(acul::bin_stream &stream, const Window &window)
        {
            auto *proxy = window.get_menu();
            Widget *menu = proxy ? proxy->get_widget() : nullptr;
            stream.write(menu != nullptr);
            if (!menu) return;

            stream.write(static_cast<umbf::Block *>(menu));
        }

        Widget *read_window_menu(acul::bin_stream &stream)
        {
            bool has_menu = false;
            stream.read(has_menu);
            if (!has_menu) return nullptr;

            umbf::Block *block = nullptr;
            stream.read(block);
            return static_cast<Widget *>(block);
        }

        void write_window(acul::bin_stream &stream, umbf::Block *block)
        {
            auto *widget = static_cast<Window *>(block);
            detail::write_widget_common_data(stream, *widget);
            const Text *title_text = widget->title_text();
            const bool translated = title_text && title_text->is_translated_text();
            const char *literal = translated ? title_text->translated_text_literal() : nullptr;
            detail::write_localized_string(stream, translated ? acul::string(literal ? literal : "") : widget->title(),
                                           translated);
            stream.write(static_cast<u32>(widget->window_flags)).write(widget->window_style_tag());

            stream.write(static_cast<umbf::Block *>(widget->content_block()));
            write_window_menu(stream, *widget);
        }

        umbf::Block *read_window(acul::bin_stream &stream)
        {
            const auto common = detail::read_widget_common_data(stream);
            const auto title = detail::read_localized_string(stream);
            u32 window_flags = 0u;
            u32 window_style_tag = AUIK_STYLE_TAG_WINDOW;
            stream.read(window_flags).read(window_style_tag);

            umbf::Block *content_block = nullptr;
            stream.read(content_block);

            auto *widget =
                acul::alloc<Window>(common.id, StringView{title.text.c_str(), title.translated}, common.bounds,
                                    WindowFlags(window_flags), WidgetFlags(common.widget_flags));
            widget->set_window_style_tag(window_style_tag);
            if (auto *content = static_cast<DrawBlock *>(content_block))
            {
                restore_window_content(widget->content_block(), content);
                acul::release(content);
            }
            detail::apply_widget_common_data(widget, common);
            if (auto *menu = read_window_menu(stream)) widget->set_menu_widget(menu);
            return widget;
        }
    } // namespace

    namespace streams
    {
        AUIK_EXPORT const umbf::streams::Stream window{read_window, write_window};
    } // namespace streams

} // namespace auik
