#pragma once

#include <acul/memory/alloc.hpp>
#include <auik/v2/detail/text.hpp>
#include <auik/v2/theme.hpp>
#include "widget.hpp"

namespace auik::v2
{
    constexpr inline WidgetFlags get_default_text_flags() { return get_default_widget_flags(); }

    constexpr inline WidgetFlags get_default_fixed_text_flags()
    {
        return get_default_text_flags() | WidgetFlagBits::fixed;
    }

    class APPLIB_API Text : public Widget
    {
    public:
        TextFlags text_flags = TextFlagBits::none;

        Text(u32 id, acul::string text, amal::vec2 size, WidgetFlags flags, Widget *parent = nullptr,
             u32 style_tag_id = AUIK_TAG_NO_PAD)
            : Widget(id, flags, EventFlagBits::none, parent, {{0.0f}, size}),
              _text(std::move(text)),
              _style({Theme::STYLE_ID_INVALID, style_tag_id})
        {
        }

        StyleUpdateFlags update_style() override;
        void update_layout_min_size() override;
        void update_layout(bool min_size_known) override;
        void translate(const amal::vec2 &delta) override;
        void rebuild_clip_rects() override;
        void draw(DrawCtx &ctx) override;

        const acul::string &text() const { return _text; }
        void set_text(const acul::string &text);

        bool multiline() const { return _layout_config.wrap == detail::TextWrapMode::word; }
        void set_multiline(bool value);

        detail::TextOverflowMode overflow_mode() const { return _layout_config.overflow; }
        void set_overflow_mode(detail::TextOverflowMode value);

        detail::TextHorizontalAlign horizontal_align() const { return _render_config.horizontal_align; }
        void set_horizontal_align(detail::TextHorizontalAlign value);

        detail::TextVerticalAlign vertical_align() const { return _render_config.vertical_align; }
        void set_vertical_align(detail::TextVerticalAlign value);

        u32 max_lines() const { return _layout_config.max_lines; }
        void set_max_lines(u32 value);

        const amal::vec4 &color() const { return _render_config.tint_color; }
        void set_color(const amal::vec4 &color);

    protected:
        void mark_layout_dirty();
        void update_content_bounds();
        acul::string _text;
        StyleSelector _style;
        bool _use_style_text_color = true;
        detail::TextLayoutConfig _layout_config{};
        detail::TextRenderConfig _render_config{};
        detail::TextLayoutResult _layout_result{};
        amal::rect _content_bounds{};
        acul::vector<TexturesInstanceData> _instances;
        acul::vector<DrawDataID> _draw_ids;
        u32 _hit_id = AUIK_INVALID_DRAW_DATA_ID;
        bool _instances_gpu_dirty = true;
        u16 _applied_clip_id = 0xFFFFu;

    private:
        bool rebuild_text_buffers(const amal::vec2 &bounds_size);
    };

    class APPLIB_API TextWithTooltip : public Text
    {
    public:
        TextWithTooltip(u32 id, acul::string text, acul::string tooltip_text, amal::vec2 size, WidgetFlags flags,
                        Widget *parent = nullptr, u32 style_tag_id = AUIK_TAG_NO_PAD)
            : Text(id, std::move(text), size, flags | WidgetFlagBits::hittable, parent, style_tag_id),
              _tooltip_text(std::move(tooltip_text))
        {
            add_event_flags(EventFlagBits::hover);
        }

        ~TextWithTooltip() override;

        const acul::string &tooltip_text() const { return _tooltip_text; }
        void set_tooltip_text(const acul::string &text) { _tooltip_text = text; }
        void on_hover(HoverState state) override;

    private:
        acul::string _tooltip_text;
    };

    inline Text *make_text(u32 id, const acul::string &text = "", amal::vec2 size = {0.0f, 0.0f})
    {
        return acul::alloc<Text>(id, text, size, get_default_text_flags(), nullptr, Theme::STYLE_ID_INVALID);
    }

    inline Text *make_fixed_text(u32 id, const acul::string &text = "", amal::vec2 size = {0.0f, 0.0f})
    {
        return acul::alloc<Text>(id, text, size, get_default_fixed_text_flags(), nullptr, Theme::STYLE_ID_INVALID);
    }

    inline TextWithTooltip *make_text_with_tooltip(u32 id, const acul::string &text = "",
                                                   const acul::string &tooltip_text = "",
                                                   amal::vec2 size = {0.0f, 0.0f})
    {
        return acul::alloc<TextWithTooltip>(id, text, tooltip_text, size, get_default_text_flags(), nullptr,
                                            Theme::STYLE_ID_INVALID);
    }

    inline TextWithTooltip *make_fixed_text_with_tooltip(u32 id, const acul::string &text = "",
                                                         const acul::string &tooltip_text = "",
                                                         amal::vec2 size = {0.0f, 0.0f})
    {
        return acul::alloc<TextWithTooltip>(id, text, tooltip_text, size, get_default_fixed_text_flags(), nullptr,
                                            Theme::STYLE_ID_INVALID);
    }
} // namespace auik::v2
