#include <imgui/imgui_internal.h>
#include <uikit/button/button.hpp>
#include <uikit/widget.hpp>

namespace uikit
{
    namespace style
    {
        Button g_Button;

        template <>
        void bindStyle<Button>()
        {
            auto &colors = ImGui::GetStyle().Colors;
            colors[ImGuiCol_Button] = g_Button.color;
            colors[ImGuiCol_ButtonActive] = g_Button.colorActive;
            colors[ImGuiCol_ButtonHovered] = g_Button.colorHovered;
        }
    } // namespace style

    bool closeButton(ImGuiID id, const ImVec2 &pos)
    {
        ImGuiContext &g = *GImGui;
        ImGuiWindow *window = g.CurrentWindow;

        // Tweak 1: Shrink hit-testing area if button covers an abnormally large proportion of the visible region.
        // That's in order to facilitate moving the window away. (#3825) This may better be applied as a general
        // hit-rect reduction mechanism for all widgets to ensure the area to move window is always accessible?
        const ImRect bb(pos, pos + ImVec2(g.FontSize, g.FontSize));
        ImRect bb_interact = bb;
        const float area_to_visible_ratio = window->OuterRectClipped.GetArea() / bb.GetArea();
        if (area_to_visible_ratio < 1.5f) bb_interact.Expand(ImTrunc(bb_interact.GetSize() * -0.25f));

        // Tweak 2: We intentionally allow interaction when clipped so that a mechanical Alt,Right,Activate sequence can
        // always close a window. (this isn't the common behavior of buttons, but it doesn't affect the user because
        // navigation tends to keep items visible in scrolling layer).
        bool is_clipped = !ImGui::ItemAdd(bb_interact, id);

        bool hovered, held;
        bool pressed = ImGui::ButtonBehavior(bb_interact, id, &hovered, &held);
        if (is_clipped) return pressed;

        // Render
        // FIXME: Clarify this mess
        ImU32 col = ImGui::GetColorU32(held ? ImGuiCol_HeaderActive : ImGuiCol_HeaderHovered);
        ImVec2 center = bb.GetCenter();
        if (hovered) window->DrawList->AddCircleFilled(center, ImMax(2.0f, g.FontSize * 0.5f + 1.0f), col);

        const float scale = 0.75f;
        float cross_extent = g.FontSize * scale * 0.5f * 0.7071f - 1.0f;
        ImU32 cross_col = ImGui::GetColorU32(ImGuiCol_Text);
        center -= ImVec2(0.5f, 0.5f);
        window->DrawList->AddLine(center + ImVec2(+cross_extent, +cross_extent),
                                  center + ImVec2(-cross_extent, -cross_extent), cross_col, 1.0f);
        window->DrawList->AddLine(center + ImVec2(+cross_extent, -cross_extent),
                                  center + ImVec2(-cross_extent, +cross_extent), cross_col, 1.0f);

        return pressed;
    }

    void rightControls(const astl::vector<std::string> &buttons, int *selected, f32 y_offset)
    {
        ImGui::SetCursorPosY(y_offset == 0.0f ? ImGui::GetCursorPosY() + 10.0f : y_offset);
        auto &bStyle = uikit::style::g_Button;
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, bStyle.padding);
        ImGui::PushStyleColor(ImGuiCol_Button, {0, 0, 0, 0});
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, bStyle.colorActive);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, bStyle.colorHovered);

        f32 offsetX{0};
        auto width = ImGui::GetWindowWidth();
        auto &style = ImGui::GetStyle();

        f32 totalWidth = 0.0f;
        for (int i = 0; i < buttons.size(); ++i)
        {
            f32 btnWidth = ImGui::CalcTextSize(buttons[i].c_str()).x + bStyle.padding.x * 2;
            if (i < buttons.size() - 1) btnWidth += style.ItemSpacing.x;
            totalWidth += btnWidth;
        }

        f32 startX = width - totalWidth - style.WindowPadding.x;
        ImGui::SetCursorPosX(startX);

        for (int i = 0; i < buttons.size(); ++i)
        {
            bool isLast = i == buttons.size() - 1;
            if (isLast) ImGui::PushStyleColor(ImGuiCol_Button, bStyle.color);
            if (ImGui::Button(buttons[i].c_str()) && selected) *selected = i;
            if (isLast) ImGui::PopStyleColor();
            if (i < buttons.size() - 1) ImGui::SameLine();
        }

        ImGui::SetCursorPosY(ImGui::GetCursorPosY());
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);
    }
} // namespace uikit