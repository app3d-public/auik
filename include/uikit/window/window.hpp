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
        ImGuiWindowFlags imguiFlags;
        WindowDockFlags dockFlags;

        Window(const acul::string &name, WindowDockFlags dockFlags = 0, ImGuiWindowFlags imguiFlags = 0)
            : Widget(name), imguiFlags(imguiFlags), dockFlags(dockFlags)
        {
        }

        virtual void render() override
        {
            if (dockFlags & WindowDockFlags_Docked)
                renderImpl();
            else
            {
                auto &style = ImGui::GetStyle();
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(style.WindowPadding.x, 5));
                ImGui::Begin(name.c_str(), nullptr, imguiFlags);
                ImGui::PopStyleVar();
                auto *window = ImGui::GetCurrentWindow();
                window->ChildFlags = dockFlags;
                renderImpl();
                ImGui::End();
            }
        }

        virtual void updateStyleStack() {};

    protected:
        virtual void renderImpl() = 0;
    };
} // namespace uikit