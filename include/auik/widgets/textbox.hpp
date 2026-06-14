#pragma once

#include "../detail/text_edit.hpp"
#include "detail/scrollbar.hpp"
#include "text.hpp"

#define AUIK_TAG_TEXTBOX         0x37C7A6D1u
#define AUIK_TAG_MULTILINE_CARET 0x7D6F3E2Au

namespace auik
{
    struct TextBoxEditData;

    constexpr inline WidgetFlags get_default_textbox_flags()
    {
        return get_default_widget_flags() | WidgetFlagBits::hittable;
    }

    constexpr inline WidgetFlags get_default_fixed_textbox_flags()
    {
        return get_default_textbox_flags() | WidgetFlagBits::fixed_layout;
    }

    class TextBox : public Widget
    {
    public:
        TextFlags text_flags = TextFlagBits::none;

        AUIK_EXPORT TextBox(u32 id, const acul::string &value, amal::vec2 size, WidgetFlags flags, Widget *parent = nullptr,
                u32 style_tag_id = AUIK_STYLE_TAG_TEXTBOX, TextFlags text_flags = TextFlagBits::none,
                const acul::string &placeholder = {}, bool read_only = false,
                detail::TextVerticalAlign text_vertical_align = detail::TextVerticalAlign::center,
                detail::TextWrapMode text_wrap = detail::TextWrapMode::none);
        AUIK_EXPORT ~TextBox() override;

        AUIK_EXPORT StyleUpdateFlags update_style() override;
        AUIK_EXPORT void update_layout_min_size() override;
        AUIK_EXPORT void update_layout(bool min_size_known) override;
        AUIK_EXPORT void translate(const amal::vec2 &delta) override;
        AUIK_EXPORT void rebuild_clip_rects() override;
        AUIK_EXPORT void update_depth(const amal::vec2 &depth_range) override;
        AUIK_EXPORT void back_hit_depth() override;
        AUIK_EXPORT void restore_hit_depth() override;
        AUIK_EXPORT void draw(DrawCtx &ctx) override;
        AUIK_EXPORT void on_scroll(const amal::vec2 &delta) override;
        AUIK_EXPORT void on_focus(bool focused) override;
        AUIK_EXPORT void on_hover(HoverState state) override;
        AUIK_EXPORT void on_click(MouseKey key, KeyPressState state, u32 click_count) override;
        AUIK_EXPORT void on_drag(const amal::vec2 &delta, KeyPressState state) override;
        AUIK_EXPORT void on_key(Key key, KeyPressState state, KeyMode mods) override;
        AUIK_EXPORT void on_char_input(u32 char_code, u32 count) override;
        AUIK_EXPORT void sync_widget_flags() override;

        const acul::string &value() const { return _value; }
        void set_value(const acul::string &value)
        {
            if (this->value() == value) return;
            set_value_internal(value);
        }

        const inline acul::string &placeholder() const
        {
            static const acul::string empty;
            return _placeholder ? _placeholder->text() : empty;
        }
        AUIK_EXPORT void set_placeholder(const acul::string &value);

    protected:
        AUIK_EXPORT void set_value_internal(const acul::string &value);

        inline bool show_placeholder() const
        {
            return detail::get_context().focus_id != id() && value().empty() && _placeholder;
        }

        inline void apply_render_update(bool layout_dirty, DrawReasonFlags reason = DrawReasonBits::external)
        {
            if (layout_dirty)
            {
                if (should_resize_to_content() && parent())
                {
                    parent()->update_layout(false);
                    rebuild_selection_rect_cache();
                    if (edit_draw_slots_need_record()) redraw_all_commands();
                    else parent()->update_draw_commands(DrawReasonBits::layout);
                    detail::get_context().dirty_flags |= DirtyFlagBits::redraw;
                    update_transient_draw_commands();
                    return;
                }
                if (has_internal_scrollbar())
                {
                    const size_t text_draw_count = _text.draw_record_count();
                    const f32 old_scroll_y = _content_scroll.y;
                    update_layout_from_current_bounds(false);
                    if (update_content_scroll_y_for_cursor() && old_scroll_y != _content_scroll.y)
                        update_layout_from_current_bounds(true);
                    rebuild_selection_rect_cache();
                    if (_text.layout_instance_count() < text_draw_count || edit_draw_slots_need_record())
                    {
                        redraw_all_commands();
                        update_transient_draw_commands();
                        return;
                    }
                    redraw_external(_bg.render_id != AUIK_INVALID_DRAW_DATA_ID, DrawReasonBits::layout);
                    update_transient_draw_commands();
                    return;
                }
                const size_t text_draw_count = _text.draw_record_count();
                _text.set_position(_content_pos);
                _text.set_layout_size(_content_size);
                _text.set_clip_id(text_content_clip_id());
                _text.update_layout(false);
                update_content_scroll_x_for_cursor();
                update_content_scroll_y_for_cursor();
                _text.translate(-_content_scroll);
                _text.set_clip_id(text_content_clip_id());
                refresh_placeholder_layout();
                detail::get_context().dirty_flags &= ~DirtyFlagBits::layout;
                rebuild_selection_rect_cache();
                if (_text.layout_instance_count() < text_draw_count || edit_draw_slots_need_record())
                {
                    redraw_all_commands();
                    return;
                }
                reason |= DrawReasonBits::layout;
            }
            else
            {
                const amal::vec2 old_scroll = _content_scroll;
                if (update_content_scroll_x_for_cursor() && old_scroll.x != _content_scroll.x)
                    _text.translate({old_scroll.x - _content_scroll.x, 0.0f});
                if (update_content_scroll_y_for_cursor() && old_scroll.y != _content_scroll.y)
                    update_layout_from_current_bounds(true);
                rebuild_selection_rect_cache();
                if (edit_draw_slots_need_record())
                {
                    redraw_all_commands();
                    update_transient_draw_commands();
                    return;
                }
            }
            redraw_external(_bg.render_id != AUIK_INVALID_DRAW_DATA_ID, reason);
            update_transient_draw_commands();
        }

        inline void commit_text_edit(bool layout_dirty = true)
        {
            if (layout_dirty)
                for (auto &cursor : _edit_state.cursors) cursor.has_preferred_x = 0;
            reset_caret_blink();
            schedule_caret_blink();
        }

        AUIK_EXPORT detail::TextEditString make_text_edit_string();
        AUIK_EXPORT int cursor_from_point(const amal::vec2 &point) const;
        AUIK_EXPORT amal::vec2 cursor_screen_pos(int cursor) const;
        AUIK_EXPORT int cursor_from_line_x(u32 line_index, f32 x) const;
        AUIK_EXPORT f32 cursor_x_on_line(u32 line_index, int cursor) const;
        AUIK_EXPORT f32 line_screen_y(u32 line_index) const;
        AUIK_EXPORT amal::rect caret_rect(u32 line_index, f32 x, f32 width) const;
        AUIK_EXPORT amal::rect line_selection_rect(u32 line_index, f32 x0, f32 x1) const;
        AUIK_EXPORT bool update_content_scroll_x_for_cursor();
        AUIK_EXPORT bool update_content_scroll_y_for_cursor();
        u16 text_content_clip_id() const { return _content_clip_id != 0xFFFFu ? _content_clip_id : clip_id(); }
        AUIK_EXPORT void update_text_content_clip_rect();
        AUIK_EXPORT void rebuild_selection_rect_cache();
        AUIK_EXPORT void draw_background(DrawCtx &ctx, DrawStream *quads_stream);
        AUIK_EXPORT void draw_selection(DrawCtx &ctx, DrawStream *quads_stream, f32 selection_z);
        AUIK_EXPORT void draw_text_content(DrawCtx &ctx);
        AUIK_EXPORT void draw_password_content(DrawCtx &ctx);
        AUIK_EXPORT void draw_scrollbar(DrawCtx &ctx);
        AUIK_EXPORT void draw_caret(DrawCtx &ctx);
        AUIK_EXPORT void draw_selection_drag_icon(DrawCtx &ctx, f32 selection_z);
        AUIK_EXPORT u32 line_index_from_cursor(int cursor) const;
        AUIK_EXPORT u32 line_index_from_cursor(int cursor, bool cursor_at_end_of_line) const;
        AUIK_EXPORT void refresh_text_layout_for_editing();
        AUIK_EXPORT void refresh_placeholder_layout();
        AUIK_EXPORT u32 required_selection_draw_slots() const;
        AUIK_EXPORT bool edit_draw_slots_need_record() const;
        AUIK_EXPORT void move_cursor_vertical(int dir, bool select);
        AUIK_EXPORT bool selection_contains_point(const amal::vec2 &point) const;
        AUIK_EXPORT bool begin_selection_drag_press();
        AUIK_EXPORT void collapse_cursor_at_point(const amal::vec2 &point);
        AUIK_EXPORT void select_text_range(int start, int end);
        AUIK_EXPORT void select_word_at_point(const amal::vec2 &point);
        AUIK_EXPORT void select_line_at_point(const amal::vec2 &point);
        AUIK_EXPORT void select_all_text();
        AUIK_EXPORT void copy_selection_to_clipboard();
        AUIK_EXPORT void cut_selection_to_clipboard();
        AUIK_EXPORT void paste_clipboard_at_cursor();
        AUIK_EXPORT void delete_selection();
        AUIK_EXPORT bool move_selection_to_cursor(int drop_cursor, bool copy);
        AUIK_EXPORT void end_selection_drag(bool commit);
        virtual bool accepts_newline() const { return false; }
        virtual bool should_resize_to_content() const { return false; }
        virtual bool has_internal_scrollbar() const { return false; }
        virtual bool should_draw_caret() const { return true; }
        bool is_password() const { return text_flags & TextFlagBits::password; }
        AUIK_EXPORT void sync_text_presentation();
        AUIK_EXPORT void reset_caret_blink();
        AUIK_EXPORT void schedule_caret_blink();
        AUIK_EXPORT void tick_caret_blink();
        AUIK_EXPORT void update_transient_draw_commands();
        AUIK_EXPORT void update_layout_from_current_bounds(bool min_size_known);
        AUIK_EXPORT void on_disabled_changed(bool disabled) override;

        acul::string _value;
        EText _text;
        Text *_placeholder = nullptr;
        TextBoxEditData *_edit = nullptr;
        detail::TextEditState _edit_state{};
        DrawDataID _bg{};
        StyleSelector _style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_TEXTBOX};
        amal::vec2 _content_pos{0.0f, 0.0f};
        amal::vec2 _content_size{0.0f, 0.0f};
        u16 _content_clip_id = 0xFFFFu;
        detail::Scrollbar *_scrollbar_y = nullptr;
        detail::Scrollbar *_drag_scrollbar = nullptr;
        amal::vec2 _content_scroll{0.0f, 0.0f};

    private:
        static int edit_string_len(void *user_data);
        static detail::TextEditChar edit_get_char(void *user_data, int char_idx);
        static bool edit_insert_chars(void *user_data, int pos, const detail::TextEditChar *text, int text_len);
        static void edit_delete_chars(void *user_data, int pos, int count);
        static bool edit_replace_chars(void *user_data, int pos, int delete_count, const detail::TextEditChar *text,
                                       int text_len);
        static int edit_next_char_index(void *user_data, int idx);
        static int edit_prev_char_index(void *user_data, int idx);
    };

    class MultilineTextBox final : public TextBox
    {
    public:
        AUIK_EXPORT MultilineTextBox(u32 id, const acul::string &value, amal::vec2 size, bool can_expand_to_content,
                         WidgetFlags flags, Widget *parent = nullptr, TextFlags text_flags = TextFlagBits::none,
                         const acul::string &placeholder = {}, bool read_only = false);

        bool can_expand_to_content() const { return _can_expand_to_content; }
        AUIK_EXPORT void set_can_expand_to_content(bool value);
        bool resize_to_content() const { return can_expand_to_content(); }
        void set_resize_to_content(bool value) { set_can_expand_to_content(value); }

    protected:
        bool accepts_newline() const override { return true; }
        bool should_resize_to_content() const override { return _can_expand_to_content; }
        bool has_internal_scrollbar() const override { return !_can_expand_to_content; }

    private:
        bool _can_expand_to_content = false;
    };

    inline TextBox *make_textbox(u32 id, const acul::string &value, const acul::string &placeholder = {},
                                 TextFlags text_flags = TextFlagBits::none, bool read_only = false)
    {
        return acul::alloc<TextBox>(id, value, amal::vec2{0.0f, 0.0f},
                                    WidgetFlagBits::visible | WidgetFlagBits::attachable | WidgetFlagBits::configurable,
                                    nullptr, AUIK_TAG_TEXTBOX, text_flags, placeholder, read_only);
    }

    inline TextBox *make_fixed_textbox(u32 id, const acul::string &value, f32 width = 240.0f,
                                       TextFlags text_flags = TextFlagBits::none, const acul::string &placeholder = {},
                                       bool read_only = false)
    {
        return acul::alloc<TextBox>(id, value, amal::vec2{width, 0.0f},
                                    WidgetFlagBits::visible | WidgetFlagBits::attachable |
                                        WidgetFlagBits::configurable | WidgetFlagBits::fixed_layout,
                                    nullptr, AUIK_TAG_TEXTBOX, text_flags, placeholder, read_only);
    }

    // Auto-width multiline input. height is the minimum/control height; when can_expand_to_content is true the widget
    // grows vertically to fit text, otherwise overflowing text is clipped and can be scrolled internally.
    inline MultilineTextBox *make_multiline_textbox(u32 id, const acul::string &value, f32 height = 96.0f,
                                                    bool can_expand_to_content = false,
                                                    TextFlags text_flags = TextFlagBits::none,
                                                    const acul::string &placeholder = {}, bool read_only = false)
    {
        return acul::alloc<MultilineTextBox>(id, value, amal::vec2{0.0f, height}, can_expand_to_content,
                                             WidgetFlagBits::visible | WidgetFlagBits::attachable |
                                                 WidgetFlagBits::configurable,
                                             nullptr, text_flags, placeholder, read_only);
    }

    inline MultilineTextBox *make_multiline_textbox(u32 id, const acul::string &value, bool can_expand_to_content,
                                                    TextFlags text_flags = TextFlagBits::none,
                                                    const acul::string &placeholder = {}, bool read_only = false)
    {
        return make_multiline_textbox(id, value, 96.0f, can_expand_to_content, text_flags, placeholder, read_only);
    }

    // Fixed multiline input. Both width and height are fixed; overflowing text is clipped and can be scrolled
    // internally instead of resizing the widget.
    inline MultilineTextBox *make_fixed_multiline_textbox(u32 id, const acul::string &value,
                                                          amal::vec2 size = {240.0f, 96.0f},
                                                          TextFlags text_flags = TextFlagBits::none,
                                                          const acul::string &placeholder = {}, bool read_only = false)
    {
        return acul::alloc<MultilineTextBox>(id, value, size, false,
                                             WidgetFlagBits::visible | WidgetFlagBits::attachable |
                                                 WidgetFlagBits::configurable | WidgetFlagBits::fixed_layout,
                                             nullptr, text_flags, placeholder, read_only);
    }

    inline MultilineTextBox *make_fixed_multiline_textbox(u32 id, const acul::string &value, amal::vec2 size,
                                                          bool /*can_expand_to_content*/,
                                                          TextFlags text_flags = TextFlagBits::none,
                                                          const acul::string &placeholder = {}, bool read_only = false)
    {
        return make_fixed_multiline_textbox(id, value, size, text_flags, placeholder, read_only);
    }
} // namespace auik
