#pragma once

#include <acul/scalars.hpp>
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
        } g_check_box;

        inline void bind_checkbox_style() { ImGui::GetStyle().Colors[ImGuiCol_CheckMark] = g_check_box.mark; }
    } // namespace style

} // namespace uikit