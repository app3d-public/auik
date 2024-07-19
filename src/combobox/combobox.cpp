#include <uikit/combobox/combobox.hpp>
#include <uikit/icon/icon.hpp>
#include <imgui/imgui_internal.h>

namespace uikit
{
    bool beginCombo(const char *label, const char *preview_value, ImGuiComboFlags flags,
                    const std::shared_ptr<Icon> chevronDownIcon)
    {
        ImGuiContext &g = *GImGui;
        ImGuiWindow *window = ImGui::GetCurrentWindow();

        ImGuiNextWindowDataFlags backup_next_window_data_flags = g.NextWindowData.Flags;
        g.NextWindowData.ClearFlags(); // We behave like Begin() and need to consume those values
        if (window->SkipItems)
            return false;

        const ImGuiStyle &style = g.Style;
        const ImGuiID id = window->GetID(label);
        IM_ASSERT((flags & (ImGuiComboFlags_NoArrowButton | ImGuiComboFlags_NoPreview)) !=
                  (ImGuiComboFlags_NoArrowButton | ImGuiComboFlags_NoPreview)); // Can't use both flags together
        if (flags & ImGuiComboFlags_WidthFitPreview)
            IM_ASSERT((flags & (ImGuiComboFlags_NoPreview | (ImGuiComboFlags)ImGuiComboFlags_CustomPreview)) == 0);

        const float arrow_size = (flags & ImGuiComboFlags_NoArrowButton) ? 0.0f : ImGui::GetFrameHeight();
        const ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);
        const float preview_width = ((flags & ImGuiComboFlags_WidthFitPreview) && (preview_value != NULL))
                                        ? ImGui::CalcTextSize(preview_value, NULL, true).x
                                        : 0.0f;
        const float w = (flags & ImGuiComboFlags_NoPreview)
                            ? arrow_size
                            : ((flags & ImGuiComboFlags_WidthFitPreview)
                                   ? (arrow_size + preview_width + style.FramePadding.x * 2.0f)
                                   : ImGui::CalcItemWidth());
        const ImRect bb(window->DC.CursorPos,
                        window->DC.CursorPos + ImVec2(w, label_size.y + style.FramePadding.y * 2.0f));
        const ImRect total_bb(
            bb.Min, bb.Max + ImVec2(label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f, 0.0f));
        ImGui::ItemSize(total_bb, style.FramePadding.y);
        if (!ImGui::ItemAdd(total_bb, id, &bb))
            return false;

        // Open on click
        bool hovered, held;
        bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);
        const ImGuiID popup_id = ImHashStr("##ComboPopup", 0, id);
        bool popup_open = ImGui::IsPopupOpen(popup_id, ImGuiPopupFlags_None);
        if (pressed && !popup_open)
        {
            ImGui::OpenPopupEx(popup_id, ImGuiPopupFlags_None);
            popup_open = true;
        }

        // Render shape
        const ImU32 frame_col = ImGui::GetColorU32(hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg);
        const float value_x2 = ImMax(bb.Min.x, bb.Max.x - arrow_size);
        ImGui::RenderNavHighlight(bb, id);
        if (!(flags & ImGuiComboFlags_NoPreview))
            window->DrawList->AddRectFilled(bb.Min, ImVec2(value_x2, bb.Max.y), frame_col, style.FrameRounding,
                                            (flags & ImGuiComboFlags_NoArrowButton) ? ImDrawFlags_RoundCornersAll
                                                                                    : ImDrawFlags_RoundCornersLeft);
        if (!(flags & ImGuiComboFlags_NoArrowButton))
        {
            ImU32 bg_col = ImGui::GetColorU32((hovered) ? ImGuiCol_ButtonHovered : ImGuiCol_Button);
            ImU32 text_col = ImGui::GetColorU32(ImGuiCol_Text);
            window->DrawList->AddRectFilled(ImVec2(value_x2, bb.Min.y), bb.Max, bg_col, style.FrameRounding,
                                            (w <= arrow_size) ? ImDrawFlags_RoundCornersAll
                                                              : ImDrawFlags_RoundCornersRight);
            if (value_x2 + arrow_size - style.FramePadding.x <= bb.Max.x)
            {
                if (chevronDownIcon)
                    chevronDownIcon->render(ImVec2(value_x2 + style.FramePadding.y, bb.Min.y + style.FramePadding.y));
                else
                    ImGui::RenderArrow(window->DrawList,
                                       ImVec2(value_x2 + style.FramePadding.y, bb.Min.y + style.FramePadding.y),
                                       text_col, ImGuiDir_Down, 1.0f);
            }
        }
        ImGui::RenderFrameBorder(bb.Min, bb.Max, style.FrameRounding);

        // Custom preview
        if (flags & ImGuiComboFlags_CustomPreview)
        {
            g.ComboPreviewData.PreviewRect = ImRect(bb.Min.x, bb.Min.y, value_x2, bb.Max.y);
            IM_ASSERT(preview_value == NULL || preview_value[0] == 0);
            preview_value = NULL;
        }

        // Render preview and label
        if (preview_value != NULL && !(flags & ImGuiComboFlags_NoPreview))
        {
            if (g.LogEnabled)
                ImGui::LogSetNextTextDecoration("{", "}");
            ImGui::RenderTextClipped(bb.Min +
                                         ImVec2{style.FramePadding.x + style.ItemInnerSpacing.x, style.FramePadding.y},
                                     ImVec2(value_x2, bb.Max.y), preview_value, NULL, NULL);
        }
        if (label_size.x > 0)
            ImGui::RenderText(ImVec2(bb.Max.x + style.ItemInnerSpacing.x, bb.Min.y + style.FramePadding.y), label);

        if (!popup_open)
            return false;

        g.NextWindowData.Flags = backup_next_window_data_flags;
        return ImGui::BeginComboPopup(popup_id, bb, flags);
    }
} // namespace ui