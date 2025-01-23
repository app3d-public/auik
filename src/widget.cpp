#include <imgui/imgui_internal.h>
#include <uikit/widget.hpp>
#include <window/window.hpp>

namespace uikit
{
    ImGuiMouseCursor g_Last_cursor = ImGuiMouseCursor_None;

    void renderFrame(ImVec2 p_min, ImVec2 p_max, ImU32 fill_col, bool borders, f32 rounding, ImDrawFlags flags)
    {
        ImGuiContext &g = *GImGui;
        ImGuiWindow *window = g.CurrentWindow;
        window->DrawList->AddRectFilled(p_min, p_max, fill_col, rounding, flags);
        const f32 border_size = g.Style.FrameBorderSize;
        if (borders && border_size > 0.0f)
        {
            window->DrawList->AddRect(p_min + ImVec2(1, 1), p_max + ImVec2(1, 1),
                                      ImGui::GetColorU32(ImGuiCol_BorderShadow), rounding, flags, border_size);
            window->DrawList->AddRect(p_min, p_max, ImGui::GetColorU32(ImGuiCol_Border), rounding, flags, border_size);
        }
    }
} // namespace uikit