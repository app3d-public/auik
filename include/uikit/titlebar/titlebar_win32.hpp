#ifndef UIKIT_WIDGETS_TITLEBAR_WIN32_H
#define UIKIT_WIDGETS_TITLEBAR_WIN32_H

#include <window/window.hpp>
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
            std::shared_ptr<Icon> icons[IconMaxValue];
        } style;
        TabBar tabbar;

        Titlebar(window::Window &window, MenuBar *menubar, const TabBar &tabbar, const Style &style);

        ~Titlebar() { events::unbindListeners(this); }

        virtual void render() override;

        void bindEvents();

    private:
        window::Window &_window;
        Point2D _dragOffset;
        ImVec2 _controlSize;
        float _captionWidth;
        float _clientWidth;
        struct ControlButton
        {
            ControlState state;
            ControlArea area;
        } _controls[3];
        ControlArea _activeArea{ControlArea::None};
        float _height;
    };
} // namespace uikit
#endif