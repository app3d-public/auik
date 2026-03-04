#include <acul/memory/alloc.hpp>
#include <auik/v2/pipelines.hpp>
#include <auik/v2/window.hpp>
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
            _style.id = theme->get_resolved_style(_style.tag_id, id(), parent_id);
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
        if (can_scroll_y) ensure_scrollbar(_scrollbar_y, this, amal::axis::y);
        if (can_scroll_x) ensure_scrollbar(_scrollbar_x, this, amal::axis::x);

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
        _window_style.id = theme->get_resolved_style(_window_style.tag_id, id(), 0);

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
        if (can_scroll_y) ensure_scrollbar(_scrollbar_y, this, amal::axis::y);
        if (can_scroll_x) ensure_scrollbar(_scrollbar_x, this, amal::axis::x);
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
        if (can_scroll_y) ensure_scrollbar(_scrollbar_y, this, amal::axis::y);
        if (can_scroll_x) ensure_scrollbar(_scrollbar_x, this, amal::axis::x);

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
        else if (_scrollbar_y)
            _scrollbar_y->set_visible(false);

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
        else if (_scrollbar_x)
            _scrollbar_x->set_visible(false);

        // Child content must be clipped to viewport area (header/padding/scroll fixed-margin safe area),
        // otherwise children can render under the header while scrolling.
        const amal::vec2 content_inset = {
            (_content_offset.x > 0.0f ? scroll_margin.x : padding.x),
            (_content_offset.y > 0.0f ? scroll_margin.y : padding.y) + _header_height};
        const amal::vec2 content_size = {
            amal::max(size().x - content_inset.x - padding.z - (need_scroll_y ? bar_w : 0.0f), 0.0f),
            amal::max(size().y - content_inset.y - padding.w - (need_scroll_x ? bar_h : 0.0f), 0.0f)};
        const amal::vec4 content_clip = {position().x + content_inset.x, position().y + content_inset.y, content_size.x,
                                         content_size.y};
        const amal::vec4 parent_clip = get_clip_rect(clip_rect_id());
        const amal::vec4 final_content_clip = intersect_rect(parent_clip, content_clip);
        for (auto *child : children)
        {
            if (!child) continue;
            const u16 clip_id = child->clip_rect_id();
            if (clip_id == 0xFFFFu) continue;
            const amal::vec4 child_clip = get_clip_rect(clip_id);
            get_clip_rect(clip_id) = intersect_rect(child_clip, final_content_clip);
        }

        detail::get_context().screen_cursor = prev_cursor;
    }

    void Window::on_scroll(u32 tag_id, const amal::vec2 &delta)
    {
        (void)tag_id;
        if (!(window_flags & WindowFlagBits::scrollable))
        {
            if (parent()) parent()->on_scroll(tag_id, delta);
            return;
        }

        const amal::vec2 step = -delta * float(AUIK_SCROLL_STEP);
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
            detail::get_context().disposal_queue->emplace([this]() {
                update_layout();
                update_draw_commands();
                detail::get_context().dirty_flags |= DirtyFlagBits::render;
            });
        }
    }
} // namespace auik::v2
