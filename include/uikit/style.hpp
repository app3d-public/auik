#pragma once

#include <astl/basic_types.hpp>
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
        } g_General;

        extern APPLIB_API struct Colors
        {
            ImVec4 windowBg;
            ImVec4 popupBg;
            ImVec4 frameBg;
            ImVec4 frameHovered;
            ImVec4 frameActive;
            ImVec4 header;
            ImVec4 headerHovered;
            ImVec4 headerActive;
            ImVec4 border;
        } g_Colors;

        template <typename T>
        APPLIB_API void bindStyle();

        template <>
        APPLIB_API void bindStyle<General>();

        template <>
        APPLIB_API void bindStyle<Colors>();
    } // namespace style
} // namespace uikit