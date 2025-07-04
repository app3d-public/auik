#ifndef AUIK_WIDGETS_TITLEBAR_WIN32_H
#define AUIK_WIDGETS_TITLEBAR_WIN32_H

#include <awin/window.hpp>
#include "../menu/menu.hpp"
#include "../tab/tab.hpp"

namespace auik
{
    namespace
    {
        enum class ControlState
        {
            Idle,
            Hover,
            Active
        };

        enum class ControlArea
        {
            None,
            Client,
            Caption,
            Min,
            Max,
            Close
        };
    } // namespace

    class APPLIB_API Titlebar final : public MenuBar
    {
        enum Icons
        {
            IconMin,
            IconMax,
            IconRestore,
            IconClose,
            IconApp,
            IconMaxValue
        };

    public:
        struct Style
        {
            ImVec4 close_color;
            ImVec4 hover_color;
            ImVec2 submenu_padding;
            ImVec4 tab_active_color;
            ImVec4 tab_background_color;
            Icon *icons[IconMaxValue];
        } style;
        TabBar tabbar;

        Titlebar(awin::Window &window, acul::events::dispatcher *ed, MenuBar *menubar, const TabBar &tabbar,
                 const Style &style);

        ~Titlebar() { ed->unbind_listeners(this); }

        virtual void render() override;

        void bind_events();

    private:
        awin::Window &_window;
        acul::events::dispatcher *ed;
        ImVec2 _control_size;
        acul::point2D<i32> _drag_offset;
        f32 _caption_width;
        f32 _client_width;
        struct ControlButton
        {
            ControlState state;
            ControlArea area;
        } _controls[3];
        ControlArea _active_area{ControlArea::None};

        void render_controls();
    };
} // namespace auik
#endif