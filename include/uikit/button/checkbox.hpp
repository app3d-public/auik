#pragma once

#include <astl/scalars.hpp>
#include <core/api.hpp>
#include <imgui/imgui.h>

namespace uikit
{
    APPLIB_API bool checkbox(const char *label, bool &value);

    namespace style
    {
        extern APPLIB_API struct CheckBox
        {
            f32 spacing;
            f32 size;
            ImVec4 bg;
            ImVec4 mark;
        } g_CheckBox;

        inline void bindCheckboxStyle() { ImGui::GetStyle().Colors[ImGuiCol_CheckMark] = g_CheckBox.mark; }
    } // namespace style

} // namespace uikit