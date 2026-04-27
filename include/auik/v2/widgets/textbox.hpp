#pragma once

#include <auik/v2/detail/text_edit.hpp>
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
                const acul::string &placeholder = {}, bool read_only = false);
        ~TextBox() override;

        StyleUpdateFlags update_style() override;
        void update_layout_min_size() override;
        void update_layout(bool min_size_known) override;
        void translate(const amal::vec2 &delta) override;
        void rebuild_clip_rects() override;
        void update_depth(const amal::vec2 &depth_range) override;
        void draw(DrawCtx &ctx) override;
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
                const size_t text_draw_count = _text.draw_record_count();
                _text.set_position(_content_pos);
                _text.set_size(_content_size);
                _text.update_layout(false);
                _text.set_clip_id(clip_id());
                detail::get_context().dirty_flags &= ~DirtyFlagBits::layout;
                if (_text.layout_instance_count() < text_draw_count)
                {
                    redraw_all_commands();
                    return;
                }
                reason |= DrawReasonBits::layout;
            }
            redraw_external(_bg.render_id != AUIK_INVALID_DRAW_DATA_ID, reason);
            update_transient_draw_commands();
        }

        inline void commit_text_edit(bool layout_dirty = true)
        {
            (void)layout_dirty;
            reset_caret_blink();
            schedule_caret_blink();
        }

        detail::TextEditString make_text_edit_string();
        int cursor_from_point(const amal::vec2 &point) const;
        amal::vec2 cursor_screen_pos(int cursor) const;
        bool selection_contains_point(const amal::vec2 &point) const;
        bool begin_selection_drag_press();
        void collapse_cursor_at_point(const amal::vec2 &point);
        void select_all_text();
        void copy_selection_to_clipboard();
        void cut_selection_to_clipboard();
        void paste_clipboard_at_cursor();
        void delete_selection();
        bool move_selection_to_cursor(int drop_cursor, bool copy);
        void end_selection_drag(bool commit);
        bool is_read_only() const { return _read_only; }
        virtual bool accepts_newline() const { return false; }
        virtual bool should_resize_to_content() const { return false; }
        virtual u32 caret_draw_slots() const { return 1u; }
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
        MultilineTextBox(u32 id, const acul::string &value, amal::vec2 size, bool resize_to_content, WidgetFlags flags,
                         Widget *parent = nullptr, TextFlags text_flags = TextFlagBits::none,
                         const acul::string &placeholder = {}, bool read_only = false);

        bool resize_to_content() const { return _resize_to_content; }
        void set_resize_to_content(bool value);

    protected:
        bool accepts_newline() const override { return true; }
        bool should_resize_to_content() const override { return _resize_to_content; }
        u32 caret_draw_slots() const override { return static_cast<u32>(_edit_state.cursors.size()); }

    private:
        bool _resize_to_content = false;
    };

    inline TextBox *make_textbox(u32 id, const acul::string &value, const acul::string &placeholder = {},
                                 TextFlags text_flags = TextFlagBits::none, bool read_only = false)
    {
        return acul::alloc<TextBox>(id, value, amal::vec2{0.0f, 0.0f},
                                    WidgetFlagBits::visible | WidgetFlagBits::attachable | WidgetFlagBits::configurable,
                                    nullptr, AUIK_TAG_TEXTBOX, text_flags, placeholder, read_only);
    }

    inline TextBox *make_fixed_textbox(u32 id, const acul::string &value, amal::vec2 size = {240.0f, 0.0f},
                                       TextFlags text_flags = TextFlagBits::none, const acul::string &placeholder = {},
                                       bool read_only = false)
    {
        return acul::alloc<TextBox>(id, value, size,
                                    WidgetFlagBits::visible | WidgetFlagBits::attachable |
                                        WidgetFlagBits::configurable | WidgetFlagBits::fixed,
                                    nullptr, AUIK_TAG_TEXTBOX, text_flags, placeholder, read_only);
    }

    inline MultilineTextBox *make_multiline_textbox(u32 id, const acul::string &value, bool resize_to_content = false,
                                                    TextFlags text_flags = TextFlagBits::none,
                                                    const acul::string &placeholder = {}, bool read_only = false)
    {
        return acul::alloc<MultilineTextBox>(id, value, amal::vec2{0.0f, 0.0f}, resize_to_content,
                                             WidgetFlagBits::visible | WidgetFlagBits::attachable |
                                                 WidgetFlagBits::configurable,
                                             nullptr, text_flags, placeholder, read_only);
    }

    inline MultilineTextBox *make_fixed_multiline_textbox(u32 id, const acul::string &value,
                                                          amal::vec2 size = {240.0f, 96.0f},
                                                          bool resize_to_content = false,
                                                          TextFlags text_flags = TextFlagBits::none,
                                                          const acul::string &placeholder = {}, bool read_only = false)
    {
        return acul::alloc<MultilineTextBox>(id, value, size, resize_to_content,
                                             WidgetFlagBits::visible | WidgetFlagBits::attachable |
                                                 WidgetFlagBits::configurable | WidgetFlagBits::fixed,
                                             nullptr, text_flags, placeholder, read_only);
    }
} // namespace auik::v2
