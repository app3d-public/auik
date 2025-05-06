#include <imgui/imgui_internal.h>
#include <uikit/button/switch.hpp>

namespace uikit
{
    namespace style
    {
        Switch g_switch;
    } // namespace style

    void Switch::render()
    {
        ImGuiWindow *window = ImGui::GetCurrentWindow();
        if (window->SkipItems) return;

        ImGuiID id = window->GetID(name.c_str());
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImGuiContext &g = *GImGui;

        f32 frame_height = ImGui::GetFrameHeight();
        f32 circle_radius = height / 2;
        f32 circle_diameter = height;
        f32 height_diff = (frame_height > circle_diameter) ? (frame_height - circle_diameter) / 2.0f : 0.0f;
        ImRect bb(p + ImVec2(0, height_diff), p + ImVec2(width, circle_diameter + height_diff));

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

        ImU32 col = ImGui::GetColorU32(_toggled ? style::g_switch.colorActive : style::g_switch.bg);
        ImDrawList *draw_list = ImGui::GetWindowDrawList();
        draw_list->AddRectFilled(bb.Min, bb.Max, col, circle_radius);

        ImVec2 circle_center =
            _toggled ? bb.Max - ImVec2(circle_radius, circle_radius) : bb.Min + ImVec2(circle_radius, circle_radius);
        draw_list->AddCircleFilled(circle_center, circle_radius, style::g_switch.color);
        ImGui::ItemSize(bb);

        const ImVec2 label_size = ImGui::CalcTextSize(name.c_str(), NULL, true);
        if (label_size.x > 0.0f)
        {
            f32 text_vertical_offset = (bb.GetHeight() - label_size.y) / 2.0f;
            ImVec2 label_pos{bb.Max.x + g.Style.ItemSpacing.x + g.Style.ItemInnerSpacing.x,
                             bb.Min.y + text_vertical_offset};
            ImGui::RenderText(label_pos, name.c_str());
        }
    }

} // namespace uikit