#include <uikit/image/image.hpp>
#include <imgui/imgui_internal.h>

namespace uikit
{
    void Image::render()
    {
        ImGuiWindow *window = ImGui::GetCurrentWindow();
        if (window->SkipItems)
            return;
        render(window->DC.CursorPos);
    }

    void Image::render(ImVec2 pos)
    {
        ImGuiWindow *window = ImGui::GetCurrentWindow();
        if (window->SkipItems)
            return;
        const ImRect bb(pos, pos + _size);
        ImGui::ItemSize(bb);
        if (!ImGui::ItemAdd(bb, 0))
            return;
        window->DrawList->AddImage(_id, bb.Min, bb.Max, _uvMin, _uvMax, ImGui::GetColorU32(ImVec4(1, 1, 1, 1)));
    }
} // namespace ui