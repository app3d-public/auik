#include <acul/memory/alloc.hpp>
#include <auik/v2/auik.hpp>
#include <auik/v2/detail/depth.hpp>
#include <auik/v2/pipelines.hpp>
#include <auik/v2/widgets/menubar.hpp>
#include <auik/v2/widgets/text.hpp>
#include <auik/v2/widgets/window.hpp>

namespace auik::v2
{
    static inline amal::vec4 intersect_rect(const amal::vec4 &a, const amal::vec4 &b)
    {
        const amal::vec2 a_min = {a.x, a.y};
        const amal::vec2 a_max = {a.x + a.z, a.y + a.w};
        const amal::vec2 b_min = {b.x, b.y};
        const amal::vec2 b_max = {b.x + b.z, b.y + b.w};

        const amal::vec2 out_min = {amal::max(a_min.x, b_min.x), amal::max(a_min.y, b_min.y)};
        const amal::vec2 out_max = {amal::min(a_max.x, b_max.x), amal::min(a_max.y, b_max.y)};
        const amal::vec2 out_size = {amal::max(out_max.x - out_min.x, 0.0f), amal::max(out_max.y - out_min.y, 0.0f)};
        return {out_min, out_size};
    }

    static amal::vec2 get_children_required_size(const acul::vector<Widget *> &children,
                                                 const acul::vector<WindowChildLayout> &layouts,
                                                 f32 inline_spacing_x, f32 wrap_width = 0.0f);
    static WindowChildLayout get_effective_child_layout(const acul::vector<Widget *> &children,
                                                        const acul::vector<WindowChildLayout> &layouts, size_t index);
    static bool starts_inline_run(const acul::vector<Widget *> &children,
                                  const acul::vector<WindowChildLayout> &layouts, size_t index);

    static constexpr detail::StylePropertyFlags AUIK_LAYOUT_STYLE_MASK =
        detail::StylePropertiesBits::padding | detail::StylePropertiesBits::margin |
        detail::StylePropertiesBits::text_size | detail::StylePropertiesBits::border_thickness |
        detail::StylePropertiesBits::border_radius | detail::StylePropertiesBits::inline_spacing;

    static inline bool has_layout_style_delta(const Style &normal, const Style &active)
    {
        const auto normal_mask = normal.mask() & AUIK_LAYOUT_STYLE_MASK;
        const auto active_mask = active.mask() & AUIK_LAYOUT_STYLE_MASK;
        return normal_mask != active_mask;
    }

    static inline bool needs_layout_on_active(Window &window)
    {
        auto *theme = get_theme();
        const Style &window_normal =
            theme->get_style(theme->get_resolved_style(AUIK_TAG_WINDOW, window.id(), 0, StyleState::normal));
        const Style &window_active =
            theme->get_style(theme->get_resolved_style(AUIK_TAG_WINDOW, window.id(), 0, StyleState::active));
        if (has_layout_style_delta(window_normal, window_active)) return true;

        if (!(window.window_flags & WindowFlagBits::decorated)) return false;
        const Style &header_normal = theme->get_style(
            theme->get_resolved_style(AUIK_TAG_WINDOW_HEADER, AUIK_TAG_WINDOW_HEADER, window.id(), StyleState::normal));
        const Style &header_active = theme->get_style(
            theme->get_resolved_style(AUIK_TAG_WINDOW_HEADER, AUIK_TAG_WINDOW_HEADER, window.id(), StyleState::active));
        return has_layout_style_delta(header_normal, header_active);
    }

    static inline Window *as_root_window(Widget *widget)
    {
        if (!widget) return nullptr;
        if (widget->parent()) return nullptr;
        if (widget->get_rect().tag_id != AUIK_TAG_WINDOW) return nullptr;
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

    static inline bool is_external_only_draw_update(const DrawCtx &ctx)
    {
        if (!ctx.is_updating()) return false;
        if (!(ctx.reason & DrawReasonBits::external)) return false;
        if (ctx.reason & (DrawReasonBits::layout | DrawReasonBits::full_redraw)) return false;
        return true;
    }

    static inline f32 snap_layout_start(f32 value)
    {
        return amal::ceil(value);
    }

    static inline f32 snap_layout_end(f32 value)
    {
        return amal::floor(value);
    }

    static inline amal::vec4 get_root_viewport_clip_rect(Widget *widget)
    {
        if (widget && widget->parent()) return widget->parent()->get_content_clip_rect();
        return get_main_viewport();
    }

    static inline amal::vec2 get_min_pos(const Widget *widget)
    {
        if (!widget || widget->parent() || widget->is_viewport_reserved()) return {0.0f, 0.0f};

        const amal::vec4 viewport = get_main_viewport();
        return {viewport.x, viewport.y};
    }

    static inline amal::vec2 clamp_root_widget_position(const Widget *widget, const amal::vec2 &local_pos,
                                                        const amal::vec2 &size)
    {
        if (!widget || widget->parent() || widget->is_viewport_reserved()) return local_pos;

        const amal::vec4 viewport = get_main_viewport();
        const f32 max_x = amal::max(viewport.z - size.x, 0.0f);
        const f32 max_y = amal::max(viewport.w - size.y, 0.0f);
        return {amal::clamp(local_pos.x, 0.0f, max_x), amal::clamp(local_pos.y, 0.0f, max_y)};
    }

    static inline amal::vec2 resolve_root_widget_position(Widget *widget, const amal::vec2 &size)
    {
        if (!widget || widget->parent()) return widget ? widget->position() : amal::vec2{0.0f, 0.0f};

        const amal::vec2 min_pos = get_min_pos(widget);
        amal::vec2 local_pos = widget->position() - widget->root_viewport_origin();
        if (widget->is_viewport_reserved())
        {
            widget->set_root_viewport_origin({0.0f, 0.0f});
            return local_pos;
        }

        local_pos = clamp_root_widget_position(widget, local_pos, size);
        widget->set_root_viewport_origin(min_pos);
        return min_pos + local_pos;
    }

    class WindowHeader final : public Widget
    {
    public:
        explicit WindowHeader(Widget *parent, acul::string text, bool hittable)
            : Widget(AUIK_TAG_WINDOW_HEADER,
                     WidgetFlagBits::visible | (hittable ? WidgetFlagBits::hittable : WidgetFlagBits::none),
                     EventFlagBits::none, parent, {}, AUIK_TAG_WINDOW_HEADER),
              _style({Theme::STYLE_ID_INVALID, AUIK_TAG_WINDOW_HEADER}),
              _title(acul::alloc<Text>(AUIK_TAG_WINDOW_HEADER ^ parent->id(), std::move(text), amal::vec2{0.0f, 0.0f},
                                       get_default_fixed_text_flags(), this))
        {
            assert(parent);
            set_depth_zone(DepthZone::foreground);
            _rect.widget_id = parent->id();
            _rect.clip_id = parent->clip_id();
            _title->set_horizontal_align(detail::TextHorizontalAlign::left);
            _title->set_vertical_align(detail::TextVerticalAlign::center);
        }
        ~WindowHeader() override { acul::release(_title); }

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
            const auto flags = resolve_style_selector(_style, id(), parent_id, style_state());
            const auto &style = get_theme()->get_style(_style.id);
            _title->update_style();
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
            _title->set_position(content_pos);
            _title->set_size(content_size);
            _title->update_layout(true);
            _title->set_clip_id(clip_id());
        }

        void translate(const amal::vec2 &delta) override
        {
            if (delta.x == 0.0f && delta.y == 0.0f) return;
            Widget::translate(delta);
            _title->translate(delta);
        }

        void update_depth(const amal::vec2 &depth_range) override
        {
            Widget::update_depth(depth_range);
            amal::vec2 text_range{};
            assign_next_depth(this->depth_range(), text_range);
            _title->update_depth(text_range);
        }

        void rebuild_clip_rects() override
        {
            _rect.clip_id = _parent->clip_id();
            _bg.hit_id = AUIK_INVALID_DRAW_DATA_ID;
            _title->set_clip_id(clip_id());
            _title->rebuild_clip_rects();
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
            if (should_emit_quads_instance(bg_visible, _bg, ctx.emit_hit_rect))
                ctx.emit(quads_stream, _bg, &data, get_rect(), ctx.emit_hit_rect);

            DrawCtx title_ctx = ctx;
            title_ctx.emit_hit_rect = _title->is_hittable();
            _title->draw(title_ctx);
            _title_draw_dirty = false;
        }

    private:
        DrawDataID _bg;
        StyleSelector _style;
        Text *_title = nullptr;
        u32 _resolved_text_color{0};
        bool _title_draw_dirty = true;
    };

    Window::Window(u32 id, acul::string title, const amal::rect &bounds, WindowFlags in_window_flags,
                   WidgetFlags in_widget_flags, Widget *parent)
        : Widget(id, in_widget_flags, EventFlagBits::click | EventFlagBits::drag | EventFlagBits::focus, parent, bounds,
                 AUIK_TAG_WINDOW),
          window_flags(in_window_flags)
    {
        widget_flags |= WidgetFlagBits::hittable;
        if (window_flags & WindowFlagBits::resizable) _rect.flags |= detail::RectBits::hitbox;
        if (window_flags & WindowFlagBits::decorated)
            _header = acul::alloc<WindowHeader>(this, std::move(title), window_flags & WindowFlagBits::movable);
    }

    Window::~Window()
    {
        clear_children();

        if (_menu_bar) acul::release(_menu_bar);
        if (_header) acul::release(_header);
        if (_scrollbar_x) acul::release(_scrollbar_x);
        if (_scrollbar_y) acul::release(_scrollbar_y);
    }

    void Window::clear_children()
    {
        for (auto *child : children)
        {
            if (!child) continue;
            if (child->widget_flags & WidgetFlagBits::attachable) child->on_detach();
            acul::release(child);
        }
        children.clear();
        _child_layouts.clear();
        _content_offset = {0.0f, 0.0f};
    }

    void Window::add_child(Widget *child, WindowChildLayout layout)
    {
        assert(child && "child is null");
        child->set_parent(this);
        child->set_focus_parent(this);
        child->update_style();
        children.push_back(child);
        _child_layouts.push_back(layout);
    }

    void Window::add_children(const acul::vector<Widget *> &new_children)
    {
        for (auto *child : new_children)
        {
            if (!child) continue;
            add_child(child, WindowChildLayout::block);
        }
    }

    void Window::set_menu_bar(MenuBar *menu_bar)
    {
        if (_menu_bar == menu_bar) return;
        if (_menu_bar)
        {
            if (_menu_bar->widget_flags & WidgetFlagBits::attachable) _menu_bar->on_detach();
            acul::release(_menu_bar);
        }
        _menu_bar = menu_bar;
        if (!_menu_bar) return;
        _menu_bar->set_parent(this);
        _menu_bar->set_focus_parent(this);
        _menu_bar->update_style();
        if (widget_flags & WidgetFlagBits::attachable) _menu_bar->on_attach();
    }

    void Window::override_content_clip_rect(const amal::vec4 &rect)
    {
        _content_clip_rect = rect;
        if (_content_clip_id == 0xFFFFu) _content_clip_id = push_clip_rect(_content_clip_rect);
        else update_clip_rect(_content_clip_id, _content_clip_rect);
    }

    void Window::on_attach()
    {
        auto &map = detail::get_context().id_map;
        map.emplace(id(), this);
        if (_menu_bar && (_menu_bar->widget_flags & WidgetFlagBits::attachable)) _menu_bar->on_attach();
        for (auto *child : children)
            if (child->widget_flags & WidgetFlagBits::attachable) child->on_attach();
    }

    void Window::on_detach()
    {
        auto &map = detail::get_context().id_map;
        map.erase(id());
        if (_menu_bar) map.erase(_menu_bar->id());
        for (auto *child : children) map.erase(child->id());
    }

    void Window::draw(DrawCtx &ctx)
    {
        auto *theme = get_theme();
        auto *quads_stream = get_primary_quads_stream();

        auto &window_style = theme->get_style(_window_style.id);
        QuadsInstanceData bg_data{};
        bg_data.rect = bounds();
        bg_data.z_order = get_z_order();
        const bool bg_visible = fill_quads_instance_by_style(window_style, clip_id(), bg_data);
        if (should_emit_quads_instance(bg_visible, _bg, ctx.emit_hit_rect))
            ctx.emit(quads_stream, _bg, &bg_data, get_rect(), ctx.emit_hit_rect);

        if (_header)
        {
            DrawCtx header_ctx = ctx;
            header_ctx.emit_hit_rect = _header->is_hittable();
            _header->draw(header_ctx);
        }
        if (_menu_bar && _menu_bar->is_visible())
        {
            DrawCtx menu_ctx = ctx;
            menu_ctx.emit_hit_rect = _menu_bar->is_hittable();
            _menu_bar->draw(menu_ctx);
        }
        const bool cull_external_children = is_external_only_draw_update(ctx);
        for (auto *child : children)
        {
            if (!child) continue;
            if (!child->is_visible()) continue;
            DrawCtx child_ctx = ctx;
            if (cull_external_children)
            {
                if (child->should_skip_external_draw_update(_content_clip_rect)) continue;
                if (child->is_external_draw_culled()) child_ctx.emit_fn = &emit_draw_invalidate;
            }
            else child->reset_external_draw_cull_state();
            child_ctx.emit_hit_rect = child->is_hittable();
            child->draw(child_ctx);
            if (child_ctx.is_invalidating()) child->mark_external_draw_invalidated();
        }

        if (_scrollbar_y && _scrollbar_y->is_visible())
        {
            DrawCtx scrollbar_ctx = ctx;
            scrollbar_ctx.emit_hit_rect = _scrollbar_y->is_hittable();
            _scrollbar_y->draw(scrollbar_ctx);
        }
        if (_scrollbar_x && _scrollbar_x->is_visible())
        {
            DrawCtx scrollbar_ctx = ctx;
            scrollbar_ctx.emit_hit_rect = _scrollbar_x->is_hittable();
            _scrollbar_x->draw(scrollbar_ctx);
        }
        if (_menu_bar && _menu_bar->is_visible())
        {
            DrawCtx menu_popup_ctx = ctx;
            _menu_bar->draw_popups(menu_popup_ctx);
        }
    }

    void Window::redraw_decorations(DrawReasonFlags reason)
    {
        DrawCtx ctx{};
        ctx.emit_fn = &emit_draw_update;
        ctx.emit_hit_rect = is_hittable();
        ctx.reason = reason;

        auto *theme = get_theme();
        auto *quads_stream = get_primary_quads_stream();
        auto &window_style = theme->get_style(_window_style.id);
        QuadsInstanceData bg_data{};
        bg_data.rect = bounds();
        bg_data.z_order = get_z_order();
        const bool bg_visible = fill_quads_instance_by_style(window_style, clip_id(), bg_data);
        if (should_emit_quads_instance(bg_visible, _bg, ctx.emit_hit_rect))
            ctx.emit(quads_stream, _bg, &bg_data, get_rect(), ctx.emit_hit_rect);

        if (_header)
        {
            DrawCtx header_ctx = ctx;
            header_ctx.emit_hit_rect = _header->is_hittable();
            _header->draw(header_ctx);
        }

        const auto transition = detail::get_widget_style_selector_transition(id());
        const bool scrollbar_transition = detail::is_scrollbar_tag(transition.current_id.tag_id) ||
                                          detail::is_scrollbar_tag(transition.prev_id.tag_id);
        if (scrollbar_transition)
        {
            if (_scrollbar_y && _scrollbar_y->is_visible())
            {
                DrawCtx scrollbar_ctx = ctx;
                scrollbar_ctx.emit_hit_rect = _scrollbar_y->is_hittable();
                _scrollbar_y->draw(scrollbar_ctx);
            }
            if (_scrollbar_x && _scrollbar_x->is_visible())
            {
                DrawCtx scrollbar_ctx = ctx;
                scrollbar_ctx.emit_hit_rect = _scrollbar_x->is_hittable();
                _scrollbar_x->draw(scrollbar_ctx);
            }
        }
    }

    bool Window::accepts_focus_on_mouse_press(detail::ElementID hit_id) const
    {
        if (hit_id.tag_id == AUIK_TAG_WINDOW_HEADER) return true;
        if (hit_id.tag_id == AUIK_TAG_HITBOX)
            return !(window_flags & WindowFlagBits::resizable) || (window_flags & WindowFlagBits::docked);
        if (detail::is_scrollbar_tag(hit_id.tag_id)) return false;
        return true;
    }

    void Window::update_depth(const amal::vec2 &depth_range)
    {
        Widget::update_depth(depth_range);

        const bool can_scroll_y =
            (window_flags & WindowFlagBits::scrollable) && !(window_flags & WindowFlagBits::no_scrollbar_y);
        const bool can_scroll_x =
            (window_flags & WindowFlagBits::scrollable) && !(window_flags & WindowFlagBits::no_scrollbar_x);
        if (can_scroll_y) ensure_y_scrollbar(_scrollbar_y, this);
        if (can_scroll_x) ensure_x_scrollbar(_scrollbar_x, this);

        // Keep content strictly in work zone of the window range.
        amal::vec2 next_child_range{};
        assign_next_depth(detail::get_depth_workzone_range(this->depth_range()), next_child_range);

        for (auto *child : children)
        {
            if (!child) continue;
            child->update_depth(next_child_range);
        }

        if (_menu_bar) _menu_bar->update_depth(next_child_range);
        if (_header) _header->update_depth(next_child_range);

        // Reserve a dedicated top lane for local overlays (scrollbars) to avoid
        // overlap with content depth and reduce z-fighting with other overlay items.
        amal::vec2 overlay_range{};
        assign_next_depth(this->depth_range(), overlay_range);
        if (_scrollbar_y && _scrollbar_x)
        {
            _scrollbar_y->update_depth(overlay_range);
            amal::vec2 overlay_next{};
            assign_next_depth(overlay_range, overlay_next);
            _scrollbar_x->update_depth(overlay_next);
        }
        else
        {
            if (_scrollbar_y) _scrollbar_y->update_depth(overlay_range);
            if (_scrollbar_x) _scrollbar_x->update_depth(overlay_range);
        }
    }

    StyleUpdateFlags Window::update_style()
    {
        StyleUpdateFlags out = resolve_style_selector(_window_style, id(), 0, style_state());

        if ((window_flags & WindowFlagBits::decorated) && !_header)
            _header = acul::alloc<WindowHeader>(this, "", window_flags & WindowFlagBits::movable);
        if (window_flags & WindowFlagBits::decorated)
        {
            if (window_flags & WindowFlagBits::movable) _header->widget_flags |= WidgetFlagBits::hittable;
            else _header->widget_flags &= ~WidgetFlagBits::hittable;
            out |= _header->update_style();
            _header_height = static_cast<WindowHeader *>(_header)->compute_height();
        }
        else
        {
            if (_header)
            {
                acul::release(_header);
                _header = nullptr;
            }
            _header_height = 0.0f;
        }

        const bool can_scroll_y =
            (window_flags & WindowFlagBits::scrollable) && !(window_flags & WindowFlagBits::no_scrollbar_y);
        const bool can_scroll_x =
            (window_flags & WindowFlagBits::scrollable) && !(window_flags & WindowFlagBits::no_scrollbar_x);
        if (can_scroll_y) ensure_y_scrollbar(_scrollbar_y, this);
        if (can_scroll_x) ensure_x_scrollbar(_scrollbar_x, this);

        if (_scrollbar_y) out |= _scrollbar_y->update_style();
        if (_scrollbar_x) out |= _scrollbar_x->update_style();
        return out;
    }

    void Window::rebuild_clip_rects()
    {
        _rect.clip_id = 0xFFFFu;
        _content_clip_id = 0xFFFFu;
        amal::vec4 self_clip = {position().x, position().y, size().x, size().y};
        if (parent()) self_clip = intersect_rect(self_clip, parent()->get_content_clip_rect());
        ensure_own_clip_rect(self_clip);
        if (_content_clip_id == 0xFFFFu) _content_clip_id = push_clip_rect(_content_clip_rect);
        else update_clip_rect(_content_clip_id, _content_clip_rect);
        _bg.hit_id = AUIK_INVALID_DRAW_DATA_ID;

        if (_header)
        {
            _header->rebuild_clip_rects();
            _header->set_clip_id(clip_id());
        }

        if (_menu_bar)
        {
            _menu_bar->set_clip_id(clip_id());
            _menu_bar->rebuild_clip_rects();
        }

        for (auto *child : children)
        {
            if (!child) continue;
            child->rebuild_clip_rects();
        }

        if (_scrollbar_y)
        {
            _scrollbar_y->set_clip_id(clip_id());
            _scrollbar_y->rebuild_clip_rects();
        }

        if (_scrollbar_x)
        {
            _scrollbar_x->set_clip_id(clip_id());
            _scrollbar_x->rebuild_clip_rects();
        }
    }

    void Window::update_layout_min_size()
    {
        auto *theme = get_theme();
        const auto &window_style = theme->get_style(_window_style.id);
        const auto &padding = window_style.padding();

        f32 menu_height = 0.0f;
        if (_menu_bar && _menu_bar->is_visible())
        {
            _menu_bar->set_size({size().x, 0.0f});
            _menu_bar->update_layout_min_size();
            menu_height = _menu_bar->required_size().y;
        }
        for (auto *child : children)
        {
            if (!child) continue;
            if (!child->is_visible()) continue;
            child->update_layout_min_size();
        }
        const f32 inline_spacing_x = amal::max(window_style.inline_spacing(), 0.0f);
        const amal::vec2 children_min_size = get_children_required_size(children, _child_layouts, inline_spacing_x);

        set_required_size({padding.x + padding.z + children_min_size.x,
                           padding.y + padding.w + children_min_size.y + _header_height + menu_height});
    }

    static amal::vec2 get_children_required_size(const acul::vector<Widget *> &children,
                                                 const acul::vector<WindowChildLayout> &layouts,
                                                 f32 inline_spacing_x, f32 wrap_width)
    {
        f32 max_width = 0.0f;
        f32 total_height = 0.0f;
        f32 row_width = 0.0f;
        f32 row_height = 0.0f;
        const bool wrap_enabled = wrap_width > 0.0f;
        for (size_t i = 0; i < children.size(); ++i)
        {
            auto *child = children[i];
            if (!child) continue;
            if (!child->is_visible()) continue;
            const WindowChildLayout layout = get_effective_child_layout(children, layouts, i);
            const amal::vec2 req = child->required_size();
            if (layout == WindowChildLayout::inline_layout)
            {
                if (starts_inline_run(children, layouts, i) && row_height > 0.0f)
                {
                    max_width = amal::max(max_width, row_width);
                    total_height += row_height;
                    row_width = 0.0f;
                    row_height = 0.0f;
                }
                const f32 gap_before = row_height > 0.0f ? inline_spacing_x : 0.0f;
                if (wrap_enabled && row_height > 0.0f && row_width + gap_before + req.x > wrap_width)
                {
                    max_width = amal::max(max_width, row_width);
                    total_height += row_height;
                    row_width = 0.0f;
                    row_height = 0.0f;
                }
                if (wrap_enabled && req.x > wrap_width)
                {
                    if (row_height > 0.0f)
                    {
                        max_width = amal::max(max_width, row_width);
                        total_height += row_height;
                        row_width = 0.0f;
                        row_height = 0.0f;
                    }
                    max_width = amal::max(max_width, req.x);
                    total_height += req.y;
                    continue;
                }
                row_width += gap_before + req.x;
                row_height = amal::max(row_height, req.y);
            }
            else
            {
                if (row_height > 0.0f)
                {
                    max_width = amal::max(max_width, row_width);
                    total_height += row_height;
                    row_width = 0.0f;
                    row_height = 0.0f;
                }
                max_width = amal::max(max_width, req.x);
                total_height += req.y;
            }
        }
        if (row_height > 0.0f)
        {
            max_width = amal::max(max_width, row_width);
            total_height += row_height;
        }
        return amal::vec2{max_width, total_height};
    };

    static WindowChildLayout get_effective_child_layout(const acul::vector<Widget *> &children,
                                                        const acul::vector<WindowChildLayout> &layouts, size_t index)
    {
        const WindowChildLayout current = index < layouts.size() ? layouts[index] : WindowChildLayout::block;
        if (current == WindowChildLayout::inline_layout) return current;

        for (size_t next = index + 1; next < children.size(); ++next)
        {
            if (!children[next]) continue;
            if (!children[next]->is_visible()) continue;
            const WindowChildLayout next_layout = next < layouts.size() ? layouts[next] : WindowChildLayout::block;
            if (next_layout == WindowChildLayout::inline_layout) return WindowChildLayout::inline_layout;
            break;
        }
        return WindowChildLayout::block;
    }

    static bool starts_inline_run(const acul::vector<Widget *> &children,
                                  const acul::vector<WindowChildLayout> &layouts, size_t index)
    {
        const WindowChildLayout current = index < layouts.size() ? layouts[index] : WindowChildLayout::block;
        return current != WindowChildLayout::inline_layout &&
               get_effective_child_layout(children, layouts, index) == WindowChildLayout::inline_layout;
    }

    static void align_inline_row_vertical(const acul::vector<Widget *> &children, size_t row_start, size_t row_end,
                                          f32 row_height)
    {
        for (size_t row_i = row_start; row_i < row_end; ++row_i)
        {
            auto *row_child = children[row_i];
            if (!row_child) continue;
            if (!row_child->is_visible()) continue;
            const f32 child_h = amal::max(row_child->required_size().y, 0.0f);
            const f32 delta_y = amal::floor((row_height - child_h) * 0.5f);
            if (delta_y <= 0.0f) continue;
            row_child->translate({0.0f, delta_y});
        }
    }

    void Window::relayout_children(f32 available_width, const amal::vec2 &content_inset)
    {
        const f32 inline_spacing_x =
            amal::max(get_theme()->get_style(_window_style.id).inline_spacing(), 0.0f);
        const amal::vec2 content_cursor = position() + content_inset - _content_offset;
        amal::vec2 cursor = content_cursor;
        f32 inline_row_height = 0.0f;
        f32 inline_row_width = 0.0f;
        size_t inline_row_start = 0;
        bool inline_row_active = false;

        for (size_t i = 0; i < children.size(); ++i)
        {
            auto *child = children[i];
            if (!child) continue;
            if (!child->is_visible()) continue;
            const WindowChildLayout layout = get_effective_child_layout(children, _child_layouts, i);
            if (layout == WindowChildLayout::block)
            {
                if (inline_row_height > 0.0f)
                {
                    align_inline_row_vertical(children, inline_row_start, i, inline_row_height);
                    cursor = {content_cursor.x, cursor.y + inline_row_height};
                    inline_row_height = 0.0f;
                    inline_row_width = 0.0f;
                    inline_row_active = false;
                }

                if (!child->is_fixed()) { child->set_size({available_width, child->size().y}); }
                child->set_position(cursor);
                child->update_layout(true);
                cursor = {content_cursor.x, cursor.y + child->required_size().y};
                continue;
            }

            if (starts_inline_run(children, _child_layouts, i) && inline_row_height > 0.0f)
            {
                align_inline_row_vertical(children, inline_row_start, i, inline_row_height);
                cursor = {content_cursor.x, cursor.y + inline_row_height};
                inline_row_height = 0.0f;
                inline_row_width = 0.0f;
                inline_row_active = false;
            }

            const amal::vec2 req = child->required_size();
            const f32 gap_before = inline_row_active ? inline_spacing_x : 0.0f;
            f32 inline_width = amal::max(req.x, 0.0f);
            const bool has_inline_row = inline_row_height > 0.0f;
            const bool needs_wrap = has_inline_row && inline_row_width + gap_before + inline_width > available_width;
            if (needs_wrap)
            {
                align_inline_row_vertical(children, inline_row_start, i, inline_row_height);
                cursor = {content_cursor.x, cursor.y + inline_row_height};
                inline_row_height = 0.0f;
                inline_row_width = 0.0f;
                inline_row_active = false;
            }

            // If inline item does not fit even on a fresh row, layout it as a block item.
            if (inline_width > available_width)
            {
                if (inline_row_height > 0.0f)
                {
                    align_inline_row_vertical(children, inline_row_start, i, inline_row_height);
                    cursor = {content_cursor.x, cursor.y + inline_row_height};
                    inline_row_height = 0.0f;
                    inline_row_width = 0.0f;
                    inline_row_active = false;
                }
                if (!child->is_fixed()) { child->set_size({available_width, child->size().y}); }
                child->set_position(cursor);
                child->update_layout(true);
                cursor = {content_cursor.x, cursor.y + child->required_size().y};
                continue;
            }

            if (!child->is_fixed()) child->set_size({inline_width, child->size().y});
            if (!inline_row_active)
            {
                inline_row_start = i;
                inline_row_active = true;
            }
            else
            {
                cursor.x += inline_spacing_x;
                inline_row_width += inline_spacing_x;
            }
            child->set_position(cursor);
            child->update_layout(true);

            const amal::vec2 occupied = child->required_size();
            const f32 occupied_w = amal::max(occupied.x, 0.0f);
            const f32 occupied_h = amal::max(occupied.y, 0.0f);
            inline_row_height = amal::max(inline_row_height, occupied_h);
            inline_row_width += occupied_w;
            cursor = {content_cursor.x + inline_row_width, cursor.y};
        }

        if (inline_row_height > 0.0f)
        {
            align_inline_row_vertical(children, inline_row_start, children.size(), inline_row_height);
            cursor = {content_cursor.x, cursor.y + inline_row_height};
        }
    };

    void Window::update_layout(bool min_size_known)
    {
        if (!min_size_known) update_layout_min_size();
        if (!parent() && !(window_flags & WindowFlagBits::docked))
            set_position(resolve_root_widget_position(this, size()));
        Widget::update_layout(true);
        const amal::vec4 parent_bounds = get_root_viewport_clip_rect(this);
        const amal::vec4 self_clip_rect =
            intersect_rect({position().x, position().y, size().x, size().y}, parent_bounds);
        ensure_own_clip_rect(self_clip_rect);

        auto *theme = get_theme();
        const auto &window_style = theme->get_style(_window_style.id);
        const auto &padding = window_style.padding();
        _content_offset = amal::max(_content_offset, 0.0f);

        f32 menu_height = 0.0f;
        if (_menu_bar && _menu_bar->is_visible())
        {
            _menu_bar->set_size({size().x, 0.0f});
            _menu_bar->update_layout_min_size();
            menu_height = _menu_bar->required_size().y;
        }

        for (auto *child : children)
        {
            if (!child) continue;
            if (!child->is_visible()) continue;
            child->update_layout_min_size();
        }
        const f32 inline_spacing_x = amal::max(window_style.inline_spacing(), 0.0f);
        const amal::vec2 children_min_size = get_children_required_size(children, _child_layouts, inline_spacing_x);
        const f32 required_height = padding.y + padding.w + children_min_size.y + _header_height + menu_height;
        const f32 required_width = padding.x + padding.z + children_min_size.x;
        set_required_size({required_width, required_height});

        const bool can_scroll_y =
            (window_flags & WindowFlagBits::scrollable) && !(window_flags & WindowFlagBits::no_scrollbar_y);
        const bool can_scroll_x =
            (window_flags & WindowFlagBits::scrollable) && !(window_flags & WindowFlagBits::no_scrollbar_x);
        if (can_scroll_y) ensure_y_scrollbar(_scrollbar_y, this);
        if (can_scroll_x) ensure_x_scrollbar(_scrollbar_x, this);

        const f32 body_width = amal::max(size().x - padding.x - padding.z, 0.0f);
        const f32 header_top_y = position().y;
        const f32 header_bottom_y = _header ? snap_layout_start(header_top_y + _header_height) : header_top_y;
        const f32 menu_top_y = header_bottom_y;
        const f32 menu_bottom_y = (_menu_bar && _menu_bar->is_visible()) ? snap_layout_start(menu_top_y + menu_height)
                                                                         : menu_top_y;
        const f32 body_top_y = snap_layout_start(menu_bottom_y + padding.y);
        const f32 body_bottom_y = snap_layout_end(position().y + size().y - padding.w);
        const f32 scrollbar_top_y = menu_bottom_y;
        const f32 scrollbar_bottom_y = snap_layout_end(position().y + size().y);
        const f32 header_layout_height = amal::max(header_bottom_y - header_top_y, 0.0f);
        const f32 menu_layout_height = amal::max(menu_bottom_y - menu_top_y, 0.0f);
        const f32 body_height = amal::max(body_bottom_y - body_top_y, 0.0f);
        const f32 scrollbar_area_height = amal::max(scrollbar_bottom_y - scrollbar_top_y, 0.0f);
        const f32 bar_w = _scrollbar_y ? _scrollbar_y->get_min_track_thickness() : 0.0f;
        const f32 bar_h = _scrollbar_x ? _scrollbar_x->get_min_track_thickness() : 0.0f;

        amal::vec2 children_layout_size = children_min_size;
        bool need_scroll_y = can_scroll_y && children_layout_size.y > body_height;
        bool need_scroll_x = can_scroll_x && children_layout_size.x > body_width;
        for (int i = 0; i < 2; ++i)
        {
            const f32 viewport_w = amal::max(body_width - (need_scroll_y ? bar_w : 0.0f), 0.0f);
            const f32 viewport_h = amal::max(body_height - (need_scroll_x ? bar_h : 0.0f), 0.0f);
            children_layout_size = get_children_required_size(children, _child_layouts, inline_spacing_x, viewport_w);
            const bool next_y = can_scroll_y && children_layout_size.y > viewport_h;
            const bool next_x = can_scroll_x && children_layout_size.x > viewport_w;
            if (next_y == need_scroll_y && next_x == need_scroll_x) break;
            need_scroll_y = next_y;
            need_scroll_x = next_x;
        }

        f32 scroll_view_width = amal::max(body_width - (need_scroll_y ? bar_w : 0.0f), 0.0f);
        f32 scroll_view_height = amal::max(body_height - (need_scroll_x ? bar_h : 0.0f), 0.0f);
        // Policy: allow horizontal overlay across the whole window width,
        // while keeping strict vertical clipping for scrolling.
        const f32 clip_viewport_width = size().x;
        const f32 content_layout_width = scroll_view_width;
        const amal::vec2 content_inset = {padding.x, body_top_y - position().y};
        amal::vec2 content_size = {clip_viewport_width, scroll_view_height};
        const amal::vec4 content_clip = {position().x, body_top_y, content_size.x, content_size.y};
        const amal::vec4 parent_clip = get_clip_rect(clip_id());
        _content_clip_rect = intersect_rect(parent_clip, content_clip);
        if (_content_clip_id == 0xFFFFu) _content_clip_id = push_clip_rect(_content_clip_rect);
        else update_clip_rect(_content_clip_id, _content_clip_rect);
        if (_scrollbar_x) _scrollbar_x->set_metrics(children_layout_size.x, scroll_view_width);
        if (_scrollbar_y) _scrollbar_y->set_metrics(children_layout_size.y, scroll_view_height);
        const amal::vec2 max_scroll = {_scrollbar_x ? _scrollbar_x->max_scroll() : 0.0f,
                                       _scrollbar_y ? _scrollbar_y->max_scroll() : 0.0f};
        _content_offset = amal::clamp(_content_offset, amal::vec2{0.0f}, max_scroll);
        const amal::vec2 pre_layout_children_size = children_layout_size;
        relayout_children(content_layout_width, content_inset);

        // Adaptive children can refine required_size() only after width allocation.
        // Recompute the wrapped content size once from the actual laid out widgets so
        // scrollbars and row wrapping decisions stabilize in the same frame.
        const amal::vec2 laid_out_children_size =
            get_children_required_size(children, _child_layouts, inline_spacing_x, content_layout_width);
        if (laid_out_children_size != pre_layout_children_size)
        {
            children_layout_size = laid_out_children_size;

            bool refined_need_scroll_y = can_scroll_y && children_layout_size.y > body_height;
            bool refined_need_scroll_x = can_scroll_x && children_layout_size.x > body_width;
            for (int i = 0; i < 2; ++i)
            {
                const f32 refined_viewport_w = amal::max(body_width - (refined_need_scroll_y ? bar_w : 0.0f), 0.0f);
                const f32 refined_viewport_h = amal::max(body_height - (refined_need_scroll_x ? bar_h : 0.0f), 0.0f);

                relayout_children(refined_viewport_w, content_inset);
                children_layout_size =
                    get_children_required_size(children, _child_layouts, inline_spacing_x, refined_viewport_w);
                const bool next_refined_y = can_scroll_y && children_layout_size.y > refined_viewport_h;
                const bool next_refined_x = can_scroll_x && children_layout_size.x > refined_viewport_w;
                if (next_refined_y == refined_need_scroll_y && next_refined_x == refined_need_scroll_x) break;
                refined_need_scroll_y = next_refined_y;
                refined_need_scroll_x = next_refined_x;
            }

            if (refined_need_scroll_y != need_scroll_y || refined_need_scroll_x != need_scroll_x)
            {
                need_scroll_y = refined_need_scroll_y;
                need_scroll_x = refined_need_scroll_x;
                scroll_view_width = amal::max(body_width - (need_scroll_y ? bar_w : 0.0f), 0.0f);
                scroll_view_height = amal::max(body_height - (need_scroll_x ? bar_h : 0.0f), 0.0f);
                content_size = {clip_viewport_width, scroll_view_height};
                const amal::vec4 refined_content_clip = {position().x, body_top_y, content_size.x, content_size.y};
                _content_clip_rect = intersect_rect(parent_clip, refined_content_clip);
                update_clip_rect(_content_clip_id, _content_clip_rect);
                relayout_children(scroll_view_width, content_inset);
                children_layout_size =
                    get_children_required_size(children, _child_layouts, inline_spacing_x, scroll_view_width);
                if (_scrollbar_x) _scrollbar_x->set_metrics(children_layout_size.x, scroll_view_width);
                if (_scrollbar_y) _scrollbar_y->set_metrics(children_layout_size.y, scroll_view_height);
                const amal::vec2 refined_max_scroll = {_scrollbar_x ? _scrollbar_x->max_scroll() : 0.0f,
                                                       _scrollbar_y ? _scrollbar_y->max_scroll() : 0.0f};
                _content_offset = amal::clamp(_content_offset, amal::vec2{0.0f}, refined_max_scroll);
                relayout_children(scroll_view_width, content_inset);
            }
            else relayout_children(content_layout_width, content_inset);
        }

        if (_header)
        {
            _header->set_position(position());
            _header->set_size({size().x, header_layout_height});
            _header->set_clip_id(clip_id());
            _header->update_layout(true);
        }
        if (_menu_bar)
        {
            _menu_bar->set_position({position().x, menu_top_y});
            _menu_bar->set_size({size().x, menu_layout_height});
            _menu_bar->set_clip_id(clip_id());
            _menu_bar->update_layout(true);
        }

        const bool was_scrollbar_y_visible = _scrollbar_y && _scrollbar_y->is_visible();
        const bool was_scrollbar_x_visible = _scrollbar_x && _scrollbar_x->is_visible();

        if (need_scroll_y && _scrollbar_y)
        {
            const amal::vec4 track_margin = _scrollbar_y->get_track_margin();
            const f32 track_w = _scrollbar_y->get_min_track_thickness();
            const amal::vec2 body_pos = {position().x, scrollbar_top_y};
            const amal::vec2 body_size = {size().x, scrollbar_area_height};
            const amal::vec2 usable_size = {body_size.x, amal::max(body_size.y - (need_scroll_x ? bar_h : 0.0f), 0.0f)};

            const amal::vec2 track_area_pos = {body_pos.x + track_margin.x, body_pos.y + track_margin.y};
            const amal::vec2 track_area_size = {amal::max(usable_size.x - track_margin.x - track_margin.z, 0.0f),
                                                amal::max(usable_size.y - track_margin.y - track_margin.w, 0.0f)};
            const amal::vec2 track_pos = {track_area_pos.x + amal::max(track_area_size.x - track_w, 0.0f),
                                          track_area_pos.y};
            const amal::vec2 track_size = {track_w, track_area_size.y};
            _scrollbar_y->set_visible(true);
            _scrollbar_y->set_scroll_offset(_content_offset.y);
            _scrollbar_y->configure(track_pos, track_size, children_layout_size.y, scroll_view_height);
            _content_offset.y = _scrollbar_y->scroll_offset();
            _scrollbar_y->set_clip_id(clip_id());
        }
        else if (_scrollbar_y) _scrollbar_y->set_visible(false);

        if (need_scroll_x && _scrollbar_x)
        {
            const amal::vec4 track_margin = _scrollbar_x->get_track_margin();
            const f32 track_h = _scrollbar_x->get_min_track_thickness();
            const amal::vec2 body_pos = {position().x, scrollbar_top_y};
            const amal::vec2 body_size = {size().x, scrollbar_area_height};
            const amal::vec2 usable_size = {amal::max(body_size.x - (need_scroll_y ? bar_w : 0.0f), 0.0f), body_size.y};

            const amal::vec2 track_area_pos = {body_pos.x + track_margin.x, body_pos.y + track_margin.y};
            const amal::vec2 track_area_size = {amal::max(usable_size.x - track_margin.x - track_margin.z, 0.0f),
                                                amal::max(usable_size.y - track_margin.y - track_margin.w, 0.0f)};
            const amal::vec2 track_pos = {track_area_pos.x,
                                          track_area_pos.y + amal::max(track_area_size.y - track_h, 0.0f)};
            const amal::vec2 track_size = {track_area_size.x, track_h};
            _scrollbar_x->set_visible(true);
            _scrollbar_x->set_scroll_offset(_content_offset.x);
            _scrollbar_x->configure(track_pos, track_size, children_layout_size.x, scroll_view_width);
            _content_offset.x = _scrollbar_x->scroll_offset();
            _scrollbar_x->set_clip_id(clip_id());
        }
        else if (_scrollbar_x) _scrollbar_x->set_visible(false);

        const bool is_scrollbar_y_visible = _scrollbar_y && _scrollbar_y->is_visible();
        const bool is_scrollbar_x_visible = _scrollbar_x && _scrollbar_x->is_visible();
        const bool needs_scroll_events = is_scrollbar_y_visible || is_scrollbar_x_visible;
        const bool needs_hover_events = needs_scroll_events || ((window_flags & WindowFlagBits::resizable) &&
                                                                !(window_flags & WindowFlagBits::docked));
        if (needs_scroll_events) add_event_flags(EventFlagBits::scroll);
        else remove_event_flags(EventFlagBits::scroll);
        if (needs_hover_events) add_event_flags(EventFlagBits::hover);
        else remove_event_flags(EventFlagBits::hover);
        if (was_scrollbar_y_visible != is_scrollbar_y_visible || was_scrollbar_x_visible != is_scrollbar_x_visible)
            redraw_all_commands();

        // Child content must be clipped to viewport area otherwise children can render under the header while
        // scrolling.
        const amal::vec4 final_content_clip = {position().x, body_top_y, content_size.x, content_size.y};
        const amal::vec4 next_content_clip_rect = intersect_rect(parent_clip, final_content_clip);
        if (next_content_clip_rect != _content_clip_rect)
        {
            _content_clip_rect = next_content_clip_rect;
            update_clip_rect(_content_clip_id, _content_clip_rect);
            relayout_children(content_layout_width, content_inset);
        }
        else
        {
            _content_clip_rect = next_content_clip_rect;
            update_clip_rect(_content_clip_id, _content_clip_rect);
        }
    }

    void Window::translate(const amal::vec2 &delta)
    {
        if (delta.x == 0.0f && delta.y == 0.0f) return;

        amal::vec2 next_position = position() + delta;
        if (!parent() && !(window_flags & WindowFlagBits::docked))
        {
            const amal::vec2 local_pos = position() - root_viewport_origin() + delta;
            next_position = get_min_pos(this) + clamp_root_widget_position(this, local_pos, size());
        }
        const amal::vec2 applied_delta = next_position - position();
        if (applied_delta.x == 0.0f && applied_delta.y == 0.0f) return;

        Widget::translate(applied_delta);
        auto &ctx = detail::get_context();
        ctx.dirty_flags |= DirtyFlagBits::hit_rect_update;

        const amal::vec4 parent_bounds = get_root_viewport_clip_rect(this);
        const amal::vec4 self_clip_rect =
            intersect_rect({position().x, position().y, size().x, size().y}, parent_bounds);
        if (clip_id() != 0xFFFFu) update_clip_rect(clip_id(), self_clip_rect);
        if (_content_clip_id != 0xFFFFu)
        {
            auto *theme = get_theme();
            const auto &window_style = theme->get_style(_window_style.id);
            const amal::vec4 padding = window_style.padding();
            const f32 menu_height = (_menu_bar && _menu_bar->is_visible()) ? _menu_bar->required_size().y : 0.0f;
            const f32 header_top_y = position().y;
            const f32 header_bottom_y = _header ? snap_layout_start(header_top_y + _header_height) : header_top_y;
            const f32 menu_top_y = header_bottom_y;
            const f32 menu_bottom_y = (_menu_bar && _menu_bar->is_visible()) ? snap_layout_start(menu_top_y + menu_height)
                                                                             : menu_top_y;
            const f32 body_top_y = snap_layout_start(menu_bottom_y + padding.y);
            const f32 body_bottom_y = snap_layout_end(position().y + size().y - padding.w);
            const f32 body_height = amal::max(body_bottom_y - body_top_y, 0.0f);
            const f32 bar_h =
                (_scrollbar_x && _scrollbar_x->is_visible()) ? _scrollbar_x->get_min_track_thickness() : 0.0f;
            const amal::vec4 content_clip = {position().x, body_top_y, size().x, amal::max(body_height - bar_h, 0.0f)};
            _content_clip_rect = intersect_rect(self_clip_rect, content_clip);
            update_clip_rect(_content_clip_id, _content_clip_rect);
        }

        if (_header) _header->translate(applied_delta);
        if (_menu_bar) _menu_bar->translate(applied_delta);
        for (auto *child : children)
        {
            if (!child) continue;
            child->translate(applied_delta);
        }
        if (_scrollbar_y) _scrollbar_y->translate(applied_delta);
        if (_scrollbar_x) _scrollbar_x->translate(applied_delta);
    }

    void Window::on_scroll(const amal::vec2 &delta)
    {
        if (!_scrollbar_x && !_scrollbar_y)
        {
            Widget::on_scroll(delta);
            return;
        }

        const amal::vec2 step = -delta * f32(AUIK_SCROLL_STEP);
        const amal::vec2 old_offset = _content_offset;

        if (_scrollbar_y && _scrollbar_y->is_visible())
        {
            _scrollbar_y->set_scroll_offset(_content_offset.y);
            _scrollbar_y->scroll_by_pixels(step.y);
            _content_offset.y = _scrollbar_y->scroll_offset();
        }
        if (_scrollbar_x && _scrollbar_x->is_visible())
        {
            _scrollbar_x->set_scroll_offset(_content_offset.x);
            _scrollbar_x->scroll_by_pixels(step.x);
            _content_offset.x = _scrollbar_x->scroll_offset();
        }
        if (_content_offset != old_offset)
        {
            add_render_command<detail::ScrollEventTraits>(this, [this]() {
                update_layout(true);
                update_draw_commands(DrawReasonBits::layout);
                detail::get_context().dirty_flags |= DirtyFlagBits::redraw;
            });
        }
    }

    void Window::on_hover(HoverState state)
    {
        auto &ctx = detail::get_context();
        const bool is_own_hitbox = ctx.hover_id.widget_id == id() && ctx.hover_id.tag_id == AUIK_TAG_HITBOX;
        const bool can_resize = (window_flags & WindowFlagBits::resizable) && !(window_flags & WindowFlagBits::docked);
        if (state != HoverState::leave && is_own_hitbox && can_resize)
        {
            ctx.hover_hitbox_zone = detail::get_hitbox_zone(get_rect(), ctx.io.mouse_pos);
            detail::set_window_cursor(detail::get_cursor_for_hitbox_zone(ctx.hover_hitbox_zone), ctx.window_ctx);
            return;
        }

        ctx.hover_hitbox_zone = detail::HitboxZoneBits::none;
        detail::set_window_cursor(detail::CursorID::arrow, ctx.window_ctx);
    }

    void Window::on_click(MouseKey key, KeyPressState state, u32 click_count)
    {
        if (key != MouseKey::left) return;
        if (state != KeyPressState::press) return;
        (void)click_count;

        auto &ctx = detail::get_context();
        _move_drag_active =
            (window_flags & WindowFlagBits::decorated) && (ctx.hover_id.tag_id == AUIK_TAG_WINDOW_HEADER);
        _resize_zone = detail::HitboxZoneBits::none;
        if ((window_flags & WindowFlagBits::resizable) && !(window_flags & WindowFlagBits::docked) &&
            ctx.hover_id.tag_id == AUIK_TAG_HITBOX)
        {
            // Capture exact zone at press time from current geometry/mouse.
            // This prevents losing diagonal resize on tiny hover-zone jitter.
            _resize_zone = detail::get_hitbox_zone(get_rect(), ctx.io.mouse_pos);
            if (_resize_zone != detail::HitboxZoneBits::none)
            {
                _move_drag_active = false;
                detail::set_window_cursor(detail::get_cursor_for_hitbox_zone(_resize_zone), ctx.window_ctx);
            }
        }

        if (!_scrollbar_x && !_scrollbar_y) return;
        if (!detail::is_scrollbar_tag(ctx.hover_id.tag_id)) return;

        bool is_offset_changed = false;
        _drag_scrollbar = nullptr;

        if (_scrollbar_y && _scrollbar_y->is_visible() &&
            (ctx.hover_id.tag_id == AUIK_TAG_SCROLLBAR_TRACK_Y || ctx.hover_id.tag_id == AUIK_TAG_SCROLLBAR_THUMB_Y))
        {
            _move_drag_active = false;
            _scrollbar_y->set_scroll_offset(_content_offset.y);
            if (ctx.hover_id.tag_id == AUIK_TAG_SCROLLBAR_THUMB_Y) _drag_scrollbar = _scrollbar_y;
            else
            {
                is_offset_changed = _scrollbar_y->scroll_to_track_click(ctx.io.mouse_pos) || is_offset_changed;
                _drag_scrollbar = _scrollbar_y;
            }
            _content_offset.y = _scrollbar_y->scroll_offset();
        }

        if (_scrollbar_x && _scrollbar_x->is_visible() &&
            (ctx.hover_id.tag_id == AUIK_TAG_SCROLLBAR_TRACK_X || ctx.hover_id.tag_id == AUIK_TAG_SCROLLBAR_THUMB_X))
        {
            _move_drag_active = false;
            _scrollbar_x->set_scroll_offset(_content_offset.x);
            if (ctx.hover_id.tag_id == AUIK_TAG_SCROLLBAR_THUMB_X) _drag_scrollbar = _scrollbar_x;
            else
            {
                is_offset_changed = _scrollbar_x->scroll_to_track_click(ctx.io.mouse_pos) || is_offset_changed;
                _drag_scrollbar = _scrollbar_x;
            }
            _content_offset.x = _scrollbar_x->scroll_offset();
        }

        if (is_offset_changed)
        {
            sync_clip_rect_cache();
            add_render_command<detail::ClickEventTraits>(this, [this]() {
                update_layout(true);
                update_draw_commands(DrawReasonBits::layout);
                detail::get_context().dirty_flags |= DirtyFlagBits::redraw;
            });
            return;
        }
    }

    void Window::on_drag(const amal::vec2 &delta, KeyPressState state)
    {
        if (state == KeyPressState::release)
        {
            auto &ctx = detail::get_context();
            _move_drag_active = false;
            if (ctx.hover_id.tag_id == AUIK_TAG_HITBOX)
                detail::set_window_cursor(detail::get_cursor_for_hitbox_zone(ctx.hover_hitbox_zone), ctx.window_ctx);
            else detail::set_window_cursor(detail::CursorID::arrow, ctx.window_ctx);
            _drag_scrollbar = nullptr;
            _resize_zone = detail::HitboxZoneBits::none;
            return;
        }

        if (_drag_scrollbar)
        {
            bool is_offset_changed = false;
            if (_drag_scrollbar == _scrollbar_y)
            {
                _scrollbar_y->set_scroll_offset(_content_offset.y);
                is_offset_changed = _scrollbar_y->scroll_thumb_by_drag_delta(delta);
                _content_offset.y = _scrollbar_y->scroll_offset();
            }
            else if (_drag_scrollbar == _scrollbar_x)
            {
                _scrollbar_x->set_scroll_offset(_content_offset.x);
                is_offset_changed = _scrollbar_x->scroll_thumb_by_drag_delta(delta);
                _content_offset.x = _scrollbar_x->scroll_offset();
            }

            if (is_offset_changed)
            {
                sync_clip_rect_cache();
                add_render_command<detail::DragEventTraits>(this, [this]() {
                    update_layout(true);
                    update_draw_commands(DrawReasonBits::layout);
                    detail::get_context().dirty_flags |= DirtyFlagBits::redraw;
                });
                return;
            }
            return;
        }

        if (_resize_zone != detail::HitboxZoneBits::none)
        {
            auto &ctx = detail::get_context();
            detail::set_window_cursor(detail::get_cursor_for_hitbox_zone(_resize_zone), ctx.window_ctx);
            if (!(window_flags & WindowFlagBits::resizable)) return;
            if (window_flags & WindowFlagBits::docked) return;

            const amal::vec2 old_pos = position();
            const amal::vec2 old_size = size();
            amal::vec2 new_pos = old_pos;
            amal::vec2 new_size = old_size;

            const bool hit_left = _resize_zone & detail::HitboxZoneBits::left;
            const bool hit_right = _resize_zone & detail::HitboxZoneBits::right;
            const bool hit_top = _resize_zone & detail::HitboxZoneBits::top;
            const bool hit_bottom = _resize_zone & detail::HitboxZoneBits::bottom;

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

            const amal::vec2 min_size = {32.0f, 32.0f};
            if (new_size.x < min_size.x)
            {
                if (hit_left) new_pos.x -= (min_size.x - new_size.x);
                new_size.x = min_size.x;
            }
            if (new_size.y < min_size.y)
            {
                if (hit_top) new_pos.y -= (min_size.y - new_size.y);
                new_size.y = min_size.y;
            }

            if (new_pos == old_pos && new_size == old_size) return;
            set_position(new_pos);
            set_size(new_size);
            add_render_command<detail::DragEventTraits>(this, [this]() {
                update_layout(true);
                redraw_all_commands();
            });
            ctx.dirty_flags |= (DirtyFlagBits::redraw);
            return;
        }

        if (!(window_flags & WindowFlagBits::movable)) return;
        if (window_flags & WindowFlagBits::docked) return;
        if (!_move_drag_active) return;

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
        if (index >= count || index == count - 1) return;

        Widget *top = tree[count - 1];
        Window *top_window = as_root_window(top);
        const amal::vec2 self_range = self_window->depth_range();
        const amal::vec2 top_range = top->depth_range();

        top->update_depth(self_range);
        self_window->update_depth(top_range);
        tree[index] = top;
        tree[count - 1] = self_window;

        const bool self_needs_layout = needs_layout_on_active(*self_window);
        add_render_command<detail::FocusEventTraits>(self_window, [self_window, self_needs_layout]() {
            auto flags = self_window->update_style();
            if (self_needs_layout)
            {
                self_window->update_layout(true);
                flags |= StyleUpdateFlagBits::layout | StyleUpdateFlagBits::redraw;
            }
            if (flags & StyleUpdateFlagBits::redraw)
            {
                const auto reason = get_draw_reason_from_style_update(flags);
                if (reason == DrawReasonBits::none) self_window->redraw_decorations();
                else self_window->update_draw_commands(reason);
            }
            detail::get_context().dirty_flags |= DirtyFlagBits::redraw;
        });

        if (top_window)
        {
            const bool top_needs_layout = needs_layout_on_active(*top_window);
            add_render_command<detail::FocusEventTraits>(top_window, [top_window, top_needs_layout]() {
                auto flags = top_window->update_style();
                if (top_needs_layout)
                {
                    top_window->update_layout(true);
                    flags |= StyleUpdateFlagBits::layout | StyleUpdateFlagBits::redraw;
                }
                if (flags & StyleUpdateFlagBits::redraw)
                {
                    const auto reason = get_draw_reason_from_style_update(flags);
                    if (reason == DrawReasonBits::none) top_window->redraw_decorations();
                    else top_window->update_draw_commands(reason);
                }
                detail::get_context().dirty_flags |= DirtyFlagBits::redraw;
            });
        }
        else
            add_render_command<detail::FocusEventTraits>(top, [top]() {
                top->update_draw_commands();
                detail::get_context().dirty_flags |= DirtyFlagBits::redraw;
            });
    }
} // namespace auik::v2
