#include <core/log.hpp>
#include <uikit/titlebar/titlebar@win32.hpp>

namespace ui
{
    Titlebar::Titlebar(window::Window &window, std::unique_ptr<MenuBar> &menubar, const TabBar &tabbar,
                       const Style &style)
        : MenuBar(std::move(*menubar)),
          _window(window),
          _controls{{ControlState::Idle, ControlArea::Min},
                    {ControlState::Idle, ControlArea::Max},
                    {ControlState::Idle, ControlArea::Close}},
          tabbar(tabbar),
          style(style)
    {
        _controlSize.x = GetSystemMetrics(SM_CXSIZE) * window::getDpi();
        _controlSize.y = GetSystemMetrics(SM_CYSIZE) * window::getDpi();
    }

    void Titlebar::render()
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, MenuBar::_style->menubar.margin);
        ImGui::PushStyleColor(ImGuiCol_MenuBarBg, MenuBar::_style->menubar.backgroundColor);

        if (ImGui::BeginMainMenuBar())
        {
            _height = ImGui::GetWindowHeight();
            const ImVec2 appPos =
                ImGui::GetCursorScreenPos() + ImVec2(0, (_height - style.icons[IconApp]->height()) * 0.5f);
            style.icons[IconApp]->render(appPos);

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, style.submenuPadding);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, MenuBar::_style->menubar.padding);
            for (auto &item : _items)
                item.render();
            ImGui::PopStyleVar(2);

            if (ImGui::IsItemActive())
            {
                if (ImGui::IsMouseDragging(ImGuiMouseButton_Left))
                {
                    Point2D cursorPos = _window.cursorPosition();
                    Point2D windowPos = _window.windowPos();
                    Point2D newPos{windowPos.x + cursorPos.x - _dragOffset.x,
                                   windowPos.y + cursorPos.y - _dragOffset.y};
                    _window.windowPos(newPos);
                }
                else
                    _dragOffset = _window.cursorPosition();
            }

            // Tabs
            _clientWidth = ImGui::GetContentRegionAvail().x - _controlSize.x * 3;
            if (_clientWidth > 100)
            {
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
                ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                                    {MenuBar::_style->menubar.padding.x, MenuBar::_style->menubar.padding.y * 0.5f});
                ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, {0, 0});
                ImGui::PushStyleColor(ImGuiCol_PopupBg, style.tabBackgroundColor);
                ImGui::PushStyleColor(ImGuiCol_ChildBg, style.tabBackgroundColor);
                ImGui::PushStyleColor(ImGuiCol_Header, style.tabActiveColor);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, style.hoverColor);
                ImGui::PushStyleColor(ImGuiCol_HeaderActive, style.tabActiveColor);
                ImVec2 tabbarSize{ImGui::GetContentRegionAvail().x - _controlSize.x * 3, _height};
                tabbar.size(tabbarSize);
                tabbar.render();
                ImGui::PopStyleVar(4);
                ImGui::PopStyleColor(5);

                _clientWidth = ImGui::GetCursorPosX() - tabbar.avaliableWidth();
            }
            _captionWidth = ImGui::GetWindowWidth() - _controlSize.x * 3;
            ImGui::SetCursorPos({_captionWidth, 0});

            // Win Controls
            // Min
            ImVec2 pos{_captionWidth - 1, 0};
            ImVec2 size = ImVec2(_controlSize.x + 1, _height);
            ImGui::GetWindowDrawList()->AddRectFilled(
                pos, ImVec2(pos.x + size.x, pos.y + size.y),
                ImGui::ColorConvertFloat4ToU32(_controls[0].state == ControlState::Hover ? style.hoverColor
                                                                                         : style.tabBackgroundColor));
            pos.x += size.x;
            ImVec2 buttonPos = ImGui::GetCursorScreenPos();
            ImVec2 buttonSize = ImVec2(_controlSize.x, _height);
            ImVec2 centerPos = ImVec2(buttonPos.x + buttonSize.x / 2, buttonPos.y + buttonSize.y / 2);
            float iconWidth = style.icons[IconMin]->width();
            float iconHeight = style.icons[IconMin]->height();
            ImVec2 iconRenderPos = ImVec2(centerPos.x - iconWidth / 2, centerPos.y - iconHeight / 2);
            style.icons[IconMin]->render(iconRenderPos);
            ImGui::SameLine();

            // Max
            ImGui::GetWindowDrawList()->AddRectFilled(
                pos, ImVec2(pos.x + size.x, pos.y + size.y),
                ImGui::ColorConvertFloat4ToU32(_controls[1].state == ControlState::Hover ? style.hoverColor
                                                                                         : style.tabBackgroundColor));
            pos.x += size.x;
            centerPos.x += _controlSize.x;
            if (_window.maximized())
            {
                iconWidth = style.icons[IconRestore]->width();
                iconHeight = style.icons[IconRestore]->height();
                iconRenderPos = ImVec2(centerPos.x - iconWidth / 2, centerPos.y - iconHeight / 2);
                style.icons[IconRestore]->render(iconRenderPos);
            }
            else
            {
                iconWidth = style.icons[IconMax]->width();
                iconHeight = style.icons[IconMax]->height();
                iconRenderPos = ImVec2(centerPos.x - iconWidth / 2, centerPos.y - iconHeight / 2);
                style.icons[IconMax]->render(iconRenderPos);
            }

            // Close
            ImGui::GetWindowDrawList()->AddRectFilled(
                pos, ImVec2(pos.x + size.x, pos.y + size.y),
                ImGui::ColorConvertFloat4ToU32(_controls[2].state == ControlState::Hover ? style.closeColor
                                                                                         : style.tabBackgroundColor));
            centerPos.x += _controlSize.x;
            iconWidth = style.icons[IconClose]->width();
            iconHeight = style.icons[IconClose]->height();
            iconRenderPos = ImVec2(centerPos.x - iconWidth / 2, centerPos.y - iconHeight / 2);
            style.icons[IconClose]->render(iconRenderPos);

            ImGui::EndMainMenuBar();
            ImGui::PopStyleVar(2);
        }
        ImGui::PopStyleColor();
    }

    void Titlebar::bindListeners()
    {
        tabbar.bindListeners();
        bindEvent<window::Win32NativeEvent>("window:NCHitTest", [this](window::Win32NativeEvent &e) {
            if (e.window != &_window)
                return;
            POINT cursorPoint = {0};
            cursorPoint.x = LOWORD(e.lParam);
            cursorPoint.y = HIWORD(e.lParam);
            ScreenToClient(e.hwnd, &cursorPoint);
            if (cursorPoint.y < _height)
            {
                if (cursorPoint.x < _clientWidth)
                    _activeArea = ControlArea::Client;
                else if (cursorPoint.x < _captionWidth)
                {
                    _activeArea = ControlArea::Caption;
                    *e.lResult = HTCAPTION;
                }
                else if (cursorPoint.x < _captionWidth + _controlSize.x)
                {
                    _activeArea = ControlArea::Min;
                    if (_controls[0].state != ControlState::Active)
                    {
                        _controls[0].state = ControlState::Hover;
                        *e.lResult = HTMINBUTTON;
                    }
                }
                else if (cursorPoint.x < _captionWidth + _controlSize.x * 2)
                {
                    _activeArea = ControlArea::Max;
                    if (_controls[1].state != ControlState::Active)
                    {
                        _controls[1].state = ControlState::Hover;
                        *e.lResult = HTMAXBUTTON;
                    }
                }
                else
                {
                    _activeArea = ControlArea::Close;
                    if (_controls[2].state != ControlState::Active)
                    {
                        _controls[2].state = ControlState::Hover;
                        *e.lResult = HTCLOSE;
                    }
                }
            }
            else
                _activeArea = ControlArea::None;

            for (auto &control : _controls)
                if (control.area != _activeArea)
                    control.state = ControlState::Idle;
        });
        bindEvent<window::Win32NativeEvent>("window:NCLMouseDown", [this](window::Win32NativeEvent &e) {
            if (e.window != &_window || _activeArea == ControlArea::None)
                return;
            switch (_activeArea)
            {
                case ControlArea::Min:
                    _controls[0].state = ControlState::Active;
                    *e.lResult = 0;
                    break;
                case ControlArea::Max:
                    _controls[1].state = ControlState::Active;
                    *e.lResult = 0;
                    break;
                case ControlArea::Close:
                    _controls[2].state = ControlState::Active;
                    *e.lResult = 0;
                    break;
                default:
                    break;
            };
        });
        bindEvent<window::MouseClickEvent>("window:input:mouse", [this](const window::MouseClickEvent &e) {
            if (e.window != &_window || _activeArea == ControlArea::None)
                return;
            switch (_activeArea)
            {
                case ControlArea::Min:
                    if (e.action == io::KeyPressState::press)
                        _controls[0].state = ControlState::Active;
                    else
                    {
                        _controls[0].state = ControlState::Hover;
                        _window.minimize();
                    }
                    break;
                case ControlArea::Max:
                    if (e.action == io::KeyPressState::press)
                        _controls[1].state = ControlState::Active;
                    else
                    {
                        _controls[1].state = ControlState::Hover;
                        _window.maximize();
                    }
                    break;
                case ControlArea::Close:
                    if (e.action == io::KeyPressState::press)
                        _controls[2].state = ControlState::Active;
                    else
                    {
                        _controls[2].state = ControlState::Hover;
                        _window.readyToClose(true);
                    }
                default:
                    break;
            }
        });
    }
} // namespace ui