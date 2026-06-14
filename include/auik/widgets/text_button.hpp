#pragma once

#include <acul/memory/alloc.hpp>
#include "../theme.hpp"
#include "text.hpp"
#include "widget.hpp"

#define AUIK_TAG_TEXT_BUTTON 0x6544FF93

namespace auik
{
    constexpr inline WidgetFlags get_default_text_button_flags()
    {
        return get_default_widget_flags() | WidgetFlagBits::hittable;
    }

    constexpr inline WidgetFlags get_default_fixed_text_button_flags()
    {
        return get_default_text_button_flags() | WidgetFlagBits::fixed_layout;
    }

    class TextButton : public Widget
    {
    public:
        TextButton(u32 id, acul::string text, amal::vec2 size, WidgetFlags widget_flags, EventFlags event_flags,
                   Widget *parent, u32 style_tag_id = AUIK_STYLE_TAG_TEXT_BUTTON)
            : Widget(id, widget_flags, event_flags, parent, {{0.0f, 0.0f}, size}, style_tag_id),
              _style({Theme::STYLE_ID_INVALID, style_tag_id}),
              _text(acul::alloc<Text>(AUIK_TAG_TEXT, std::move(text), amal::vec2{0.0f, 0.0f},
                                      WidgetFlagBits::visible | WidgetFlagBits::fixed_layout, this))
        {
            _text->set_horizontal_align(detail::TextHorizontalAlign::center);
            _text->set_vertical_align(detail::TextVerticalAlign::center);
        }
        ~TextButton() override { acul::release(_text); }

        AUIK_EXPORT StyleUpdateFlags update_style() override;
        AUIK_EXPORT void update_layout_min_size() override;
        AUIK_EXPORT void update_layout(bool min_size_known) override;
        AUIK_EXPORT void translate(const amal::vec2 &delta) override;
        AUIK_EXPORT void rebuild_clip_rects() override;
        AUIK_EXPORT void update_depth(const amal::vec2 &depth_range) override;
        AUIK_EXPORT void back_hit_depth() override;
        AUIK_EXPORT void restore_hit_depth() override;

        AUIK_EXPORT void draw(DrawCtx &ctx) override;

    private:
        DrawDataID _bg;
        StyleSelector _style;
        Text *_text = nullptr;
        u32 _resolved_text_color{0};
        bool _text_draw_dirty = true;
    };

    inline TextButton *make_text_button(u32 id, const acul::string &text = "",
                                        u32 style_tag_id = AUIK_STYLE_TAG_TEXT_BUTTON)
    {
        return acul::alloc<TextButton>(id, text, amal::vec2{0.0f, 0.0f}, get_default_text_button_flags(),
                                       EventFlagBits::none, nullptr, style_tag_id);
    }

    inline TextButton *make_fixed_text_button(u32 id, const acul::string &text = "", amal::vec2 size = {120.0f, 0.0f},
                                              u32 style_tag_id = AUIK_STYLE_TAG_TEXT_BUTTON)
    {
        return acul::alloc<TextButton>(id, text, size, get_default_fixed_text_button_flags(), EventFlagBits::none,
                                       nullptr, style_tag_id);
    }
} // namespace auik
