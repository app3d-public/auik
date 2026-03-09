#include <acul/memory/alloc.hpp>
#include <auik/v2/auik.hpp>
#include <auik/v2/pipelines.hpp>
#include <auik/v2/widgets/window.hpp>
#include <cassert>

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

    static constexpr detail::StylePropertyFlags AUIK_LAYOUT_STYLE_MASK =
        detail::StylePropertiesBits::padding | detail::StylePropertiesBits::margin |
        detail::StylePropertiesBits::text_size | detail::StylePropertiesBits::border_thickness |
        detail::StylePropertiesBits::border_radius;

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

    class WindowHeader final : public Widget
    {
    public:
        explicit WindowHeader(Widget *parent)
            : Widget(AUIK_TAG_WINDOW_HEADER, WidgetFlagBits::visible | WidgetFlagBits::foreground, parent, {0.0f, 0.0f},
                     {0.0f, 0.0f}, AUIK_TAG_WINDOW_HEADER),
              _style({0, AUIK_TAG_WINDOW_HEADER})
        {
            assert(parent);
            _rect.widget_id = parent->id();
            _rect.clip_rect_id = parent->clip_rect_id();
        }

        f32 compute_height() const
        {
            auto *theme = get_theme();
            const auto &style = theme->get_style(_style.id);
            return style.text_size() + style.padding().y * 2.0f;
        }

        void update_style() override
        {
            auto *theme = get_theme();
            const u32 parent_id = parent() ? parent()->id() : 0u;
            const StyleState state =
                (detail::get_context().active_id == parent_id) ? StyleState::active : StyleState::normal;
            set_style_state(state);
            _style.id = theme->get_resolved_style(_style.tag_id, id(), parent_id, style_state());
        }

        void rebuild_clip_rects() override
        {
            _rect.clip_rect_id = _parent->clip_rect_id();
            _bg.hit_id = AUIK_INVALID_DRAW_DATA_ID;
        }

        void draw(DrawCtx &ctx) override
        {
            auto *theme = get_theme();
            auto *quads_stream = get_primary_quad_stream();

            QuadsInstanceData data{};
            data.position = position();
            data.size = size();
            data.z_order = get_z_order();
            fill_quads_instance_by_style(theme->get_style(_style.id), clip_rect_id(), data);
            ctx.emit(quads_stream, _bg, &data, get_rect());
        }

    private:
        DrawDataID _bg;
        StyleSelector _style;
    };

    Window::Window(u32 id, amal::vec2 pos, amal::vec2 size, WindowFlags in_window_flags, WidgetFlags in_widget_flags,
                   Widget *parent)
        : Widget(id, in_widget_flags, parent, pos, size, AUIK_TAG_WINDOW), window_flags(in_window_flags)
    {
        widget_flags |= WidgetFlagBits::active_from_child;
        if (window_flags & WindowFlagBits::resizable) _rect.flags |= detail::RectBits::hitbox;
        if (window_flags & WindowFlagBits::decorated) _header = acul::alloc<WindowHeader>(this);
    }

    Window::~Window()
    {
        for (auto *child : children)
            if (child) acul::release(child);
        children.clear();

        if (_header) acul::release(_header);
        if (_scrollbar_x) acul::release(_scrollbar_x);
        if (_scrollbar_y) acul::release(_scrollbar_y);
    }

    void Window::add_child(Widget *child)
    {
        assert(child && "child is null");
        child->set_parent(this);
        children.push_back(child);
    }

    void Window::add_children(const acul::vector<Widget *> &new_children)
    {
        for (auto *child : new_children)
        {
            if (!child) continue;
            add_child(child);
        }
    }

    void Window::draw(DrawCtx &ctx)
    {
        auto *theme = get_theme();
        auto *quads_stream = get_primary_quad_stream();

        auto &window_style = theme->get_style(_window_style.id);
        QuadsInstanceData bg_data{};
        bg_data.position = position();
        bg_data.size = size();
        bg_data.z_order = get_z_order();
        fill_quads_instance_by_style(window_style, clip_rect_id(), bg_data);
        ctx.emit(quads_stream, _bg, &bg_data, get_rect());

        if (_header) _header->draw(ctx);

        const amal::vec2 prev_cursor = detail::get_context().screen_cursor;
        const auto &padding = window_style.padding();
        amal::vec2 cursor = position() + amal::vec2{padding.x, padding.y};
        if (_header_height > 0.0f) cursor.y += _header_height;
        detail::get_context().screen_cursor = cursor;

        for (auto *child : children)
        {
            if (!child) continue;
            child->draw(ctx);
        }

        if (_scrollbar_y && _scrollbar_y->is_visible()) _scrollbar_y->draw(ctx);
        if (_scrollbar_x && _scrollbar_x->is_visible()) _scrollbar_x->draw(ctx);

        detail::get_context().screen_cursor = prev_cursor;
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

        amal::vec2 next_child_range{};
        assign_next_depth(this->depth_range(), next_child_range);

        for (auto *child : children)
        {
            if (!child) continue;
            child->update_depth(next_child_range);
        }

        if (_header) _header->update_depth(next_child_range);
        if (_scrollbar_y) _scrollbar_y->update_depth(this->depth_range());
        if (_scrollbar_x) _scrollbar_x->update_depth(this->depth_range());
    }

    void Window::update_style()
    {
        auto *theme = get_theme();
        const StyleState state = (detail::get_context().active_id == id()) ? StyleState::active : StyleState::normal;
        set_style_state(state);
        _window_style.id = theme->get_resolved_style(_window_style.tag_id, id(), 0, style_state());

        if ((window_flags & WindowFlagBits::decorated) && !_header) _header = acul::alloc<WindowHeader>(this);
        if (window_flags & WindowFlagBits::decorated)
        {
            _header->update_style();
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

        if (_header)
        {
            _header->set_position(position());
            _header->set_size({size().x, _header_height});
        }

        for (auto *child : children)
        {
            if (!child) continue;
            child->update_style();
        }

        const bool can_scroll_y =
            (window_flags & WindowFlagBits::scrollable) && !(window_flags & WindowFlagBits::no_scrollbar_y);
        const bool can_scroll_x =
            (window_flags & WindowFlagBits::scrollable) && !(window_flags & WindowFlagBits::no_scrollbar_x);
        if (can_scroll_y) ensure_y_scrollbar(_scrollbar_y, this);
        if (can_scroll_x) ensure_x_scrollbar(_scrollbar_x, this);
        if (_scrollbar_y) _scrollbar_y->update_style();
        if (_scrollbar_x) _scrollbar_x->update_style();
    }

    void Window::rebuild_clip_rects()
    {
        Widget::rebuild_clip_rects();
        _bg.hit_id = AUIK_INVALID_DRAW_DATA_ID;

        if (_header)
        {
            _header->rebuild_clip_rects();
            _header->set_clip_rect_id(clip_rect_id());
        }

        for (auto *child : children)
        {
            if (!child) continue;
            child->rebuild_clip_rects();
        }

        if (_scrollbar_y)
        {
            _scrollbar_y->set_clip_rect_id(clip_rect_id());
            _scrollbar_y->rebuild_clip_rects();
        }

        if (_scrollbar_x)
        {
            _scrollbar_x->set_clip_rect_id(clip_rect_id());
            _scrollbar_x->rebuild_clip_rects();
        }
    }

    void Window::update_layout()
    {
        Widget::update_layout();

        const amal::vec2 prev_cursor = detail::get_context().screen_cursor;
        auto *theme = get_theme();
        const auto &window_style = theme->get_style(_window_style.id);
        const auto &padding = window_style.padding();
        const auto &scroll_margin = window_style.margin();
        _content_offset = amal::max(_content_offset, 0.0f);
        const f32 content_inset_x = (_content_offset.x > 0.0f) ? scroll_margin.x : padding.x;
        const f32 content_inset_y = (_content_offset.y > 0.0f) ? scroll_margin.y : padding.y;
        amal::vec2 cursor = position() + amal::vec2{content_inset_x, content_inset_y + _header_height};
        detail::get_context().screen_cursor = cursor - _content_offset;

        f32 content_max_width = 0.0f;
        for (auto *child : children)
        {
            if (!child) continue;
            child->update_layout();
            content_max_width = amal::max(content_max_width, child->required_size().x);
        }

        f32 content_height = detail::get_context().screen_cursor.y - (cursor.y - _content_offset.y);
        const f32 required_height = padding.y + padding.w + content_height + _header_height;
        const f32 required_width = padding.x + padding.z + content_max_width;
        set_required_size({required_width, required_height});

        const bool can_scroll_y =
            (window_flags & WindowFlagBits::scrollable) && !(window_flags & WindowFlagBits::no_scrollbar_y);
        const bool can_scroll_x =
            (window_flags & WindowFlagBits::scrollable) && !(window_flags & WindowFlagBits::no_scrollbar_x);
        if (can_scroll_y) ensure_y_scrollbar(_scrollbar_y, this);
        if (can_scroll_x) ensure_x_scrollbar(_scrollbar_x, this);

        const f32 body_width = amal::max(size().x - padding.x - padding.z, 0.0f);
        const f32 body_height = amal::max(size().y - padding.y - padding.w - _header_height, 0.0f);
        const f32 bar_w = _scrollbar_y ? _scrollbar_y->get_min_track_thickness() : 0.0f;
        const f32 bar_h = _scrollbar_x ? _scrollbar_x->get_min_track_thickness() : 0.0f;

        bool need_scroll_y = can_scroll_y && content_height > body_height;
        bool need_scroll_x = can_scroll_x && content_max_width > body_width;
        for (int i = 0; i < 2; ++i)
        {
            const f32 viewport_w = amal::max(body_width - (need_scroll_y ? bar_w : 0.0f), 0.0f);
            const f32 viewport_h = amal::max(body_height - (need_scroll_x ? bar_h : 0.0f), 0.0f);
            const bool next_y = can_scroll_y && content_height > viewport_h;
            const bool next_x = can_scroll_x && content_max_width > viewport_w;
            if (next_y == need_scroll_y && next_x == need_scroll_x) break;
            need_scroll_y = next_y;
            need_scroll_x = next_x;
        }

        const f32 viewport_width = amal::max(body_width - (need_scroll_y ? bar_w : 0.0f), 0.0f);
        const f32 viewport_height = amal::max(body_height - (need_scroll_x ? bar_h : 0.0f), 0.0f);
        if (_scrollbar_x) _scrollbar_x->set_metrics(content_max_width, viewport_width);
        if (_scrollbar_y) _scrollbar_y->set_metrics(content_height, viewport_height);
        const amal::vec2 max_scroll = {_scrollbar_x ? _scrollbar_x->max_scroll() : 0.0f,
                                       _scrollbar_y ? _scrollbar_y->max_scroll() : 0.0f};
        const amal::vec2 clamped_offset = amal::clamp(_content_offset, amal::vec2{0.0f}, max_scroll);
        if (clamped_offset != _content_offset)
        {
            _content_offset = clamped_offset;
            const f32 clamped_inset_x = (_content_offset.x > 0.0f) ? scroll_margin.x : padding.x;
            const f32 clamped_inset_y = (_content_offset.y > 0.0f) ? scroll_margin.y : padding.y;
            cursor = position() + amal::vec2{clamped_inset_x, clamped_inset_y + _header_height};
            detail::get_context().screen_cursor = cursor - _content_offset;
            content_max_width = 0.0f;
            for (auto *child : children)
            {
                if (!child) continue;
                child->update_layout();
                content_max_width = amal::max(content_max_width, child->required_size().x);
            }
            content_height = detail::get_context().screen_cursor.y - (cursor.y - _content_offset.y);
            set_required_size(
                {padding.x + padding.z + content_max_width, padding.y + padding.w + content_height + _header_height});
            if (_scrollbar_x) _scrollbar_x->set_metrics(content_max_width, viewport_width);
            if (_scrollbar_y) _scrollbar_y->set_metrics(content_height, viewport_height);
        }

        if (_header)
        {
            _header->set_position(position());
            _header->set_size({size().x, _header_height});
            _header->set_clip_rect_id(clip_rect_id());
        }

        const bool was_scrollbar_y_visible = _scrollbar_y && _scrollbar_y->is_visible();
        const bool was_scrollbar_x_visible = _scrollbar_x && _scrollbar_x->is_visible();

        if (need_scroll_y && _scrollbar_y)
        {
            const amal::vec4 track_margin = _scrollbar_y->get_track_margin();
            const f32 track_w = _scrollbar_y->get_min_track_thickness();
            const amal::vec2 body_pos = {position().x, position().y + _header_height};
            const amal::vec2 body_size = {size().x, amal::max(size().y - _header_height, 0.0f)};
            const amal::vec2 usable_size = {body_size.x, amal::max(body_size.y - (need_scroll_x ? bar_h : 0.0f), 0.0f)};

            const amal::vec2 track_area_pos = {body_pos.x + track_margin.x, body_pos.y + track_margin.y};
            const amal::vec2 track_area_size = {amal::max(usable_size.x - track_margin.x - track_margin.z, 0.0f),
                                                amal::max(usable_size.y - track_margin.y - track_margin.w, 0.0f)};
            const amal::vec2 track_pos = {track_area_pos.x + amal::max(track_area_size.x - track_w, 0.0f),
                                          track_area_pos.y};
            const amal::vec2 track_size = {track_w, track_area_size.y};
            _scrollbar_y->set_visible(true);
            _scrollbar_y->set_scroll_offset(_content_offset.y);
            _scrollbar_y->configure(track_pos, track_size, content_height, viewport_height);
            _content_offset.y = _scrollbar_y->scroll_offset();
            _scrollbar_y->set_clip_rect_id(clip_rect_id());
        }
        else if (_scrollbar_y) _scrollbar_y->set_visible(false);

        if (need_scroll_x && _scrollbar_x)
        {
            const amal::vec4 track_margin = _scrollbar_x->get_track_margin();
            const f32 track_h = _scrollbar_x->get_min_track_thickness();
            const amal::vec2 body_pos = {position().x, position().y + _header_height};
            const amal::vec2 body_size = {size().x, amal::max(size().y - _header_height, 0.0f)};
            const amal::vec2 usable_size = {amal::max(body_size.x - (need_scroll_y ? bar_w : 0.0f), 0.0f), body_size.y};

            const amal::vec2 track_area_pos = {body_pos.x + track_margin.x, body_pos.y + track_margin.y};
            const amal::vec2 track_area_size = {amal::max(usable_size.x - track_margin.x - track_margin.z, 0.0f),
                                                amal::max(usable_size.y - track_margin.y - track_margin.w, 0.0f)};
            const amal::vec2 track_pos = {track_area_pos.x,
                                          track_area_pos.y + amal::max(track_area_size.y - track_h, 0.0f)};
            const amal::vec2 track_size = {track_area_size.x, track_h};
            _scrollbar_x->set_visible(true);
            _scrollbar_x->set_scroll_offset(_content_offset.x);
            _scrollbar_x->configure(track_pos, track_size, content_max_width, viewport_width);
            _content_offset.x = _scrollbar_x->scroll_offset();
            _scrollbar_x->set_clip_rect_id(clip_rect_id());
        }
        else if (_scrollbar_x) _scrollbar_x->set_visible(false);

        const bool is_scrollbar_y_visible = _scrollbar_y && _scrollbar_y->is_visible();
        const bool is_scrollbar_x_visible = _scrollbar_x && _scrollbar_x->is_visible();
        if (was_scrollbar_y_visible != is_scrollbar_y_visible || was_scrollbar_x_visible != is_scrollbar_x_visible)
        {
            auto &ctx = detail::get_context();
            ctx.disposal_queue.emplace([]() {
                detail::get_context().dirty_flags |= DirtyFlagBits::hit_rect_draw;
                record_all_commands();
                detail::get_context().dirty_flags |= DirtyFlagBits::redraw;
            });
            ctx.dirty_flags |= DirtyFlagBits::host_update;
        }

        // Child content must be clipped to viewport area (header/padding/scroll fixed-margin safe area),
        // otherwise children can render under the header while scrolling.
        const amal::vec2 content_inset = {(_content_offset.x > 0.0f ? scroll_margin.x : padding.x),
                                          (_content_offset.y > 0.0f ? scroll_margin.y : padding.y) + _header_height};
        const amal::vec2 content_size = {
            amal::max(size().x - content_inset.x - padding.z - (need_scroll_y ? bar_w : 0.0f), 0.0f),
            amal::max(size().y - content_inset.y - padding.w - (need_scroll_x ? bar_h : 0.0f), 0.0f)};
        const amal::vec4 content_clip = {position().x + content_inset.x, position().y + content_inset.y, content_size.x,
                                         content_size.y};
        const amal::vec4 parent_clip = get_clip_rect(clip_rect_id());
        _children_clip_rect = intersect_rect(parent_clip, content_clip);
        for (auto *child : children)
        {
            if (!child) continue;
            const u16 clip_id = child->clip_rect_id();
            if (clip_id == 0xFFFFu) continue;
            const amal::vec2 child_pos = child->position();
            const amal::vec2 child_size = child->size();
            const amal::vec4 child_rect = {child_pos.x, child_pos.y, child_size.x, child_size.y};
            update_clip_rect(clip_id, intersect_rect(child_rect, _children_clip_rect));
        }

        detail::get_context().screen_cursor = prev_cursor;
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
            auto &ctx = detail::get_context();
            ctx.disposal_queue.emplace([this]() {
                // Queue flush can happen after next_frame(); ensure clip cache for current frame is valid
                // before any layout code reads parent/child clip rects.
                sync_clip_rect_cache();
                update_layout();
                update_draw_commands();
                detail::get_context().dirty_flags |= DirtyFlagBits::redraw;
            });
            ctx.dirty_flags |= DirtyFlagBits::host_update;
        }
    }

    void Window::on_hover(HoverState state, u32 prev_tag_id)
    {
        if (!_scrollbar_x && !_scrollbar_y) return;
        auto &ctx = detail::get_context();

        const bool now_thumb_y =
            (ctx.hover_id.widget_id == id()) && (ctx.hover_id.tag_id == AUIK_TAG_SCROLLBAR_THUMB_Y);
        const bool now_thumb_x =
            (ctx.hover_id.widget_id == id()) && (ctx.hover_id.tag_id == AUIK_TAG_SCROLLBAR_THUMB_X);
        const bool was_thumb_y = prev_tag_id == AUIK_TAG_SCROLLBAR_THUMB_Y;
        const bool was_thumb_x = prev_tag_id == AUIK_TAG_SCROLLBAR_THUMB_X;
        if (!now_thumb_y && !now_thumb_x && !was_thumb_y && !was_thumb_x) return;

        bool changed_y = (_scrollbar_y && _scrollbar_y->set_thumb_hovered(now_thumb_y));
        bool changed_x = (_scrollbar_x && _scrollbar_x->set_thumb_hovered(now_thumb_x));

        if (changed_x || changed_y)
        {
            ctx.disposal_queue.emplace([this]() {
                if (_scrollbar_y)
                {
                    _scrollbar_y->update_style();
                    if (_scrollbar_y->has_draw_record()) _scrollbar_y->update_draw_commands();
                    else _scrollbar_y->record_draw_commands();
                }
                if (_scrollbar_x)
                {
                    _scrollbar_x->update_style();
                    if (_scrollbar_x->has_draw_record()) _scrollbar_x->update_draw_commands();
                    else _scrollbar_x->record_draw_commands();
                }
                detail::get_context().dirty_flags |= DirtyFlagBits::redraw;
            });
            ctx.dirty_flags |= DirtyFlagBits::host_update;
        }
        apply_hover_style_state(*this, state);
    }

    void Window::on_click(MouseKey key, KeyPressState state, u32 click_count)
    {
        if (key != MouseKey::left) return;
        if (state != KeyPressState::press) return;
        (void)click_count;

        auto &ctx = detail::get_context();
        _resize_zone = detail::HitboxZoneBits::none;
        if ((window_flags & WindowFlagBits::resizable) && !(window_flags & WindowFlagBits::docked) &&
            ctx.hover_id.tag_id == AUIK_TAG_HITBOX)
        {
            // Capture exact zone at press time from current geometry/mouse.
            // This prevents losing diagonal resize on tiny hover-zone jitter.
            _resize_zone = detail::get_hitbox_zone(get_rect(), ctx.io.mouse_pos);
            if (_resize_zone != detail::HitboxZoneBits::none)
                detail::set_window_cursor(detail::get_cursor_for_hitbox_zone(_resize_zone), ctx.window_ctx);
        }

        if (!_scrollbar_x && !_scrollbar_y) return;
        if (!detail::is_scrollbar_tag(ctx.hover_id.tag_id)) return;

        bool is_offset_changed = false;
        bool is_thumb_drag_started = false;
        _drag_scrollbar = nullptr;

        if (_scrollbar_y && _scrollbar_y->is_visible() &&
            (ctx.hover_id.tag_id == AUIK_TAG_SCROLLBAR_TRACK_Y || ctx.hover_id.tag_id == AUIK_TAG_SCROLLBAR_THUMB_Y))
        {
            _scrollbar_y->set_scroll_offset(_content_offset.y);
            if (ctx.hover_id.tag_id == AUIK_TAG_SCROLLBAR_THUMB_Y)
            {
                _drag_scrollbar = _scrollbar_y;
                is_thumb_drag_started = true;
            }
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
            _scrollbar_x->set_scroll_offset(_content_offset.x);
            if (ctx.hover_id.tag_id == AUIK_TAG_SCROLLBAR_THUMB_X)
            {
                _drag_scrollbar = _scrollbar_x;
                is_thumb_drag_started = true;
            }
            else
            {
                is_offset_changed = _scrollbar_x->scroll_to_track_click(ctx.io.mouse_pos) || is_offset_changed;
                _drag_scrollbar = _scrollbar_x;
            }
            _content_offset.x = _scrollbar_x->scroll_offset();
        }

        bool active_state_changed = false;
        if (is_thumb_drag_started)
        {
            active_state_changed = (_scrollbar_y && _scrollbar_y->set_thumb_active(_drag_scrollbar == _scrollbar_y)) ||
                                   (_scrollbar_x && _scrollbar_x->set_thumb_active(_drag_scrollbar == _scrollbar_x));
        }
        else
        {
            active_state_changed = (_scrollbar_y && _scrollbar_y->set_thumb_active(false)) ||
                                   (_scrollbar_x && _scrollbar_x->set_thumb_active(false));
        }

        if (is_offset_changed)
        {
            ctx.disposal_queue.emplace([this]() {
                sync_clip_rect_cache();
                update_layout();
                update_draw_commands();
                detail::get_context().dirty_flags |= DirtyFlagBits::redraw;
            });
            ctx.dirty_flags |= DirtyFlagBits::host_update;
            return;
        }

        if (active_state_changed)
        {
            ctx.disposal_queue.emplace([this]() {
                if (_scrollbar_y)
                {
                    _scrollbar_y->update_style();
                    if (_scrollbar_y->has_draw_record()) _scrollbar_y->update_draw_commands();
                    else _scrollbar_y->record_draw_commands();
                }
                if (_scrollbar_x)
                {
                    _scrollbar_x->update_style();
                    if (_scrollbar_x->has_draw_record()) _scrollbar_x->update_draw_commands();
                    else _scrollbar_x->record_draw_commands();
                }
                detail::get_context().dirty_flags |= DirtyFlagBits::redraw;
            });
            ctx.dirty_flags |= DirtyFlagBits::host_update;
        }
    }

    void Window::on_drag(const amal::vec2 &delta, KeyPressState state)
    {
        if (state == KeyPressState::release)
        {
            auto &ctx = detail::get_context();
            bool active_state_changed = false;
            active_state_changed = (_scrollbar_y && _scrollbar_y->set_thumb_active(false)) || active_state_changed;
            active_state_changed = (_scrollbar_x && _scrollbar_x->set_thumb_active(false)) || active_state_changed;
            if (active_state_changed)
            {
                ctx.disposal_queue.emplace([this]() {
                    if (_scrollbar_y)
                    {
                        _scrollbar_y->update_style();
                        if (_scrollbar_y->has_draw_record()) _scrollbar_y->update_draw_commands();
                        else _scrollbar_y->record_draw_commands();
                    }
                    if (_scrollbar_x)
                    {
                        _scrollbar_x->update_style();
                        if (_scrollbar_x->has_draw_record()) _scrollbar_x->update_draw_commands();
                        else _scrollbar_x->record_draw_commands();
                    }
                    detail::get_context().dirty_flags |= DirtyFlagBits::redraw;
                });
                ctx.dirty_flags |= DirtyFlagBits::host_update;
            }
            if (ctx.hover_id.tag_id == AUIK_TAG_HITBOX)
                detail::set_window_cursor(detail::get_cursor_for_hitbox_zone(ctx.hover_hitbox_zone), ctx.window_ctx);
            else detail::set_window_cursor(detail::CursorID::arrow, ctx.window_ctx);
            _drag_scrollbar = nullptr;
            _resize_zone = detail::HitboxZoneBits::none;
            return;
        }

        if (_drag_scrollbar)
        {
            bool active_state_changed = false;
            active_state_changed = (_scrollbar_y && _scrollbar_y->set_thumb_active(_drag_scrollbar == _scrollbar_y)) ||
                                   active_state_changed;
            active_state_changed = (_scrollbar_x && _scrollbar_x->set_thumb_active(_drag_scrollbar == _scrollbar_x)) ||
                                   active_state_changed;

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

            auto &ctx = detail::get_context();
            if (is_offset_changed)
            {
                ctx.disposal_queue.emplace([this]() {
                    sync_clip_rect_cache();
                    update_layout();
                    update_draw_commands();
                    detail::get_context().dirty_flags |= DirtyFlagBits::redraw;
                });
                ctx.dirty_flags |= DirtyFlagBits::host_update;
                return;
            }

            if (active_state_changed)
            {
                ctx.disposal_queue.emplace([this]() {
                    if (_scrollbar_y)
                    {
                        _scrollbar_y->update_style();
                        if (_scrollbar_y->has_draw_record()) _scrollbar_y->update_draw_commands();
                        else _scrollbar_y->record_draw_commands();
                    }
                    if (_scrollbar_x)
                    {
                        _scrollbar_x->update_style();
                        if (_scrollbar_x->has_draw_record()) _scrollbar_x->update_draw_commands();
                        else _scrollbar_x->record_draw_commands();
                    }
                    detail::get_context().dirty_flags |= DirtyFlagBits::redraw;
                });
                ctx.dirty_flags |= DirtyFlagBits::host_update;
            }
            return;
        }

        if (_resize_zone != detail::HitboxZoneBits::none)
        {
            auto &ctx = detail::get_context();
            detail::set_window_cursor(detail::get_cursor_for_hitbox_zone(_resize_zone), ctx.window_ctx);
            if (!(window_flags & WindowFlagBits::resizable)) return;
            if (window_flags & WindowFlagBits::docked) return;
            if (delta.x == 0.0f && delta.y == 0.0f) return;

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
            ctx.disposal_queue.emplace([this]() {
                const bool was_scrollbar_x_visible = _scrollbar_x && _scrollbar_x->is_visible();
                const bool was_scrollbar_y_visible = _scrollbar_y && _scrollbar_y->is_visible();
                sync_clip_rect_cache();
                update_layout();
                const bool is_scrollbar_x_visible = _scrollbar_x && _scrollbar_x->is_visible();
                const bool is_scrollbar_y_visible = _scrollbar_y && _scrollbar_y->is_visible();
                const bool need_record_for_scrollbar_x = is_scrollbar_x_visible && !_scrollbar_x->has_draw_record();
                const bool need_record_for_scrollbar_y = is_scrollbar_y_visible && !_scrollbar_y->has_draw_record();
                const bool is_scrollbar_visibility_changed = (was_scrollbar_x_visible != is_scrollbar_x_visible) ||
                                                             (was_scrollbar_y_visible != is_scrollbar_y_visible);
                if (is_scrollbar_visibility_changed || need_record_for_scrollbar_x || need_record_for_scrollbar_y)
                    record_draw_commands();
                else update_draw_commands();
                detail::get_context().dirty_flags |= DirtyFlagBits::redraw;
            });
            ctx.dirty_flags |= (DirtyFlagBits::host_update | DirtyFlagBits::redraw);
            return;
        }

        if (!(window_flags & WindowFlagBits::movable)) return;
        if (window_flags & WindowFlagBits::docked) return;
        if (delta.x == 0.0f && delta.y == 0.0f) return;

        set_position(position() + delta);
        auto &ctx = detail::get_context();
        ctx.disposal_queue.emplace([this]() {
            sync_clip_rect_cache();
            update_layout();
            update_draw_commands();
            detail::get_context().dirty_flags |= DirtyFlagBits::redraw;
        });
        ctx.dirty_flags |= DirtyFlagBits::host_update;
    }

    void Window::on_active()
    {
        Widget::on_active();

        if (widget_flags & WidgetFlagBits::active_to_child)
        {
            for (auto *child : children)
            {
                if (!child) continue;
                child->on_parent_active();
            }
        }

        if ((window_flags & WindowFlagBits::docked) || parent()) return;

        auto &ctx = detail::get_context();
        const u32 self_id = id();
        ctx.disposal_queue.emplace([self_id]() {
            auto &ctx = detail::get_context();
            auto self_it = ctx.id_map.find(self_id);
            if (self_it == ctx.id_map.end()) return;
            auto *self_widget = self_it->second;
            auto *self_window = as_root_window(self_widget);
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

            const bool this_needs_layout = needs_layout_on_active(*self_window);
            self_window->update_style();
            if (this_needs_layout) self_window->update_layout();
            self_window->update_draw_commands();

            if (top_window)
            {
                const bool top_needs_layout = needs_layout_on_active(*top_window);
                top_window->update_style();
                if (top_needs_layout) top_window->update_layout();
                top_window->update_draw_commands();
            }
            else top->update_draw_commands();
            ctx.dirty_flags |= DirtyFlagBits::redraw;
        });
        ctx.dirty_flags |= DirtyFlagBits::host_update;
    }
} // namespace auik::v2
