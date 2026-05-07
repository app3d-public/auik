#pragma once

#include <amal/vector.hpp>
#include "../widgets/widget.hpp"

namespace auik::v2::detail
{
    amal::vec2 depth_zone_range(const amal::vec2 &base, DepthZone::enum_type zone);
    inline amal::vec2 get_depth_workzone_range(const amal::vec2 &r) { return depth_zone_range(r, DepthZone::work); }
    amal::vec2 normalize_depth_range(const amal::vec2 &src);
    amal::vec2 get_root_depth_range(DepthZone::enum_type zone, int lane_index);
} // namespace auik::v2::detail
