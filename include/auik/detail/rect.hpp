#pragma once

#include <acul/scalars.hpp>
#include <amal/common.hpp>
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
} // namespace auik::detail
