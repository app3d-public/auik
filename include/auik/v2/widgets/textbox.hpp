#pragma once

#include <acul/memory/alloc.hpp>
#include <auik/v2/detail/text_edit.hpp>
#include "text.hpp"
#include "widget.hpp"

#define AUIK_TAG_TEXTBOX 0x37C7A6D1u
#define AUIK_TEXTBOX_MAX_CARETS 16u

namespace auik::v2
{
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

        TextBox(u32 id, acul::string *value, amal::vec2 size, WidgetFlags flags,
                Widget *parent = nullptr, u32 style_tag_id = AUIK_TAG_TEXTBOX);
        ~TextBox() override;

        StyleUpdateFlags update_style() override;
        void update_layout_min_size() override;
        void update_layout(bool min_size_known) override;
        void translate(const amal::vec2 &delta) override;
        void rebuild_clip_rects() override;
        void update_depth(const amal::vec2 &depth_range) override;
        void draw(DrawCtx &ctx) override;
        void on_focus(bool focused) override;
        void on_click(MouseKey key, KeyPressState state, u32 click_count) override;
        void on_drag(const amal::vec2 &delta, KeyPressState state) override;
        void on_key(Key key, KeyPressState state, KeyMode mods) override;
        void on_char_input(u32 char_code, u32 count) override;

        const acul::string &value() const { return _value ? *_value : _owned_value; }
        void set_value(const acul::string &value);

    protected:
        acul::string &mutable_value() { return _value ? *_value : _owned_value; }
        const acul::string &mutable_value() const { return _value ? *_value : _owned_value; }
        void sync_text_from_value();
        void commit_text_edit(bool layout_dirty = true);
        ::auik::detail::TextEditString make_text_edit_string();
        int cursor_from_point(const amal::vec2 &point) const;
        amal::vec2 cursor_screen_pos(int cursor) const;
        virtual bool accepts_newline() const { return false; }
        virtual bool should_resize_to_content() const { return false; }
        void reset_caret_blink();

        acul::string *_value = nullptr;
        acul::string _owned_value;
        Text *_text = nullptr;
        ::auik::detail::TextEditState _edit_state{};
        DrawDataID _bg{};
        DrawDataID _carets[AUIK_TEXTBOX_MAX_CARETS]{};
        StyleSelector _style{Theme::STYLE_ID_INVALID, AUIK_TAG_TEXTBOX};
        amal::vec2 _content_pos{0.0f, 0.0f};
        amal::vec2 _content_size{0.0f, 0.0f};
        f64 _caret_anim_reset_time = 0.0;
        bool _focused = false;

    private:
        static int edit_string_len(void *user_data);
        static ::auik::detail::TextEditChar edit_get_char(void *user_data, int char_idx);
        static bool edit_insert_chars(void *user_data, int pos, const ::auik::detail::TextEditChar *text, int text_len);
        static void edit_delete_chars(void *user_data, int pos, int count);
        static int edit_next_char_index(void *user_data, int idx);
        static int edit_prev_char_index(void *user_data, int idx);
    };

    class APPLIB_API MultilineTextBox final : public TextBox
    {
    public:
        MultilineTextBox(u32 id, acul::string *value, amal::vec2 size, bool resize_to_content,
                         WidgetFlags flags, Widget *parent = nullptr);

        bool resize_to_content() const { return _resize_to_content; }
        void set_resize_to_content(bool value);

    protected:
        bool accepts_newline() const override { return true; }
        bool should_resize_to_content() const override { return _resize_to_content; }

    private:
        bool _resize_to_content = false;
    };

    inline TextBox *make_textbox(u32 id, acul::string *value)
    {
        return acul::alloc<TextBox>(id, value, amal::vec2{0.0f, 0.0f}, get_default_textbox_flags(), nullptr,
                                    AUIK_TAG_TEXTBOX);
    }

    inline TextBox *make_fixed_textbox(u32 id, acul::string *value, amal::vec2 size = {240.0f, 0.0f})
    {
        return acul::alloc<TextBox>(id, value, size, get_default_fixed_textbox_flags(), nullptr, AUIK_TAG_TEXTBOX);
    }

    inline MultilineTextBox *make_multiline_textbox(u32 id, acul::string *value, bool resize_to_content = false)
    {
        return acul::alloc<MultilineTextBox>(id, value, amal::vec2{0.0f, 0.0f}, resize_to_content,
                                             get_default_textbox_flags(), nullptr);
    }

    inline MultilineTextBox *make_fixed_multiline_textbox(u32 id, acul::string *value,
                                                          amal::vec2 size = {240.0f, 96.0f},
                                                          bool resize_to_content = false)
    {
        return acul::alloc<MultilineTextBox>(id, value, size, resize_to_content, get_default_fixed_textbox_flags(),
                                             nullptr);
    }
} // namespace auik::v2
