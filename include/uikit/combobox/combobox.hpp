#ifndef UIKIT_WIDGETS_COMBOBOX_H
#define UIKIT_WIDGETS_COMBOBOX_H

#include <imgui/imgui.h>
#include "../icon/icon.hpp"
#include <memory>

namespace ui
{
    bool beginCombo(const char *label, const char *preview_value, ImGuiComboFlags flags = 0, const std::shared_ptr<Icon> chevronDownIcon = nullptr);

    inline void endCombo() { ImGui::EndPopup(); }

} // namespace ui

#endif