#ifndef UIKIT_WIDGETS_BUTTON_H
#define UIKIT_WIDGETS_BUTTON_H

#include <acul/vector.hpp>
#include "../icon/icon.hpp"

namespace uikit
{
    namespace style
    {
        extern APPLIB_API struct Button
        {
            ImVec4 color;
            ImVec4 active_color;
            ImVec4 hovered_color;
            ImVec2 padding;
            Icon *arrow_right = nullptr;
            Icon *arrow_down = nullptr;
        } g_button;

        APPLIB_API void bind_button_style();
    } // namespace style

    // Button to close a window
    bool close_button(ImGuiID id, const ImVec2 &pos);

    // Control buttons (Yes/No/Canel/etc)
    APPLIB_API void right_controls(const acul::vector<acul::string> &buttons, int *selected = nullptr,
                                   f32 y_offset = 0);
} // namespace uikit

#endif