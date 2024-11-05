#include <uikit/utils.hpp>
#include <uikit/widget.hpp>

namespace uikit
{
    void renderFrame(ImVec2 p_min, ImVec2 p_max, ImU32 fill_col, bool borders, float rounding, ImDrawFlags flags)
    {
        ImGuiContext &g = *GImGui;
        ImGuiWindow *window = g.CurrentWindow;
        window->DrawList->AddRectFilled(p_min, p_max, fill_col, rounding, flags);
        const float border_size = g.Style.FrameBorderSize;
        if (borders && border_size > 0.0f)
        {
            window->DrawList->AddRect(p_min + ImVec2(1, 1), p_max + ImVec2(1, 1),
                                      ImGui::GetColorU32(ImGuiCol_BorderShadow), rounding, flags, border_size);
            window->DrawList->AddRect(p_min, p_max, ImGui::GetColorU32(ImGuiCol_Border), rounding, flags, border_size);
        }
    }
}