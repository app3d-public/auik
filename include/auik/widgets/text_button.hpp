#pragma once

#include "../theme.hpp"
#include "text.hpp"
#include "widget.hpp"

#define AUIK_TAG_TEXT_BUTTON 0x6544FF93u

namespace auik
{
    class TextButton : public Widget
    {
    public:
        TextButton(u32 id, StringView text, amal::vec2 inline_size, WidgetFlags widget_flags, EventFlags event_flags,
                   u32 style_tag_id)
            : Widget(id, widget_flags, event_flags, {{0.0f, 0.0f}, inline_size}, style_tag_id),
              _style({Theme::STYLE_ID_INVALID, style_tag_id}),
              _text(acul::alloc<Text>(AUIK_TAG_TEXT, text, amal::vec2{0.0f, 0.0f}, WidgetFlagBits::visible,
                                      default_text_layout_flags()))
        {
            _text->set_parent(this);
            _text->set_style_tag(AUIK_STYLE_TAG_NO_PAD);
        }
        ~TextButton() override { acul::release(_text); }

        AUIK_EXPORT StyleUpdateFlags update_style() override;
        AUIK_EXPORT void update_layout_min_size_force() override;
        AUIK_EXPORT void update_layout(bool min_size_known) override;
        AUIK_EXPORT void translate(const amal::vec2 &delta) override;
        AUIK_EXPORT void rebuild_clip_rects() override;
        AUIK_EXPORT void update_depth(const amal::vec2 &depth_range) override;
        AUIK_EXPORT void back_hit_depth() override;
        AUIK_EXPORT void restore_hit_depth() override;

        AUIK_EXPORT void draw(DrawCtx &ctx) override;
        u32 signature() const override { return AUIK_TAG_TEXT_BUTTON; }

        const acul::string &text() const { return _text->text(); }
        void set_text(const acul::string &text) { _text->set_text(text); }
        void set_text(StringView text) { _text->set_text(text); }
        void set_model_binding(ModelBinding *binding) { _text->set_model_binding(binding); }
        bool is_translated_text() const { return _text && _text->is_translated_text(); }
        const char *translated_text_literal() const { return _text ? _text->translated_text_literal() : nullptr; }
        u32 style_tag() const { return _style.tag_id; }
        void set_style_tag(u32 tag_id)
        {
            if (_style.tag_id == tag_id) return;
            _style = {Theme::STYLE_ID_INVALID, tag_id};
            set_rect_tag_id(tag_id);
        }

    private:
        DrawDataID _bg;
        StyleSelector _style;
        Text *_text = nullptr;
        u32 _resolved_text_color{0};
        bool _text_draw_dirty = true;
    };

    inline TextButton *make_text_button(u32 id, StringView text = "", amal::vec2 inline_size = AUIK_SIZE_INHERIT)
    {
        constexpr WidgetFlags widget_flags = WidgetFlagBits::visible | WidgetFlagBits::attachable |
                                             WidgetFlagBits::configurable | WidgetFlagBits::hittable;
        return acul::alloc<TextButton>(id, text, inline_size, widget_flags, EventFlagBits::none,
                                       AUIK_STYLE_TAG_TEXT_BUTTON);
    }

    namespace streams
    {
        extern AUIK_EXPORT const umbf::streams::Stream text_button;
    }
} // namespace auik
