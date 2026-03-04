#pragma once
#include <acul/scalars.hpp>

namespace auik::v2
{
    constexpr u32 AUIK_INVALID_DRAW_DATA_ID = 0xFFFFFFFFu;

    struct DrawDataID
    {
        u32 render_id = AUIK_INVALID_DRAW_DATA_ID;
        u32 hit_id = AUIK_INVALID_DRAW_DATA_ID;
    };

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
