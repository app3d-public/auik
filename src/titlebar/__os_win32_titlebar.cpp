#include <acul/log.hpp>
#include <dwmapi.h>
#include <auik/titlebar/titlebar_win32.hpp>

namespace auik
{
    ImVec2 get_controls_size(HWND hwnd)
    {
        ImVec2 control_size;
        BOOL is_composition_enabled = FALSE;
        HRESULT hr = DwmIsCompositionEnabled(&is_composition_enabled);
        if (SUCCEEDED(hr) && is_composition_enabled)
        {
            RECT rect;
            hr = DwmGetWindowAttribute(hwnd, DWMWA_CAPTION_BUTTON_BOUNDS, &rect, sizeof(rect));
            if (SUCCEEDED(hr))
            {
                control_size.x = (rect.right - rect.left) / 3.0f * awin::get_dpi();
                control_size.y = (rect.bottom - rect.top) * awin::get_dpi();
                return control_size;
            }
        }
        LOG_WARN("Failed to get DWMWA_CAPTION_BUTTON_BOUNDS");
        control_size.x = GetSystemMetrics(SM_CXSIZE) * awin::get_dpi();
        control_size.y = GetSystemMetrics(SM_CYSIZE) * awin::get_dpi();
        return control_size;
    }

    Titlebar::Titlebar(awin::Window &window, acul::events::dispatcher *ed, MenuBar *menubar, const TabBar &tabbar,
                       const Style &style)
        : MenuBar(std::move(*menubar), "titlebar"),
          style(style),
          tabbar(tabbar),
          _window(window),
          ed(ed),
          _control_size(get_controls_size(awin::platform::native_access::get_hwnd(window))),
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
        f32 target_height = _control_size.y;
        f32 padding_y = IM_ROUND((target_height - g.FontSize) / 2.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, padding_y));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, style::g_hmenu.padding);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_MenuBarBg, style::g_hmenu.background_color);
        ImGui::PushStyleColor(ImGuiCol_Header, style::g_hmenu.hover_color);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, style::g_hmenu.hover_color);
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, style::g_hmenu.background_color);
        if (ImGui::BeginMainMenuBar())
        {
            auto *icon_app = style.icons[IconApp];
            const ImVec2 app_pos =
                ImGui::GetCursorScreenPos() + ImVec2(0, (_control_size.y - icon_app->size().y) * 0.5f);
            icon_app->render(app_pos);

            // Menu
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, style::g_vmenu.margin);
            render_menu_nodes(nodes);
            ImGui::PopStyleVar();
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 5);

            if (ImGui::IsItemActive())
            {
                if (ImGui::IsMouseDragging(ImGuiMouseButton_Left))
                {
                    acul::point2D<i32> cursorPos = _window.cursor_position();
                    acul::point2D<i32> windowPos = _window.position();
                    acul::point2D<i32> newPos{windowPos.x + cursorPos.x - _drag_offset.x,
                                              windowPos.y + cursorPos.y - _drag_offset.y};
                    _window.position(newPos);
                }
                else
                    _drag_offset = _window.cursor_position();
            }

            // Tabs
            _client_width = ImGui::GetContentRegionAvail().x - _control_size.x * 3;
            if (_client_width > 100)
            {
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
                ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, {0, 0});
                ImGui::PushStyleColor(ImGuiCol_PopupBg, style.tab_background_color);
                ImGui::PushStyleColor(ImGuiCol_ChildBg, style.tab_background_color);
                ImGui::PushStyleColor(ImGuiCol_Header, style.tab_active_color);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, style.hover_color);
                ImGui::PushStyleColor(ImGuiCol_HeaderActive, style.tab_active_color);
                ImVec2 tabbarSize{ImGui::GetContentRegionAvail().x - _control_size.x * 3, _control_size.y + 10};
                tabbar.size(tabbarSize);
                tabbar.render();
                ImGui::PopStyleVar(3);
                ImGui::PopStyleColor(5);

                _client_width = ImGui::GetCursorPosX() - tabbar.avaliable_width();
            }
            _caption_width = ImGui::GetWindowWidth() - _control_size.x * 3;
            ImGui::SetCursorPos({_caption_width, 0});

            render_controls();
            ImGui::EndMainMenuBar();
        }

        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar(3);
    }

    void Titlebar::render_controls()
    {
        // Min
        ImVec2 pos{_caption_width - 1, 0};
        ImVec2 size = ImVec2(_control_size.x + 1, _control_size.y + 1);
        ImGui::GetWindowDrawList()->AddRectFilled(
            pos, ImVec2(pos.x + size.x, pos.y + size.y),
            ImGui::ColorConvertFloat4ToU32(_controls[0].state == ControlState::Hover ? style.hover_color
                                                                                     : style.tab_background_color));
        pos.x += size.x;
        ImVec2 button_pos = ImGui::GetCursorScreenPos();
        ImVec2 center_pos = ImVec2(button_pos.x + _control_size.x / 2, button_pos.y + _control_size.y / 2);
        auto *icon = style.icons[IconMin];
        icon->render(center_pos - icon->size() * 0.5f);
        ImGui::SameLine();

        // Max
        ImGui::GetWindowDrawList()->AddRectFilled(
            pos, ImVec2(pos.x + size.x, pos.y + size.y),
            ImGui::ColorConvertFloat4ToU32(_controls[1].state == ControlState::Hover ? style.hover_color
                                                                                     : style.tab_background_color));
        pos.x += size.x;
        center_pos.x += _control_size.x;
        if (_window.maximized())
        {
            auto *icon = style.icons[IconRestore];
            icon->render(center_pos - icon->size() * 0.5f);
        }
        else
        {
            auto *icon = style.icons[IconMax];
            icon->render(center_pos - icon->size() * 0.5f);
        }

        // Close
        ImGui::GetWindowDrawList()->AddRectFilled(
            pos, ImVec2(pos.x + size.x, pos.y + size.y),
            ImGui::ColorConvertFloat4ToU32(_controls[2].state == ControlState::Hover ? style.close_color
                                                                                     : style.tab_background_color));
        center_pos.x += _control_size.x;
        icon = style.icons[IconClose];
        icon->render(center_pos - icon->size() * 0.5f);
    }

    void Titlebar::bind_events()
    {
        tabbar.bind_events();
        ed->bind_event(this, awin::event_id::NCHitTest, [this](awin::Win32NativeEvent &e) {
            if (e.window != &_window) return;
            POINT cursorPoint = {0};
            cursorPoint.x = LOWORD(e.lParam);
            cursorPoint.y = HIWORD(e.lParam);
            ScreenToClient(e.hwnd, &cursorPoint);
            if (cursorPoint.y < _control_size.y)
            {
                if (cursorPoint.x < _client_width)
                    _active_area = ControlArea::Client;
                else if (cursorPoint.x < _caption_width)
                {
                    _active_area = ControlArea::Caption;
                    e.lResult = HTCAPTION;
                }
                else if (cursorPoint.x < _caption_width + _control_size.x)
                {
                    _active_area = ControlArea::Min;
                    if (_controls[0].state != ControlState::Active)
                    {
                        _controls[0].state = ControlState::Hover;
                        e.lResult = HTMINBUTTON;
                    }
                }
                else if (cursorPoint.x < _caption_width + _control_size.x * 2)
                {
                    _active_area = ControlArea::Max;
                    if (_controls[1].state != ControlState::Active)
                    {
                        _controls[1].state = ControlState::Hover;
                        e.lResult = HTMAXBUTTON;
                    }
                }
                else
                {
                    _active_area = ControlArea::Close;
                    if (_controls[2].state != ControlState::Active)
                    {
                        _controls[2].state = ControlState::Hover;
                        e.lResult = HTCLOSE;
                    }
                }
            }
            else
                _active_area = ControlArea::None;

            for (auto &control : _controls)
                if (control.area != _active_area) control.state = ControlState::Idle;
        });
        ed->bind_event(this, awin::event_id::NCMouseDown, [this](awin::Win32NativeEvent &e) {
            if (e.window != &_window || _active_area == ControlArea::None) return;
            switch (_active_area)
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
        ed->bind_event(this, awin::event_id::MouseClick, [this](const awin::MouseClickEvent &e) {
            if (e.window != &_window || _active_area == ControlArea::None) return;
            switch (_active_area)
            {
                case ControlArea::Min:
                    if (e.action == awin::io::KeyPressState::Press)
                        _controls[0].state = ControlState::Active;
                    else
                    {
                        _controls[0].state = ControlState::Hover;
                        _window.minimize();
                    }
                    break;
                case ControlArea::Max:
                    if (e.action == awin::io::KeyPressState::Press)
                        _controls[1].state = ControlState::Active;
                    else
                    {
                        _controls[1].state = ControlState::Hover;
                        _window.maximize();
                    }
                    break;
                case ControlArea::Close:
                    if (e.action == awin::io::KeyPressState::Press)
                        _controls[2].state = ControlState::Active;
                    else
                    {
                        _controls[2].state = ControlState::Hover;
                        _window.ready_to_close(true);
                    }
                default:
                    break;
            }
        });
    }
} // namespace auik
