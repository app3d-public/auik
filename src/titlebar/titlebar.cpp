#include <auik/titlebar/titlebar.hpp>
#ifdef _WIN32
    #include <acul/log.hpp>
    #include <dwmapi.h>
#endif

namespace auik
{
#ifdef _WIN32
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

    void bind_events(acul::events::dispatcher *ed, awin::Window &window, Titlebar *self)
    {
        auto *decorator = self->_decorator;
        using ControlArea = Titlebar::Decorator::ControlArea;
        using ControlState = Titlebar::Decorator::ControlState;

        ed->bind_event(self, awin::event_id::NCHitTest, [&window, self, decorator](awin::Win32NativeEvent &e) {
            if (e.window != &window) return;
            POINT cursor_point = {0};
            cursor_point.x = LOWORD(e.lParam);
            cursor_point.y = HIWORD(e.lParam);
            ScreenToClient(e.hwnd, &cursor_point);
            if (cursor_point.y < decorator->_control_size.y)
            {
                if (cursor_point.x < self->_client_width)
                    decorator->_active_area = ControlArea::Client;
                else if (cursor_point.x < self->_caption_width)
                {
                    decorator->_active_area = ControlArea::Caption;
                    e.lResult = HTCAPTION;
                }
                else if (cursor_point.x < self->_caption_width + decorator->_control_size.x)
                {
                    decorator->_active_area = ControlArea::Min;
                    if (decorator->_controls[0].state != ControlState::Active)
                    {
                        decorator->_controls[0].state = ControlState::Hover;
                        e.lResult = HTMINBUTTON;
                    }
                }
                else if (cursor_point.x < self->_caption_width + decorator->_control_size.x * 2)
                {
                    decorator->_active_area = ControlArea::Max;
                    if (decorator->_controls[1].state != ControlState::Active)
                    {
                        decorator->_controls[1].state = ControlState::Hover;
                        e.lResult = HTMAXBUTTON;
                    }
                }
                else
                {
                    decorator->_active_area = ControlArea::Close;
                    if (decorator->_controls[2].state != ControlState::Active)
                    {
                        decorator->_controls[2].state = ControlState::Hover;
                        e.lResult = HTCLOSE;
                    }
                }
            }
            else
                decorator->_active_area = ControlArea::None;

            for (auto &control : decorator->_controls)
                if (control.area != decorator->_active_area) control.state = ControlState::Idle;
        });
        ed->bind_event(self, awin::event_id::NCMouseDown, [&window, decorator](awin::Win32NativeEvent &e) {
            if (e.window != &window || decorator->_active_area == ControlArea::None) return;
            switch (decorator->_active_area)
            {
                case ControlArea::Min:
                    decorator->_controls[0].state = ControlState::Active;
                    e.lResult = 0;
                    break;
                case ControlArea::Max:
                    decorator->_controls[1].state = ControlState::Active;
                    e.lResult = 0;
                    break;
                case ControlArea::Close:
                    decorator->_controls[2].state = ControlState::Active;
                    e.lResult = 0;
                    break;
                default:
                    break;
            };
        });
        ed->bind_event(self, awin::event_id::MouseClick, [decorator, &window](const awin::MouseClickEvent &e) {
            if (e.window != &window || decorator->_active_area == ControlArea::None) return;
            switch (decorator->_active_area)
            {
                case ControlArea::Min:
                    if (e.action == awin::io::KeyPressState::Press)
                        decorator->_controls[0].state = ControlState::Active;
                    else
                    {
                        decorator->_controls[0].state = ControlState::Hover;
                        window.minimize();
                    }
                    break;
                case ControlArea::Max:
                    if (e.action == awin::io::KeyPressState::Press)
                        decorator->_controls[1].state = ControlState::Active;
                    else
                    {
                        decorator->_controls[1].state = ControlState::Hover;
                        window.maximize();
                    }
                    break;
                case ControlArea::Close:
                    if (e.action == awin::io::KeyPressState::Press)
                        decorator->_controls[2].state = ControlState::Active;
                    else
                    {
                        decorator->_controls[2].state = ControlState::Hover;
                        window.ready_to_close(true);
                    }
                default:
                    break;
            }
        });
    }
#endif

    Titlebar::Titlebar(awin::Window &window, MenuBar *menubar, const TabBar &tabbar)
        : MenuBar(std::move(*menubar), "titlebar"),
          tabbar(tabbar),
          _window(window),
#ifdef _WIN32
          _decorator(acul::alloc<Decorator>(get_controls_size(awin::platform::native_access::get_hwnd(window)),
                                            auik::bind_events))
#else
          _decorator(nullptr)
#endif

    {
    }

    void TitlebarDecorator::render_controls(awin::Window &window, const TitlebarStyle &style, f32 caption_width)
    {
        // Min
        ImVec2 pos{caption_width - 1, 0};
        ImVec2 size = ImVec2(_control_size.x + 1, _control_size.y + 1);
        ImGui::GetWindowDrawList()->AddRectFilled(
            pos, ImVec2(pos.x + size.x, pos.y + size.y),
            ImGui::ColorConvertFloat4ToU32(_controls[0].state == ControlState::Hover ? style.hover_color
                                                                                     : style.tab_background_color));
        pos.x += size.x;
        ImVec2 button_pos = ImGui::GetCursorScreenPos();
        ImVec2 center_pos = ImVec2(button_pos.x + _control_size.x / 2, button_pos.y + _control_size.y / 2);
        auto *icon = this->style.icons[IconMin];
        icon->render(center_pos - icon->size() * 0.5f);
        ImGui::SameLine();

        // Max
        ImGui::GetWindowDrawList()->AddRectFilled(
            pos, ImVec2(pos.x + size.x, pos.y + size.y),
            ImGui::ColorConvertFloat4ToU32(_controls[1].state == ControlState::Hover ? style.hover_color
                                                                                     : style.tab_background_color));
        pos.x += size.x;
        center_pos.x += _control_size.x;
        icon = this->style.icons[window.maximized() ? IconRestore : IconMax];
        icon->render(center_pos - icon->size() * 0.5f);

        // Close
        ImGui::GetWindowDrawList()->AddRectFilled(
            pos, ImVec2(pos.x + size.x, pos.y + size.y),
            ImGui::ColorConvertFloat4ToU32(_controls[2].state == ControlState::Hover ? this->style.close_color
                                                                                     : style.tab_background_color));
        center_pos.x += _control_size.x;
        icon = this->style.icons[IconClose];
        icon->render(center_pos - icon->size() * 0.5f);
    }

    void Titlebar::render()
    {
        ImGuiContext &g = *GImGui;
        f32 control_width = 0.0f, target_height;
        if (_decorator)
        {
            auto control_size = _decorator->control_size();
            control_width = control_size.x;
            target_height = control_size.y;
        }
        else
            target_height = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 4.0f;
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
            // App Icon
            if (_decorator) _decorator->render_app_icon();

            // Menu
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, style::g_vmenu.margin);
            render_menu_nodes(nodes);
            ImGui::PopStyleVar();
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 5);

            if (_decorator && ImGui::IsItemActive()) _decorator->process_drag(_window);

            // Tabs
            _client_width = ImGui::GetContentRegionAvail().x - control_width * 3;
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
                ImVec2 tabbar_size{ImGui::GetContentRegionAvail().x - control_width * 3, target_height + 10};
                tabbar.size(tabbar_size);
                tabbar.render();
                ImGui::PopStyleVar(3);
                ImGui::PopStyleColor(5);

                _client_width = ImGui::GetCursorPosX() - tabbar.avaliable_width();
            }
            _caption_width = ImGui::GetWindowWidth() - control_width * 3;
            ImGui::SetCursorPos({_caption_width, 0});

            if (_decorator) _decorator->render_controls(_window, style, _caption_width);
            ImGui::EndMainMenuBar();
        }

        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar(3);
    }
} // namespace auik