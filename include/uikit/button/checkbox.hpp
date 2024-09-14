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
        } *g_StyleCheckBox;

        template <>
        inline void registerStyle<CheckBox>(CheckBox *style)
        {
            g_StyleCheckBox = style;
            ImGui::GetStyle().Colors[ImGuiCol_CheckMark] = style->mark;
        }
    } // namespace style

} // namespace uikit