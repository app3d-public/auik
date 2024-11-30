#pragma once

#include "../widget.hpp"

namespace uikit
{
    class Window : public Widget
    {
    public:
        ImGuiWindowFlags flags;
        bool isDocked;
        bool isDockStretched;

        Window(const std::string &name, ImGuiWindowFlags flags = 0, bool isDockStretched = true, bool docked = false)
            : Widget(name), flags(flags), isDocked(docked), isDockStretched(isDockStretched)
        {
        }

        virtual void render() override
        {
            if (isDocked)
                renderImpl();
            else
            {
                ImGui::Begin(name.c_str(), nullptr, flags);
                renderImpl();
                ImGui::End();
            }
        }

    protected:
        virtual void renderImpl() = 0;
    };
} // namespace uikit