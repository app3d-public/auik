#pragma once

#include "context.hpp"

namespace auik::detail
{
    AUIK_EXPORT void acquire_global_depth(GlobalDepthAllocation &allocation);
    AUIK_EXPORT void release_global_depth(GlobalDepthAllocation &allocation);
    AUIK_EXPORT void raise_global_depth(GlobalDepthAllocation &allocation);
    amal::vec2 depth_slice(const amal::vec2 &base, u32 index, u32 count);
    inline amal::vec2 depth_background_range(const amal::vec2 &r) { return depth_slice(r, 0u, 3u); }
    inline amal::vec2 depth_work_range(const amal::vec2 &r) { return depth_slice(r, 1u, 3u); }
    inline amal::vec2 depth_foreground_range(const amal::vec2 &r) { return depth_slice(r, 2u, 3u); }
    amal::vec2 normalize_depth_range(const amal::vec2 &src);
    amal::vec2 get_root_depth_zone_range(DepthZone zone);
    AUIK_EXPORT amal::vec2 get_dock_overlay_depth_range();
} // namespace auik::detail
