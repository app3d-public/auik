#include <acul/memory/alloc.hpp>
#include <auik/v2/pipelines.hpp>
#include <auik/v2/window.hpp>
#include <cassert>

#define AUIK_WINDOW_HEADER_ID 0x646DC8B1u

namespace auik::v2
{
    class WindowHeader final : public Widget
    {
    public:
        explicit WindowHeader(Widget *parent)
            : Widget(AUIK_WINDOW_HEADER_ID, WidgetFlagBits::visible | WidgetFlagBits::foreground, parent),
              _style({0, AUIK_STYLE_ID_WINDOW_HEADER_TYPE})
        {
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

        void update_layout() override
        {
            Widget::update_layout();
            set_required_size(size());
        }

        void draw(DrawCtx &ctx) override
        {
            auto *theme = get_theme();
            auto *quads_stream = get_primary_quad_stream();

            QuadsInstanceData data{};
            data.position = position();
            data.size = size();
            data.z_order = get_z_order();
            fill_quads_instance_by_style(theme->get_style(_style.id), data);
            set_quads_clip_rect(data, clip_rect_id());
            ctx.emit(quads_stream, _bg, &data);
        }

    private:
        DrawDataID _bg;
        StyleSelector _style;
    };

    Window::Window(u32 id, amal::vec2 pos, amal::vec2 size, WindowFlags in_window_flags, WidgetFlags in_widget_flags,
                   Widget *parent)
        : Widget(id, in_widget_flags, parent, pos, size), window_flags(in_window_flags)
    {
        if (window_flags & WindowFlagBits::decorated) _header = acul::alloc<WindowHeader>(this);
    }

    Window::~Window()
    {
        for (auto *child : children)
            if (child) acul::release(child);
        children.clear();

        if (_header) acul::release(_header);
        if (_scrollbar) acul::release(_scrollbar);
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
        fill_quads_instance_by_style(window_style, bg_data);
        set_quads_clip_rect(bg_data, clip_rect_id());
        ctx.emit(quads_stream, _bg, &bg_data);

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

        if (_scrollbar && _scrollbar->is_visible()) _scrollbar->draw(ctx);

        detail::get_context().screen_cursor = prev_cursor;
    }

    void Window::update_depth(const amal::vec2 &depth_range)
    {
        Widget::update_depth(depth_range);

        detail::DepthData next_child_depth{};
        assign_next_depth(depth_data(), next_child_depth);

        for (auto *child : children)
        {
            if (!child) continue;
            child->update_depth(next_child_depth.range);
        }

        if (_header) _header->update_depth(next_child_depth.range);
        if (_scrollbar) _scrollbar->update_depth(depth_data().range);
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

        if (_scrollbar) _scrollbar->update_style();
    }

    void Window::update_layout()
    {
        Widget::update_layout();

        const amal::vec2 prev_cursor = detail::get_context().screen_cursor;
        auto *theme = get_theme();
        const auto &window_style = theme->get_style(_window_style.id);
        const auto &padding = window_style.padding();
        amal::vec2 cursor = position() + amal::vec2{padding.x, padding.y};
        if (_header_height > 0.0f) cursor.y += _header_height;
        detail::get_context().screen_cursor = cursor;

        f32 content_max_width = 0.0f;
        for (auto *child : children)
        {
            if (!child) continue;
            child->update_layout();
            content_max_width = amal::max(content_max_width, child->required_size().x);
        }

        const f32 content_height = detail::get_context().screen_cursor.y - cursor.y;
        const f32 required_height = padding.y + padding.w + content_height + _header_height;
        const f32 required_width = padding.x + padding.z + content_max_width;
        set_required_size({required_width, required_height});

        const f32 viewport_height = size().y - padding.y - padding.w - _header_height;
        const bool need_scroll =
            (widget_flags & WidgetFlagBits::scrolable) && viewport_height > 0.0f && content_height > viewport_height;

        if (_header)
        {
            _header->set_position(position());
            _header->set_size({size().x, _header_height});
            _header->update_layout();
        }

        if (need_scroll)
        {
            if (!_scrollbar)
            {
                _scrollbar = acul::alloc<Scrollbar>(this);
                _scrollbar->update_style();
                _scrollbar->update_depth(depth_data().range);
            }

            const amal::vec4 track_margin = _scrollbar->get_track_margin();
            const f32 track_w = _scrollbar->get_min_track_width();

            const f32 body_top = position().y + _header_height;
            const f32 body_height = amal::max(size().y - (body_top - position().y), 0.0f);

            const f32 track_area_x = position().x + track_margin.x;
            const f32 track_area_w = amal::max(size().x - track_margin.x - track_margin.z, 0.0f);
            const f32 track_x = track_area_x + amal::max(track_area_w - track_w, 0.0f);
            const f32 track_y = body_top + track_margin.y;
            const f32 track_h = amal::max(body_height - track_margin.y - track_margin.w, 0.0f);

            const amal::vec2 track_pos = {track_x, track_y};
            const amal::vec2 track_size = {track_w, track_h};
            _scrollbar->set_visible(true);
            _scrollbar->configure(track_pos, track_size, content_height, viewport_height);
            _scrollbar->update_layout();
        }
        else if (_scrollbar)
            _scrollbar->set_visible(false);

        detail::get_context().screen_cursor = prev_cursor;
    }

} // namespace auik::v2
