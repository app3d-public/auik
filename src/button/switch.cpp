#include <imgui/imgui_internal.h>
#include <uikit/button/switch.hpp>

namespace uikit
{
    style::Switch style::switchStyle;

    void Switch::render()
    {
        ImGuiWindow *window = ImGui::GetCurrentWindow();
        if (window->SkipItems) return;

        ImGuiID id = window->GetID(_name.c_str());
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImGuiContext &g = *GImGui;
        ImRect bb(p, p + ImVec2(width, height));
        bool held;
        ImGuiButtonFlags buttonFlags = 0;
        buttonFlags |= ImGuiButtonFlags_NoSetKeyOwner;
        buttonFlags |= ImGuiButtonFlags_PressedOnClick;
        _pressed = ImGui::ButtonBehavior(bb, id, &_hovered, &held, buttonFlags);

        if (_pressed)
        {
            _toggled = !_toggled;
            ImGui::MarkItemEdited(id);
        }

        ImU32 col = ImGui::GetColorU32(_toggled ? style::switchStyle.bgColorActive : style::switchStyle.bgColor);
        ImDrawList *draw_list = ImGui::GetWindowDrawList();
        draw_list->AddRectFilled(bb.Min, bb.Max, col, height);

        f32 circle_radius = height / 2;
        ImVec2 circle_center =
            _toggled ? bb.Max - ImVec2(circle_radius, circle_radius) : bb.Min + ImVec2(circle_radius, circle_radius);
        draw_list->AddCircleFilled(circle_center, circle_radius, style::switchStyle.switchColor);

        const ImVec2 label_size = ImGui::CalcTextSize(_name.c_str(), NULL, true);
        ImVec2 label_pos = ImVec2(bb.Max.x + g.Style.ItemSpacing.x + g.Style.ItemInnerSpacing.x, bb.Min.y);
        if (label_size.x > 0.0f) ImGui::RenderText(label_pos, _name.c_str());
    }

} // namespace uikit