#pragma once

#include "scrollbar.hpp"
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
               WidgetFlags widget_flags = get_default_widget_flags(), Widget *parent = nullptr);
        ~Window() override;

        void add_child(Widget *child);
        void add_children(const acul::vector<Widget *> &new_children);

        virtual void update_style() override;

    private:
        DrawDataID _bg;
        f32 _header_height = 0.0f;
        StyleSelector _window_style{0, AUIK_STYLE_ID_WINDOW_TYPE};
        class WindowHeader *_header = nullptr;
        Scrollbar *_scrollbar = nullptr;

        virtual void update_depth(const amal::vec2 &depth_range) override;
        virtual void update_layout() override;

        virtual void draw(DrawCtx &ctx) override;
    };
} // namespace auik::v2
