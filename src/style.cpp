#include <uikit/style.hpp>

namespace uikit
{
    namespace style
    {
        General *g_StyleGeneral = nullptr;
        Colors *g_StyleColors = nullptr;

        template <>
        void registerStyle<General>(General *style)
        {
            g_StyleGeneral = style;
            ImGuiStyle &imguiStyle = ImGui::GetStyle();
            imguiStyle.WindowBorderSize = g_StyleGeneral->windowBorderSize;
            imguiStyle.PopupBorderSize = g_StyleGeneral->popupBorderSize;
            imguiStyle.WindowRounding = g_StyleGeneral->windowRounding;
            imguiStyle.FrameRounding = g_StyleGeneral->frameRounding;
            imguiStyle.ItemSpacing = g_StyleGeneral->itemSpacing;
            imguiStyle.TabBarBorderSize = 0;
            imguiStyle.TabBorderSize = 0;
            imguiStyle.TabRounding = 0;
            imguiStyle.SelectableTextAlign.y = 0.5f;
        }

        template <>
        void registerStyle<Colors>(Colors *style)
        {
            g_StyleColors = style;
            auto &colors = ImGui::GetStyle().Colors;
            colors[ImGuiCol_WindowBg] = style->windowBg;
            colors[ImGuiCol_FrameBg] = style->frameBg;
            colors[ImGuiCol_TitleBgActive] = style->frameActive;
            colors[ImGuiCol_FrameBgHovered] = style->frameHovered;
            colors[ImGuiCol_FrameBgActive] = style->frameActive;
            colors[ImGuiCol_Header] = style->header;
            colors[ImGuiCol_HeaderHovered] = style->headerHovered;
            colors[ImGuiCol_HeaderActive] = style->headerActive;
            colors[ImGuiCol_Border] = style->border;
            colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        }
    } // namespace style
} // namespace uikit