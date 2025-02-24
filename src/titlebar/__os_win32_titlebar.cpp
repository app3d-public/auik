#include <core/log.hpp>
#include <dwmapi.h>
#include <uikit/titlebar/titlebar_win32.hpp>

namespace uikit
{
    ImVec2 getControllsSize(HWND hwnd)
    {
        ImVec2 controlSize;
        BOOL isCompositionEnabled = FALSE;
        HRESULT hr = DwmIsCompositionEnabled(&isCompositionEnabled);
        if (SUCCEEDED(hr) && isCompositionEnabled)
        {
            RECT rect;
            hr = DwmGetWindowAttribute(hwnd, DWMWA_CAPTION_BUTTON_BOUNDS, &rect, sizeof(rect));
            if (SUCCEEDED(hr))
            {
                controlSize.x = (rect.right - rect.left) / 3.0f * window::getDpi();
                controlSize.y = (rect.bottom - rect.top) * window::getDpi();
                return controlSize;
            }
        }
        logWarn("Failed to get DWMWA_CAPTION_BUTTON_BOUNDS");
        controlSize.x = GetSystemMetrics(SM_CXSIZE) * window::getDpi();
        controlSize.y = GetSystemMetrics(SM_CYSIZE) * window::getDpi();
        return controlSize;
    }

    Titlebar::Titlebar(window::Window &window, events::Manager *e, MenuBar *menubar, const TabBar &tabbar,
                       const Style &style)
        : MenuBar(std::move(*menubar), "titlebar"),
          style(style),
          tabbar(tabbar),
          _window(window),
          e(e),
          _controlSize(getControllsSize(window::platform::native_access::getHWND(window))),
          _controls{{ControlState::Idle, ControlArea::Min},
                    {ControlState::Idle, ControlArea::Max},
                    {ControlState::Idle, ControlArea::Close}}
    {
    }

    bool BeginMainMenuBar(float v = ImGui::GetFrameHeight())
    {
        ImGuiContext &g = *GImGui;
        ImGuiViewportP *viewport = (ImGuiViewportP *)(void *)ImGui::GetMainViewport();

        // For the main menu bar, which cannot be moved, we honor g.Style.DisplaySafeAreaPadding to ensure text can be
        // visible on a TV set.
        // FIXME: This could be generalized as an opt-in way to clamp window->DC.CursorStartPos to avoid SafeArea?
        // FIXME: Consider removing support for safe area down the line... it's messy. Nowadays consoles have support
        // for TV calibration in OS settings.
        g.NextWindowData.MenuBarOffsetMinVal = ImVec2(
            g.Style.DisplaySafeAreaPadding.x, ImMax(g.Style.DisplaySafeAreaPadding.y - g.Style.FramePadding.y, 0.0f));
        ImGuiWindowFlags window_flags =
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar;
        float height = v;
        bool is_open = ImGui::BeginViewportSideBar("##MainMenuBar", viewport, ImGuiDir_Up, height, window_flags);
        g.NextWindowData.MenuBarOffsetMinVal = ImVec2(0.0f, 0.0f);

        if (is_open)
            ImGui::BeginMenuBar();
        else
            ImGui::End();
        return is_open;
    }

    void Titlebar::render()
    {
        ImGuiContext &g = *GImGui;
        f32 target_height = _controlSize.y;
        f32 padding_y = IM_ROUND((target_height - g.FontSize) / 2.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, padding_y));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, style::g_HMenu.padding);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_MenuBarBg, style::g_HMenu.backgroundColor);
        ImGui::PushStyleColor(ImGuiCol_Header, style::g_HMenu.hoverColor);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, style::g_HMenu.hoverColor);
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, style::g_HMenu.backgroundColor);
        if (ImGui::BeginMainMenuBar())
        {
            auto *icon_app = style.icons[IconApp];
            const ImVec2 appPos = ImGui::GetCursorScreenPos() + ImVec2(0, (_controlSize.y - icon_app->size().y) * 0.5f);
            icon_app->render(appPos);

            // Menu
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, style::g_VMenu.margin);
            renderMenuNodes(nodes);
            ImGui::PopStyleVar();
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 5);

            if (ImGui::IsItemActive())
            {
                if (ImGui::IsMouseDragging(ImGuiMouseButton_Left))
                {
                    astl::point2D<i32> cursorPos = _window.cursorPosition();
                    astl::point2D<i32> windowPos = _window.windowPos();
                    astl::point2D<i32> newPos{windowPos.x + cursorPos.x - _dragOffset.x,
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
                ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, {0, 0});
                ImGui::PushStyleColor(ImGuiCol_PopupBg, style.tabBackgroundColor);
                ImGui::PushStyleColor(ImGuiCol_ChildBg, style.tabBackgroundColor);
                ImGui::PushStyleColor(ImGuiCol_Header, style.tabActiveColor);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, style.hoverColor);
                ImGui::PushStyleColor(ImGuiCol_HeaderActive, style.tabActiveColor);
                ImVec2 tabbarSize{ImGui::GetContentRegionAvail().x - _controlSize.x * 3, _controlSize.y + 10};
                tabbar.size(tabbarSize);
                tabbar.render();
                ImGui::PopStyleVar(3);
                ImGui::PopStyleColor(5);

                _clientWidth = ImGui::GetCursorPosX() - tabbar.avaliableWidth();
            }
            _captionWidth = ImGui::GetWindowWidth() - _controlSize.x * 3;
            ImGui::SetCursorPos({_captionWidth, 0});

            renderControls();

            ImGui::EndMainMenuBar();
        }

        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar(3);
    }

    void Titlebar::renderControls()
    {
        // Min
        ImVec2 pos{_captionWidth - 1, 0};
        ImVec2 size = ImVec2(_controlSize.x + 1, _controlSize.y + 1);
        ImGui::GetWindowDrawList()->AddRectFilled(
            pos, ImVec2(pos.x + size.x, pos.y + size.y),
            ImGui::ColorConvertFloat4ToU32(_controls[0].state == ControlState::Hover ? style.hoverColor
                                                                                     : style.tabBackgroundColor));
        pos.x += size.x;
        ImVec2 buttonPos = ImGui::GetCursorScreenPos();
        ImVec2 centerPos = ImVec2(buttonPos.x + _controlSize.x / 2, buttonPos.y + _controlSize.y / 2);
        auto *icon = style.icons[IconMin];
        icon->render(centerPos - icon->size() * 0.5f);
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
            auto *icon = style.icons[IconRestore];
            icon->render(centerPos - icon->size() * 0.5f);
        }
        else
        {
            auto *icon = style.icons[IconMax];
            icon->render(centerPos - icon->size() * 0.5f);
        }

        // Close
        ImGui::GetWindowDrawList()->AddRectFilled(
            pos, ImVec2(pos.x + size.x, pos.y + size.y),
            ImGui::ColorConvertFloat4ToU32(_controls[2].state == ControlState::Hover ? style.closeColor
                                                                                     : style.tabBackgroundColor));
        centerPos.x += _controlSize.x;
        icon = style.icons[IconClose];
        icon->render(centerPos - icon->size() * 0.5f);
    }

    void Titlebar::bindEvents()
    {
        tabbar.bindEvents();
        e->bindEvent(this, "window:NCHitTest", [this](window::Win32NativeEvent &e) {
            if (e.window != &_window) return;
            POINT cursorPoint = {0};
            cursorPoint.x = LOWORD(e.lParam);
            cursorPoint.y = HIWORD(e.lParam);
            ScreenToClient(e.hwnd, &cursorPoint);
            if (cursorPoint.y < _controlSize.y)
            {
                if (cursorPoint.x < _clientWidth)
                    _activeArea = ControlArea::Client;
                else if (cursorPoint.x < _captionWidth)
                {
                    _activeArea = ControlArea::Caption;
                    e.lResult = HTCAPTION;
                }
                else if (cursorPoint.x < _captionWidth + _controlSize.x)
                {
                    _activeArea = ControlArea::Min;
                    if (_controls[0].state != ControlState::Active)
                    {
                        _controls[0].state = ControlState::Hover;
                        e.lResult = HTMINBUTTON;
                    }
                }
                else if (cursorPoint.x < _captionWidth + _controlSize.x * 2)
                {
                    _activeArea = ControlArea::Max;
                    if (_controls[1].state != ControlState::Active)
                    {
                        _controls[1].state = ControlState::Hover;
                        e.lResult = HTMAXBUTTON;
                    }
                }
                else
                {
                    _activeArea = ControlArea::Close;
                    if (_controls[2].state != ControlState::Active)
                    {
                        _controls[2].state = ControlState::Hover;
                        e.lResult = HTCLOSE;
                    }
                }
            }
            else
                _activeArea = ControlArea::None;

            for (auto &control : _controls)
                if (control.area != _activeArea) control.state = ControlState::Idle;
        });
        e->bindEvent(this, "window:NCLMouseDown", [this](window::Win32NativeEvent &e) {
            if (e.window != &_window || _activeArea == ControlArea::None) return;
            switch (_activeArea)
            {
                case ControlArea::Min:
                    _controls[0].state = ControlState::Active;
                    e.lResult = 0;
                    break;
                case ControlArea::Max:
                    _controls[1].state = ControlState::Active;
                    e.lResult = 0;
                    break;
                case ControlArea::Close:
                    _controls[2].state = ControlState::Active;
                    e.lResult = 0;
                    break;
                default:
                    break;
            };
        });
        e->bindEvent(this, "window:input:mouse", [this](const window::MouseClickEvent &e) {
            if (e.window != &_window || _activeArea == ControlArea::None) return;
            switch (_activeArea)
            {
                case ControlArea::Min:
                    if (e.action == window::io::KeyPressState::press)
                        _controls[0].state = ControlState::Active;
                    else
                    {
                        _controls[0].state = ControlState::Hover;
                        _window.minimize();
                    }
                    break;
                case ControlArea::Max:
                    if (e.action == window::io::KeyPressState::press)
                        _controls[1].state = ControlState::Active;
                    else
                    {
                        _controls[1].state = ControlState::Hover;
                        _window.maximize();
                    }
                    break;
                case ControlArea::Close:
                    if (e.action == window::io::KeyPressState::press)
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
} // namespace uikit
