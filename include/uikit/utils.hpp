#pragma once

#include <core/api.hpp>
#include <imgui/imgui_internal.h>

namespace uikit
{
    APPLIB_API void renderFrame(ImVec2 p_min, ImVec2 p_max, ImU32 fill_col, bool borders, float rounding,
                                ImDrawFlags flags = 0);
} // namespace uikit