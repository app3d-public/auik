#include <auik/v2/auik.hpp>
#include <auik/v2/pipelines.hpp>
#include <auik/v2/widgets/textbox.hpp>
#include <cmath>

namespace auik::v2
{
    namespace
    {
        static TextBox *g_active_textbox = nullptr;

        static bool is_utf8_trail(unsigned char ch) { return (ch & 0xC0u) == 0x80u; }

        static int next_utf8_index(const acul::string &text, int idx)
        {
            const int len = static_cast<int>(text.size());
            if (idx >= len) return len;
            ++idx;
            while (idx < len && is_utf8_trail(static_cast<unsigned char>(text[idx]))) ++idx;
            return idx;
        }

        static int prev_utf8_index(const acul::string &text, int idx)
        {
            if (idx <= 0) return 0;
            --idx;
            while (idx > 0 && is_utf8_trail(static_cast<unsigned char>(text[idx]))) --idx;
            return idx;
        }

        static acul::string encode_codepoint(u32 char_code, TextFlags flags)
        {
            acul::string out;
            add_char_to_string(out, char_code, flags);
            return out;
        }

        static ::auik::detail::TextEditKey map_key(Key key, KeyMode mods, bool multiline)
        {
            ::auik::detail::TextEditKey out = 0;
            switch (key)
            {
                case Key::left: out = (mods & KeyModeBits::control) ? AUIK_TEXT_EDIT_KEY_WORD_LEFT : AUIK_TEXT_EDIT_KEY_LEFT; break;
                case Key::right: out = (mods & KeyModeBits::control) ? AUIK_TEXT_EDIT_KEY_WORD_RIGHT : AUIK_TEXT_EDIT_KEY_RIGHT; break;
                case Key::up: out = multiline ? AUIK_TEXT_EDIT_KEY_UP : AUIK_TEXT_EDIT_KEY_TEXT_START; break;
                case Key::down: out = multiline ? AUIK_TEXT_EDIT_KEY_DOWN : AUIK_TEXT_EDIT_KEY_TEXT_END; break;
                case Key::home: out = (mods & KeyModeBits::control) ? AUIK_TEXT_EDIT_KEY_TEXT_START : AUIK_TEXT_EDIT_KEY_LINE_START; break;
                case Key::end: out = (mods & KeyModeBits::control) ? AUIK_TEXT_EDIT_KEY_TEXT_END : AUIK_TEXT_EDIT_KEY_LINE_END; break;
                case Key::backspace: out = AUIK_TEXT_EDIT_KEY_BACKSPACE; break;
                case Key::del: out = AUIK_TEXT_EDIT_KEY_DELETE; break;
                default: break;
            }
            if (out && (mods & KeyModeBits::shift)) out |= AUIK_TEXT_EDIT_KEY_SHIFT;
            return out;
        }
    } // namespace

    TextBox::TextBox(u32 id, acul::string *value, amal::vec2 size, WidgetFlags flags, Widget *parent,
                     u32 style_tag_id)
        : Widget(id, flags | WidgetFlagBits::hittable, EventFlagBits::click | EventFlagBits::drag |
                                                    EventFlagBits::focus | EventFlagBits::key_input |
                                                    EventFlagBits::char_input,
                 parent, {{0.0f, 0.0f}, size}, style_tag_id),
          _value(value),
          _text(acul::alloc<Text>(id ^ 0x5E771EDu, value ? *value : acul::string(), amal::vec2{0.0f, 0.0f},
                                  get_default_fixed_text_flags(), this, Theme::STYLE_ID_INVALID))
    {
        _text->set_overflow_mode(detail::TextOverflowMode::clip);
        ::auik::detail::text_edit_initialize_state(&_edit_state, true);
    }

    TextBox::~TextBox() { acul::release(_text); }

    StyleUpdateFlags TextBox::update_style()
    {
        const u32 parent_id = parent() ? parent()->id() : 0u;
        const auto flags = resolve_style_selector(_style, id(), parent_id, style_state());
        _text->update_style();
        return flags;
    }

    void TextBox::update_layout_min_size()
    {
        sync_text_from_value();
        _text->update_layout_min_size();
        const auto &style = get_theme()->get_style(_style.id);
        const auto padding = style.padding();
        const auto margin = style.margin();

        amal::vec2 min_size = size();
        if (min_size.x <= 0.0f) min_size.x = 160.0f;
        if (min_size.y <= 0.0f)
            min_size.y = amal::max(_text->required_size().y, style.text_size()) + padding.y + padding.w;
        set_required_size({min_size.x + margin.x + margin.z, min_size.y + margin.y + margin.w});
    }

    void TextBox::update_layout(bool min_size_known)
    {
        sync_text_from_value();
        if (!min_size_known) update_layout_min_size();

        const auto &style = get_theme()->get_style(_style.id);
        const auto padding = style.padding();
        const auto margin = style.margin();
        const amal::vec2 layout_origin = position();

        amal::vec2 box_size = size();
        if (box_size.x <= 0.0f) box_size.x = required_size().x - margin.x - margin.z;
        if (box_size.y <= 0.0f || should_resize_to_content())
            box_size.y = required_size().y - margin.y - margin.w;
        box_size.x = amal::max(box_size.x, 1.0f);
        box_size.y = amal::max(box_size.y, style.text_size() + padding.y + padding.w);

        const amal::vec2 box_pos = {layout_origin.x + margin.x, layout_origin.y + margin.y};
        set_position(box_pos);
        set_size(box_size);
        Widget::update_layout(true);
        assert(parent() && "TextBox must have parent");
        set_clip_id(parent()->content_clip_id());

        _content_pos = {box_pos.x + padding.x, box_pos.y + padding.y};
        _content_size = {amal::max(box_size.x - padding.x - padding.z, 0.0f),
                         amal::max(box_size.y - padding.y - padding.w, 0.0f)};
        _text->set_position(_content_pos);
        _text->set_size(_content_size);
        _text->update_layout(true);
        _text->set_clip_id(clip_id());
    }

    void TextBox::translate(const amal::vec2 &delta)
    {
        if (delta.x == 0.0f && delta.y == 0.0f) return;
        Widget::translate(delta);
        _content_pos += delta;
        _text->translate(delta);
    }

    void TextBox::rebuild_clip_rects()
    {
        assert(parent() && "TextBox must have parent");
        set_clip_id(parent()->content_clip_id());
        _bg.hit_id = AUIK_INVALID_DRAW_DATA_ID;
        _text->set_clip_id(clip_id());
        _text->rebuild_clip_rects();
    }

    void TextBox::update_depth(const amal::vec2 &depth_range)
    {
        Widget::update_depth(depth_range);
        amal::vec2 text_range{};
        assign_next_depth(this->depth_range(), text_range);
        _text->update_depth(text_range);
    }

    void TextBox::draw(DrawCtx &ctx)
    {
        auto *quads_stream = get_primary_quads_stream();
        QuadsInstanceData bg{};
        bg.rect = bounds();
        bg.z_order = get_z_order();
        fill_quads_instance_by_style(get_theme()->get_style(_style.id), clip_id(), bg);
        ctx.emit(quads_stream, _bg, &bg, get_rect(), ctx.emit_hit_rect);

        DrawCtx text_ctx = ctx;
        text_ctx.emit_hit_rect = false;
        _text->draw(text_ctx);

        auto *overlay_stream = get_overlay_quads_stream();
        if (!overlay_stream) return;

        const bool active = detail::get_context().focus_id == id();
        if (!active && g_active_textbox == this) g_active_textbox = nullptr;
        else if (active) g_active_textbox = this;
        bool blink_on = active;
        if (active)
        {
            detail::update_window_time(detail::get_context().window_ctx);
            const f64 elapsed = detail::get_context().window_ctx->time - _caret_anim_reset_time;
            blink_on = elapsed < 0.20 || std::fmod(elapsed, 1.20) <= 0.80;
            detail::mark_host_refresh_request();
        }

        const auto &style = get_theme()->get_style(_style.id);
        for (u32 i = 0; i < AUIK_TEXTBOX_MAX_CARETS; ++i)
        {
            const bool visible = blink_on && i < _edit_state.cursors.size();
            const auto caret_pos = visible ? cursor_screen_pos(_edit_state.cursors[i].cursor) : _content_pos;
            QuadsInstanceData caret{};
            caret.rect = {caret_pos, {2.0f, amal::max(style.text_size(), 1.0f)}};
            caret.z_order = next_depth(depth_range());
            caret.background_color = visible ? style.text_color_packed() : detail::pack_rgba8(0, 0, 0, 0);
            caret.mask = clip_id();
            ctx.emit(overlay_stream, _carets[i], &caret, detail::make_rect_data(id(), AUIK_TAG_TEXTBOX, caret.rect),
                     false);
        }
    }

    void TextBox::on_focus(bool focused)
    {
        _focused = focused;
        if (focused) g_active_textbox = this;
        else if (g_active_textbox == this) g_active_textbox = nullptr;
        reset_caret_blink();
        redraw_external(_bg.render_id != AUIK_INVALID_DRAW_DATA_ID);
    }

    void TextBox::on_click(MouseKey key, KeyPressState state, u32 click_count)
    {
        (void)click_count;
        if (key != MouseKey::left || state != KeyPressState::press) return;
        g_active_textbox = this;
        _focused = true;
        set_style_state(StyleState::focus);
        update_style();
        auto &cursor = _edit_state.primary_cursor();
        cursor.cursor = cursor_from_point(get_mouse_pos());
        cursor.select_start = cursor.cursor;
        cursor.select_end = cursor.cursor;
        cursor.has_preferred_x = 0;
        _edit_state.cursors.resize(1);
        reset_caret_blink();
        redraw_external(_bg.render_id != AUIK_INVALID_DRAW_DATA_ID);
    }

    void TextBox::on_drag(const amal::vec2 &delta, KeyPressState state)
    {
        (void)delta;
        if (state == KeyPressState::press) return;
        auto &cursor = _edit_state.primary_cursor();
        if (!cursor.has_selection()) cursor.select_start = cursor.cursor;
        cursor.cursor = cursor_from_point(get_mouse_pos());
        cursor.select_end = cursor.cursor;
        reset_caret_blink();
        redraw_external(_bg.render_id != AUIK_INVALID_DRAW_DATA_ID);
    }

    void TextBox::on_key(Key key, KeyPressState state, KeyMode mods)
    {
        if (state == KeyPressState::release) return;
        if (key == Key::enter && accepts_newline())
        {
            ::auik::detail::TextEditChar newline = '\n';
            auto edit_string = make_text_edit_string();
            ::auik::detail::text_edit_text(&edit_string, &_edit_state, &newline, 1);
            commit_text_edit();
            return;
        }

        const auto edit_key = map_key(key, mods, accepts_newline());
        if (!edit_key) return;

        auto edit_string = make_text_edit_string();
        ::auik::detail::text_edit_key(&edit_string, &_edit_state, edit_key);
        commit_text_edit(key == Key::backspace || key == Key::del);
    }

    void TextBox::on_char_input(u32 char_code, u32 count)
    {
        if (char_code < 32 && char_code != '\t') return;
        const acul::string encoded = encode_codepoint(char_code, text_flags);
        if (encoded.empty()) return;

        acul::vector<::auik::detail::TextEditChar> chars;
        chars.reserve(encoded.size());
        for (size_t i = 0; i < encoded.size(); ++i)
            chars.push_back(static_cast<unsigned char>(encoded[i]));

        auto edit_string = make_text_edit_string();
        for (u32 i = 0; i < count; ++i)
            ::auik::detail::text_edit_text(&edit_string, &_edit_state, chars.data(), static_cast<int>(chars.size()));
        commit_text_edit();
    }

    void TextBox::set_value(const acul::string &value)
    {
        mutable_value() = value;
        sync_text_from_value();
        auto &cursor = _edit_state.primary_cursor();
        cursor.cursor = static_cast<int>(mutable_value().size());
        cursor.select_start = cursor.cursor;
        cursor.select_end = cursor.cursor;
        detail::get_context().dirty_flags |= DirtyFlagBits::layout;
        detail::mark_host_refresh_request();
    }

    void TextBox::sync_text_from_value()
    {
        if (!_text || _text->text() == mutable_value()) return;
        _text->set_text(mutable_value());
    }

    void TextBox::commit_text_edit(bool layout_dirty)
    {
        sync_text_from_value();
        reset_caret_blink();
        if (layout_dirty)
        {
            detail::get_context().dirty_flags |= DirtyFlagBits::layout;
            detail::mark_host_refresh_request();
        }
        else redraw_external(_bg.render_id != AUIK_INVALID_DRAW_DATA_ID);
    }

    void TextBox::reset_caret_blink()
    {
        detail::update_window_time(detail::get_context().window_ctx);
        _caret_anim_reset_time = detail::get_context().window_ctx->time;
    }

    ::auik::detail::TextEditString TextBox::make_text_edit_string()
    {
        ::auik::detail::TextEditString out{};
        out.user_data = this;
        out.string_len = edit_string_len;
        out.get_char = edit_get_char;
        out.insert_chars = edit_insert_chars;
        out.delete_chars = edit_delete_chars;
        out.next_char_index = edit_next_char_index;
        out.prev_char_index = edit_prev_char_index;
        return out;
    }

    int TextBox::cursor_from_point(const amal::vec2 &point) const
    {
        const auto &layout = _text->layout_result();
        if (layout.lines.empty()) return point.x < _content_pos.x + _content_size.x * 0.5f ? 0 : static_cast<int>(mutable_value().size());

        const f32 local_x = point.x - _content_pos.x;
        if (local_x <= 0.0f) return 0;
        if (local_x >= layout.size.x) return static_cast<int>(mutable_value().size());

        const auto &line = layout.lines[0];
        int best = static_cast<int>(line.text_start);
        f32 best_dist = 3.4e38f;
        for (u32 i = 0; i < line.glyph_count; ++i)
        {
            const auto &glyph = layout.glyphs[line.glyph_offset + i];
            const f32 start_dist = amal::abs(local_x - glyph.pen.x);
            if (start_dist < best_dist)
            {
                best_dist = start_dist;
                best = static_cast<int>(glyph.cluster);
            }
            const f32 end_dist = amal::abs(local_x - (glyph.pen.x + glyph.advance.x));
            if (end_dist < best_dist)
            {
                best_dist = end_dist;
                best = static_cast<int>(line.text_end);
            }
        }
        return best;
    }

    amal::vec2 TextBox::cursor_screen_pos(int cursor) const
    {
        const auto &layout = _text->layout_result();
        if (layout.lines.empty()) return _content_pos;
        const auto &line = layout.lines[0];
        f32 x = 0.0f;
        for (u32 i = 0; i < line.glyph_count; ++i)
        {
            const auto &glyph = layout.glyphs[line.glyph_offset + i];
            if (static_cast<int>(glyph.cluster) >= cursor) break;
            x = glyph.pen.x + glyph.advance.x;
        }
        if (cursor >= static_cast<int>(line.text_end)) x = line.width;
        return {_content_pos.x + x, _content_pos.y};
    }

    int TextBox::edit_string_len(void *user_data)
    {
        auto *self = static_cast<TextBox *>(user_data);
        return self ? static_cast<int>(self->mutable_value().size()) : 0;
    }

    ::auik::detail::TextEditChar TextBox::edit_get_char(void *user_data, int char_idx)
    {
        auto *self = static_cast<TextBox *>(user_data);
        if (!self || char_idx < 0 || char_idx >= static_cast<int>(self->mutable_value().size())) return 0;
        return static_cast<unsigned char>(self->mutable_value()[char_idx]);
    }

    bool TextBox::edit_insert_chars(void *user_data, int pos, const ::auik::detail::TextEditChar *text, int text_len)
    {
        auto *self = static_cast<TextBox *>(user_data);
        if (!self || !text || text_len <= 0) return false;
        acul::string insert;
        for (int i = 0; i < text_len; ++i) insert += static_cast<char>(text[i]);
        self->mutable_value().replace(static_cast<size_t>(pos), 0, insert);
        return true;
    }

    void TextBox::edit_delete_chars(void *user_data, int pos, int count)
    {
        auto *self = static_cast<TextBox *>(user_data);
        if (!self || count <= 0) return;
        self->mutable_value().replace(static_cast<size_t>(pos), static_cast<size_t>(count), acul::string());
    }

    int TextBox::edit_next_char_index(void *user_data, int idx)
    {
        auto *self = static_cast<TextBox *>(user_data);
        return self ? next_utf8_index(self->mutable_value(), idx) : idx + 1;
    }

    int TextBox::edit_prev_char_index(void *user_data, int idx)
    {
        auto *self = static_cast<TextBox *>(user_data);
        return self ? prev_utf8_index(self->mutable_value(), idx) : idx - 1;
    }

    MultilineTextBox::MultilineTextBox(u32 id, acul::string *value, amal::vec2 size, bool resize_to_content,
                                       WidgetFlags flags, Widget *parent)
        : TextBox(id, value, size, flags, parent, AUIK_TAG_TEXTBOX),
          _resize_to_content(resize_to_content)
    {
        _text->set_multiline(true);
        ::auik::detail::text_edit_initialize_state(&_edit_state, false);
    }

    void MultilineTextBox::set_resize_to_content(bool value)
    {
        if (_resize_to_content == value) return;
        _resize_to_content = value;
        detail::get_context().dirty_flags |= DirtyFlagBits::layout;
        detail::mark_host_refresh_request();
    }
} // namespace auik::v2
