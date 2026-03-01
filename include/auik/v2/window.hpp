#pragma once

#include "theme.hpp"
#include "widget.hpp"

#define AUIK_STYLE_ID_WINDOW_TYPE        0xB4382179
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

    constexpr inline WindowFlags get_default_window_flags()
    {
        return WindowFlagBits::resizable | WindowFlagBits::movable | WindowFlagBits::decorated;
    }

    class APPLIB_API Window : public Widget
    {
    public:
        WindowFlags window_flags;
        acul::vector<Widget *> children;

        Window(u32 id, amal::vec2 pos = amal::vec2(0.0f), amal::vec2 size = amal::vec2(0.0f),
               WindowFlags window_flags = get_default_window_flags(),
               WidgetFlags widget_flags = get_default_widget_flags(), Widget *parent = nullptr)
            : Widget(id, widget_flags, parent),
              window_flags(window_flags),
              _pos(pos),
              _size(size),
              _styles({0, AUIK_STYLE_ID_WINDOW_TYPE}, {0, AUIK_STYLE_ID_WINDOW_HEADER_TYPE})
        {
        }

        inline void add_child(Widget *child)
        {
            assert(child && "child is null");
            child->set_parent(this);
            children.push_back(child);
        }

        inline void add_children(const acul::vector<Widget *> &new_children)
        {
            for (auto *child : new_children)
            {
                if (!child) continue;
                add_child(child);
            }
        }

        virtual void update_style() override;

    private:
        DrawDataID _bg;
        amal::vec2 _pos;
        amal::vec2 _size;
        f32 _header_height = 0.0f;
        StyleSelector _styles[2];

        virtual void update_depth(const amal::vec2 &depth_range) override;

        virtual void draw(DrawCtx &ctx) override;
    };
} // namespace auik::v2
