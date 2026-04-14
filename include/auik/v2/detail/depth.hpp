#pragma once

#include <amal/vector.hpp>
#include "../widgets/widget.hpp"

namespace auik::v2::detail
{
    struct DepthZone
    {
        enum enum_type
        {
            foreground,
            work,
            background
        };
    };

    amal::vec2 depth_zone_range(const amal::vec2 &base, DepthZone::enum_type zone);
    inline amal::vec2 get_depth_workzone_range(const amal::vec2 &r) { return depth_zone_range(r, DepthZone::work); }
    amal::vec2 normalize_depth_range(const amal::vec2 &src);
    DepthZone::enum_type get_depth_zone_by_flags(WidgetFlags flags);
    amal::vec2 get_root_depth_range(DepthZone::enum_type zone, int lane_index);
    inline f32 mid_depth(const amal::vec2 &range) { return (range.x + range.y) * 0.5f; }
} // namespace auik::v2::detail
