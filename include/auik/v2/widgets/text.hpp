#pragma once

#include <auik/v2/detail/text.hpp>
#include <auik/v2/theme.hpp>
#include "widget.hpp"

#define AUIK_TAG_TEXT 0x60F46B05u

namespace auik::v2
{
    constexpr inline WidgetFlags get_default_text_flags() { return get_default_widget_flags(); }

    constexpr inline WidgetFlags get_default_fixed_text_flags()
    {
        return get_default_text_flags() | WidgetFlagBits::fixed_layout;
    }

    class APPLIB_API Text : public Widget
    {
    public:
        TextFlags text_flags = TextFlagBits::none;

        Text(u32 id, acul::string text, amal::vec2 size, WidgetFlags flags, Widget *parent = nullptr,
             u32 style_tag_id = AUIK_STYLE_TAG_NO_PAD,
             detail::TextOverflowMode overflow = detail::TextOverflowMode::ellipsis,
             detail::TextVerticalAlign vertical_align = detail::TextVerticalAlign::top,
             detail::TextWrapMode wrap = detail::TextWrapMode::none,
             detail::TextLayoutWidthMode width_mode = detail::TextLayoutWidthMode::bounds)
            : Widget(id, flags, EventFlagBits::none, parent, {{0.0f}, size}),
              _text(std::move(text)),
              _style({Theme::STYLE_ID_INVALID, style_tag_id})
        {
            _layout_config.overflow = overflow;
            _layout_config.wrap = wrap;
            _layout_config.width_mode = width_mode;
            _render_config.vertical_align = vertical_align;
        }
        StyleUpdateFlags update_style() override;
        void update_layout_min_size() override;
        void update_layout(bool min_size_known) override;
        void translate(const amal::vec2 &delta) override;
        void update_depth(const amal::vec2 &depth_range) override;
        void rebuild_clip_rects() override;
        void reset_draw_records() override;
        void invalidate_draw_records();
        void draw(DrawCtx &ctx) override;

        const acul::string &text() const { return _text; }
        void set_text(const acul::string &text)
        {
            if (_text == text) return;
            _text = text;
        }

        const detail::TextLayoutResult &layout_result() const { return _layout_result; }

        bool multiline() const { return _layout_config.wrap == detail::TextWrapMode::word; }
        void set_multiline(bool value)
        {
            const auto next = value ? detail::TextWrapMode::word : detail::TextWrapMode::none;
            if (_layout_config.wrap == next) return;
            _layout_config.wrap = next;
        }

        detail::TextOverflowMode overflow_mode() const { return _layout_config.overflow; }
        void set_overflow_mode(detail::TextOverflowMode value)
        {
            if (_layout_config.overflow == value) return;
            _layout_config.overflow = value;
        }

        bool trim_trailing_spaces() const { return _layout_config.trim_trailing_spaces; }
        void set_trim_trailing_spaces(bool value)
        {
            if (_layout_config.trim_trailing_spaces == value) return;
            _layout_config.trim_trailing_spaces = value;
        }

        detail::TextLayoutWidthMode width_mode() const { return _layout_config.width_mode; }
        void set_width_mode(detail::TextLayoutWidthMode value)
        {
            if (_layout_config.width_mode == value) return;
            _layout_config.width_mode = value;
        }

        detail::TextHorizontalAlign horizontal_align() const { return _render_config.horizontal_align; }
        void set_horizontal_align(detail::TextHorizontalAlign value)
        {
            if (_render_config.horizontal_align == value) return;
            _render_config.horizontal_align = value;
        }

        detail::TextVerticalAlign vertical_align() const { return _render_config.vertical_align; }
        void set_vertical_align(detail::TextVerticalAlign value)
        {
            assert(value != detail::TextVerticalAlign::none && "Text vertical alignment does not support none");
            if (_render_config.vertical_align == value) return;
            _render_config.vertical_align = value;
        }

        u32 max_lines() const { return _layout_config.max_lines; }
        void set_max_lines(u32 value)
        {
            if (_layout_config.max_lines == value) return;
            _layout_config.max_lines = value;
        }

        bool tight_content_height() const { return _tight_content_height; }
        void set_tight_content_height(bool value)
        {
            if (_tight_content_height == value) return;
            _tight_content_height = value;
        }

        size_t draw_record_count() const { return _draw_ids.size(); }
        size_t layout_instance_count() const { return _instances.size(); }

    protected:
        acul::string _text;
        StyleSelector _style;
        detail::TextLayoutConfig _layout_config{};
        detail::TextRenderConfig _render_config{};
        detail::TextLayoutResult _layout_result{};
        amal::rect _content_bounds{};
        acul::vector<TexturesInstanceData> _instances;
        acul::vector<DrawDataID> _draw_ids;
        u32 _hit_id = AUIK_INVALID_DRAW_DATA_ID;
        bool _instances_gpu_dirty = true;
        bool _tight_content_height = false;
        u16 _applied_clip_id = 0xFFFFu;

        void update_content_bounds();

    private:
        bool rebuild_text_buffers(const amal::vec2 &bounds_size);
    };

    class APPLIB_API TextWithTooltip : public Text
    {
    public:
        TextWithTooltip(u32 id, acul::string text, acul::string tooltip_text, amal::vec2 size, WidgetFlags flags,
                        Widget *parent = nullptr, u32 style_tag_id = AUIK_STYLE_TAG_NO_PAD,
                        detail::TextOverflowMode overflow = detail::TextOverflowMode::ellipsis,
                        detail::TextVerticalAlign vertical_align = detail::TextVerticalAlign::top,
                        detail::TextWrapMode wrap = detail::TextWrapMode::none)
            : Text(id, std::move(text), size, flags | WidgetFlagBits::hittable, parent, style_tag_id, overflow,
                   vertical_align, wrap),
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

    inline Text *make_text(u32 id, const acul::string &text = "")
    {
        auto *out = acul::alloc<Text>(id, text, amal::vec2{0.0f, 0.0f}, get_default_text_flags(), nullptr,
                                      Theme::STYLE_ID_INVALID, detail::TextOverflowMode::ellipsis,
                                      detail::TextVerticalAlign::center);
        return out;
    }

    inline Text *make_fixed_text(u32 id, const acul::string &text = "", amal::vec2 size = {0.0f, 0.0f})
    {
        return acul::alloc<Text>(id, text, size, get_default_fixed_text_flags(), nullptr, Theme::STYLE_ID_INVALID,
                                 detail::TextOverflowMode::ellipsis, detail::TextVerticalAlign::center);
    }

    inline TextWithTooltip *make_text_with_tooltip(u32 id, const acul::string &text = "",
                                                   const acul::string &tooltip_text = "")
    {
        auto *out = acul::alloc<TextWithTooltip>(id, text, tooltip_text, amal::vec2{0.0f, 0.0f},
                                                 get_default_text_flags(), nullptr, Theme::STYLE_ID_INVALID,
                                                 detail::TextOverflowMode::ellipsis, detail::TextVerticalAlign::center);
        return out;
    }

    inline TextWithTooltip *make_fixed_text_with_tooltip(u32 id, const acul::string &text = "",
                                                         const acul::string &tooltip_text = "",
                                                         amal::vec2 size = {0.0f, 0.0f})
    {
        return acul::alloc<TextWithTooltip>(id, text, tooltip_text, size, get_default_fixed_text_flags(), nullptr,
                                            Theme::STYLE_ID_INVALID, detail::TextOverflowMode::ellipsis,
                                            detail::TextVerticalAlign::center);
    }
} // namespace auik::v2
