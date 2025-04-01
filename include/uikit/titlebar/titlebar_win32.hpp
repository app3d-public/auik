#ifndef UIKIT_WIDGETS_TITLEBAR_WIN32_H
#define UIKIT_WIDGETS_TITLEBAR_WIN32_H

#include <awin/window.hpp>
#include "../menu/menu.hpp"
#include "../tab/tab.hpp"

namespace uikit
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
            ImVec4 closeColor;
            ImVec4 hoverColor;
            ImVec2 submenuPadding;
            ImVec4 tabActiveColor;
            ImVec4 tabBackgroundColor;
            Icon *icons[IconMaxValue];
        } style;
        TabBar tabbar;

        Titlebar(awin::Window &window, acul::events::dispatcher *ed, MenuBar *menubar, const TabBar &tabbar,
                 const Style &style);

        ~Titlebar() { ed->unbind_listeners(this); }

        virtual void render() override;

        void bindEvents();

    private:
        awin::Window &_window;
        acul::events::dispatcher *ed;
        ImVec2 _controlSize;
        acul::point2D<i32> _dragOffset;
        f32 _captionWidth;
        f32 _clientWidth;
        struct ControlButton
        {
            ControlState state;
            ControlArea area;
        } _controls[3];
        ControlArea _activeArea{ControlArea::None};

        void renderControls();
    };
} // namespace uikit
#endif