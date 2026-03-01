#pragma once

#include "theme.hpp"
#include "widget.hpp"

#define AUIK_STYLE_ID_SCROLLBAR_TRACK_TYPE 0x5E57D9C1
#define AUIK_STYLE_ID_SCROLLBAR_THUMB_TYPE 0x0DA3B8EE
#define AUIK_SCROLLBAR_ID                  0x2F8B5D22

namespace auik::v2
{
    class APPLIB_API Scrollbar : public Widget
    {
    public:
        Scrollbar(Widget *parent = nullptr)
            : Widget(AUIK_SCROLLBAR_ID, WidgetFlagBits::visible | WidgetFlagBits::foreground, parent, {0.0f, 0.0f},
                     {0.0f, 0.0f}),
              _track_style({0, AUIK_STYLE_ID_SCROLLBAR_TRACK_TYPE}),
              _thumb_style({0, AUIK_STYLE_ID_SCROLLBAR_THUMB_TYPE})
        {
        }

        void set_visible(bool value)
        {
            if (value)
                widget_flags |= WidgetFlagBits::visible;
            else
                widget_flags &= ~WidgetFlagBits::visible;
        }
        bool is_visible() const { return (widget_flags & WidgetFlagBits::visible); }
        amal::vec4 get_track_margin() const;
        f32 get_min_track_width() const;

        void configure(const amal::vec2 &track_pos, const amal::vec2 &track_size, f32 content_size, f32 view_size);

        void update_style() override;
        void update_layout() override;

        void draw(DrawCtx &ctx) override;

    private:
        DrawDataID _track_draw_id;
        DrawDataID _thumb_draw_id;
        StyleSelector _track_style;
        StyleSelector _thumb_style;
        amal::vec2 _thumb_pos{0.0f, 0.0f};
        amal::vec2 _thumb_size{0.0f, 0.0f};
        f32 _scroll = 0.0f;
    };
} // namespace auik::v2
