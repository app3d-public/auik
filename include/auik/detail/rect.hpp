#pragma once

#include <acul/scalars.hpp>
#include <amal/common.hpp>
#include <amal/rect.hpp>
#include <amal/vector.hpp>

namespace auik::detail
{
    inline amal::vec4 intersect_rects(const amal::vec4 &a, const amal::vec4 &b)
    {
        const f32 left = amal::max(a.x, b.x);
        const f32 top = amal::max(a.y, b.y);
        const f32 right = amal::min(a.x + a.z, b.x + b.z);
        const f32 bottom = amal::min(a.y + a.w, b.y + b.w);
        return {left, top, amal::max(right - left, 0.0f), amal::max(bottom - top, 0.0f)};
    }

    inline f32 clamp_corner_rounding(const amal::rect &rect, f32 rounding, u32 corner_mask)
    {
        if (rounding < 0.5f || corner_mask == 0u) return 0.0f;

        const bool round_top = (corner_mask & 0x3u) != 0u;
        const bool round_bottom = (corner_mask & 0xCu) != 0u;
        const bool round_left = (corner_mask & 0x9u) != 0u;
        const bool round_right = (corner_mask & 0x6u) != 0u;

        rounding = amal::min(rounding, amal::abs(rect.size.x) * ((round_top || round_bottom) ? 0.5f : 1.0f) - 1.0f);
        rounding = amal::min(rounding, amal::abs(rect.size.y) * ((round_left || round_right) ? 0.5f : 1.0f) - 1.0f);
        return amal::max(rounding, 0.0f);
    }
} // namespace auik::detail
