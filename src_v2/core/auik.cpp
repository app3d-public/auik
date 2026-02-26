#include <auik/v2/auik.hpp>
#include <auik/v2/detail/context.hpp>
#include <auik/v2/detail/events.hpp>
#include <auik/v2/detail/gpu_context.hpp>
#include <auik/v2/widget.hpp>

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
    } // namespace detail

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
        if (ctx.dirty_flags & DirtyFlagBits::layout)
        {
            clear_all_streams(ctx);
            for (Widget *widget : ctx.widget_tree)
            {
                if (!widget) continue;
                widget->update_layout();
                widget->record_draw_commands();
            }
            ctx.dirty_flags = DirtyFlagBits::none;
            return;
        }

        if (!(ctx.dirty_flags & DirtyFlagBits::render)) return;
        for (Widget *widget : ctx.widget_tree)
        {
            if (!widget) continue;
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

    void update_widget_style(Widget *widget)
    {
        assert(widget && "widget is null");

        auto &ctx = detail::get_context();
        Theme *theme = ctx.theme;
        assert(theme && "theme is null");

        const Widget *parent = widget->parent();
        const u32 parent_type = parent ? parent->style.type_id : 0;
        widget->style.id = theme->get_resolved_style(widget->style.type_id, widget->id(), parent_type);
    }

    APPLIB_API void add_widget_to_root(Widget *widget)
    {
        auto &ctx = detail::get_context();
        ctx.widget_tree.push_back(widget);
        update_widget_style(widget);
        widget->update_layout();
        widget->record_draw_commands();
        ctx.dirty_flags |= DirtyFlagBits::render;
    }
} // namespace auik::v2
