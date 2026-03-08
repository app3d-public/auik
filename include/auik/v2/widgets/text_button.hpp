#pragma once

#include "../theme.hpp"
#include "widget.hpp"

#define AUIK_TAG_TEXT_BUTTON 0x6544FF93

namespace auik::v2
{
    class APPLIB_API TextButton : public Widget
    {
    public:
        TextButton(u32 id, amal::vec2 size = {120.0f, 0.0f}, WidgetFlags widget_flags = get_default_widget_flags(),
                   Widget *parent = nullptr)
            : Widget(id, widget_flags, parent, {0.0f, 0.0f}, size, AUIK_TAG_TEXT_BUTTON),
              _style({0, AUIK_TAG_TEXT_BUTTON})
        {
        }

        void update_style() override;
        void update_layout() override;
        void rebuild_clip_rects() override;
        void on_hover(HoverState state, u32 prev_tag_id) override;
        void on_click(MouseKey key, KeyPressState state, u32 click_count) override;

        void draw(DrawCtx &ctx) override;

    private:
        DrawDataID _bg;
        StyleSelector _style;
    };
} // namespace auik::v2
