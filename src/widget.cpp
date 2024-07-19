#include <uikit/widget.hpp>

namespace uikit
{
    namespace style
    {
        General general;
        Button button;
        CheckBox checkbox;
        Colors colors;

        void setupNativeStyle()
        {
            ImGuiStyle &style = ImGui::GetStyle();
            style.WindowBorderSize = general.windowBorderSize;
            style.PopupBorderSize = general.popupBorderSize;
            style.WindowRounding = general.windowRounding;
            style.FrameRounding = general.frameRounding;
            style.TabBarBorderSize = 0;
            style.TabBorderSize = 0;
            style.TabRounding = 0;
            style.SelectableTextAlign.y = 0.5f;
            style.Colors[ImGuiCol_WindowBg] = colors.windowBg;
            style.Colors[ImGuiCol_FrameBg] = colors.frameBg;
            style.Colors[ImGuiCol_FrameBgHovered] = colors.frameHovered;
            style.Colors[ImGuiCol_FrameBgActive] = colors.frameActive;
            style.Colors[ImGuiCol_Header] = colors.header;
            style.Colors[ImGuiCol_HeaderHovered] = colors.headerHovered;
            style.Colors[ImGuiCol_HeaderActive] = colors.headerActive;
            style.Colors[ImGuiCol_Border] = colors.border;
            style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
            style.Colors[ImGuiCol_Button] = button.color;
            style.Colors[ImGuiCol_ButtonActive] = button.colorActive;
            style.Colors[ImGuiCol_ButtonHovered] = button.colorHovered;
            style.Colors[ImGuiCol_CheckMark] = checkbox.mark;
        }
    } // namespace style
} // namespace ui