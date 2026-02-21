#pragma once

#include <amal/vector.hpp>
#include "detail/context.hpp"
#include "draw.hpp"
#include "pipelines.hpp"
#include "widget.hpp"

#define AUIK_STYLE_ID_WINDOW_TYPE 0xB4382179
#define AUIK_STYLE_ID_WINDOW_HEADER_TYPE 0x663566BE

namespace auik::v2
{
    struct WindowFlagBits
    {
        enum enum_type
        {
            none = 0x0,
            resizable = 0x1,
            movable = 0x2,
            decorated = 0x4,
            docked = 0x8,
            dockable = 0x10,
            fixed = 0x20,
        };

        using flag_bitmask = std::true_type;
    };

    using WindowFlags = acul::flags<WindowFlagBits>;

    class Window : public Widget
    {
    public:
        WindowFlags window_flags;

        Window(u32 id, WindowFlags window_flags, WidgetFlags widget_flags, Widget *parent = nullptr,
               amal::vec2 pos = amal::vec2(0.0f), amal::vec2 size = amal::vec2(0.0f))
            : Widget(id, widget_flags, parent),
              window_flags(window_flags),
              _pos(pos),
              _size(size)
        {
        }

        virtual void record_commands() override
        {
            auto &style = get_theme()->get_style(Widget::style.id);

            QuadsInstanceData background_data;
            background_data.position = _pos;
            background_data.size = _size;
            background_data.z_order = _z_order;
            fill_quads_instance_by_style(style, background_data);
            push_data_to_stream(get_default_quad_stream(), &background_data);
        }

    private:
        amal::vec2 _pos;
        amal::vec2 _size;
    };
} // namespace auik::v2
