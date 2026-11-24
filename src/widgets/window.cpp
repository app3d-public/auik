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
#include <algorithm>
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

    static inline bool is_docked_window(const Window &window) { return window.window_flags & WindowFlagBits::docked; }
    static inline u32 effective_window_style_tag(const Window &window)
    {
        return is_docked_window(window) && window.dock_style_tag() != 0u ? window.dock_style_tag()
                                                                         : window.window_style_tag();
    }

    static inline bool needs_layout_on_active(Window &window)
    {
        auto *theme = get_theme();
        const u32 window_style_tag = effective_window_style_tag(window);
        const Style &window_normal =
            theme->get_style(theme->get_resolved_style(window_style_tag, window.id(), 0, StyleState::normal));
        const Style &window_active =
            theme->get_style(theme->get_resolved_style(window_style_tag, window.id(), 0, StyleState::active));
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

    static inline f32 snap_layout_start(f32 value) { return amal::ceil(value); }

    static inline f32 snap_layout_end(f32 value) { return amal::floor(value); }

    static inline amal::vec4 window_border_insets(const Style &style)
    {
        const f32 thickness = amal::max(style.border_thickness(), 0.0f);
        const u32 border_mask = style.border_mask();
        return {(border_mask & AUIK_BORDER_LEFT_BIT) ? thickness : 0.0f,
                (border_mask & AUIK_BORDER_TOP_BIT) ? thickness : 0.0f,
                (border_mask & AUIK_BORDER_RIGHT_BIT) ? thickness : 0.0f,
                (border_mask & AUIK_BORDER_BOTTOM_BIT) ? thickness : 0.0f};
    }

    static inline bool owns_classic_menu_bar_tree(const Window &window)
    {
        return window.menu_bar() && !is_docked_window(window);
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
        // A window initially created as fit-content can later receive an explicit size
        // (for example, a popup frame rounded outward to the pixel grid). Do not
        // replace that request with the fractional measured size after the auto-size probe.
        if (is_size_concrete(requested.x)) next_size.x = requested.x;
        else if (is_size_fill(requested.x)) next_size.x = viewport.z;
        else if (is_size_fit(requested.x) || next_size.x <= 0.0f) next_size.x = required.x;
        if (is_size_concrete(requested.y)) next_size.y = requested.y;
        else if (is_size_fill(requested.y)) next_size.y = viewport.w;
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

    static inline f32 resolve_fit_window_height(f32 window_top, f32 header_height, f32 menu_height, f32 content_height,
                                                bool has_header, bool has_menu)
    {
        const f32 header_bottom = has_header ? snap_layout_start(window_top + header_height) : window_top;
        const f32 menu_bottom = has_menu ? snap_layout_start(header_bottom + menu_height) : header_bottom;
        const f32 body_top = snap_layout_start(menu_bottom);
        const f32 body_bottom = snap_layout_start(body_top + content_height);
        return amal::max(body_bottom - window_top, 0.0f);
    }

    static inline f32 get_window_header_height(Widget *header, const Window &window)
    {
        if (!header || is_docked_window(window)) return 0.0f;
        header->update_layout_min_size_force();
        return header->required_size().y;
    }

    Window::Window(u32 id, StringView title, const amal::rect &bounds, WindowFlags in_window_flags,
                   WidgetFlags in_widget_flags, DrawBlock *header)
        : Widget(id, in_widget_flags, EventFlagBits::click | EventFlagBits::drag | EventFlagBits::focus, bounds,
                 AUIK_TAG_WINDOW),
          _content_block(acul::alloc<ContentBlock>(id, WidgetFlagBits::visible | WidgetFlagBits::hittable,
                                                   AUIK_TAG_WINDOW_CONTENT)),
          children(_content_block->children),
          window_flags(in_window_flags),
          _title_storage(title.str ? title.str : ""),
          _title(_title_storage.c_str(), title.is_translated)
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
        if (_title_storage.empty())
        {
            _title.str = nullptr;
            _title.is_translated = false;
        }
        if (header) set_header(header);
        sync_window_flags();
        if (window_flags & WindowFlagBits::rubber_band)
        {
            _rubber_band = acul::alloc<RubberBand>(id, WidgetFlagBits::none);
            _rubber_band->set_parent(this);
        }
    }

    const Style &Window::resolved_window_style() const
    {
        // A child can rebuild its clip rect after the window style was invalidated but before
        // Window::update_style() gets its turn in the deferred refresh. Resolve the selector
        // lazily so internal content never observes the transient invalid cache entry.
        const u32 style_tag = effective_window_style_tag(*this);
        if (_window_style.id == Theme::STYLE_ID_INVALID || _window_style.tag_id != style_tag)
        {
            _window_style = {Theme::STYLE_ID_INVALID, style_tag};
            resolve_style_selector(_window_style, id(), 0u, style_state());
        }
        return get_theme()->get_style(_window_style.id);
    }

    bool Window::preserves_dock_size() const
    {
        const auto *settings = resolved_window_style().aspect_ratio_settings();
        return settings && settings->mode == AspectRatioMode::preserve;
    }

    bool Window::ContentBlock::apply_window_style(StyleID style_id, const Style &style)
    {
        const bool changed = content_padding() != style.padding() || inline_spacing() != style.inline_spacing() ||
                             _scroll.style_id != style_id;
        _scroll.style_id = style_id;
        set_content_padding(style.padding());
        set_inline_spacing(style.inline_spacing());
        return changed;
    }

    void Window::sync_window_flags()
    {
        if (!(window_flags & WindowFlagBits::decorated)) set_header(nullptr);
        else if (!_header)
        {
            auto *header = acul::alloc<DrawBlock>(AUIK_TAG_WINDOW_HEADER, WidgetFlagBits::visible,
                                                  AUIK_TAG_WINDOW_HEADER);
            header->set_style_tag(AUIK_STYLE_TAG_WINDOW_HEADER);
            header->set_size({AUIK_SIZE_X_FILL, AUIK_SIZE_Y_FIT});
            header->set_draw_block_flags(DrawBlockFlagBits::none);
            auto *title = make_text(AUIK_TAG_TEXT, _title, AUIK_SIZE_FIT);
            title->set_style_tag(AUIK_STYLE_TAG_NO_PAD);
            header->add_child(title, make_layout_flags(ChildLayout::inline_, HAlign::left, VAlign::center));
            set_header(header);
            bind_header_title(title);
        }
        if (_header && _header->id() == AUIK_TAG_WINDOW_HEADER)
        {
            const bool closable = window_flags & WindowFlagBits::closable;
            const bool has_close = _header->children.size() > 1u;
            if (closable && !has_close)
            {
                auto *close = make_close_button(AUIK_TAG_CLOSE_BUTTON);
                close->set_size(AUIK_SIZE_FIT);
                close->get_rect().id = make_element_id(id(), AUIK_TAG_CLOSE_BUTTON);
                _header->add_child(close, make_layout_flags(ChildLayout::inline_, HAlign::right, VAlign::center));
            }
            else if (!closable && has_close)
            {
                auto *close = _header->children[1];
                if (close->clip_id() != 0xFFFFu && !close->is_attached())
                {
                    close->update_style_invalidated();
                    close->invalidate_draw_commands(DrawReasonBits::layout);
                }
                _header->erase_children(1u, 1u);
            }
        }
        if (_header)
        {
            _header->sync_widget_flags();
            if (window_flags & WindowFlagBits::movable) _header->widget_flags |= WidgetFlagBits::hittable;
            else _header->widget_flags &= ~WidgetFlagBits::hittable;
        }
        invalidate_style();
        invalidate_layout_measure();
        if (is_attached()) detail::mark_layout_dirty();
    }

    void Window::set_header(DrawBlock *header)
    {
        if (header == _header) return;
        _header_title = nullptr;
        if (_header)
        {
            if (_header->is_attached()) _header->on_detach();
            if (is_attached()) _header->invalidate_draw_commands(DrawReasonBits::layout);
            acul::release(_header);
        }
        _header = header;
        if (_header)
        {
            _header->set_parent(this);
            _header->set_focus_parent(this);
            _header->get_rect().id = make_element_id(id(), AUIK_TAG_WINDOW_HEADER);
            if (!(window_flags & WindowFlagBits::movable)) _header->widget_flags &= ~WidgetFlagBits::hittable;
            if (is_attached() && (_header->widget_flags & WidgetFlagBits::attachable)) _header->on_attach();
        }
        invalidate_layout_measure();
        if (is_attached()) detail::mark_layout_dirty();
    }

    void Window::bind_header_title(Text *text)
    {
        _header_title = text;
        if (_header_title) _header_title->set_text(_title);
    }

    void Window::set_title(StringView title)
    {
        if (_title.str == title.str && _title.is_translated == title.is_translated) return;
        _title_storage = title.str ? title.str : "";
        _title.str = _title_storage.empty() ? nullptr : _title_storage.c_str();
        _title.is_translated = _title.str && title.is_translated;
        if (_header_title) _header_title->set_text(_title);
        invalidate_layout_measure();
        if (is_attached()) detail::mark_layout_dirty();
    }

    Window::~Window()
    {
        if (is_overlay_window(this)) remove_overlay_window(this);
        if (_rubber_band) acul::release(_rubber_band);
        if (_header) acul::release(_header);
        if (_content_block) acul::release(_content_block);
        if (_menu_bar) acul::release(_menu_bar);
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
        if (_content_block) _content_block->add_children(new_children);
    }

    template <class T>
    static void replace_window_menu(Window &window, T *&slot, T *next)
    {
        if (slot == next) return;
        if (slot && slot->is_attached()) slot->on_detach();
        if (slot) acul::release(slot);
        slot = next;
        if (slot)
        {
            slot->set_parent(&window);
            slot->set_focus_parent(&window);
            slot->attach_to_viewport(window.viewport());
            if (window.is_attached()) slot->on_attach();
        }
        window.invalidate_layout_measure();
        if (window.is_attached()) detail::mark_layout_dirty();
    }

    void Window::set_menu_bar(MenuBar *menu) { replace_window_menu(*this, _menu_bar, menu); }

    bool Window::open_context_menu(const amal::vec2 &position)
    {
        if (auto *menu = menu_bar())
        {
            if (!menu->open_content_at(position)) return false;
            focus_widget(menu);
            return true;
        }
        return _context_menu && _context_menu(position);
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
        if ((widget_flags & WidgetFlagBits::cache_global) && !(window_flags & WindowFlagBits::docked))
        {
            const auto state = detail::find_widget_global_cache(id());
            if (state && state->signature() == AUIK_TAG_WINDOW_STATE)
            {
                const auto value = acul::static_pointer_cast<WindowStateData>(state);
                if (!is_position_undefined(value->bounds.offset)) set_position(value->bounds.offset);
                if (is_size_concrete(value->bounds.size.x) && is_size_concrete(value->bounds.size.y) &&
                    value->bounds.size.x > 0.0f && value->bounds.size.y > 0.0f)
                    set_size(value->bounds.size);
            }
            else
            {
                auto value = acul::make_shared<WindowStateData>();
                value->bounds = bounds();
                detail::set_widget_global_cache(id(), value);
            }
        }
        if (!parent() && !is_overlay_window(this)) detail::setup_root_window(this);
        Widget::on_attach();
        if (_header && (_header->widget_flags & WidgetFlagBits::attachable) && !_header->is_attached())
            _header->on_attach();
        if (_content_block && !_content_block->is_attached()) _content_block->on_attach();
        if (_menu_bar && !_menu_bar->is_attached()) _menu_bar->on_attach();
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
        if (!parent() && !is_overlay_window(this)) detail::teardown_root_window(this);
        if (_menu_bar && _menu_bar->is_attached()) _menu_bar->on_detach();
        if (_header && _header->is_attached()) _header->on_detach();
        if (_content_block && _content_block->is_attached()) _content_block->on_detach();
        Widget::on_detach();
    }

    void Window::add_state_flags_inherit(WidgetStateFlags flags)
    {
        Widget::add_state_flags_inherit(flags);
        if ((flags & WidgetStateFlagBits::visible) && !is_visible()) flags &= ~WidgetStateFlagBits::visible;
        if (_header) _header->add_state_flags_inherit(flags);
        if (_content_block) _content_block->add_state_flags_inherit(flags);
        if (_menu_bar) _menu_bar->add_state_flags_inherit(flags);
        if (_rubber_band) _rubber_band->add_state_flags_inherit(flags);
    }

    void Window::remove_state_flags_inherit(WidgetStateFlags flags)
    {
        Widget::remove_state_flags_inherit(flags);
        if (_header) _header->remove_state_flags_inherit(flags);
        if (_content_block) _content_block->remove_state_flags_inherit(flags);
        if (_menu_bar) _menu_bar->remove_state_flags_inherit(flags);
        if (_rubber_band) _rubber_band->remove_state_flags_inherit(flags);
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
        auto *classic_menu = menu_bar();
        const bool invalidating = ctx.reason & DrawReasonBits::invalidate;
        const bool draw_menu_bar =
            classic_menu && owns_classic_menu_bar_tree(*this) && (classic_menu->is_visible() || invalidating);
        if (draw_menu_bar)
        {
            DrawCtx menu_ctx = ctx;
            classic_menu->draw_local(menu_ctx);
        }
        if (_content_block && (_content_block->is_visible() || invalidating))
        {
            DrawCtx content_ctx = ctx;
            // Internal popup content must route input to the same public owner
            // as the window surface (a combo/menu may override this hit ID).
            _content_block->get_rect().id.widget_id = get_rect().id.widget_id;
            _content_block->draw_local(content_ctx);
        }

        if (_rubber_band && (_rubber_band->is_visible() || invalidating))
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
        const f32 prev_resize_hit_depth = _resize_hit_depth;
        Widget::update_depth(depth_range);

        u32 content_requirement = _content_block ? _content_block->get_depth_requirement() : 1u;

        u32 content_chrome_requirement = content_requirement;
        if (auto *classic_menu = menu_bar())
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

        if (auto *classic_menu = menu_bar()) classic_menu->update_depth(child_work_range);
        if (_header && !is_docked_window(*this)) _header->update_depth(child_work_range);

        if (_rubber_band) _rubber_band->update_depth(foreground_range);
    }

    u32 Window::get_depth_requirement() const
    {
        u32 content_requirement = _content_block ? _content_block->get_depth_requirement() : 1u;

        u32 content_chrome_requirement = content_requirement;
        if (auto *classic_menu = menu_bar())
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
        if (_menu_bar) _menu_bar->back_hit_depth();
        if (_rubber_band) _rubber_band->back_hit_depth();
        if (_content_block) _content_block->back_hit_depth();
    }

    void Window::restore_hit_depth()
    {
        get_rect().hit_depth = get_rect().depth;
        _resize_hit_rect.hit_depth = _resize_hit_depth;
        detail::get_context().dirty_flags |= DirtyFlagBits::hit_rect_update;
        if (_header) _header->restore_hit_depth();
        if (_menu_bar) _menu_bar->restore_hit_depth();
        if (_rubber_band) _rubber_band->restore_hit_depth();
        if (_content_block) _content_block->restore_hit_depth();
    }

    StyleUpdateFlags Window::update_style()
    {
        StyleUpdateFlags out = StyleUpdateFlagBits::none;
        const u32 style_tag = effective_window_style_tag(*this);
        if (_window_style.tag_id != style_tag) _window_style = {Theme::STYLE_ID_INVALID, style_tag};
        out |= resolve_style_selector(_window_style, id(), 0u, style_state());
        const Style &window_style = resolved_window_style();
        _min_size = {window_style.min_width(), window_style.min_height()};
        if (_content_block && _content_block->apply_window_style(_window_style.id, window_style))
            _content_block->invalidate_layout_measure();
        if (_content_block) out |= _content_block->update_style_invalidated();
        if (_menu_bar) out |= _menu_bar->update_style_invalidated();

        if (_header) out |= _header->update_style_invalidated();

        if (_rubber_band) out |= _rubber_band->update_style_invalidated();
        return out;
    }

    void Window::reset_clip_rect_records()
    {
        Widget::reset_clip_rect_records();
        _resize_hit_rect.clip_id = 0xFFFFu;
        if (_header) _header->reset_clip_rect_records();
        if (_menu_bar) _menu_bar->reset_clip_rect_records();
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
            _header->set_clip_id(clip_id());
            _header->rebuild_clip_rects();
        }

        if (!is_docked_window(*this) && _menu_bar)
        {
            _menu_bar->set_clip_id(clip_id());
            _menu_bar->rebuild_clip_rects();
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
        if (_menu_bar) _menu_bar->reset_draw_records();
        if (_rubber_band) _rubber_band->reset_draw_records();
        if (_content_block) _content_block->reset_draw_records();
    }

    void Window::invalidate_style()
    {
        Widget::invalidate_style();
        if (_content_block) _content_block->invalidate_style();
        if (_header) _header->invalidate_style();
        if (_menu_bar) _menu_bar->invalidate_style();
        if (_rubber_band) _rubber_band->invalidate_style();
    }

    bool Window::update_locale()
    {
        bool changed = Widget::update_locale();
        if (_content_block) changed |= _content_block->update_locale();
        if (_header) changed |= _header->update_locale();
        if (_menu_bar) changed |= _menu_bar->update_locale();
        if (_rubber_band) changed |= _rubber_band->update_locale();
        return changed;
    }

    void Window::update_layout_min_size_force()
    {
        const amal::vec4 border = window_border_insets(resolved_window_style());
        f32 menu_height = 0.0f;
        if (auto *classic_menu = menu_bar();
            classic_menu && owns_classic_menu_bar_tree(*this) && classic_menu->is_visible())
        {
            classic_menu->set_layout_size({amal::max(size().x - border.x - border.z, 0.0f), 0.0f});
            classic_menu->update_layout_min_size();
            menu_height = classic_menu->required_size().y;
        }
        if (_content_block)
        {
            _content_block->update_layout_min_size();
        }
        const amal::vec2 children_min_size = _content_block ? _content_block->required_size() : amal::vec2{0.0f, 0.0f};

        const f32 header_height = get_window_header_height(_header, *this);
        set_required_size(
            amal::max(_min_size, amal::vec2{border.x + children_min_size.x + border.z,
                                            border.y + header_height + menu_height + children_min_size.y + border.w}));
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

        const amal::vec4 border = window_border_insets(resolved_window_style());
        const f32 inner_left = position().x + border.x;
        const f32 inner_width = amal::max(size().x - border.x - border.z, 0.0f);

        // A fit-content window is measured as header + menu + content, while arrange snaps
        // every boundary to the pixel grid. Account for that snapping here so the body never
        // ends up fractionally smaller than the content and spuriously enables a scrollbar.
        if (!parent() && !(window_flags & WindowFlagBits::docked) && _auto_size.y && is_size_fit(style_size().y))
        {
            auto *classic_menu = menu_bar();
            const bool layout_menu_bar =
                classic_menu && owns_classic_menu_bar_tree(*this) && classic_menu->is_visible();
            const f32 menu_height = layout_menu_bar ? classic_menu->required_size().y : 0.0f;
            const f32 header_height = get_window_header_height(_header, *this);
            const f32 content_height = _content_block ? _content_block->required_size().y : 0.0f;
            const f32 fit_height = border.y +
                                   resolve_fit_window_height(position().y + border.y, header_height, menu_height,
                                                             content_height, _header != nullptr, layout_menu_bar) +
                                   border.w;
            const amal::vec4 viewport = get_widget_viewport_rect(this);
            auto next_size = size();
            next_size.y =
                amal::clamp(amal::max(next_size.y, fit_height), amal::min(_min_size.y, viewport.w), viewport.w);
            set_layout_size(next_size);
        }

        if (detail::is_fast_layout_update())
        {
            Widget::update_layout(true);
            const amal::vec4 parent_bounds = get_window_clip_bounds(this);
            const amal::vec4 self_clip_rect =
                detail::intersect_rects({position().x, position().y, size().x, size().y}, parent_bounds);
            ensure_own_clip_rect(self_clip_rect);
            fill_window_resize_hit_rect(id(), bounds(), _resize_hit_depth, clip_id(), _resize_hit_rect);

            auto *classic_menu = menu_bar();
            const bool layout_menu_bar =
                classic_menu && owns_classic_menu_bar_tree(*this) && classic_menu->is_visible();
            const f32 menu_height = layout_menu_bar ? classic_menu->required_size().y : 0.0f;
            const amal::vec2 children_min_size =
                _content_block ? _content_block->required_size() : amal::vec2{0.0f, 0.0f};
            const f32 header_height = get_window_header_height(_header, *this);
            set_required_size(amal::max(
                _min_size, amal::vec2{border.x + children_min_size.x + border.z,
                                      border.y + header_height + menu_height + children_min_size.y + border.w}));

            const f32 header_top_y = position().y + border.y;
            const f32 header_bottom_y =
                (_header && !is_docked_window(*this)) ? snap_layout_start(header_top_y + header_height) : header_top_y;
            const f32 menu_top_y = header_bottom_y;
            const f32 menu_bottom_y = layout_menu_bar ? snap_layout_start(menu_top_y + menu_height) : menu_top_y;
            const f32 body_top_y = snap_layout_start(menu_bottom_y);
            const f32 body_bottom_y = snap_layout_end(position().y + size().y - border.w);
            const f32 body_height = amal::max(body_bottom_y - body_top_y, 0.0f);
            const f32 header_layout_height = amal::max(header_bottom_y - header_top_y, 0.0f);
            const f32 menu_layout_height = amal::max(menu_bottom_y - menu_top_y, 0.0f);

            if (_header && !is_docked_window(*this))
            {
                _header->set_clip_id(clip_id());
                _header->set_position({inner_left, header_top_y});
                _header->set_layout_size({inner_width, header_layout_height});
                _header->update_layout(true);
            }
            if (classic_menu)
            {
                classic_menu->set_popup_parent(classic_menu);
                classic_menu->set_clip_id(clip_id());
                classic_menu->set_position({inner_left, menu_top_y});
                classic_menu->set_layout_size(layout_menu_bar ? amal::vec2{inner_width, menu_layout_height}
                                                              : amal::vec2{0.0f, 0.0f});
                classic_menu->update_layout(true);
            }

            if (_content_block)
            {
                _content_block->set_clip_id(clip_id());
                _content_block->set_position({inner_left, body_top_y});
                _content_block->set_layout_size({inner_width, body_height});
                _content_block->update_layout(true);
            }

            const bool is_scrollbar_y_visible = _content_block && _content_block->has_visible_scrollbar_y();
            const bool is_scrollbar_x_visible = _content_block && _content_block->has_visible_scrollbar_x();
            const bool needs_scroll_events = is_scrollbar_y_visible || is_scrollbar_x_visible;
            const bool needs_hover_events = needs_scroll_events || ((window_flags & WindowFlagBits::resizable) &&
                                                                    !(window_flags & WindowFlagBits::docked));
            sync_window_event_flags(needs_scroll_events, needs_hover_events);

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
        if (auto *classic_menu = menu_bar();
            classic_menu && owns_classic_menu_bar_tree(*this) && classic_menu->is_visible())
        {
            const bool menu_width_changed = classic_menu->size().x != inner_width;
            classic_menu->set_layout_size({inner_width, 0.0f});
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
            amal::max(_min_size, amal::vec2{border.x + children_min_size.x + border.z,
                                            border.y + header_height + menu_height + children_min_size.y + border.w}));

        const f32 header_top_y = position().y + border.y;
        const f32 header_bottom_y =
            (_header && !is_docked_window(*this)) ? snap_layout_start(header_top_y + header_height) : header_top_y;
        const f32 menu_top_y = header_bottom_y;
        auto *classic_menu = menu_bar();
        const bool layout_menu_bar = classic_menu && owns_classic_menu_bar_tree(*this) && classic_menu->is_visible();
        const f32 menu_bottom_y = layout_menu_bar ? snap_layout_start(menu_top_y + menu_height) : menu_top_y;
        const f32 body_top_y = snap_layout_start(menu_bottom_y);
        const f32 body_bottom_y = snap_layout_end(position().y + size().y - border.w);
        const f32 body_height = amal::max(body_bottom_y - body_top_y, 0.0f);
        const f32 header_layout_height = amal::max(header_bottom_y - header_top_y, 0.0f);
        const f32 menu_layout_height = amal::max(menu_bottom_y - menu_top_y, 0.0f);

        if (_header && !is_docked_window(*this))
        {
            _header->set_clip_id(clip_id());
            _header->set_position({inner_left, header_top_y});
            _header->set_layout_size({inner_width, header_layout_height});
            _header->update_layout(true);
        }
        {
            if (classic_menu)
            {
                classic_menu->set_popup_parent(classic_menu);
                classic_menu->set_clip_id(clip_id());
                classic_menu->set_position({inner_left, menu_top_y});
                classic_menu->set_layout_size(layout_menu_bar ? amal::vec2{inner_width, menu_layout_height}
                                                              : amal::vec2{0.0f, 0.0f});
                classic_menu->update_layout(true);
            }
        }

        const bool was_scrollbar_y_visible = _content_block && _content_block->has_visible_scrollbar_y();
        const bool was_scrollbar_x_visible = _content_block && _content_block->has_visible_scrollbar_x();
        if (_content_block)
        {
            _content_block->set_clip_id(clip_id());
            _content_block->set_position({inner_left, body_top_y});
            _content_block->set_layout_size({inner_width, body_height});
            _content_block->update_layout(true);
        }

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
        if (auto *classic_menu = menu_bar()) classic_menu->translate(applied_delta);
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
        if ((window_flags & WindowFlagBits::closable) && ctx.hover_id == make_element_id(id(), AUIK_TAG_CLOSE_BUTTON))
        {
            add_render_command([window_id = id()] { remove_widget(window_id); });
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
            const bool was_moving = _move_drag_active;
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
            if ((was_moving || was_resizing) && (widget_flags & WidgetFlagBits::cache_global) &&
                !(window_flags & WindowFlagBits::docked))
            {
                const auto cache_state = detail::find_widget_global_cache(id());
                if (cache_state && cache_state->signature() == AUIK_TAG_WINDOW_STATE)
                    acul::static_pointer_cast<WindowStateData>(cache_state)->bounds = bounds();
            }
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
            _content_block && detail::is_scrollbar_tag(drag_id.tag_id) && drag_id.widget_id == get_rect().id.widget_id;
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
        if ((window_flags & WindowFlagBits::docked) || parent() || is_overlay_window(this)) return;

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

        const DepthZone depth_zone = self_window->get_depth_zone();
        u32 top_index = count;
        for (u32 i = count; i > 0u; --i)
        {
            auto *candidate = as_root_window(tree[i - 1u]);
            if (!candidate || candidate->get_depth_zone() != depth_zone) continue;
            top_index = i - 1u;
            break;
        }
        if (top_index >= count || index == top_index) return;

        Widget *top = tree[top_index];
        Window *top_window = as_root_window(top);

        tree[index] = top;
        tree[top_index] = self_window;
        raise_global_depth(self_window);

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
        void restore_window_content(Block *dst, Block *src)
        {
            if (!dst || !src) return;

            detail::WidgetCommonData common{};
            common.id = src->id();
            common.widget_flags = src->widget_flags;
            common.bounds = src->bounds();
            common.inline_size = src->inline_size();
            detail::apply_widget_common_data(dst, common);

            dst->set_size(src->explicit_size());

            dst->clear_children();
            const auto &layouts = src->child_layouts();
            dst->add_children(src->children, layouts);
            src->children.clear();
        }

        void write_window(acul::bin_stream &stream, umbf::Block *block)
        {
            auto *widget = static_cast<Window *>(block);
            detail::write_widget_common_data(stream, *widget);
            const auto title = widget->get_title();
            detail::write_localized_string(stream, acul::string(title.str ? title.str : ""), title.is_translated);
            stream.write(static_cast<u32>(widget->window_flags))
                .write(widget->window_style_tag())
                .write(widget->dock_style_tag())
                .write(static_cast<Widget *>(widget->content_block()));
            stream.write(widget->menu_bar() != nullptr);
            if (widget->menu_bar()) stream.write(static_cast<Widget *>(widget->menu_bar()));
        }

        umbf::Block *read_window(acul::bin_stream &stream)
        {
            const auto common = detail::read_widget_common_data(stream);
            const auto title = detail::read_localized_string(stream);
            u32 window_flags = 0u;
            u32 window_style_tag = AUIK_STYLE_TAG_WINDOW;
            u32 dock_style_tag = AUIK_STYLE_TAG_DOCKED_WINDOW;
            stream.read(window_flags).read(window_style_tag).read(dock_style_tag);

            Widget *content_block = nullptr;
            stream.read(content_block);

            auto *widget =
                acul::alloc<Window>(common.id, StringView{title.text.c_str(), title.translated}, common.bounds,
                                    WindowFlags(window_flags), WidgetFlags(common.widget_flags));
            widget->set_window_style_tag(window_style_tag);
            widget->set_dock_style_tag(dock_style_tag);
            if (content_block &&
                (content_block->signature() == AUIK_TAG_BLOCK || content_block->signature() == AUIK_TAG_DRAW_BLOCK))
            {
                auto *content = static_cast<Block *>(content_block);
                restore_window_content(widget->content_block(), content);
                acul::release(content);
            }
            else if (content_block) acul::release(content_block);
            detail::apply_widget_common_data(widget, common);
            bool present = false;
            stream.read(present);
            if (present)
            {
                Widget *menu = nullptr;
                stream.read(menu);
                if (auto *bar = dynamic_cast<MenuBar *>(menu)) widget->set_menu_bar(bar);
                else if (menu) acul::release(menu);
            }
            return widget;
        }

        void write_window_state(acul::bin_stream &stream, umbf::Block *block)
        {
            const auto *state = static_cast<WindowStateData *>(block);
            stream.write(state->widget_id).write(state->bounds);
        }

        umbf::Block *read_window_state(acul::bin_stream &stream)
        {
            auto *state = acul::alloc<WindowStateData>();
            stream.read(state->widget_id).read(state->bounds);
            return state;
        }
    } // namespace

    namespace streams
    {
        extern AUIK_EXPORT const umbf::registry::BlockStream window{read_window, write_window};
        extern AUIK_EXPORT const umbf::registry::BlockStream window_state{read_window_state, write_window_state};
    } // namespace streams

} // namespace auik
