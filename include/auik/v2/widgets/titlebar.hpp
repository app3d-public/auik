#pragma once

#include <acul/functional/unique_function.hpp>
#include <acul/memory/alloc.hpp>
#include <acul/vector.hpp>
#include <amal/rect.hpp>
#include <auik/v2/detail/gpu_context.hpp>
#include "../theme.hpp"
#include "widget.hpp"

namespace auik::v2
{
    class Image;

    struct TitlebarMetrics
    {
        f32 left_inset = 0.0f;
        f32 right_inset = 0.0f;
        f32 height = 0.0f;
    };

    class APPLIB_API Titlebar final : public Widget
    {
    public:
        acul::unique_function<void(const acul::vector<amal::irect> &)> on_drag_regions_changed = nullptr;

        APPLIB_API explicit Titlebar(u32 id = AUIK_TAG_TITLEBAR,
                                     WidgetFlags widget_flags = get_default_widget_flags() | WidgetFlagBits::fixed |
                                                                WidgetFlagBits::viewport_reserved);
        ~Titlebar() override;

        APPLIB_API void set_metrics(const TitlebarMetrics &metrics);
        const TitlebarMetrics &metrics() const { return _metrics; }
        void set_show_icon(bool value);
        bool show_icon() const { return _show_icon; }
        void add_child(Widget *child);
        void add_children(const acul::vector<Widget *> &children);

        StyleUpdateFlags update_style() override;
        void update_layout_min_size() override;
        void update_layout(bool min_size_known) override;
        void update_depth(const amal::vec2 &depth_range) override;
        void rebuild_clip_rects() override;
        void draw(DrawCtx &ctx) override;
        void translate(const amal::vec2 &delta) override;

    private:
        void ensure_icon_widget();
        void update_drag_regions();

        DrawDataID _bg;
        DrawDataID _icon_bg;
        StyleSelector _style{Theme::STYLE_ID_INVALID, AUIK_TAG_TITLEBAR};
        StyleSelector _icon_style{Theme::STYLE_ID_INVALID, AUIK_TAG_TITLEBAR_ICON};
        TitlebarMetrics _metrics{};
        acul::vector<amal::irect> _drag_regions;
        acul::vector<Widget *> _children;
        Image *_icon = nullptr;
        detail::ImageTextureResource _icon_texture{};
        f32 _content_start_x = 0.0f;
        bool _show_icon = false;
    };

    inline Titlebar *make_titlebar(u32 id = AUIK_TAG_TITLEBAR,
                                   WidgetFlags widget_flags = get_default_widget_flags() | WidgetFlagBits::fixed |
                                                              WidgetFlagBits::viewport_reserved)
    {
        return acul::alloc<Titlebar>(id, widget_flags);
    }
} // namespace auik::v2
