#include <imgui/imgui_internal.h>
#include <uikit/button/button.hpp>
#include <uikit/button/collapse_header.hpp>

namespace uikit
{
    void CollapseHeader::render()
    {
        ImGuiContext &g = *GImGui;
        auto *window = g.CurrentWindow;
        auto *draw_list = window->DrawList;
        f32 height = ImGui::GetFrameHeight();
        ImRect bb;
        bb.Min = window->DC.CursorPos;
        bb.Max.x = bb.Min.x + ImGui::GetContentRegionAvail().x;
        bb.Max.y = bb.Min.y + height;
        const char *name = _content->name.c_str();
        const ImGuiID id = window->GetID(name);
        bool hovered, held;
        _pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held, ImGuiButtonFlags_PressedOnClick);
        if (_pressed) _collapsed = !_collapsed;
        auto &style = g.Style;
        ImU32 col = ImGui::GetColorU32(style.Colors[held      ? ImGuiCol_FrameBgActive
                                                    : hovered ? ImGuiCol_FrameBgHovered
                                                              : ImGuiCol_FrameBg]);
        draw_list->AddRectFilled(bb.Min, bb.Max, col, style.FrameRounding);
        auto *icon = _collapsed ? style::g_Button.arrowRight : style::g_Button.arrowDown;
        ImVec2 icon_size = icon->size();
        icon->render({bb.Min.x + style.WindowPadding.x, bb.Min.y + (height - icon_size.y) * 0.5f});

        ImVec2 text_size = ImGui::CalcTextSize(name);
        ImVec2 text_pos = window->DC.CursorPos;
        ImVec2 init_pos = window->DC.CursorPos;
        window->DC.CursorPos.x += style.WindowPadding.x + icon_size.x + style.ItemSpacing.x;
        window->DC.CursorPos.y += (height - text_size.y) * 0.5f;
        window->DC.IsSetPos = true;
        ImGui::TextUnformatted(name);

        init_pos.y += height + style.WindowPadding.y;
        ImGui::SetCursorScreenPos(init_pos);

        if (!_collapsed) _content->render();
    }
} // namespace uikit