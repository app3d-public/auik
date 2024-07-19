#include <imgui/imgui_internal.h>
#include <uikit/icon/icon.hpp>

namespace uikit
{
    void FontIcon::render(ImVec2 pos)
    {
        ImGui::PushFont(_font);
        ImGui::RenderText(pos, _u8sequence.c_str());
        ImGui::PopFont();
    }
} // namespace ui