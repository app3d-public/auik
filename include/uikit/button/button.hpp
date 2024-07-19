#ifndef UIKIT_WIDGETS_BUTTON_H
#define UIKIT_WIDGETS_BUTTON_H

#include <imgui/imgui.h>

namespace uikit
{
    // Button to close a window
    bool closeButton(ImGuiID id, const ImVec2 &pos);
} // namespace ui

#endif