#pragma once
#include <acul/scalars.hpp>

namespace auik::v2
{
    using DrawDataID = u32;
    constexpr DrawDataID AUIK_INVALID_DRAW_DATA_ID = 0xFFFFFFFFu;

    class Theme;
    class Widget;
    struct DrawStream;
    struct DrawPipeline;

    namespace detail
    {
        struct WindowContext;
        struct Context;
        struct GPUContext;
    } // namespace detail
} // namespace auik::v2
