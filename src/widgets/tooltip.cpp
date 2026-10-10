#include <auik/auik.hpp>
#include <auik/detail/context.hpp>
#include <auik/detail/depth.hpp>
#include <auik/widgets/tooltip.hpp>

#define AUIK_POPUP_TIP_SCREEN_PADDING 8.0f
#define AUIK_POPUP_TIP_CURSOR_OFFSET_Y 10.0f
#define AUIK_POPUP_TIP_SHOW_DELAY     0.35
#define AUIK_UD_POPUP_TIP             0xB51567D1u

namespace auik
{
    namespace
    {
        struct PopupTipInfo
        {
            Widget *widget = nullptr;
            Window *window = nullptr;
            acul::unique_function<void(HoverEvent &)> previous_hover;
            u64 task_owner = 0u;
            bool tracking = false;

            ~PopupTipInfo()
            {
                cancel_delayed_tasks(task_owner);
                if (!window) return;
                if (window->is_transient()) erase_widget_from_transient_cache(window);
                acul::release(window);
            }
        };

        void hide_popup_tip(PopupTipInfo &info)
        {
            cancel_delayed_tasks(info.task_owner);
            if (!info.window || !info.window->is_visible()) return;
            info.window->unset_visible();
            info.window->sync_widget_flags();
            if (info.window->is_transient()) erase_widget_from_transient_cache(info.window);
            mark_host_refresh_request();
        }

        void show_popup_tip(PopupTipInfo &info)
        {
            if (!info.tracking || !info.widget || !info.window) return;
            auto &ctx = detail::get_context();
            const auto widget_it = ctx.id_map.find(info.widget->id());
            if (widget_it == ctx.id_map.end() || widget_it->second != info.widget ||
                ctx.hover_id.widget_id != info.widget->id())
                return;

            auto *window = info.window;
            window->set_auto_position(false, false);
            const amal::vec4 viewport = get_widget_viewport_rect(window);
            window->set_root_viewport_origin({viewport.x, viewport.y});
            window->set_visible();
            window->sync_widget_flags();
            window->update_style_invalidated();
            window->update_layout_min_size_force();

            const amal::vec2 extent{
                amal::min(window->required_size().x,
                          amal::max(viewport.z - AUIK_POPUP_TIP_SCREEN_PADDING * 2.0f, 0.0f)),
                window->required_size().y};
            const auto cursor = get_mouse_pos();
            const amal::vec2 min_pos{viewport.x + AUIK_POPUP_TIP_SCREEN_PADDING,
                                     viewport.y + AUIK_POPUP_TIP_SCREEN_PADDING};
            const f32 max_x = amal::max(viewport.x + viewport.z - extent.x - AUIK_POPUP_TIP_SCREEN_PADDING,
                                        min_pos.x);
            const f32 x = amal::clamp(cursor.x, min_pos.x, max_x);
            const f32 below = cursor.y + AUIK_POPUP_TIP_CURSOR_OFFSET_Y;
            const f32 max_y = amal::max(viewport.y + viewport.w - extent.y - AUIK_POPUP_TIP_SCREEN_PADDING,
                                        min_pos.y);
            const f32 y = below <= max_y
                              ? below
                              : amal::clamp(cursor.y - extent.y - AUIK_POPUP_TIP_CURSOR_OFFSET_Y,
                                            min_pos.y, max_y);

            window->set_position({amal::floor(x), amal::floor(y)});
            window->set_size(extent);
            window->update_depth(detail::get_global_foreground_depth_range());
            window->update_layout(false);
            window->rebuild_clip_rects();
            push_widget_to_transient_cache(window);
            mark_host_refresh_request();
        }

        void schedule_popup_tip(PopupTipInfo &info)
        {
            cancel_delayed_tasks(info.task_owner);
            detail::update_window_time(detail::get_context().window_ctx);
            const f64 due_time = detail::get_context().window_ctx->time + AUIK_POPUP_TIP_SHOW_DELAY;
            schedule_delayed_host_task(info.task_owner, due_time, [&info]() { show_popup_tip(info); });
        }
    } // namespace

    Window *make_popup_tip_window(u32 id)
    {
        constexpr WidgetFlags flags =
            WidgetFlagBits::visible | WidgetFlagBits::attachable | WidgetFlagBits::cache_snapshot;
        auto *window =
            acul::alloc<Window>(id, "", amal::rect{AUIK_POS_IGNORE, AUIK_SIZE_AUTO}, WindowFlagBits::none, flags);
        window->set_window_style_tag(AUIK_STYLE_TAG_TOOLTIP);
        window->set_dock_style_tag(0u);
        window->set_auto_size();
        window->unset_visible();
        if (auto *content = window->content_block()) content->unset_widget_flag(WidgetFlagBits::hittable);
        return window;
    }

    bool add_popup_tip_to_widget(Widget *widget, Window *window)
    {
        if (!widget || !window) return false;
        remove_popup_tip_from_widget(widget);

        auto *info = widget->emplace_user_data_tagged<PopupTipInfo>(AUIK_UD_POPUP_TIP);
        info->widget = widget;
        info->window = window;
        info->task_owner = (static_cast<u64>(AUIK_UD_POPUP_TIP) << 32u) | widget->id();
        auto &binding = widget->bind();
        info->previous_hover = std::move(binding.on_hover_fn);
        binding.on_hover([info](HoverEvent &event) {
            if (info->previous_hover) info->previous_hover(event);
            info->tracking = event.state == HoverState::enter;
            if (info->tracking) schedule_popup_tip(*info);
            else hide_popup_tip(*info);
        });
        return true;
    }

    void remove_popup_tip_from_widget(Widget *widget)
    {
        if (!widget) return;
        auto *info = widget->get_user_data<PopupTipInfo>(AUIK_UD_POPUP_TIP);
        if (!info) return;
        auto previous_hover = std::move(info->previous_hover);
        widget->bind().on_hover(std::move(previous_hover));
        widget->erase_user_data(AUIK_UD_POPUP_TIP);
    }
} // namespace auik
