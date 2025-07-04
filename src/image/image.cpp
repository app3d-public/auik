#include <auik/image/image.hpp>
#include <imgui/imgui_internal.h>

namespace auik
{
    void Image::render(ImVec2 pos, ImVec2 size)
    {
        ImGuiWindow *window = ImGui::GetCurrentWindow();
        if (window->SkipItems)
            return;
        const ImRect bb(pos, pos + size);
        ImGui::ItemSize(bb);
        if (!ImGui::ItemAdd(bb, 0))
            return;
        window->DrawList->AddImage(_id, bb.Min, bb.Max, _uvMin, _uvMax, ImGui::GetColorU32(ImVec4(1, 1, 1, 1)));
    }
} // namespace ui