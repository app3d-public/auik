#include <auik/v2/detail/depth.hpp>
#include <auik/v2/widgets/widget.hpp>

#define AUIK_ROOT_DEPTH_ATOMS_COUNT  32
#define AUIK_CHILD_DEPTH_ATOMS_COUNT 16
#define AUIK_DEPTH_MIN_STEP          1e-6f

namespace auik::v2::detail
{
    amal::vec2 depth_slice(const amal::vec2 &base, u32 index, u32 count)
    {
        const amal::vec2 r = normalize_depth_range(base);
        if (count == 0u) return r;
        if (index >= count) index = count - 1u;
        const f32 span = r.y - r.x;
        const f32 step = span / static_cast<f32>(count);
        return {r.x + step * static_cast<f32>(index), r.x + step * static_cast<f32>(index + 1u)};
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

    amal::vec2 get_root_depth_zone_range(DepthZone zone)
    {
        constexpr amal::vec2 global = {0.0f, 1.0f};
        switch (zone)
        {
        case DepthZone::background:
            return depth_background_range(global);
        case DepthZone::work:
            return depth_work_range(global);
        case DepthZone::foreground:
            return depth_foreground_range(global);
        }
        return depth_work_range(global);
    }

    amal::vec2 get_root_depth_range(DepthZone zone, int lane_index)
    {
        const amal::vec2 zone_range = get_root_depth_zone_range(zone);

        const f32 span = zone_range.y - zone_range.x;
        const f32 step = amal::max(span / static_cast<f32>(AUIK_ROOT_DEPTH_ATOMS_COUNT), AUIK_DEPTH_MIN_STEP);

        const f32 r0 = zone_range.x + step * static_cast<f32>(lane_index);
        const f32 r1 = (r0 + step <= zone_range.y) ? (r0 + step) : zone_range.y;

        return {r0, r1};
    }

    amal::vec2 get_global_foreground_depth_range()
    {
        return get_root_depth_range(DepthZone::foreground, AUIK_ROOT_DEPTH_ATOMS_COUNT - 1);
    }
} // namespace auik::v2::detail

namespace auik::v2
{
    void Widget::update_depth(const amal::vec2 &depth_range)
    {
        const f32 prev_depth = _rect.depth;
        _depth_range = detail::normalize_depth_range(depth_range);
        _rect.depth = (_depth_range.x + _depth_range.y) * 0.5f;
        _rect.hit_depth = _rect.depth;
        if (prev_depth != _rect.depth) detail::get_context().dirty_flags |= DirtyFlagBits::hit_rect_update;
    }

    void Widget::back_hit_depth()
    {
        _rect.hit_depth = detail::get_root_depth_zone_range(DepthZone::background).x;
        detail::get_context().dirty_flags |= DirtyFlagBits::hit_rect_update;
    }

    void Widget::restore_hit_depth()
    {
        _rect.hit_depth = _rect.depth;
        detail::get_context().dirty_flags |= DirtyFlagBits::hit_rect_update;
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
