#pragma once

#include <auik/v2/detail/text_edit.hpp>
#include "detail/scrollbar.hpp"
#include "text.hpp"

#define AUIK_TAG_TEXTBOX 0x37C7A6D1u

namespace auik::v2
{
    struct TextBoxEditData;

    constexpr inline WidgetFlags get_default_textbox_flags()
    {
        return get_default_widget_flags() | WidgetFlagBits::hittable;
    }

    constexpr inline WidgetFlags get_default_fixed_textbox_flags()
    {
        return get_default_textbox_flags() | WidgetFlagBits::fixed;
    }

    class APPLIB_API TextBox : public Widget
    {
    public:
        TextFlags text_flags = TextFlagBits::none;

        TextBox(u32 id, const acul::string &value, amal::vec2 size, WidgetFlags flags, Widget *parent = nullptr,
                u32 style_tag_id = AUIK_TAG_TEXTBOX, TextFlags text_flags = TextFlagBits::none,
                const acul::string &placeholder = {}, bool read_only = false,
                detail::TextVerticalAlign text_vertical_align = detail::TextVerticalAlign::center,
                detail::TextWrapMode text_wrap = detail::TextWrapMode::none);
        ~TextBox() override;

        StyleUpdateFlags update_style() override;
        void update_layout_min_size() override;
        void update_layout(bool min_size_known) override;
        void translate(const amal::vec2 &delta) override;
        void rebuild_clip_rects() override;
        void update_depth(const amal::vec2 &depth_range) override;
        void draw(DrawCtx &ctx) override;
        void on_scroll(const amal::vec2 &delta) override;
        void on_focus(bool focused) override;
        void on_hover(HoverState state) override;
        void on_click(MouseKey key, KeyPressState state, u32 click_count) override;
        void on_drag(const amal::vec2 &delta, KeyPressState state) override;
        void on_key(Key key, KeyPressState state, KeyMode mods) override;
        void on_char_input(u32 char_code, u32 count) override;

        const acul::string &value() const { return _text.text(); }
        void set_value(const acul::string &value);
        const inline acul::string &placeholder() const
        {
            static const acul::string empty;
            return _placeholder ? _placeholder->text() : empty;
        }
        void set_placeholder(const acul::string &value);

    protected:
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
                    else
                        parent()->update_draw_commands(DrawReasonBits::layout);
                    detail::get_context().dirty_flags |= DirtyFlagBits::redraw;
                    update_transient_draw_commands();
                    return;
                }
                if (has_internal_scrollbar())
                {
                    const size_t text_draw_count = _text.draw_record_count();
                    update_layout(false);
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
                _text.set_size(_content_size);
                _text.update_layout(false);
                update_content_scroll_x_for_cursor();
                _text.translate({-_content_scroll_x, -_content_scroll_y});
                _text.set_clip_id(text_content_clip_id());
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
                const f32 old_scroll_x = _content_scroll_x;
                if (update_content_scroll_x_for_cursor() && old_scroll_x != _content_scroll_x)
                    _text.translate({old_scroll_x - _content_scroll_x, 0.0f});
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

        detail::TextEditString make_text_edit_string();
        int cursor_from_point(const amal::vec2 &point) const;
        amal::vec2 cursor_screen_pos(int cursor) const;
        int cursor_from_line_x(u32 line_index, f32 x) const;
        f32 cursor_x_on_line(u32 line_index, int cursor) const;
        f32 line_screen_y(u32 line_index) const;
        amal::rect line_selection_rect(u32 line_index, f32 x0, f32 x1) const;
        bool update_content_scroll_x_for_cursor();
        u16 text_content_clip_id() const { return _content_clip_id != 0xFFFFu ? _content_clip_id : clip_id(); }
        void update_text_content_clip_rect();
        void rebuild_selection_rect_cache();
        void draw_background(DrawCtx &ctx, DrawStream *quads_stream);
        void draw_selection(DrawCtx &ctx, DrawStream *quads_stream, f32 selection_z);
        void draw_text_content(DrawCtx &ctx);
        void draw_scrollbar(DrawCtx &ctx);
        void draw_caret(DrawCtx &ctx);
        void draw_selection_drag_icon(DrawCtx &ctx, f32 selection_z);
        u32 line_index_from_cursor(int cursor) const;
        void refresh_text_layout_for_editing();
        u32 required_selection_draw_slots() const;
        bool edit_draw_slots_need_record() const;
        void move_cursor_vertical(int dir, bool select);
        bool selection_contains_point(const amal::vec2 &point) const;
        bool begin_selection_drag_press();
        void collapse_cursor_at_point(const amal::vec2 &point);
        void select_text_range(int start, int end);
        void select_word_at_point(const amal::vec2 &point);
        void select_line_at_point(const amal::vec2 &point);
        void select_all_text();
        void copy_selection_to_clipboard();
        void cut_selection_to_clipboard();
        void paste_clipboard_at_cursor();
        void delete_selection();
        bool move_selection_to_cursor(int drop_cursor, bool copy);
        void end_selection_drag(bool commit);
        bool is_read_only() const { return _read_only; }
        bool text_changed() const { return _changed; }
        void mark_text_unchanged() { _changed = false; }
        virtual bool accepts_newline() const { return false; }
        virtual bool should_resize_to_content() const { return false; }
        virtual bool has_internal_scrollbar() const { return false; }
        virtual bool should_draw_caret() const { return true; }
        void reset_caret_blink();
        void schedule_caret_blink();
        void tick_caret_blink();
        void update_transient_draw_commands();

        Text _text;
        Text *_placeholder = nullptr;
        TextBoxEditData *_edit = nullptr;
        detail::TextEditState _edit_state{};
        DrawDataID _bg{};
        StyleSelector _style{Theme::STYLE_ID_INVALID, AUIK_TAG_TEXTBOX};
        amal::vec2 _content_pos{0.0f, 0.0f};
        amal::vec2 _content_size{0.0f, 0.0f};
        u16 _content_clip_id = 0xFFFFu;
        detail::Scrollbar *_scrollbar_y = nullptr;
        f32 _content_scroll_x = 0.0f;
        f32 _content_scroll_y = 0.0f;
        bool _changed = false;
        bool _read_only = false;

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

    class APPLIB_API MultilineTextBox final : public TextBox
    {
    public:
        MultilineTextBox(u32 id, const acul::string &value, amal::vec2 size, bool can_expand_to_content,
                         WidgetFlags flags,
                         Widget *parent = nullptr, TextFlags text_flags = TextFlagBits::none,
                         const acul::string &placeholder = {}, bool read_only = false);

        bool can_expand_to_content() const { return _can_expand_to_content; }
        void set_can_expand_to_content(bool value);
        bool resize_to_content() const { return can_expand_to_content(); }
        void set_resize_to_content(bool value) { set_can_expand_to_content(value); }

    protected:
        bool accepts_newline() const override { return true; }
        bool should_resize_to_content() const override { return _can_expand_to_content; }
        bool has_internal_scrollbar() const override { return true; }

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
                                        WidgetFlagBits::configurable | WidgetFlagBits::fixed,
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
                                                 WidgetFlagBits::configurable | WidgetFlagBits::fixed,
                                             nullptr, text_flags, placeholder, read_only);
    }

    inline MultilineTextBox *make_fixed_multiline_textbox(u32 id, const acul::string &value, amal::vec2 size,
                                                          bool /*can_expand_to_content*/,
                                                          TextFlags text_flags = TextFlagBits::none,
                                                          const acul::string &placeholder = {}, bool read_only = false)
    {
        return make_fixed_multiline_textbox(id, value, size, text_flags, placeholder, read_only);
    }
} // namespace auik::v2
