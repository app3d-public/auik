#pragma once

#include <core/api.hpp>
#include "../style.hpp" // IWYU pragma: keep

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

        template <>
        inline void bindStyle<CheckBox>()
        {
            ImGui::GetStyle().Colors[ImGuiCol_CheckMark] = g_CheckBox.mark;
        }
    } // namespace style

} // namespace uikit