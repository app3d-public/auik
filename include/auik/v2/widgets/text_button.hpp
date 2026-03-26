#pragma once

#include <acul/memory/alloc.hpp>
#include "text.hpp"
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
        TextButton(u32 id, acul::string text, amal::vec2 size, WidgetFlags widget_flags, EventFlags event_flags,
                   Widget *parent)
            : Widget(id, widget_flags, event_flags, parent, {}, AUIK_TAG_TEXT_BUTTON),
              _style({Theme::STYLE_ID_INVALID, AUIK_TAG_TEXT_BUTTON}),
              _text(acul::alloc<Text>(id ^ 0xA8115EEDu, std::move(text), amal::vec2{0.0f, 0.0f},
                                       get_default_fixed_text_flags(), this))
        {
            _text->set_horizontal_align(detail::TextHorizontalAlign::center);
            _text->set_vertical_align(detail::TextVerticalAlign::center);
        }
        ~TextButton() override { acul::release(_text); }

        StyleUpdateFlags update_style() override;
        void update_layout_min_size() override;
        void update_layout(bool min_size_known) override;
        void translate(const amal::vec2 &delta) override;
        void rebuild_clip_rects() override;
        void update_depth(const amal::vec2 &depth_range) override;

        void draw(DrawCtx &ctx) override;

    private:
        DrawDataID _bg;
        StyleSelector _style;
        Text *_text = nullptr;
        amal::vec4 _resolved_text_color{-1.0f, -1.0f, -1.0f, -1.0f};
        bool _text_draw_dirty = true;
    };

    inline TextButton *make_text_button(u32 id, const acul::string &text = "", amal::vec2 size = {0.0f, 0.0f})
    {
        return acul::alloc<TextButton>(id, text, size, get_default_text_button_flags(), EventFlagBits::none, nullptr);
    }

    inline TextButton *make_fixed_text_button(u32 id, const acul::string &text = "", amal::vec2 size = {120.0f, 0.0f})
    {
        return acul::alloc<TextButton>(id, text, size, get_default_fixed_text_button_flags(), EventFlagBits::none,
                                       nullptr);
    }
} // namespace auik::v2
