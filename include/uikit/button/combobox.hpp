#ifndef UIKIT_WIDGETS_COMBOBOX_H
#define UIKIT_WIDGETS_COMBOBOX_H

#include <imgui/imgui_internal.h>
#include "../icon/icon.hpp"
#include "../style.hpp" // IWYU pragma: keep

namespace uikit
{
    namespace style
    {
        extern APPLIB_API struct ComboBox
        {
            Icon *arrowIcon = nullptr;
            ImVec2 itemSpacing;
        } *g_StyleComboBox;

        template <>
        inline void registerStyle(ComboBox *style)
        {
            g_StyleComboBox = style;
        }
    } // namespace style

    APPLIB_API bool beginCombo(const char *label, const char *preview_value, ImGuiComboFlags flags = 0);

    inline void endCombo()
    {
        ImGui::PopStyleVar(2);
        ImGui::EndPopup();
        GImGui->BeginComboDepth--;
    }

} // namespace uikit

#endif