#pragma once

#include <auik/detail/text.hpp>
#include <auik/detail/text_edit.hpp>
#include <auik/model.hpp>
#include <auik/theme.hpp>
#include "widget.hpp"

#define AUIK_TAG_TEXT  0x60F46B05u
#define AUIK_TAG_ETEXT 0x25CF6588u

namespace auik
{

    class Text : public Widget
    {
    public:
        TextFlags text_flags = TextFlagBits::none;

        Text(u32 id, acul::string text, amal::vec2 max_size, WidgetFlags flags,
             TextLayoutFlags layout_flags = default_text_layout_flags())
            : Widget(id, flags, EventFlagBits::none, {{0.0f}, max_size}, AUIK_TAG_TEXT),
              _text(std::move(text)),
              _style({Theme::STYLE_ID_INVALID, Theme::STYLE_ID_INVALID})
        { _layout_config.flags = layout_flags; }
        Text(u32 id, StringView text, amal::vec2 max_size, WidgetFlags flags,
             TextLayoutFlags layout_flags = default_text_layout_flags())
            : Text(id, acul::string(text.str ? text.str : ""), max_size, flags, layout_flags)
        {
            if (text.is_translated) set_translated_text_literal(text.str);
        }
        Text(u32 id, const char *text, amal::vec2 max_size, WidgetFlags flags,
             TextLayoutFlags layout_flags = default_text_layout_flags())
            : Text(id, StringView{text}, max_size, flags, layout_flags)
        {
        }
        
        Text(u32 id, ModelBinding *binding, amal::vec2 max_size, WidgetFlags flags,
             TextLayoutFlags layout_flags = default_text_layout_flags())
            : Text(id, acul::string{}, max_size, flags, layout_flags)
        { set_model_binding(binding); }
        AUIK_EXPORT StyleUpdateFlags update_style() override;
        AUIK_EXPORT void update_layout_min_size() override;
        AUIK_EXPORT void update_layout(bool min_size_known) override;
        AUIK_EXPORT void translate(const amal::vec2 &delta) override;
        AUIK_EXPORT void update_depth(const amal::vec2 &depth_range) override;
        AUIK_EXPORT void rebuild_clip_rects() override;
        AUIK_EXPORT void reset_draw_records() override;
        AUIK_EXPORT void invalidate_draw_records();
        AUIK_EXPORT void draw(DrawCtx &ctx) override;
        u32 signature() const override { return AUIK_TAG_TEXT; }

        AUIK_EXPORT Text *clone(u32 id) const;

        const acul::string &text() const { return _text; }
        ModelBinding *model_binding() const { return _model_value_binding; }
        StringView source_text() const
        {
            const char *literal = _translated_text ? translated_text_literal() : nullptr;
            return _translated_text ? StringView{literal, true} : StringView{_text};
        }
        void set_text(const Text &source) { set_text(source.source_text()); }
        void set_text(const acul::string &text)
        {
            if (_text == text)
            {
                if (_translated_text) clear_translated_text_literal();
                return;
            }
            _text = text;
            clear_translated_text_literal();
        }
        AUIK_EXPORT void set_text(StringView text);
        AUIK_EXPORT void set_model_binding(ModelBinding *binding);
        AUIK_EXPORT void set_translated_text_literal(const char *literal);
        AUIK_EXPORT void clear_translated_text_literal();
        AUIK_EXPORT const char *translated_text_literal() const;
        bool is_translated_text() const { return _translated_text; }
        AUIK_EXPORT bool update_translated_text();

        const detail::TextLayoutResult &layout_result() const { return _layout_result; }
        u32 style_tag() const { return _style.tag_id; }
        void set_style_tag(u32 tag_id)
        {
            if (_style.tag_id == tag_id) return;
            _style = {Theme::STYLE_ID_INVALID, tag_id};
        }

        bool multiline() const { return text_wrap_mode(_layout_config.flags) == TextWrapMode::word; }
        void set_multiline(bool value)
        {
            if (multiline() == value) return;
            if (value) _layout_config.flags |= TextLayoutFlagBits::wrap_word;
            else _layout_config.flags &= ~TextLayoutFlagBits::wrap_word;
        }

        TextLayoutFlags layout_flags() const { return _layout_config.flags; }
        void set_layout_flags(TextLayoutFlags value) { _layout_config.flags = value; }

        TextOverflowMode overflow_mode() const { return text_overflow_mode(_layout_config.flags); }
        void set_overflow_mode(TextOverflowMode value)
        {
            if (overflow_mode() == value) return;
            if (value == TextOverflowMode::ellipsis) _layout_config.flags |= TextLayoutFlagBits::ellipsis;
            else _layout_config.flags &= ~TextLayoutFlagBits::ellipsis;
        }

        bool trim_trailing_spaces() const { return _layout_config.flags & TextLayoutFlagBits::trim_trailing_spaces; }
        void set_trim_trailing_spaces(bool value)
        {
            if (trim_trailing_spaces() == value) return;
            if (value) _layout_config.flags |= TextLayoutFlagBits::trim_trailing_spaces;
            else _layout_config.flags &= ~TextLayoutFlagBits::trim_trailing_spaces;
        }

        TextLayoutWidthMode width_mode() const { return text_width_mode(_layout_config.flags); }
        void set_width_mode(TextLayoutWidthMode value)
        {
            if (width_mode() == value) return;
            _layout_config.flags &= ~(TextLayoutFlagBits::width_viewport | TextLayoutFlagBits::width_bounds);
            if (value == TextLayoutWidthMode::viewport) _layout_config.flags |= TextLayoutFlagBits::width_viewport;
            else if (value == TextLayoutWidthMode::bounds) _layout_config.flags |= TextLayoutFlagBits::width_bounds;
        }

        u32 max_lines() const { return _layout_config.max_lines; }
        void set_max_lines(u32 value)
        {
            if (_layout_config.max_lines == value) return;
            _layout_config.max_lines = value;
        }

        f32 max_width() const { return _layout_config.max_width; }
        void set_max_width(f32 value)
        {
            if (_layout_config.max_width == value) return;
            _layout_config.max_width = value;
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
        acul::string _translation_literal;
        ModelBinding *_model_value_binding = nullptr;
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
        bool _translated_text = false;
        u16 _applied_clip_id = 0xFFFFu;
        const PostFxChain *_applied_post_fx_chain = nullptr;

        AUIK_EXPORT void update_content_bounds();

    private:
        bool apply_model_binding_value();
        bool rebuild_text_buffers(const amal::vec2 &bounds_size);
    };

    class EText : public Text
    {
    public:
        AUIK_EXPORT EText(u32 id, acul::string text, amal::vec2 max_size, WidgetFlags flags,
                          TextLayoutFlags layout_flags);
        AUIK_EXPORT EText(u32 id, StringView text, amal::vec2 max_size, WidgetFlags flags,
                          TextLayoutFlags layout_flags);
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
        u32 signature() const override { return AUIK_TAG_ETEXT; }

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

    inline Text *make_text(u32 id, StringView text = "", amal::vec2 max_size = AUIK_SIZE_FIT,
                           TextLayoutFlags layout_flags = default_text_layout_flags())
    {
        constexpr WidgetFlags flags =
            WidgetFlagBits::visible | WidgetFlagBits::attachable | WidgetFlagBits::configurable;
        auto *out = acul::alloc<Text>(id, text, max_size, flags, layout_flags);
        return out;
    }

    inline Model *make_text_value_model(ModelDB *db, ModelID model_id = 0u, ModelFieldID field_id = 1u,
                                        acul::string value = {})
    { return make_value_model<acul::string>(db, model_id, field_id, std::move(value)); }

    AUIK_EXPORT Widget *present_model_text_field(ModelBinding *binding, ModelRecord &record, ModelFieldID field_id,
                                                 void *data);

    inline EText *make_etext(u32 id, StringView text = "", amal::vec2 max_size = AUIK_SIZE_FIT)
    {
        constexpr WidgetFlags flags = WidgetFlagBits::visible | WidgetFlagBits::attachable |
                                      WidgetFlagBits::configurable | WidgetFlagBits::hittable |
                                      WidgetFlagBits::read_only;
        auto *out = acul::alloc<EText>(id, text, max_size, flags, default_text_layout_flags());
        if (text.is_translated) out->set_translated_text_literal(text.str);
        return out;
    }

    namespace streams
    {
        extern AUIK_EXPORT const umbf::streams::Stream text;
        extern AUIK_EXPORT const umbf::streams::Stream etext;
    } // namespace streams
} // namespace auik
