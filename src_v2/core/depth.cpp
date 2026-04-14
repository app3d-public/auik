#include <auik/v2/detail/depth.hpp>

#define AUIK_ROOT_DEPTH_ATOMS_COUNT  32
#define AUIK_CHILD_DEPTH_ATOMS_COUNT 16
#define AUIK_DEPTH_MIN_STEP          1e-6f

namespace auik::v2::detail
{
    amal::vec2 depth_zone_range(const amal::vec2 &base, DepthZone::enum_type zone)
    {
        const f32 span = base.y - base.x;
        switch (zone)
        {
            case DepthZone::background:
                return {base.x + span * 0.00f, base.x + span * (1.0f / 3.0f)};
            case DepthZone::work:
                return {base.x + span * (1.0f / 3.0f), base.x + span * (2.0f / 3.0f)};
            case DepthZone::foreground:
                return {base.x + span * (2.0f / 3.0f), base.x + span * 1.00f};
            default:
                return {base.x + span * (1.0f / 3.0f), base.x + span * (2.0f / 3.0f)};
        }
    }

    amal::vec2 normalize_depth_range(const amal::vec2 &src)
    {
        f32 z_min = src.x;
        f32 z_max = src.y;
        if (z_min > z_max)
        {
            const f32 t = z_min;
            z_min = z_max;
            z_max = t;
        }
        return {z_min, z_max};
    }

    DepthZone::enum_type get_depth_zone_by_flags(WidgetFlags flags)
    {
        if (flags & WidgetFlagBits::foreground) return DepthZone::foreground;
        if (flags & WidgetFlagBits::background) return DepthZone::background;
        return DepthZone::work;
    }

    amal::vec2 get_root_depth_range(DepthZone::enum_type zone, int lane_index)
    {
        constexpr amal::vec2 global = {0.0f, 1.0f};

        const amal::vec2 lane_range = depth_zone_range(global, zone);
        const f32 span = lane_range.y - lane_range.x;
        const f32 step = amal::max(span / static_cast<f32>(AUIK_ROOT_DEPTH_ATOMS_COUNT), AUIK_DEPTH_MIN_STEP);

        const f32 r0 = lane_range.x + step * static_cast<f32>(lane_index);
        const f32 r1 = (r0 + step <= lane_range.y) ? (r0 + step) : lane_range.y;

        return {r0, r1};
    }
} // namespace auik::v2::detail

namespace auik::v2
{
    void Widget::update_depth(const amal::vec2 &depth_range)
    {
        _depth_range = detail::normalize_depth_range(depth_range);
        amal::vec2 active_range = _depth_range;
        if (widget_flags & WidgetFlagBits::foreground)
            active_range = detail::depth_zone_range(_depth_range, detail::DepthZone::foreground);
        else if (widget_flags & WidgetFlagBits::background)
            active_range = detail::depth_zone_range(_depth_range, detail::DepthZone::background);
        else active_range = detail::depth_zone_range(_depth_range, detail::DepthZone::work);
        _depth_range = detail::normalize_depth_range(active_range);
        _rect.depth = (_depth_range.x + _depth_range.y) * 0.5f;
    }

    void assign_next_depth(const amal::vec2 &parent_range, amal::vec2 &dst_range)
    {
        const amal::vec2 w = detail::normalize_depth_range(parent_range);

        const f32 span = w.y - w.x;
        if (span <= 0.0f)
        {
            dst_range = {w.x, w.x};
            return;
        }

        const f32 step = amal::max(span / static_cast<f32>(AUIK_CHILD_DEPTH_ATOMS_COUNT), AUIK_DEPTH_MIN_STEP);
        const f32 r1 = w.y;
        const f32 r0 = (r1 - step >= w.x) ? (r1 - step) : w.x;
        dst_range = {r0, r1};
    }
} // namespace auik::v2
