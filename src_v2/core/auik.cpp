#include <auik/v2/auik.hpp>
#include <auik/v2/detail/context.hpp>
#include <auik/v2/detail/events.hpp>
#include <auik/v2/detail/gpu_context.hpp>
#include <auik/v2/widget.hpp>
#include <auik/v2/window.hpp>

#define AUIK_ROOT_DEPTH_ATOMS_COUNT  32
#define AUIK_CHILD_DEPTH_ATOMS_COUNT 16
#define AUIK_DEPTH_MIN_STEP          1e-6f

namespace auik::v2
{
    static void clear_all_streams(detail::Context &ctx)
    {
        for (u32 stream_id = 0; stream_id < ctx.streams.stream_count; ++stream_id)
        {
            auto &stream = ctx.streams.attached_streams[stream_id];
            // If cached stream and non-empty
            if (stream.draw_sizes[ctx.frame_id] <= 0) continue;
            clear_draw_stream(&stream, ctx.frame_id);
        }
    }

    namespace detail
    {
        Context *g_context = nullptr;

        APPLIB_API void on_resize_event(const acul::point2D<i32> &size)
        {
            auto &ctx = get_context();
            ctx.window_size = size;
            mark_layout_dirty();
        }

        struct DepthZone
        {
            enum enum_type
            {
                foreground,
                hitbox,
                work,
                background
            };
        };

        static inline amal::vec2 depth_zone_range(const amal::vec2 &base, DepthZone::enum_type zone)
        {
            constexpr f32 k = 0.25f;

            const f32 t0 = k * static_cast<f32>(zone);
            const f32 t1 = t0 + k;

            const f32 span = base.y - base.x;
            return {base.x + span * t0, base.x + span * t1};
        }

        APPLIB_API amal::vec2 get_depth_workzone_range(const amal::vec2 &r)
        {
            return depth_zone_range(r, DepthZone::work);
        }

        static void build_depth_data(const amal::vec2 &src, detail::DepthData &out)
        {
            f32 z_min = src.x;
            f32 z_max = src.y;
            if (z_min > z_max)
            {
                const f32 t = z_min;
                z_min = z_max;
                z_max = t;
            }

            out.range = {z_min, z_max};

            const amal::vec2 hit = depth_zone_range(out.range, DepthZone::hitbox);
            const amal::vec2 work = depth_zone_range(out.range, DepthZone::work);

            out.z_order = amal::mid(work.x, work.y);
            out.hitbox = amal::mid(hit.x, hit.y);
        }

        static inline DepthZone::enum_type get_depth_zone_by_flags(WidgetFlags flags)
        {
            if (flags & WidgetFlagBits::foreground) return DepthZone::foreground;
            if (flags & WidgetFlagBits::background) return DepthZone::background;
            return DepthZone::work;
        }

        static inline amal::vec2 get_root_depth_range(DepthZone::enum_type zone, int lane_index)
        {
            constexpr amal::vec2 global = {0.0f, 1.0f};

            const amal::vec2 lane_range = depth_zone_range(global, zone);
            const f32 span = lane_range.y - lane_range.x;
            const f32 step = amal::max(span / (f32)AUIK_ROOT_DEPTH_ATOMS_COUNT, AUIK_DEPTH_MIN_STEP);

            const f32 r0 = lane_range.x + step * static_cast<f32>(lane_index);
            const f32 r1 = (r0 + step <= lane_range.y) ? (r0 + step) : lane_range.y;

            return {r0, r1};
        }
    } // namespace detail

    void Widget::update_depth(const amal::vec2 &depth_range)
    {
        detail::build_depth_data(depth_range, _depth);

        if (widget_flags & WidgetFlagBits::foreground)
        {
            const amal::vec2 overlay = detail::depth_zone_range(_depth.range, detail::DepthZone::foreground);
            _depth.z_order = amal::mid(overlay.x, overlay.y);
        }
        else if (widget_flags & WidgetFlagBits::background)
        {
            const amal::vec2 bg = detail::depth_zone_range(_depth.range, detail::DepthZone::background);
            _depth.z_order = amal::mid(bg.x, bg.y);
        }
        else
        {
            const amal::vec2 work = detail::depth_zone_range(_depth.range, detail::DepthZone::work);
            _depth.z_order = amal::mid(work.x, work.y);
        }
    }

    APPLIB_API void assign_next_depth(const detail::DepthData &parent, detail::DepthData &dst)
    {
        const amal::vec2 w = detail::get_depth_workzone_range(parent.range);

        const f32 span = w.y - w.x;
        if (span <= 0.0f)
        {
            detail::build_depth_data({w.x, w.x}, dst);
            return;
        }

        const f32 step = amal::max(span / (f32)AUIK_CHILD_DEPTH_ATOMS_COUNT, AUIK_DEPTH_MIN_STEP);
        const f32 r0 = w.x;
        const f32 r1 = (r0 + step <= w.y) ? (r0 + step) : w.y;

        detail::build_depth_data({r0, r1}, dst);
    }

    void init_library(const CreateInfo &create_info)
    {
        if (detail::g_context) destroy_library();
        detail::g_context = acul::alloc<detail::Context>();
        auto &ctx = detail::get_context();
        ctx.ed = create_info.ed;
        ctx.disposal_queue = create_info.disposal_queue;
        ctx.streams.attached_streams = create_info.streams;
        ctx.streams.stream_count = create_info.streams_count;
        ctx.gpu_ctx = create_info.gpu_ctx;
        ctx.frames_in_flight = create_info.frames_in_flight;
        ctx.window_size = create_info.window_size;
        ctx.window_ctx = create_info.window_ctx;
        ctx.dirty_flags = DirtyFlagBits::render | DirtyFlagBits::layout;
    }

    void destroy_library()
    {
        if (!detail::g_context) return;
        for (auto *widget : detail::g_context->widget_tree) acul::release(widget);
        detail::destroy_gpu_context(detail::g_context->gpu_ctx);
        detail::destroy_window_context(detail::g_context->window_ctx);
        acul::release(detail::g_context);
        detail::g_context = nullptr;
    }

    void record_all_commands()
    {
        auto &ctx = detail::get_context();
        if (ctx.dirty_flags == DirtyFlagBits::none) return;
        clear_all_streams(ctx);
        for (Widget *widget : ctx.widget_tree)
        {
            if (!widget) continue;
            if (ctx.dirty_flags & DirtyFlagBits::layout)
            {
                widget->update_layout();
                widget->record_draw_commands();
            }
            else
                widget->update_draw_commands();
        }
        ctx.dirty_flags = DirtyFlagBits::none;
    }

    APPLIB_API void record_all_commands_force()
    {
        auto &ctx = detail::get_context();
        clear_all_streams(ctx);
        for (Widget *widget : ctx.widget_tree)
        {
            if (!widget) continue;
            widget->update_layout();
            widget->record_draw_commands();
        }
        ctx.dirty_flags = DirtyFlagBits::none;
    }

    APPLIB_API void add_widget_to_root(Widget *widget)
    {
        assert(widget && "widget is null");
        assert(widget->parent() == nullptr && "Root widget must not have a parent");
        auto &ctx = detail::get_context();
        ctx.widget_tree.push_back(widget);
        const auto zone = detail::get_depth_zone_by_flags(widget->widget_flags);
        const int lane_index = ctx.root_depth_counts[zone];
        assert(lane_index < AUIK_ROOT_DEPTH_ATOMS_COUNT && "Max depth zone exceeded");
        widget->update_depth(detail::get_root_depth_range(zone, lane_index));
        ++ctx.root_depth_counts[zone];
        widget->update_style();
        widget->update_layout();
        widget->record_draw_commands();
        ctx.dirty_flags |= DirtyFlagBits::render;
    }
} // namespace auik::v2
