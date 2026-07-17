#pragma once

#include <acul/scalars.hpp>
#include <amal/rect.hpp>
#include <amal/vector.hpp>

namespace auik
{
    struct Viewport;
    using PFN_sync_viewport = void (*)(Viewport *);

    struct Viewport
    {
        amal::rect rect{};
        u16 clip_id = 0xFFFFu;
        f32 clip_depth = 0.0f;
        PFN_sync_viewport sync_viewport = nullptr;
        bool fit_content = true;
    };

    struct ViewportGroup final : Viewport
    {
        Viewport *left = nullptr;
        Viewport *right = nullptr;
        Viewport *top = nullptr;
        Viewport *bottom = nullptr;
    };
} // namespace auik
