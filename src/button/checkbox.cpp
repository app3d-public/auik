#include <imgui/imgui_internal.h>
#include <uikit/button/checkbox.hpp>
#include <uikit/widget.hpp>

namespace uikit
{
    namespace style
    {
        CheckBox *g_StyleCheckBox = nullptr;
    } // namespace style

    APPLIB_API bool checkbox(const char *label, bool &value)
    {
        ImGuiWindow *window = ImGui::GetCurrentWindow();
        if (window->SkipItems) return false;

        ImGuiContext &g = *GImGui;
        const ImGuiStyle &style = g.Style;
        const ImGuiID id = window->GetID(label);
        const ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);

        const f32 square_sz = style::g_StyleCheckBox->size;
        const f32 frame_height = ImGui::GetFrameHeight();
        const f32 vertical_offset = (frame_height - square_sz) * 0.5f;

        const ImVec2 pos = window->DC.CursorPos;
        const ImRect total_bb(
            pos + ImVec2(0, vertical_offset), // Смещаем чекбокс по вертикали для центровки
            pos + ImVec2(square_sz + (label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f),
                         label_size.y + style.FramePadding.y * 2.0f));
        ImGui::ItemSize(total_bb, style.FramePadding.y);

        const bool is_visible = ImGui::ItemAdd(total_bb, id);
        const bool is_multi_select = (g.LastItemData.InFlags & ImGuiItemFlags_IsMultiSelect) != 0;

        bool hovered, held;
        bool pressed = ImGui::ButtonBehavior(total_bb, id, &hovered, &held);

        if (pressed) value = !value;

        const ImRect check_bb(pos + ImVec2(0, vertical_offset),
                              pos + ImVec2(square_sz, square_sz) + ImVec2(0, vertical_offset));

        if (is_visible)
        {
            ImGui::RenderNavHighlight(total_bb, id);
            ImU32 color;
            if (hovered)
                color = ImGui::GetColorU32(held ? ImGuiCol_FrameBgActive : ImGuiCol_FrameBgHovered);
            else if (value)
                color = ImGui::ColorConvertFloat4ToU32(style::g_StyleCheckBox->bg);
            else
                color = ImGui::GetColorU32(ImGuiCol_FrameBg);
            ImGui::RenderFrame(check_bb.Min, check_bb.Max, color, true, style.FrameRounding);

            if (value)
            {
                ImU32 check_col = ImGui::ColorConvertFloat4ToU32(style::g_StyleCheckBox->mark);
                const f32 pad = ImMax(1.0f, IM_TRUNC(square_sz / 6.0f));
                ImGui::RenderCheckMark(window->DrawList, check_bb.Min + ImVec2(pad, pad), check_col,
                                       square_sz - pad * 2.0f);
            }
        }

        const ImVec2 label_pos = ImVec2(check_bb.Max.x + style.ItemInnerSpacing.x, pos.y + vertical_offset);
        if (is_visible && label_size.x > 0.0f) ImGui::RenderText(label_pos, label);

        return pressed;
    }

} // namespace uikit