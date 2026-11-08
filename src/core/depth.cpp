#include <algorithm>
#include <acul/exception/exception.hpp>
#include <auik/detail/depth.hpp>
#include <auik/widgets/widget.hpp>
#include <auik/widgets/dockspace.hpp>

#define AUIK_CHILD_DEPTH_ATOMS_COUNT 16
#define AUIK_DEPTH_MIN_STEP          1e-6f

namespace auik::detail
{
    void acquire_global_depth(GlobalDepthAllocation &allocation)
    {
        if (allocation.index < global_depth_slot_count) return;
        assert(allocation.update_depth);
        auto &ctx = get_context();
        if (ctx.global_depth_end == global_depth_slot_count)
        {
            bool moved[global_depth_slot_count]{};
            u32 count = 0, moved_count = 0;
            for (auto *entry : ctx.global_depth_slots)
            {
                if (!entry) continue;
                ctx.global_depth_slots[count] = entry;
                if (entry->index != count)
                {
                    entry->index = count;
                    moved[count] = true;
                    ++moved_count;
                }
                ++count;
            }
            std::fill(ctx.global_depth_slots + count, ctx.global_depth_slots + global_depth_slot_count, nullptr);
            ctx.global_depth_end = count;
            for (u32 i = 0; i < count; ++i)
            {
                // Updating one owner can close another window; release clears that slot immediately.
                auto *entry = ctx.global_depth_slots[i];
                if (moved[i] && entry) entry->update_depth(entry->owner, global_depth_range(entry->index));
            }
            if (moved_count)
            {
                // Re-record retained GPU data through the existing layout/draw path.
                ctx.dirty_flags |= DirtyFlagBits::layout | DirtyFlagBits::redraw;
                mark_host_refresh_request();
            }
            if (ctx.global_depth_end == global_depth_slot_count)
                throw acul::runtime_error("All 32 global depth slots are in use");
        }
        allocation.index = ctx.global_depth_end++;
        ctx.global_depth_slots[allocation.index] = &allocation;
        allocation.update_depth(allocation.owner, global_depth_range(allocation.index));
    }

    void release_global_depth(GlobalDepthAllocation &allocation)
    {
        if (allocation.index == global_depth_slot_count) return;
        auto &ctx = get_context();
        assert(ctx.global_depth_slots[allocation.index] == &allocation);
        ctx.global_depth_slots[allocation.index] = nullptr;
        allocation.index = global_depth_slot_count;
        while (ctx.global_depth_end && !ctx.global_depth_slots[ctx.global_depth_end - 1]) --ctx.global_depth_end;
    }

    void raise_global_depth(GlobalDepthAllocation &allocation)
    {
        if (allocation.index == global_depth_slot_count || allocation.index + 1 == get_context().global_depth_end) return;
        release_global_depth(allocation);
        acquire_global_depth(allocation);
    }

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

    amal::vec2 get_dock_overlay_depth_range()
    {
        const auto *dock = get_context().dockspace_context;
        return dock && dock->drag_depth.index < global_depth_slot_count
                   ? global_depth_range(dock->drag_depth.index) : amal::vec2{0, 0};
    }
} // namespace auik::detail

namespace auik
{
    amal::vec2 global_depth_range(u32 index)
    {
        assert(index < global_depth_slot_count && "Global depth index must be in [0, 32)");
        constexpr f32 step = 1.0f / global_depth_slot_count;
        return {static_cast<f32>(index) * step, static_cast<f32>(index + 1u) * step};
    }

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
} // namespace auik
