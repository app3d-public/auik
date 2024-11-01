#ifndef UIKIT_WIDGETS_BUTTON_H
#define UIKIT_WIDGETS_BUTTON_H

#include <core/api.hpp>
#include <astl/vector.hpp>
#include <imgui/imgui.h>
#include "../style.hpp" // IWYU pragma: keep

namespace uikit
{
    namespace style
    {
        extern APPLIB_API struct Button
        {
            ImVec4 color;
            ImVec4 colorActive;
            ImVec4 colorHovered;
            ImVec2 padding;
        } g_Button;

        template <>
        APPLIB_API void bindStyle<Button>();
    } // namespace style

    // Button to close a window
    bool closeButton(ImGuiID id, const ImVec2 &pos);

    // Control buttons (Yes/No/Canel/etc)
    APPLIB_API void rightControls(const astl::vector<std::string> &buttons, int *selected = nullptr, f32 y_offset = 0);
} // namespace uikit

#endif