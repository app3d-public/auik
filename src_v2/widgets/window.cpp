#include <auik/v2/pipelines.hpp>
#include <auik/v2/window.hpp>

namespace auik::v2
{
    void Window::draw(DrawCtx &ctx)
    {
        auto *theme = get_theme();
        auto *quads_stream = get_primary_quad_stream();

        auto &window_style = theme->get_style(_styles[0].id);
        QuadsInstanceData bg_data{};
        bg_data.position = _pos;
        bg_data.size = _size;
        bg_data.z_order = get_z_order();
        fill_quads_instance_by_style(window_style, bg_data);
        ctx.emit(quads_stream, _bg, &bg_data);

        if ((window_flags & WindowFlagBits::decorated) && _header_height > 0.0f)
        {
            auto &header_style = theme->get_style(_styles[1].id);
            QuadsInstanceData header_data{};
            header_data.position = _pos;
            header_data.size = {_size.x, _header_height};
            header_data.z_order = next_depth(_depth);
            fill_quads_instance_by_style(header_style, header_data);
            ctx.emit(quads_stream, _bg, &header_data);
        }
    }

    void Window::update_depth(const amal::vec2 &depth_range)
    {
        Widget::update_depth(depth_range);

        // Parent workzone range for children
        const amal::vec2 w = detail::get_depth_workzone_range(depth_data().range);

        for (auto *child : children)
        {
            if (!child) continue;
            child->update_depth(w);
        }
    }

    void Window::update_style()
    {
        auto *theme = get_theme();
        _styles[0].id = theme->get_resolved_style(_styles[0].tag_id, id(), 0);
        _header_height = 0.0f;
        if (window_flags & WindowFlagBits::decorated)
        {
            _styles[1].id = theme->get_resolved_style(_styles[1].tag_id, id(), 0);
            const auto &header_style = theme->get_style(_styles[1].id);
            _header_height = header_style.text_size() + header_style.padding().y * 2.0f;
        }
    }

} // namespace auik::v2