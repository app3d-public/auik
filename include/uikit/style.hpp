#pragma once

#include <core/std/basic_types.hpp>
#include <imgui/imgui.h>
#include <uikit/icon/icon.hpp>

namespace uikit
{
    namespace style
    {
        extern APPLIB_API struct General
        {
            f32 windowBorderSize;
            f32 popupBorderSize;
            f32 windowRounding;
            f32 frameRounding;
            ImVec2 itemSpacing;
        } *g_StyleGeneral;

        extern APPLIB_API struct Colors
        {
            ImVec4 windowBg;
            ImVec4 frameBg;
            ImVec4 frameHovered;
            ImVec4 frameActive;
            ImVec4 header;
            ImVec4 headerHovered;
            ImVec4 headerActive;
            ImVec4 border;
        } *g_StyleColors;

        template <typename T>
        APPLIB_API void registerStyle(T *style);

        template <>
        APPLIB_API void registerStyle<General>(General *style);

        template <>
        APPLIB_API void registerStyle<Colors>(Colors *style);
    } // namespace style
} // namespace uikit