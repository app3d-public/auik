#pragma once

#include <acul/memory/alloc.hpp>
#include "../theme.hpp"
#include "widget.hpp"

#define AUIK_TAG_TEXT_BUTTON 0x6544FF93

namespace auik::v2
{
    constexpr inline WidgetFlags get_default_text_button_flags()
    {
        return get_default_widget_flags() | WidgetFlagBits::hittable;
    }

    constexpr inline WidgetFlags get_default_fixed_text_button_flags()
    {
        return get_default_text_button_flags() | WidgetFlagBits::fixed;
    }

    class APPLIB_API TextButton : public Widget
    {
    public:
        TextButton(u32 id, amal::vec2 size = {120.0f, 0.0f},
                   WidgetFlags widget_flags = get_default_text_button_flags(),
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

    inline TextButton *make_text_button(u32 id, amal::vec2 size = {0.0f, 0.0f}, Widget *parent = nullptr)
    {
        return acul::alloc<TextButton>(id, size, get_default_text_button_flags(), parent);
    }

    inline TextButton *make_fixed_text_button(u32 id, amal::vec2 size = {120.0f, 0.0f}, Widget *parent = nullptr)
    {
        return acul::alloc<TextButton>(id, size, get_default_fixed_text_button_flags(), parent);
    }
} // namespace auik::v2
