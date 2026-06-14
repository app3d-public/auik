#pragma once

#include <auik/detail/text.hpp>
#include <auik/detail/text_edit.hpp>
#include <auik/theme.hpp>
#include "widget.hpp"

#define AUIK_TAG_TEXT 0x60F46B05u

namespace auik
{
    constexpr inline WidgetFlags get_default_text_flags() { return get_default_widget_flags(); }

    constexpr inline WidgetFlags get_default_fixed_text_flags()
    {
        return get_default_text_flags() | WidgetFlagBits::fixed_layout;
    }

    constexpr inline WidgetFlags make_etext_flags()
    {
        return get_default_text_flags() | WidgetFlagBits::hittable | WidgetFlagBits::read_only;
    }

    constexpr inline WidgetFlags make_fixed_etext_flags() { return make_etext_flags() | WidgetFlagBits::fixed_layout; }

    class Text : public Widget
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
        AUIK_EXPORT StyleUpdateFlags update_style() override;
        AUIK_EXPORT void update_layout_min_size() override;
        AUIK_EXPORT void update_layout(bool min_size_known) override;
        AUIK_EXPORT void translate(const amal::vec2 &delta) override;
        AUIK_EXPORT void update_depth(const amal::vec2 &depth_range) override;
        AUIK_EXPORT void rebuild_clip_rects() override;
        AUIK_EXPORT void reset_draw_records() override;
        AUIK_EXPORT void invalidate_draw_records();
        AUIK_EXPORT void draw(DrawCtx &ctx) override;

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
        const PostFxChain *_applied_post_fx_chain = nullptr;

        AUIK_EXPORT void update_content_bounds();

    private:
        bool rebuild_text_buffers(const amal::vec2 &bounds_size);
    };

    class EText : public Text
    {
    public:
        AUIK_EXPORT EText(u32 id, acul::string text, amal::vec2 size, WidgetFlags flags = make_etext_flags(),
                         Widget *parent = nullptr, u32 style_tag_id = AUIK_STYLE_TAG_NO_PAD,
                         detail::TextOverflowMode overflow = detail::TextOverflowMode::ellipsis,
                         detail::TextVerticalAlign vertical_align = detail::TextVerticalAlign::top,
                         detail::TextWrapMode wrap = detail::TextWrapMode::none,
                         detail::TextLayoutWidthMode width_mode = detail::TextLayoutWidthMode::bounds);
        AUIK_EXPORT ~EText() override;

        AUIK_EXPORT void sync_widget_flags() override;
        AUIK_EXPORT StyleUpdateFlags update_style() override;
        AUIK_EXPORT void update_layout_min_size() override;
        AUIK_EXPORT void translate(const amal::vec2 &delta) override;
        AUIK_EXPORT void reset_draw_records() override;
        AUIK_EXPORT void draw(DrawCtx &ctx) override;
        AUIK_EXPORT void on_hover(HoverState state) override;
        AUIK_EXPORT void on_focus(bool focused) override;
        AUIK_EXPORT void on_click(MouseKey key, KeyPressState state, u32 click_count) override;
        AUIK_EXPORT void on_drag(const amal::vec2 &delta, KeyPressState state) override;
        AUIK_EXPORT void on_key(Key key, KeyPressState state, KeyMode mods) override;
        AUIK_EXPORT void on_char_input(u32 char_code, u32 count) override;

    private:
        struct ETextEditData *_edit = nullptr;
        detail::TextEditState _edit_state{};
        StyleSelector _caret_style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_CARET};
        StyleSelector _selection_style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_SELECTION};
        DrawDataID _caret{};
        acul::vector<DrawDataID> _selection_draw_ids;
        acul::vector<amal::rect> _selection_rects;
        int _drag_anchor = 0;
        bool _dragging_selection = false;

        detail::TextEditString make_text_edit_string();
        int cursor_from_point(const amal::vec2 &point) const;
        int cursor_from_line_x(u32 line_index, f32 x) const;
        f32 cursor_x_on_line(u32 line_index, int cursor) const;
        u32 line_index_from_cursor(int cursor) const;
        amal::vec2 cursor_screen_pos(int cursor) const;
        amal::rect caret_rect(u32 line_index, f32 x, f32 width) const;
        amal::rect line_selection_rect(u32 line_index, f32 x0, f32 x1) const;
        void collapse_cursor_at_point(const amal::vec2 &point);
        void select_text_range(int start, int end);
        void select_word_at_point(const amal::vec2 &point);
        void select_all_text();
        void copy_selection_to_clipboard();
        void delete_selection();
        void rebuild_selection_rect_cache();
        void reset_caret_blink();
        void schedule_caret_blink();
        void apply_edit_render_update(bool layout_dirty);
        void update_transient_draw_commands();
        bool edit_draw_slots_need_record() const;
        bool should_show_lazy_tooltip() const;
        void schedule_lazy_tooltip();
        void clear_lazy_tooltip(bool clear_source);
        void on_disabled_changed(bool disabled) override;
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

    inline EText *make_etext(u32 id, const acul::string &text = "")
    {
        return acul::alloc<EText>(id, text, amal::vec2{0.0f, 0.0f}, make_etext_flags(), nullptr,
                                  Theme::STYLE_ID_INVALID, detail::TextOverflowMode::ellipsis,
                                  detail::TextVerticalAlign::center);
    }

    inline EText *make_fixed_etext(u32 id, const acul::string &text = "", amal::vec2 size = {0.0f, 0.0f})
    {
        return acul::alloc<EText>(id, text, size, make_fixed_etext_flags(), nullptr, Theme::STYLE_ID_INVALID,
                                  detail::TextOverflowMode::ellipsis, detail::TextVerticalAlign::center);
    }
} // namespace auik
