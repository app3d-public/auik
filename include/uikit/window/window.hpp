#pragma once

#include <imgui/imgui_internal.h>
#include "../widget.hpp"

namespace uikit
{
    enum WindowDockFlags_
    {
        WindowDockFlags_Docked = 1 << 10,
        WindowDockFlags_TabMenu = 1 << 11,
        WindowDockFlags_Stretch = 1 << 12,
        WindowDockFlags_NoDock = 1 << 13
    };

    typedef int WindowDockFlags;

    class Window : public Widget
    {
    public:
        ImGuiWindowFlags imgui_flags;
        WindowDockFlags dock_flags;

        Window(const acul::string &name, WindowDockFlags dock_flags = 0, ImGuiWindowFlags imgui_flags = 0)
            : Widget(name), imgui_flags(imgui_flags), dock_flags(dock_flags)
        {
        }

        virtual void render() override
        {
            if (dock_flags & WindowDockFlags_Docked)
                render_impl();
            else
            {
                auto &style = ImGui::GetStyle();
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(style.WindowPadding.x, 5));
                ImGui::Begin(name.c_str(), nullptr, imgui_flags);
                ImGui::PopStyleVar();
                auto *window = ImGui::GetCurrentWindow();
                window->ChildFlags = dock_flags;
                render_impl();
                ImGui::End();
            }
        }

        virtual void update_style_stack() {};

    protected:
        virtual void render_impl() = 0;
    };
} // namespace uikit