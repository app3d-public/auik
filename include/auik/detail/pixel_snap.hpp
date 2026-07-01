#pragma once

#include <acul/scalars.hpp>
#include <amal/common.hpp>
#include <amal/rect.hpp>

namespace auik::detail
{
    inline amal::rect snap_rect_to_pixel_grid(const amal::rect &rect)
    {
        const f32 left = amal::floor(rect.offset.x);
        const f32 top = amal::floor(rect.offset.y);
        const f32 right = amal::ceil(rect.offset.x + rect.size.x);
        const f32 bottom = amal::ceil(rect.offset.y + rect.size.y);
        return {{left, top}, {amal::max(right - left, 0.0f), amal::max(bottom - top, 0.0f)}};
    }

    inline amal::rect snap_rect_offset_to_pixel_grid(const amal::rect &rect)
    {
        return {{amal::round(rect.offset.x), amal::round(rect.offset.y)}, rect.size};
    }

    inline amal::vec4 snap_rect_to_pixel_grid(const amal::vec4 &rect)
    {
        const f32 left = amal::floor(rect.x);
        const f32 top = amal::floor(rect.y);
        const f32 right = amal::ceil(rect.x + rect.z);
        const f32 bottom = amal::ceil(rect.y + rect.w);
        return {left, top, amal::max(right - left, 0.0f), amal::max(bottom - top, 0.0f)};
    }
} // namespace auik::detail
