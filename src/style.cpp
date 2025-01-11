#include <uikit/style.hpp>

namespace uikit
{
    namespace style
    {
        General g_General;
        Colors g_Colors;

        template <>
        void bindStyle<General>()
        {
            ImGuiStyle &imguiStyle = ImGui::GetStyle();
            imguiStyle.WindowBorderSize = g_General.windowBorderSize;
            imguiStyle.ChildBorderSize = g_General.popupBorderSize;
            imguiStyle.PopupBorderSize = g_General.popupBorderSize;
            imguiStyle.WindowRounding = g_General.windowRounding;
            imguiStyle.PopupRounding = g_General.windowRounding;
            imguiStyle.FrameRounding = g_General.frameRounding;
            imguiStyle.ItemSpacing = g_General.itemSpacing;
            imguiStyle.TabBarBorderSize = 0;
            imguiStyle.TabBorderSize = 0;
            imguiStyle.TabRounding = 0;
            imguiStyle.SelectableTextAlign.y = 0.5f;
            imguiStyle.GrabMinSize = 4;
        }

        template <>
        void bindStyle<Colors>()
        {
            auto &colors = ImGui::GetStyle().Colors;
            colors[ImGuiCol_WindowBg] = g_Colors.windowBg;
            colors[ImGuiCol_PopupBg] = g_Colors.popupBg;
            colors[ImGuiCol_FrameBg] = g_Colors.frameBg;
            colors[ImGuiCol_TitleBgActive] = g_Colors.frameActive;
            colors[ImGuiCol_FrameBgHovered] = g_Colors.frameHovered;
            colors[ImGuiCol_FrameBgActive] = g_Colors.frameActive;
            colors[ImGuiCol_Header] = g_Colors.header;
            colors[ImGuiCol_HeaderHovered] = g_Colors.headerHovered;
            colors[ImGuiCol_HeaderActive] = g_Colors.headerActive;
            colors[ImGuiCol_Border] = g_Colors.border;
            colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        }
    } // namespace style
} // namespace uikit