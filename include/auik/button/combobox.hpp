#ifndef AUIK_WIDGETS_COMBOBOX_H
#define AUIK_WIDGETS_COMBOBOX_H

#include <imgui/imgui_internal.h>
#include "../icon/icon.hpp"

namespace auik
{
    namespace style
    {
        extern APPLIB_API struct ComboBox
        {
            Icon *arrowIcon = nullptr;
            ImVec2 itemSpacing;
        } g_combo_box;
    } // namespace style

    APPLIB_API bool begin_combo(const char *label, const char *preview_value, ImGuiComboFlags flags = 0);

    inline void end_combo()
    {
        ImGui::PopStyleVar(2);
        ImGui::EndPopup();
        GImGui->BeginComboDepth--;
    }

} // namespace auik

#endif