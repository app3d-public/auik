#pragma once

#include <acul/scalars.hpp>
#include <amal/vector.hpp>

namespace auik::v2
{
    struct Viewport
    {
        Viewport *base_viewport = nullptr;
        amal::vec4 base{0.0f, 0.0f, 0.0f, 0.0f};
        amal::vec4 available{0.0f, 0.0f, 0.0f, 0.0f};
    };

    enum class ViewportEdge : u8
    {
        top,
        bottom,
        left,
        right
    };

    enum class ViewportLayoutMode : u8
    {
        none,
        reserve,
        flow,
        fill
    };

    struct ViewportLayout
    {
        ViewportLayoutMode mode = ViewportLayoutMode::none;
        ViewportEdge edge = ViewportEdge::top;
    };
} // namespace auik::v2
