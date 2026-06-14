#include <auik/auik.hpp>
#include <auik/pipelines.hpp>
#include <auik/widgets/textbox.hpp>
#include <cctype>

#define AUIK_TEXTBOX_CARET_ON_TIME                    0.80
#define AUIK_TEXTBOX_CARET_PERIOD                     1.20
#define AUIK_TEXTBOX_CARET_BLINK_TASK_SCHEDULED_BIT   0x01u
#define AUIK_TEXTBOX_SELECTION_DRAG_PRESS_PENDING_BIT 0x02u
#define AUIK_TEXTBOX_SELECTION_DRAG_ACTIVE_BIT        0x04u
#define AUIK_TEXTBOX_SELECTION_DRAG_DOT_COUNT         5u

namespace auik
{
    static WidgetFlags resolve_textbox_widget_flags(WidgetFlags flags, bool read_only)
    {
        flags |= WidgetFlagBits::hittable;
        if (read_only) flags |= WidgetFlagBits::read_only;
        return flags;
    }

    static EventFlags resolve_textbox_event_flags()
    {
        return EventFlagBits::hover | EventFlagBits::click | EventFlagBits::drag | EventFlagBits::focus |
               EventFlagBits::key_input | EventFlagBits::char_input | EventFlagBits::shortcut;
    }

    static acul::string make_password_presentation(const acul::string &value)
    {
        acul::string out;
        for (size_t idx = 0; idx < value.size();)
        {
            detail::decode_utf8_codepoint(value, idx);
            out += 'o';
        }
        return out;
    }

    static acul::string make_textbox_presentation(const acul::string &value, TextFlags flags)
    {
        return (flags & TextFlagBits::password) ? make_password_presentation(value) : value;
    }

    static detail::TextEditKey map_key(Key key, KeyMode mods, bool multiline, bool read_only, bool password)
    {
        detail::TextEditKey out = 0;
        if (read_only && (key == Key::backspace || key == Key::del)) return out;
        switch (key)
        {
            case Key::left:
                out = ((mods & KeyModeBits::control) && !password) ? AUIK_TEXT_EDIT_KEY_WORD_LEFT
                                                                    : AUIK_TEXT_EDIT_KEY_LEFT;
                break;
            case Key::right:
                out = ((mods & KeyModeBits::control) && !password) ? AUIK_TEXT_EDIT_KEY_WORD_RIGHT
                                                                    : AUIK_TEXT_EDIT_KEY_RIGHT;
                break;
            case Key::up:
                out = multiline ? AUIK_TEXT_EDIT_KEY_UP : AUIK_TEXT_EDIT_KEY_TEXT_START;
                break;
            case Key::down:
                out = multiline ? AUIK_TEXT_EDIT_KEY_DOWN : AUIK_TEXT_EDIT_KEY_TEXT_END;
                break;
            case Key::home:
                out = (mods & KeyModeBits::control) ? AUIK_TEXT_EDIT_KEY_TEXT_START : AUIK_TEXT_EDIT_KEY_LINE_START;
                break;
            case Key::end:
                out = (mods & KeyModeBits::control) ? AUIK_TEXT_EDIT_KEY_TEXT_END : AUIK_TEXT_EDIT_KEY_LINE_END;
                break;
            case Key::backspace:
                out = AUIK_TEXT_EDIT_KEY_BACKSPACE;
                break;
            case Key::del:
                out = AUIK_TEXT_EDIT_KEY_DELETE;
                break;
            default:
                break;
        }
        if (out && (mods & KeyModeBits::shift)) out |= AUIK_TEXT_EDIT_KEY_SHIFT;
        return out;
    }

    static void draw_text_drag_icon(DrawCtx &ctx, DrawDataID (&draw_ids)[AUIK_TEXTBOX_SELECTION_DRAG_DOT_COUNT],
                                    const amal::vec2 &drop_pos, f32 line_h, f32 z_order, u16 clip_id,
                                    const Style &style, bool visible)
    {
        auto *stream = get_primary_quads_stream();
        if (!stream) return;

        if (!visible)
        {
            DrawCtx invalidate_ctx = ctx;
            invalidate_ctx.reason |= DrawReasonBits::invalidate;
            emit_context_draw_batch(invalidate_ctx, stream, draw_ids, nullptr, AUIK_TEXTBOX_SELECTION_DRAG_DOT_COUNT);
            return;
        }

        const f32 dot_size = amal::max(amal::round(line_h * 0.14f), 1.5f);
        const f32 dot_step = line_h > dot_size ? (line_h - dot_size) / 4.0f : dot_size;
        const u32 fill = style.background_color();
        const u32 border = style.border_color();
        QuadsInstanceData dots[AUIK_TEXTBOX_SELECTION_DRAG_DOT_COUNT]{};
        for (u32 i = 0; i < AUIK_TEXTBOX_SELECTION_DRAG_DOT_COUNT; ++i)
        {
            auto &dot = dots[i];
            dot.rect = {{drop_pos.x - dot_size * 0.5f, drop_pos.y + static_cast<f32>(i) * dot_step},
                        {dot_size, dot_size}};
            dot.background_color = fill;
            dot.border_color = border;
            dot.border_radius = dot_size * 0.5f;
            dot.border_thickness = style.border_thickness();
            dot.z_order = z_order;
            u32 flags = 0u;
            if (dot.border_radius > 0.0f) flags |= AUIK_HAS_RADIUS_BIT;
            if (dot.border_thickness > 0.0f) flags |= AUIK_HAS_BORDER_BIT;
            dot.mask = static_cast<u32>(clip_id) | (flags << 20u);
        }

        emit_context_draw_batch(ctx, stream, draw_ids, dots, AUIK_TEXTBOX_SELECTION_DRAG_DOT_COUNT);
    }

    struct TextBoxEditData
    {
        acul::vector<amal::rect> selection_rects;
        acul::vector<DrawDataID> selections;
        acul::vector<DrawDataID> password_dots;
        DrawDataID caret{};
        DrawDataID selection_drag_dots[AUIK_TEXTBOX_SELECTION_DRAG_DOT_COUNT]{};
        StyleSelector caret_style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_CARET};
        StyleSelector selection_style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_SELECTION};
        StyleSelector drag_icon_style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_TEXT_DRAG_ICON};
        f64 caret_anim_reset_time = 0.0;
        u8 flags = 0;
    };

    TextBox::TextBox(u32 id, const acul::string &value, amal::vec2 size, WidgetFlags flags, Widget *parent,
                     u32 style_tag_id, TextFlags text_flags, const acul::string &placeholder, bool read_only,
                     detail::TextVerticalAlign text_vertical_align, detail::TextWrapMode text_wrap)
        : Widget(id, resolve_textbox_widget_flags(flags, read_only), resolve_textbox_event_flags(), parent,
                 {{0.0f, 0.0f}, size}, style_tag_id),
          _value(value),
          _text(AUIK_TAG_TEXT, make_textbox_presentation(value, text_flags), amal::vec2{0.0f, 0.0f},
                WidgetFlagBits::visible |
                    ((flags & WidgetFlagBits::fixed_layout) ? WidgetFlagBits::fixed_layout : WidgetFlagBits::none),
                this, AUIK_STYLE_TAG_NO_PAD, detail::TextOverflowMode::clip, text_vertical_align, text_wrap,
                text_wrap != detail::TextWrapMode::none ? detail::TextLayoutWidthMode::bounds
                                                        : detail::TextLayoutWidthMode::viewport),
          _placeholder(placeholder.empty()
                           ? nullptr
                           : acul::alloc<Text>(AUIK_TAG_TEXT, placeholder, amal::vec2{0.0f, 0.0f},
                                               WidgetFlagBits::visible | ((flags & WidgetFlagBits::fixed_layout)
                                                                              ? WidgetFlagBits::fixed_layout
                                                                              : WidgetFlagBits::none),
                                               this, AUIK_STYLE_TAG_PLACEHOLDER, detail::TextOverflowMode::clip,
                                               text_vertical_align, text_wrap)),
          _edit(acul::alloc<TextBoxEditData>())
    {
        this->text_flags = text_flags;
        sync_widget_flags();
        detail::text_edit_initialize_state(&_edit_state, true);
        register_shortcut(this->id(), Shortcut{.mods = KeyModeBits::control, .keys = {Key::c}},
                          [this]() { copy_selection_to_clipboard(); });
        register_shortcut(this->id(), Shortcut{.mods = KeyModeBits::control, .keys = {Key::v}},
                          [this]() { paste_clipboard_at_cursor(); });
        register_shortcut(this->id(), Shortcut{.mods = KeyModeBits::control, .keys = {Key::a}},
                          [this]() { select_all_text(); });
        register_shortcut(this->id(), Shortcut{.mods = KeyModeBits::control, .keys = {Key::x}},
                          [this]() { cut_selection_to_clipboard(); });
    }

    TextBox::~TextBox()
    {
        erase_widget_from_transient_cache(this);
        cancel_delayed_tasks(id());
        acul::release(_scrollbar_y);
        acul::release(_placeholder);
        acul::release(_edit);
    }

    StyleUpdateFlags TextBox::update_style()
    {
        const u32 parent_id = parent() ? parent()->id() : 0u;
        auto flags = resolve_style_selector(_style, id(), parent_id, style_state());
        if (_edit)
        {
            flags |= resolve_style_selector(_edit->caret_style, id(), parent_id, StyleState::normal);
            flags |= resolve_style_selector(_edit->selection_style, id(), parent_id, StyleState::normal);
            flags |= resolve_style_selector(_edit->drag_icon_style, id(), parent_id, StyleState::normal);
        }
        if (_scrollbar_y) flags |= _scrollbar_y->update_style();
        _text.update_style();
        if (_placeholder) _placeholder->update_style();
        return flags;
    }

    void TextBox::sync_widget_flags()
    {
        Widget::sync_widget_flags(is_disabled() ? EventFlagBits::none : requested_event_flags);
    }

    void TextBox::on_disabled_changed(bool disabled)
    {
        if (!disabled || !_edit) return;
        cancel_delayed_tasks(id());
        _edit->flags &= ~(AUIK_TEXTBOX_CARET_BLINK_TASK_SCHEDULED_BIT |
                          AUIK_TEXTBOX_SELECTION_DRAG_PRESS_PENDING_BIT |
                          AUIK_TEXTBOX_SELECTION_DRAG_ACTIVE_BIT);
        _drag_scrollbar = nullptr;
        erase_widget_from_transient_cache(this);
    }

    void TextBox::update_layout_min_size()
    {
        _text.update_layout_min_size();
        if (_placeholder) _placeholder->update_layout_min_size();
        const auto &style = get_theme()->get_style(_style.id);
        const auto padding = style.padding();
        const auto margin = style.margin();
        f32 text_h = _text.required_size().y;
        if (_placeholder) text_h = amal::max(text_h, _placeholder->required_size().y);

        amal::vec2 min_size = is_fixed() ? amal::vec2{is_size_concrete(requested_size().x) ? requested_size().x : 0.0f,
                                                      is_size_concrete(requested_size().y) ? requested_size().y : 0.0f}
                                         : amal::vec2{0.0f, 0.0f};
        if (fill_width()) min_size.x = 160.0f;
        if (fill_height()) min_size.y = 0.0f;
        if (min_size.x <= 0.0f) min_size.x = 160.0f;
        if (!is_fixed() && size().y > 0.0f && !should_resize_to_content()) min_size.y = size().y;
        if (min_size.y <= 0.0f) min_size.y = amal::max(text_h, style.text_size()) + padding.y + padding.w;
        set_required_size({min_size.x + margin.x + margin.z, min_size.y + margin.y + margin.w});
    }

    void TextBox::update_layout(bool min_size_known)
    {
        if (!min_size_known) update_layout_min_size();

        const auto &style = get_theme()->get_style(_style.id);
        const auto padding = style.padding();
        const auto margin = style.margin();
        const amal::vec2 layout_origin = position();

        amal::vec2 box_size = size();
        if (box_size.x <= 0.0f) box_size.x = required_size().x - margin.x - margin.z;
        if (box_size.y <= 0.0f || (!fill_height() && should_resize_to_content()))
            box_size.y = required_size().y - margin.y - margin.w;
        box_size.x = amal::max(box_size.x, 1.0f);
        box_size.y = amal::max(box_size.y, style.text_size() + padding.y + padding.w);

        const amal::vec2 box_pos = {layout_origin.x + margin.x, layout_origin.y + margin.y};
        set_position(box_pos);
        set_layout_size(box_size);
        Widget::update_layout(true);
        assert(parent() && "TextBox must have parent");
        set_clip_id(parent()->content_clip_id());

        _content_pos = {box_pos.x + padding.x, box_pos.y + padding.y};
        _content_size = {amal::max(box_size.x - padding.x - padding.z, 0.0f),
                         amal::max(box_size.y - padding.y - padding.w, 0.0f)};
        update_text_content_clip_rect();
        if (has_internal_scrollbar())
        {
            detail::ensure_internal_y_scrollbar(_scrollbar_y, this);
            _scrollbar_y->update_style();
            amal::vec2 scrollbar_range{};
            assign_next_depth(_text.depth_range(), scrollbar_range);
            _scrollbar_y->update_depth(scrollbar_range);
            const amal::vec4 track_margin = _scrollbar_y->get_track_margin();
            const f32 bar_w = _scrollbar_y->get_min_track_thickness();
            const f32 scrollbar_x = box_pos.x + box_size.x - track_margin.z - bar_w;
            const f32 content_view_w = amal::max(scrollbar_x - track_margin.x - _content_pos.x, 0.0f);
            _text.set_position(_content_pos);
            _text.set_layout_size({content_view_w, _content_size.y});
            _text.set_clip_id(text_content_clip_id());
            _text.update_layout(true);
            _text.set_clip_id(text_content_clip_id());
            const f32 content_h = _text.layout_result().size.y;
            const bool need_scrollbar = content_h > _content_size.y && !should_resize_to_content();
            if (need_scrollbar)
            {
                _content_size.x = content_view_w;
                _scrollbar_y->set_visible();
                _scrollbar_y->sync_widget_flags();
                add_event_flags(EventFlagBits::scroll);
                _scrollbar_y->set_clip_id(clip_id());
                _scrollbar_y->configure({scrollbar_x, box_pos.y + track_margin.y},
                                        {bar_w, amal::max(box_size.y - track_margin.y - track_margin.w, 0.0f)},
                                        content_h, _content_size.y);
                _content_scroll.y = _scrollbar_y->scroll_offset();
            }
            else
            {
                _scrollbar_y->set_invisible();
                _scrollbar_y->sync_widget_flags();
                remove_event_flags(EventFlagBits::scroll);
                _scrollbar_y->set_scroll_offset(0.0f);
                _content_scroll.y = 0.0f;
            }
        }
        else if (_scrollbar_y)
        {
            _scrollbar_y->set_invisible();
            _scrollbar_y->sync_widget_flags();
            remove_event_flags(EventFlagBits::scroll);
            _scrollbar_y->set_scroll_offset(0.0f);
            _content_scroll.y = 0.0f;
        }
        _text.set_position(_content_pos);
        _text.set_layout_size(_content_size);
        _text.set_clip_id(text_content_clip_id());
        _text.update_layout(true);
        update_content_scroll_x_for_cursor();
        _text.translate({-_content_scroll.x, -_content_scroll.y});
        _text.set_clip_id(text_content_clip_id());
        refresh_placeholder_layout();
        rebuild_selection_rect_cache();
    }

    void TextBox::translate(const amal::vec2 &delta)
    {
        if (delta.x == 0.0f && delta.y == 0.0f) return;
        Widget::translate(delta);
        _content_pos += delta;
        update_text_content_clip_rect();
        _text.translate(delta);
        if (_placeholder) _placeholder->translate(delta);
        if (_scrollbar_y) _scrollbar_y->translate(delta);
        if (_edit)
            for (auto &rect : _edit->selection_rects) rect.offset += delta;
    }

    void TextBox::rebuild_clip_rects()
    {
        assert(parent() && "TextBox must have parent");
        set_clip_id(parent()->content_clip_id());
        _content_clip_id = 0xFFFFu;
        update_text_content_clip_rect();
        _bg.hit_id = AUIK_INVALID_DRAW_DATA_ID;
        _text.rebuild_clip_rects();
        _text.set_clip_id(text_content_clip_id());
        if (_scrollbar_y)
        {
            _scrollbar_y->set_clip_id(clip_id());
            _scrollbar_y->rebuild_clip_rects();
        }
        if (_placeholder)
        {
            _placeholder->rebuild_clip_rects();
            _placeholder->set_clip_id(text_content_clip_id());
        }
    }

    void TextBox::update_depth(const amal::vec2 &depth_range)
    {
        Widget::update_depth(depth_range);
        amal::vec2 text_range{};
        assign_next_depth(this->depth_range(), text_range);
        _text.update_depth(text_range);
        if (_placeholder) _placeholder->update_depth(text_range);
        if (_scrollbar_y)
        {
            amal::vec2 scrollbar_range{};
            assign_next_depth(text_range, scrollbar_range);
            _scrollbar_y->update_depth(scrollbar_range);
        }
    }

    void TextBox::back_hit_depth()
    {
        Widget::back_hit_depth();
        _text.back_hit_depth();
        if (_placeholder) _placeholder->back_hit_depth();
        if (_scrollbar_y) _scrollbar_y->back_hit_depth();
    }

    void TextBox::restore_hit_depth()
    {
        Widget::restore_hit_depth();
        _text.restore_hit_depth();
        if (_placeholder) _placeholder->restore_hit_depth();
        if (_scrollbar_y) _scrollbar_y->restore_hit_depth();
    }

    void TextBox::draw(DrawCtx &ctx)
    {
        const bool transient = ctx.reason & DrawReasonBits::transient;
        const bool draw_transient_payload = transient || (ctx.reason & DrawReasonBits::record);
        const f32 selection_z = next_depth(depth_range());
        if (!transient)
        {
            auto *quads_stream = get_primary_quads_stream();
            draw_background(ctx, quads_stream);
            draw_selection(ctx, quads_stream, selection_z);
            draw_text_content(ctx);
            if (_scrollbar_y && _scrollbar_y->is_visible()) draw_scrollbar(ctx);
        }

        if (!_edit) return;
        if (!draw_transient_payload) return;

        draw_caret(ctx);
        draw_selection_drag_icon(ctx, selection_z);
    }

    void TextBox::draw_background(DrawCtx &ctx, DrawStream *quads_stream)
    {
        QuadsInstanceData bg{};
        bg.rect = bounds();
        bg.z_order = get_z_order();
        const bool bg_visible = fill_quads_instance_by_style(get_theme()->get_style(_style.id), clip_id(), bg);
        emit_quads_instance(ctx, quads_stream, _bg, bg, get_rect(), bg_visible, can_emit_hit(ctx));
    }

    void TextBox::draw_selection(DrawCtx &ctx, DrawStream *quads_stream, f32 selection_z)
    {
        if (!_edit) return;

        const auto &selection_style = get_theme()->get_style(_edit->selection_style.id);
        const f32 selection_h =
            amal::max(_text.layout_result().line_height, get_theme()->get_style(_style.id).text_size());
        const u32 selection_slots =
            amal::max(required_selection_draw_slots(), static_cast<u32>(_edit->selections.size()));
        if (_edit->selections.size() < selection_slots) _edit->selections.resize(selection_slots);

        u32 selection_slot = 0;
        auto emit_selection = [&](const amal::rect &rect, bool visible) {
            if (selection_slot >= _edit->selections.size()) return;
            QuadsInstanceData selection{};
            selection.rect = visible ? rect : amal::rect{_content_pos, {0.0f, selection_h}};
            selection.z_order = selection_z;
            selection.background_color = visible ? selection_style.background_color() : 0;
            selection.mask = text_content_clip_id();

            emit_context_draw(ctx, quads_stream, _edit->selections[selection_slot], &selection,
                     detail::make_rect_data(id(), AUIK_STYLE_TAG_SELECTION, selection.rect), false);
            ++selection_slot;
        };

        for (const auto &rect : _edit->selection_rects) emit_selection(rect, rect.size.x > 0.0f && rect.size.y > 0.0f);
        while (selection_slot < _edit->selections.size()) emit_selection({_content_pos, {0.0f, selection_h}}, false);
    }

    void TextBox::draw_text_content(DrawCtx &ctx)
    {
        DrawCtx text_ctx = ctx;
        text_ctx.is_hit_allowed = false;
        Text *display_text = show_placeholder() ? _placeholder : &_text;
        if (!display_text) return;
        if (display_text == &_text && is_password())
        {
            draw_password_content(text_ctx);
            return;
        }
        display_text->draw_local(text_ctx);
    }

    void TextBox::draw_password_content(DrawCtx &ctx)
    {
        auto *quads_stream = get_primary_quads_stream();
        if (!quads_stream || !_edit)
        {
            _text.draw(ctx);
            return;
        }
        if ((ctx.reason & DrawReasonBits::invalidate))
        {
            emit_context_draw_batch(ctx, quads_stream, _edit->password_dots.data(), nullptr,
                           static_cast<u32>(_edit->password_dots.size()));
            _edit->password_dots.clear();
            return;
        }

        const auto &style = get_theme()->get_style(_style.id);
        const auto &layout = _text.layout_result();
        const f32 line_h = amal::max(layout.line_height, style.text_size());
        const f32 dot_size = amal::max(amal::round(style.text_size() * 0.42f), 2.0f);
        const u32 dot_slots = amal::max(static_cast<u32>(layout.glyphs.size()),
                                        static_cast<u32>(_edit->password_dots.size()));
        if (_edit->password_dots.size() < dot_slots) _edit->password_dots.resize(dot_slots);

        Style dot_style;
        dot_style.background_color(style.text_color()).border_radius(dot_size * 0.5f).border_thickness(0.0f);
        acul::vector<QuadsInstanceData> dots;
        dots.resize(dot_slots);

        u32 dot_slot = 0;
        auto push_dot = [&](const amal::rect &rect, bool visible) {
            if (dot_slot >= dots.size()) return;
            auto &dot = dots[dot_slot];
            dot.rect = visible ? rect : amal::rect{_content_pos, {0.0f, 0.0f}};
            dot.z_order = _text.get_z_order();
            if (!visible || !fill_quads_instance_by_style(dot_style, text_content_clip_id(), dot))
            {
                const amal::rect hidden_rect = dot.rect;
                dot = {};
                dot.rect = hidden_rect;
                dot.z_order = _text.get_z_order();
                dot.mask = text_content_clip_id();
            }
            ++dot_slot;
        };

        for (const auto &glyph : layout.glyphs)
        {
            const f32 center_x = _text.position().x + glyph.pen.x + glyph.advance.x * 0.5f;
            const f32 center_y = _text.position().y + glyph.pen.y - layout.ascender + line_h * 0.5f;
            push_dot({{amal::round(center_x - dot_size * 0.5f), amal::round(center_y - dot_size * 0.5f)},
                      {dot_size, dot_size}},
                     glyph.visible());
        }

        while (dot_slot < dots.size()) push_dot({_content_pos, {0.0f, 0.0f}}, false);

        emit_context_draw_batch(ctx, quads_stream, _edit->password_dots.data(), dots.data(), static_cast<u32>(dots.size()));
    }

    void TextBox::draw_scrollbar(DrawCtx &ctx)
    {
        DrawCtx scrollbar_ctx = ctx;
        _scrollbar_y->draw_local(scrollbar_ctx);
    }

    void TextBox::draw_caret(DrawCtx &ctx)
    {
        auto *overlay_stream = get_overlay_quads_stream();
        if (!overlay_stream) return;

        const bool active = detail::get_context().focus_id == id();
        bool blink_on = active;
        if (active)
        {
            detail::update_window_time(detail::get_context().window_ctx);
            const f64 elapsed = detail::get_context().window_ctx->time - _edit->caret_anim_reset_time;
            blink_on = std::fmod(elapsed, AUIK_TEXTBOX_CARET_PERIOD) < AUIK_TEXTBOX_CARET_ON_TIME;
        }

        const auto &caret_style = get_theme()->get_style(_edit->caret_style.id);
        const auto caret_padding = caret_style.padding();
        const bool has_selection = _edit_state.has_selection();
        const bool visible = blink_on && !_edit_state.cursors.empty() && !has_selection && should_draw_caret();
        QuadsInstanceData caret{};
        const f32 caret_w = amal::max(amal::round(caret_padding.x), 1.0f);
        if (visible)
        {
            const auto &edit_cursor = _edit_state.primary_cursor();
            const int cursor = edit_cursor.cursor;
            const u32 line_index = line_index_from_cursor(cursor, edit_cursor.cursor_at_end_of_line);
            const f32 x = cursor_x_on_line(line_index, cursor);
            caret.rect = caret_rect(line_index, x, caret_w);
        }
        else caret.rect = {_content_pos, {caret_w, 0.0f}};
        caret.z_order = next_depth(depth_range());
        caret.background_color = visible ? caret_style.background_color() : 0;
        caret.mask = text_content_clip_id();
        emit_context_draw(ctx, overlay_stream, _edit->caret, &caret,
                 detail::make_rect_data(id(), _edit->caret_style.tag_id, caret.rect), false);
    }

    void TextBox::draw_selection_drag_icon(DrawCtx &ctx, f32 selection_z)
    {
        if (!_edit) return;

        const bool transient = ctx.reason & DrawReasonBits::transient;
        const auto &style = get_theme()->get_style(_style.id);
        bool show_drag_icon = transient && (_edit->flags & AUIK_TEXTBOX_SELECTION_DRAG_ACTIVE_BIT);
        const auto &drag_icon_style = get_theme()->get_style(_edit->drag_icon_style.id);
        amal::vec2 icon_pos = _content_pos;
        f32 icon_line_h = style.text_size();
        if (show_drag_icon)
        {
            const auto &cursor = _edit_state.primary_cursor();
            const int start = cursor.select_start < cursor.select_end ? cursor.select_start : cursor.select_end;
            const int end = cursor.select_start < cursor.select_end ? cursor.select_end : cursor.select_start;
            const int drop_cursor = cursor_from_point(get_mouse_pos());
            show_drag_icon = drop_cursor != start && drop_cursor != end;
            const amal::vec2 drop_pos = cursor_screen_pos(drop_cursor);
            icon_pos = drop_pos;
        }
        draw_text_drag_icon(ctx, _edit->selection_drag_dots, icon_pos, icon_line_h,
                            (selection_z + _text.get_z_order()) * 0.5f, text_content_clip_id(), drag_icon_style,
                            show_drag_icon);
    }

    void TextBox::on_focus(bool focused)
    {
        if (!focused && _edit)
        {
            _edit->flags &= ~AUIK_TEXTBOX_CARET_BLINK_TASK_SCHEDULED_BIT;
            for (auto &cursor : _edit_state.cursors)
            {
                cursor.select_start = cursor.cursor;
                cursor.select_end = cursor.cursor;
            }
        }

        add_render_command<detail::FocusEventTraits>(this, [this, focused]() {
            if (focused) push_widget_to_transient_cache(this);
            else
            {
                erase_widget_from_transient_cache(this);
                cancel_delayed_tasks(id());
                if (_edit) _edit->flags &= ~AUIK_TEXTBOX_CARET_BLINK_TASK_SCHEDULED_BIT;
            }
            reset_caret_blink();
            if (focused) schedule_caret_blink();

            auto &ctx = detail::get_context();
            if (ctx.dirty_flags & DirtyFlagBits::layout) return;
            rebuild_selection_rect_cache();
            redraw_all_commands();
        });
    }

    void TextBox::on_hover(HoverState state)
    {
        auto &ctx = detail::get_context();
        const bool scrollbar_hover = state != HoverState::leave && detail::is_scrollbar_tag(ctx.hover_id.tag_id);
        detail::set_window_cursor(state == HoverState::leave || scrollbar_hover ? detail::CursorID::arrow
                                                                                : detail::CursorID::ibeam,
                                  ctx.window_ctx);
    }

    void TextBox::on_click(MouseKey key, KeyPressState state, u32 click_count)
    {
        if (key != MouseKey::left) return;
        auto &ctx = detail::get_context();
        if (_scrollbar_y && _scrollbar_y->is_visible() && detail::is_scrollbar_tag(ctx.hover_id.tag_id))
        {
            if (state != KeyPressState::press) return;
            _drag_scrollbar = nullptr;
            _scrollbar_y->set_scroll_offset(_content_scroll.y);
            bool is_offset_changed = false;
            if (ctx.hover_id.tag_id == AUIK_TAG_SCROLLBAR_THUMB_Y) _drag_scrollbar = _scrollbar_y;
            else if (ctx.hover_id.tag_id == AUIK_TAG_SCROLLBAR_TRACK_Y)
            {
                is_offset_changed = _scrollbar_y->scroll_to_track_click(ctx.io.mouse_pos);
                _drag_scrollbar = _scrollbar_y;
            }
            _content_scroll.y = _scrollbar_y->scroll_offset();
            if (is_offset_changed)
                add_render_command<detail::ClickEventTraits>(this, [this]() {
                    update_layout_from_current_bounds(true);
                    redraw_external(!edit_draw_slots_need_record(), DrawReasonBits::layout);
                });
            return;
        }
        if (state == KeyPressState::release)
        {
            if (!_edit || !(_edit->flags & AUIK_TEXTBOX_SELECTION_DRAG_PRESS_PENDING_BIT)) return;
            _edit->flags &= ~AUIK_TEXTBOX_SELECTION_DRAG_PRESS_PENDING_BIT;
            collapse_cursor_at_point(get_mouse_pos());
            add_render_command<detail::ClickEventTraits>(this, [this]() { apply_render_update(false); });
            return;
        }
        if (state != KeyPressState::press) return;
        if (_edit && (_edit->flags & AUIK_TEXTBOX_SELECTION_DRAG_PRESS_PENDING_BIT)) return;
        if (_edit) _edit->flags &= ~AUIK_TEXTBOX_SELECTION_DRAG_ACTIVE_BIT;
        set_style_state(StyleState::focus);
        if (click_count >= 3) select_line_at_point(get_mouse_pos());
        else if (click_count == 2) select_word_at_point(get_mouse_pos());
        else collapse_cursor_at_point(get_mouse_pos());
        add_render_command<detail::ClickEventTraits>(this, [this]() {
            update_style();
            apply_render_update(false);
        });
    }

    void TextBox::on_drag(const amal::vec2 &delta, KeyPressState state)
    {
        if (_drag_scrollbar && state == KeyPressState::release)
        {
            _drag_scrollbar = nullptr;
            return;
        }
        if (_drag_scrollbar)
        {
            _scrollbar_y->set_scroll_offset(_content_scroll.y);
            const bool is_offset_changed = _scrollbar_y->scroll_thumb_by_drag_delta(delta);
            _content_scroll.y = _scrollbar_y->scroll_offset();
            if (is_offset_changed)
                add_render_command<detail::DragEventTraits>(this, [this]() {
                    update_layout_from_current_bounds(true);
                    redraw_external(!edit_draw_slots_need_record(), DrawReasonBits::layout);
                });
            return;
        }
        if (state == KeyPressState::press)
        {
            begin_selection_drag_press();
            return;
        }
        if ((_edit->flags & AUIK_TEXTBOX_SELECTION_DRAG_PRESS_PENDING_BIT) && state == KeyPressState::release)
        {
            _edit->flags &= ~AUIK_TEXTBOX_SELECTION_DRAG_PRESS_PENDING_BIT;
            collapse_cursor_at_point(get_mouse_pos());
            add_render_command<detail::DragEventTraits>(this, [this]() { apply_render_update(false); });
            return;
        }
        if ((_edit->flags & AUIK_TEXTBOX_SELECTION_DRAG_PRESS_PENDING_BIT) && delta != amal::vec2{0.0f})
        {
            _edit->flags &= ~AUIK_TEXTBOX_SELECTION_DRAG_PRESS_PENDING_BIT;
            _edit->flags |= AUIK_TEXTBOX_SELECTION_DRAG_ACTIVE_BIT;
        }
        if (_edit->flags & AUIK_TEXTBOX_SELECTION_DRAG_ACTIVE_BIT)
        {
            if (state == KeyPressState::release) end_selection_drag(true);
            add_render_command<detail::DragEventTraits>(this, [this]() { apply_render_update(false); });
            return;
        }
        if (state == KeyPressState::release) return;
        auto &cursor = _edit_state.primary_cursor();
        if (!cursor.has_selection()) cursor.select_start = cursor.cursor;
        cursor.cursor = cursor_from_point(get_mouse_pos());
        cursor.select_end = cursor.cursor;
        reset_caret_blink();
        schedule_caret_blink();
        add_render_command<detail::DragEventTraits>(this, [this]() { apply_render_update(false); });
    }

    bool TextBox::selection_contains_point(const amal::vec2 &point) const
    {
        if (!_edit) return false;
        for (const auto &rect : _edit->selection_rects)
            if (point.x >= rect.offset.x && point.x <= rect.offset.x + rect.size.x && point.y >= rect.offset.y &&
                point.y <= rect.offset.y + rect.size.y)
                return true;
        return false;
    }

    bool TextBox::begin_selection_drag_press()
    {
        if (!_edit) return false;
        if (detail::get_context().io.click_count > 1)
        {
            _edit->flags &= ~(AUIK_TEXTBOX_SELECTION_DRAG_PRESS_PENDING_BIT | AUIK_TEXTBOX_SELECTION_DRAG_ACTIVE_BIT);
            return false;
        }
        const bool press_pending = selection_contains_point(get_mouse_pos());
        if (press_pending) _edit->flags |= AUIK_TEXTBOX_SELECTION_DRAG_PRESS_PENDING_BIT;
        else _edit->flags &= ~AUIK_TEXTBOX_SELECTION_DRAG_PRESS_PENDING_BIT;
        _edit->flags &= ~AUIK_TEXTBOX_SELECTION_DRAG_ACTIVE_BIT;
        return press_pending;
    }

    void TextBox::collapse_cursor_at_point(const amal::vec2 &point)
    {
        auto &cursor = _edit_state.primary_cursor();
        cursor.cursor = cursor_from_point(point);
        cursor.select_start = cursor.cursor;
        cursor.select_end = cursor.cursor;
        cursor.has_preferred_x = 0;
        cursor.cursor_at_end_of_line = 0;
        _edit_state.cursors.resize(1);
        reset_caret_blink();
        schedule_caret_blink();
    }

    void TextBox::select_text_range(int start, int end)
    {
        const int len = static_cast<int>(value().size());
        start = amal::clamp(start, 0, len);
        end = amal::clamp(end, 0, len);

        auto &cursor = _edit_state.primary_cursor();
        cursor.cursor = end;
        cursor.select_start = start;
        cursor.select_end = end;
        cursor.has_preferred_x = 0;
        cursor.cursor_at_end_of_line = 0;
        _edit_state.cursors.resize(1);
        reset_caret_blink();
        schedule_caret_blink();
    }

    void TextBox::select_word_at_point(const amal::vec2 &point)
    {
        const auto &text = value();
        if (text.empty())
        {
            collapse_cursor_at_point(point);
            return;
        }
        const int pos = cursor_from_point(point);
        int start = amal::clamp(pos, 0, static_cast<int>(text.size()));
        if (start == static_cast<int>(text.size()) && start > 0) start = detail::prev_utf8_index(text, start);

        const auto is_word_space = [&](int index) {
            return index >= 0 && index < static_cast<int>(text.size()) &&
                   isspace(static_cast<unsigned char>(text[index]));
        };
        const bool space_word = is_word_space(start);
        int end = space_word ? detail::next_utf8_index(text, start) : start;
        while (start > 0 && is_word_space(detail::prev_utf8_index(text, start)) == space_word)
            start = detail::prev_utf8_index(text, start);
        while (end < static_cast<int>(text.size()) && is_word_space(end) == space_word)
            end = detail::next_utf8_index(text, end);

        select_text_range(start, end);
    }

    void TextBox::select_line_at_point(const amal::vec2 &point)
    {
        const auto &text = value();
        if (text.empty())
        {
            collapse_cursor_at_point(point);
            return;
        }
        if (!accepts_newline())
        {
            select_text_range(0, static_cast<int>(text.size()));
            return;
        }

        const int cursor = cursor_from_point(point);
        const auto &layout = _text.layout_result();
        if (layout.lines.empty())
        {
            select_text_range(0, static_cast<int>(text.size()));
            return;
        }
        const auto &line = layout.lines[line_index_from_cursor(cursor)];
        select_text_range(static_cast<int>(line.text_start), static_cast<int>(line.text_end));
    }

    void TextBox::select_all_text()
    {
        const auto &text = value();
        if (text.empty()) return;

        select_text_range(0, static_cast<int>(text.size()));
        add_render_command<detail::KeyEventTraits>(this, [this]() { apply_render_update(false); });
    }

    void TextBox::copy_selection_to_clipboard()
    {
        if (is_password()) return;
        if (_edit_state.cursors.empty()) return;
        const auto &cursor = _edit_state.primary_cursor();
        if (!cursor.has_selection()) return;

        int start = cursor.select_start < cursor.select_end ? cursor.select_start : cursor.select_end;
        int end = cursor.select_start < cursor.select_end ? cursor.select_end : cursor.select_start;
        const auto &text = value();
        start = amal::clamp(start, 0, static_cast<int>(text.size()));
        end = amal::clamp(end, 0, static_cast<int>(text.size()));
        if (end <= start) return;

        detail::set_clipboard_string(detail::get_context().window_ctx,
                                     text.substr(static_cast<size_t>(start), static_cast<size_t>(end - start)));
    }

    void TextBox::cut_selection_to_clipboard()
    {
        if (is_password()) return;
        if (is_read_only()) return;
        copy_selection_to_clipboard();
        delete_selection();
    }

    void TextBox::delete_selection()
    {
        if (is_read_only()) return;
        if (_edit_state.cursors.empty()) return;
        auto &cursor = _edit_state.primary_cursor();
        if (!cursor.has_selection()) return;

        int start = cursor.select_start < cursor.select_end ? cursor.select_start : cursor.select_end;
        int end = cursor.select_start < cursor.select_end ? cursor.select_end : cursor.select_start;
        acul::string text = value();
        start = amal::clamp(start, 0, static_cast<int>(text.size()));
        end = amal::clamp(end, 0, static_cast<int>(text.size()));
        if (end <= start) return;

        text.erase(static_cast<size_t>(start), static_cast<size_t>(end - start));
        _value = text;
        sync_text_presentation();
        cursor.cursor = start;
        cursor.select_start = start;
        cursor.select_end = start;
        cursor.has_preferred_x = 0;
        cursor.cursor_at_end_of_line = 0;
        _edit_state.cursors.resize(1);
        reset_caret_blink();
        schedule_caret_blink();
        if (!mark_changed()) add_render_command<detail::KeyEventTraits>(this, [this]() { apply_render_update(true); });
    }

    void TextBox::paste_clipboard_at_cursor()
    {
        if (is_read_only()) return;
        acul::string clip = detail::filter_text_input(detail::get_clipboard_string(detail::get_context().window_ctx),
                                                      text_flags, accepts_newline());
        if (clip.empty()) return;

        auto edit_string = make_text_edit_string();
        acul::vector<detail::TextEditChar> chars;
        chars.reserve(clip.size());
        for (size_t i = 0; i < clip.size(); ++i) chars.push_back(static_cast<unsigned char>(clip[i]));
        detail::text_edit_text(&edit_string, &_edit_state, chars.data(), static_cast<int>(chars.size()));
        for (auto &cursor : _edit_state.cursors) cursor.cursor_at_end_of_line = 0;
        commit_text_edit();
        add_render_command<detail::KeyEventTraits>(this, [this]() { apply_render_update(true); });
    }

    bool TextBox::move_selection_to_cursor(int drop_cursor, bool copy)
    {
        if (is_read_only()) return false;
        if (_edit_state.cursors.empty()) return false;
        auto &cursor = _edit_state.primary_cursor();
        if (!cursor.has_selection()) return false;

        int start = cursor.select_start < cursor.select_end ? cursor.select_start : cursor.select_end;
        int end = cursor.select_start < cursor.select_end ? cursor.select_end : cursor.select_start;
        if (drop_cursor >= start && drop_cursor <= end) return false;

        acul::string text = value();
        start = amal::clamp(start, 0, static_cast<int>(text.size()));
        end = amal::clamp(end, 0, static_cast<int>(text.size()));
        drop_cursor = amal::clamp(drop_cursor, 0, static_cast<int>(text.size()));
        if (end <= start) return false;

        acul::string selection = text.substr(static_cast<size_t>(start), static_cast<size_t>(end - start));
        if (!copy)
        {
            text.erase(static_cast<size_t>(start), static_cast<size_t>(end - start));
            if (drop_cursor > end) drop_cursor -= end - start;
        }
        drop_cursor = amal::clamp(drop_cursor, 0, static_cast<int>(text.size()));
        acul::string moved = text.substr(0, static_cast<size_t>(drop_cursor));
        moved += selection;
        moved += text.substr(static_cast<size_t>(drop_cursor));
        _value = moved;
        sync_text_presentation();

        cursor.cursor = drop_cursor + static_cast<int>(selection.size());
        cursor.select_start = drop_cursor;
        cursor.select_end = cursor.cursor;
        cursor.has_preferred_x = 0;
        cursor.cursor_at_end_of_line = 0;
        _edit_state.cursors.resize(1);
        reset_caret_blink();
        schedule_caret_blink();
        return true;
    }

    void TextBox::end_selection_drag(bool commit)
    {
        if (!_edit) return;
        const bool was_active = _edit->flags & AUIK_TEXTBOX_SELECTION_DRAG_ACTIVE_BIT;
        bool layout_dirty = false;
        if (commit && was_active)
            layout_dirty = move_selection_to_cursor(cursor_from_point(get_mouse_pos()),
                                                    detail::get_context().io.active_mods & KeyModeBits::control);
        _edit->flags &= ~(AUIK_TEXTBOX_SELECTION_DRAG_PRESS_PENDING_BIT | AUIK_TEXTBOX_SELECTION_DRAG_ACTIVE_BIT);
        if (layout_dirty && !mark_changed())
            add_render_command<detail::DragEventTraits>(this, [this]() { apply_render_update(true); });
    }

    void TextBox::on_key(Key key, KeyPressState state, KeyMode mods)
    {
        if (state == KeyPressState::release) return;
        if (key == Key::enter || key == Key::kp_enter)
        {
            if (is_read_only()) return;
            if (accepts_newline())
            {
                detail::TextEditChar newline = '\n';
                auto edit_string = make_text_edit_string();
                detail::text_edit_text(&edit_string, &_edit_state, &newline, 1);
                for (auto &cursor : _edit_state.cursors) cursor.cursor_at_end_of_line = 0;
                commit_text_edit();
                add_render_command<detail::KeyEventTraits>(this, [this]() { apply_render_update(true); });
            }
            else focus_widget(focus_parent() ? focus_parent() : parent());
            return;
        }

        if ((key == Key::up || key == Key::down) && accepts_newline())
        {
            move_cursor_vertical(key == Key::up ? -1 : 1, mods & KeyModeBits::shift);
            commit_text_edit(false);
            add_render_command<detail::KeyEventTraits>(this, [this]() { apply_render_update(false); });
            return;
        }

        const auto edit_key = map_key(key, mods, accepts_newline(), is_read_only(), is_password());
        if (!edit_key) return;

        const acul::string before_text = value();
        auto edit_string = make_text_edit_string();
        detail::text_edit_key(&edit_string, &_edit_state, edit_key);
        for (auto &cursor : _edit_state.cursors) cursor.cursor_at_end_of_line = 0;
        const bool layout_dirty = before_text != value();
        commit_text_edit(layout_dirty);
        add_render_command<detail::KeyEventTraits>(this, [this, layout_dirty]() { apply_render_update(layout_dirty); });
    }

    void TextBox::on_char_input(u32 char_code, u32 count)
    {
        if (is_read_only()) return;
        if (count == 0) return;
        if (char_code < 32 && char_code != '\t') return;
        const acul::string encoded = detail::encode_utf8_codepoint(char_code, text_flags);
        if (encoded.empty()) return;

        constexpr u32 max_repeat_count = 1024u;
        count = amal::min(count, max_repeat_count);

        acul::vector<detail::TextEditChar> chars;
        chars.reserve(encoded.size() * static_cast<size_t>(count));
        for (u32 repeat = 0; repeat < count; ++repeat)
            for (size_t i = 0; i < encoded.size(); ++i) chars.push_back(static_cast<unsigned char>(encoded[i]));

        auto edit_string = make_text_edit_string();
        detail::text_edit_text(&edit_string, &_edit_state, chars.data(), static_cast<int>(chars.size()));
        for (auto &cursor : _edit_state.cursors) cursor.cursor_at_end_of_line = 0;
        commit_text_edit();
        add_render_command<detail::CharEventTraits>(this, [this]() { apply_render_update(true); });
    }

    void TextBox::set_value_internal(const acul::string &value)
    {
        _value = value;
        sync_text_presentation();

        auto &cursor = _edit_state.primary_cursor();
        cursor.cursor = static_cast<int>(this->value().size());
        cursor.select_start = cursor.cursor;
        cursor.select_end = cursor.cursor;
    }

    void TextBox::sync_text_presentation()
    {
        _text.set_text(make_textbox_presentation(_value, text_flags));
    }

    void TextBox::set_placeholder(const acul::string &value)
    {
        if (value.empty())
        {
            if (!_placeholder) return;
            acul::release(_placeholder);
            _placeholder = nullptr;
        }
        else if (_placeholder)
        {
            if (_placeholder->text() == value) return;
            _placeholder->set_text(value);
        }
        else
        {
            _placeholder = acul::alloc<Text>(AUIK_TAG_TEXT, value, amal::vec2{0.0f, 0.0f},
                                             WidgetFlagBits::visible | ((widget_flags & WidgetFlagBits::fixed_layout)
                                                                            ? WidgetFlagBits::fixed_layout
                                                                            : WidgetFlagBits::none),
                                             this, AUIK_STYLE_TAG_PLACEHOLDER);
            _placeholder->set_overflow_mode(detail::TextOverflowMode::clip);
            _placeholder->set_vertical_align(detail::TextVerticalAlign::center);
        }
    }

    void TextBox::reset_caret_blink()
    {
        if (!_edit) return;
        detail::update_window_time(detail::get_context().window_ctx);
        _edit->caret_anim_reset_time = detail::get_context().window_ctx->time;
        cancel_delayed_tasks(id());
        _edit->flags &= ~AUIK_TEXTBOX_CARET_BLINK_TASK_SCHEDULED_BIT;
    }

    void TextBox::schedule_caret_blink()
    {
        if (!_edit) return;
        if (detail::get_context().focus_id != id()) return;
        if (_edit->flags & AUIK_TEXTBOX_CARET_BLINK_TASK_SCHEDULED_BIT) return;

        detail::update_window_time(detail::get_context().window_ctx);
        const f64 now = detail::get_context().window_ctx->time;
        const f64 elapsed = now - _edit->caret_anim_reset_time;
        const f64 phase = std::fmod(elapsed, AUIK_TEXTBOX_CARET_PERIOD);
        const f64 remaining = phase < AUIK_TEXTBOX_CARET_ON_TIME ? (AUIK_TEXTBOX_CARET_ON_TIME - phase)
                                                                 : (AUIK_TEXTBOX_CARET_PERIOD - phase);
        _edit->flags |= AUIK_TEXTBOX_CARET_BLINK_TASK_SCHEDULED_BIT;
        schedule_delayed_host_task(id(), now + (remaining > 0.001 ? remaining : 0.001),
                                   [this]() { tick_caret_blink(); });
    }

    void TextBox::tick_caret_blink()
    {
        if (!_edit) return;
        _edit->flags &= ~AUIK_TEXTBOX_CARET_BLINK_TASK_SCHEDULED_BIT;
        if (detail::get_context().focus_id != id()) return;
        redraw_external(_bg.render_id != AUIK_INVALID_DRAW_DATA_ID, DrawReasonBits::transient);
        schedule_caret_blink();
    }

    void TextBox::on_scroll(const amal::vec2 &delta)
    {
        if (!_scrollbar_y || !_scrollbar_y->is_visible()) return;
        if (!_scrollbar_y->scroll_by_pixels(-delta.y * static_cast<f32>(AUIK_SCROLL_STEP))) return;
        _content_scroll.y = _scrollbar_y->scroll_offset();
        add_render_command<detail::ScrollEventTraits>(this, [this]() {
            update_layout_from_current_bounds(true);
            redraw_external(!edit_draw_slots_need_record(), DrawReasonBits::layout);
            update_transient_draw_commands();
        });
    }

    void TextBox::update_transient_draw_commands()
    {
        if (!_edit) return;
        if (detail::get_context().focus_id != id()) return;
        if (edit_draw_slots_need_record())
        {
            redraw_all_commands();
            return;
        }
        update_draw_commands(DrawReasonBits::transient);
    }

    void TextBox::update_layout_from_current_bounds(bool min_size_known)
    {
        const auto &style = get_theme()->get_style(_style.id);
        const amal::vec4 margin = style.margin();
        const amal::vec2 current_pos = position();
        set_position({current_pos.x - margin.x, current_pos.y - margin.y});
        update_layout(min_size_known);
    }

    detail::TextEditString TextBox::make_text_edit_string()
    {
        detail::TextEditString out{};
        out.user_data = this;
        out.string_len = edit_string_len;
        out.get_char = edit_get_char;
        out.insert_chars = edit_insert_chars;
        out.delete_chars = edit_delete_chars;
        out.replace_chars = edit_replace_chars;
        out.next_char_index = edit_next_char_index;
        out.prev_char_index = edit_prev_char_index;
        return out;
    }

    int TextBox::cursor_from_point(const amal::vec2 &point) const
    {
        const auto &layout = _text.layout_result();
        if (layout.lines.empty())
            return point.x < _content_pos.x + _content_size.x * 0.5f ? 0 : static_cast<int>(value().size());

        const f32 local_x = point.x - _content_pos.x + _content_scroll.x;
        const f32 local_y = point.y - _content_pos.y + _content_scroll.y;
        u32 line_index = 0;
        f32 best_dist = 3.4e38f;
        for (u32 i = 0; i < layout.lines.size(); ++i)
        {
            const auto &line = layout.lines[i];
            f32 line_min_y = line.glyph_count > 0 ? 3.4e38f : static_cast<f32>(i) * layout.line_height;
            f32 line_max_y = line.glyph_count > 0 ? -3.4e38f : line_min_y + layout.line_height;
            for (u32 glyph_i = 0; glyph_i < line.glyph_count; ++glyph_i)
            {
                const auto &glyph = layout.glyphs[line.glyph_offset + glyph_i];
                line_min_y = amal::min(line_min_y, glyph.rect.offset.y);
                line_max_y = amal::max(line_max_y, glyph.rect.offset.y + glyph.rect.size.y);
            }

            if (local_y >= line_min_y && local_y <= line_max_y) return cursor_from_line_x(i, local_x);
            const f32 line_center = (line_min_y + line_max_y) * 0.5f;
            const f32 dist = amal::abs(local_y - line_center);
            if (dist < best_dist)
            {
                best_dist = dist;
                line_index = i;
            }
        }
        return cursor_from_line_x(line_index, local_x);
    }

    int TextBox::cursor_from_line_x(u32 line_index, f32 x) const
    {
        const auto &layout = _text.layout_result();
        if (layout.lines.empty()) return x < _content_size.x * 0.5f ? 0 : static_cast<int>(value().size());
        if (line_index >= layout.lines.size()) line_index = static_cast<u32>(layout.lines.size() - 1);

        const auto &line = layout.lines[line_index];
        if (x <= 0.0f) return static_cast<int>(line.text_start);
        if (x >= line.width) return static_cast<int>(line.text_end);

        const auto &text = value();
        int best = static_cast<int>(line.text_start);
        f32 best_dist = 3.4e38f;
        for (u32 i = 0; i < line.glyph_count; ++i)
        {
            const auto &glyph = layout.glyphs[line.glyph_offset + i];
            const int glyph_start = static_cast<int>(glyph.cluster);
            const int glyph_end = detail::next_utf8_index(text, glyph_start);
            const f32 start_dist = amal::abs(x - glyph.pen.x);
            if (start_dist < best_dist)
            {
                best_dist = start_dist;
                best = glyph_start;
            }
            const f32 end_dist = amal::abs(x - (glyph.pen.x + glyph.advance.x));
            if (end_dist < best_dist)
            {
                best_dist = end_dist;
                best = glyph_end;
            }
        }
        return best;
    }

    f32 TextBox::cursor_x_on_line(u32 line_index, int cursor) const
    {
        const auto &layout = _text.layout_result();
        if (layout.lines.empty()) return 0.0f;
        if (line_index >= layout.lines.size()) line_index = static_cast<u32>(layout.lines.size() - 1);

        const auto &line = layout.lines[line_index];
        if (cursor <= static_cast<int>(line.text_start)) return 0.0f;
        if (cursor >= static_cast<int>(line.text_end)) return line.width;

        f32 x = 0.0f;
        for (u32 i = 0; i < line.glyph_count; ++i)
        {
            const auto &glyph = layout.glyphs[line.glyph_offset + i];
            if (static_cast<int>(glyph.cluster) >= cursor) break;
            x = glyph.pen.x + glyph.advance.x;
        }
        return x;
    }

    f32 TextBox::line_screen_y(u32 line_index) const { return line_selection_rect(line_index, 0.0f, 0.0f).offset.y; }

    amal::rect TextBox::caret_rect(u32 line_index, f32 x, f32 width) const
    {
        const auto &layout = _text.layout_result();
        const auto &style = get_theme()->get_style(_style.id);
        f32 line_h = layout.line_height;
        if (line_h <= 0.0f)
        {
            if (auto *font = style.font())
                line_h = detail::TextFontAccess::line_height(*font, round_font_px(style.text_size()));
            if (line_h <= 0.0f) line_h = style.text_size();
        }
        const f32 caret_h = amal::min(style.text_size(), _content_size.y);
        line_h = amal::max(line_h, caret_h);
        const f32 layout_h = accepts_newline() ? amal::max(layout.size.y, line_h) : _content_size.y;
        f32 align_y = 0.0f;
        switch (_text.vertical_align())
        {
            case detail::TextVerticalAlign::center:
                align_y = amal::max((_content_size.y - layout_h) * 0.5f, 0.0f);
                break;
            case detail::TextVerticalAlign::bottom:
                align_y = amal::max(_content_size.y - layout_h, 0.0f);
                break;
            case detail::TextVerticalAlign::top:
            default:
                break;
        }

        f32 line_y = amal::max((layout_h - caret_h) * 0.5f, 0.0f);
        if (accepts_newline())
        {
            if (!layout.lines.empty() && line_index >= layout.lines.size())
                line_index = static_cast<u32>(layout.lines.size() - 1);
            line_y = static_cast<f32>(line_index) * line_h + amal::max((line_h - caret_h) * 0.5f, 0.0f);
        }

        return {{amal::round(_content_pos.x + x - _content_scroll.x),
                 amal::round(_content_pos.y + align_y + line_y - _content_scroll.y)},
                {width, caret_h}};
    }

    amal::rect TextBox::line_selection_rect(u32 line_index, f32 x0, f32 x1) const
    {
        const auto &layout = _text.layout_result();
        const auto &style = get_theme()->get_style(_style.id);
        const f32 metrics_h = amal::max(layout.ascender - layout.descender, style.text_size());
        const f32 line_h = amal::max(layout.line_height, style.text_size());
        const f32 selection_h = accepts_newline() ? amal::min(style.text_size(), line_h) : line_h;
        const f32 layout_h = layout.lines.size() <= 1 ? (accepts_newline() ? line_h : metrics_h) : layout.size.y;
        f32 align_y = 0.0f;
        switch (_text.vertical_align())
        {
            case detail::TextVerticalAlign::center:
                align_y = amal::max((_content_size.y - layout_h) * 0.5f, 0.0f);
                break;
            case detail::TextVerticalAlign::bottom:
                align_y = amal::max(_content_size.y - layout_h, 0.0f);
                break;
            case detail::TextVerticalAlign::top:
            default:
                break;
        }
        if (layout.lines.empty())
            return {{_content_pos.x + x0 - _content_scroll.x,
                     _content_pos.y + align_y + amal::max((line_h - selection_h) * 0.5f, 0.0f) - _content_scroll.y},
                    {amal::max(x1 - x0, 0.0f), selection_h}};
        if (line_index >= layout.lines.size()) line_index = static_cast<u32>(layout.lines.size() - 1);

        const auto &line = layout.lines[line_index];
        f32 line_y = line.glyph_count > 0 ? layout.glyphs[line.glyph_offset].pen.y - layout.ascender
                                          : static_cast<f32>(line_index) * layout.line_height;
        if (accepts_newline()) line_y += amal::max((line_h - selection_h) * 0.5f, 0.0f);
        return {{_content_pos.x + x0 - _content_scroll.x, _content_pos.y + align_y + line_y - _content_scroll.y},
                {amal::max(x1 - x0, 0.0f), selection_h}};
    }

    bool TextBox::update_content_scroll_x_for_cursor()
    {
        const f32 old_scroll_x = _content_scroll.x;
        if (accepts_newline() || value().empty() || _edit_state.cursors.empty())
        {
            _content_scroll.x = 0.0f;
            return old_scroll_x != _content_scroll.x;
        }

        const auto &layout = _text.layout_result();
        if (layout.lines.empty() || _content_size.x <= 0.0f)
        {
            _content_scroll.x = 0.0f;
            return old_scroll_x != _content_scroll.x;
        }

        const int cursor = amal::clamp(_edit_state.primary_cursor().cursor, 0, static_cast<int>(value().size()));
        const auto &edit_cursor = _edit_state.primary_cursor();
        const u32 line_index = line_index_from_cursor(cursor, edit_cursor.cursor_at_end_of_line);
        const f32 cursor_x = cursor_x_on_line(line_index, cursor);
        const f32 max_scroll = amal::max(layout.size.x - _content_size.x, 0.0f);
        const f32 right_edge = _content_scroll.x + _content_size.x;
        const f32 caret_margin = 1.0f;
        if (cursor_x > right_edge - caret_margin) _content_scroll.x = cursor_x - _content_size.x + caret_margin;
        if (cursor_x < _content_scroll.x) _content_scroll.x = cursor_x;
        _content_scroll.x = amal::clamp(_content_scroll.x, 0.0f, max_scroll);
        return old_scroll_x != _content_scroll.x;
    }

    bool TextBox::update_content_scroll_y_for_cursor()
    {
        const f32 old_scroll_y = _content_scroll.y;
        if (!accepts_newline() || !has_internal_scrollbar() || value().empty() || _edit_state.cursors.empty())
        {
            if (!has_internal_scrollbar()) _content_scroll.y = 0.0f;
            return old_scroll_y != _content_scroll.y;
        }

        const auto &layout = _text.layout_result();
        if (layout.lines.empty() || _content_size.y <= 0.0f)
        {
            _content_scroll.y = 0.0f;
            if (_scrollbar_y) _scrollbar_y->set_scroll_offset(_content_scroll.y);
            return old_scroll_y != _content_scroll.y;
        }

        const auto &edit_cursor = _edit_state.primary_cursor();
        const int cursor = amal::clamp(edit_cursor.cursor, 0, static_cast<int>(value().size()));
        const u32 line_index = line_index_from_cursor(cursor, edit_cursor.cursor_at_end_of_line);
        const auto &style = get_theme()->get_style(_style.id);
        const f32 metrics_h = amal::max(layout.ascender - layout.descender, style.text_size());
        const f32 line_h = amal::max(layout.line_height, style.text_size());
        const f32 layout_h = layout.lines.size() <= 1 ? metrics_h : layout.size.y;
        f32 align_y = 0.0f;
        switch (_text.vertical_align())
        {
            case detail::TextVerticalAlign::center:
                align_y = amal::max((_content_size.y - layout_h) * 0.5f, 0.0f);
                break;
            case detail::TextVerticalAlign::bottom:
                align_y = amal::max(_content_size.y - layout_h, 0.0f);
                break;
            case detail::TextVerticalAlign::top:
            default:
                break;
        }

        const auto &line = layout.lines[line_index];
        const f32 line_y = line.glyph_count > 0 ? layout.glyphs[line.glyph_offset].pen.y - layout.ascender
                                                : static_cast<f32>(line_index) * layout.line_height;
        const amal::vec4 clip = get_clip_rect(text_content_clip_id());
        const f32 visible_top = amal::max(clip.y - _content_pos.y, 0.0f);
        const f32 visible_bottom = amal::min(clip.y + clip.w - _content_pos.y, _content_size.y);
        const f32 visible_h = amal::max(visible_bottom - visible_top, 0.0f);
        const f32 viewport_top = _content_scroll.y + visible_top;
        const f32 viewport_bottom = _content_scroll.y + (visible_h > 0.0f ? visible_bottom : _content_size.y);
        const f32 caret_top = align_y + line_y;
        const f32 caret_bottom = caret_top + line_h;

        if (caret_bottom > viewport_bottom) _content_scroll.y += caret_bottom - viewport_bottom;
        if (caret_top < viewport_top) _content_scroll.y -= viewport_top - caret_top;

        const f32 max_scroll =
            _scrollbar_y ? _scrollbar_y->max_scroll() : amal::max(layout.size.y - _content_size.y, 0.0f);
        _content_scroll.y = amal::clamp(_content_scroll.y, 0.0f, max_scroll);
        if (_scrollbar_y) _scrollbar_y->set_scroll_offset(_content_scroll.y);
        return old_scroll_y != _content_scroll.y;
    }

    void TextBox::update_text_content_clip_rect()
    {
        if (_content_size.x <= 0.0f || _content_size.y <= 0.0f) return;
        amal::vec4 rect{_content_pos.x, _content_pos.y, _content_size.x, _content_size.y};
        amal::vec4 parent_clip = parent() ? parent()->get_content_clip_rect() : rect;
        if (parent())
        {
            const amal::vec2 rect_min = {rect.x, rect.y};
            const amal::vec2 rect_max = {rect.x + rect.z, rect.y + rect.w};
            const amal::vec2 parent_min = {parent_clip.x, parent_clip.y};
            const amal::vec2 parent_max = {parent_clip.x + parent_clip.z, parent_clip.y + parent_clip.w};
            const amal::vec2 out_min = {amal::max(rect_min.x, parent_min.x), amal::max(rect_min.y, parent_min.y)};
            const amal::vec2 out_max = {amal::min(rect_max.x, parent_max.x), amal::min(rect_max.y, parent_max.y)};
            rect = {out_min.x, out_min.y, amal::max(out_max.x - out_min.x, 0.0f),
                    amal::max(out_max.y - out_min.y, 0.0f)};
        }
        if (_content_clip_id == 0xFFFFu) _content_clip_id = push_clip_rect(rect);
        else update_clip_rect(_content_clip_id, rect);
    }

    void TextBox::rebuild_selection_rect_cache()
    {
        if (!_edit) return;

        _edit->selection_rects.clear();
        if (_edit_state.cursors.empty()) return;

        const auto &cursor = _edit_state.primary_cursor();
        if (!cursor.has_selection()) return;

        const int start = cursor.select_start < cursor.select_end ? cursor.select_start : cursor.select_end;
        const int end = cursor.select_start < cursor.select_end ? cursor.select_end : cursor.select_start;
        const auto &layout = _text.layout_result();
        for (u32 line_i = 0; line_i < layout.lines.size(); ++line_i)
        {
            const auto &line = layout.lines[line_i];
            const int line_start = static_cast<int>(line.text_start);
            const int line_end = static_cast<int>(line.text_end);
            if (end < line_start || start > line_end) continue;

            const int segment_start = amal::max(start, line_start);
            const int segment_end = amal::min(end, line_end);
            if (segment_end < segment_start) continue;

            const f32 x0 = cursor_x_on_line(line_i, segment_start);
            const f32 x1 = cursor_x_on_line(line_i, segment_end);
            if (x1 <= x0) continue;
            _edit->selection_rects.push_back(line_selection_rect(line_i, x0, x1));
        }
    }

    u32 TextBox::line_index_from_cursor(int cursor) const { return line_index_from_cursor(cursor, false); }

    u32 TextBox::line_index_from_cursor(int cursor, bool cursor_at_end_of_line) const
    {
        const auto &layout = _text.layout_result();
        if (layout.lines.empty()) return 0;

        u32 best = 0;
        for (u32 i = 0; i < layout.lines.size(); ++i)
        {
            const auto &line = layout.lines[i];
            if (cursor < static_cast<int>(line.text_start)) return best;
            best = i;
            if (cursor < static_cast<int>(line.text_end)) return i;
            if (cursor == static_cast<int>(line.text_end))
            {
                const u32 next_i = i + 1;
                if (next_i < layout.lines.size() && cursor >= static_cast<int>(layout.lines[next_i].text_start))
                {
                    if (cursor_at_end_of_line) return i;
                    continue;
                }
                return i;
            }
        }
        return best;
    }

    void TextBox::refresh_text_layout_for_editing()
    {
        _text.set_position(_content_pos);
        _text.set_layout_size(_content_size);
        _text.set_clip_id(text_content_clip_id());
        _text.update_layout(true);
        update_content_scroll_x_for_cursor();
        _text.translate({-_content_scroll.x, -_content_scroll.y});
        _text.set_clip_id(text_content_clip_id());
        refresh_placeholder_layout();
        rebuild_selection_rect_cache();
    }

    void TextBox::refresh_placeholder_layout()
    {
        if (!_placeholder) return;
        _placeholder->set_position(_content_pos);
        _placeholder->set_layout_size(_content_size);
        _placeholder->set_clip_id(text_content_clip_id());
        _placeholder->update_layout(true);
        _placeholder->translate({0.0f, -_content_scroll.y});
        _placeholder->set_clip_id(text_content_clip_id());
    }

    u32 TextBox::required_selection_draw_slots() const
    {
        if (!_edit) return 0;
        return amal::max(static_cast<u32>(_edit->selection_rects.size()), 1u);
    }

    bool TextBox::edit_draw_slots_need_record() const
    {
        if (!_edit) return false;
        if (_edit->selections.size() < required_selection_draw_slots()) return true;
        if (is_password() && _edit->password_dots.size() < _text.layout_result().glyphs.size()) return true;
        if (is_password())
            for (const auto &draw_id : _edit->password_dots)
                if (draw_id.render_id == AUIK_INVALID_DRAW_DATA_ID) return true;
        if (_edit->caret.render_id == AUIK_INVALID_DRAW_DATA_ID) return true;
        for (const auto &draw_id : _edit->selection_drag_dots)
            if (draw_id.render_id == AUIK_INVALID_DRAW_DATA_ID) return true;
        if (_scrollbar_y && _scrollbar_y->is_visible() && !_scrollbar_y->has_draw_record()) return true;
        return false;
    }

    amal::vec2 TextBox::cursor_screen_pos(int cursor) const
    {
        const auto &layout = _text.layout_result();
        if (layout.lines.empty()) return {_content_pos.x, line_screen_y(0)};
        const bool is_primary = !_edit_state.cursors.empty() && _edit_state.primary_cursor().cursor == cursor;
        const bool at_end = is_primary && _edit_state.primary_cursor().cursor_at_end_of_line;
        const u32 line_index = line_index_from_cursor(cursor, at_end);
        return {_content_pos.x + cursor_x_on_line(line_index, cursor) - _content_scroll.x, line_screen_y(line_index)};
    }

    void TextBox::move_cursor_vertical(int dir, bool select)
    {
        if (_edit_state.cursors.empty()) return;
        refresh_text_layout_for_editing();
        const auto &layout = _text.layout_result();
        if (layout.lines.empty()) return;

        for (auto &cursor : _edit_state.cursors)
        {
            cursor.cursor = amal::clamp(cursor.cursor, 0, static_cast<int>(value().size()));
            if (!cursor.has_preferred_x)
            {
                const u32 line_index = line_index_from_cursor(cursor.cursor, cursor.cursor_at_end_of_line);
                cursor.preferred_x = cursor_x_on_line(line_index, cursor.cursor);
                cursor.has_preferred_x = 1;
            }

            const u32 line_index = line_index_from_cursor(cursor.cursor, cursor.cursor_at_end_of_line);
            const u32 target_line = dir < 0 ? (line_index > 0 ? line_index - 1 : 0)
                                            : amal::min(line_index + 1, static_cast<u32>(layout.lines.size() - 1));

            const int new_pos = cursor_from_line_x(target_line, cursor.preferred_x);
            const auto &target = layout.lines[target_line];
            const u32 next_line = target_line + 1;
            cursor.cursor_at_end_of_line = new_pos == static_cast<int>(target.text_end) &&
                                           next_line < layout.lines.size() &&
                                           new_pos == static_cast<int>(layout.lines[next_line].text_start);
            if (select)
            {
                if (!cursor.has_selection()) cursor.select_start = cursor.cursor;
                cursor.select_end = new_pos;
            }
            cursor.cursor = new_pos;
            if (!select)
            {
                cursor.select_start = cursor.cursor;
                cursor.select_end = cursor.cursor;
            }
        }
    }

    int TextBox::edit_string_len(void *user_data)
    {
        auto *self = static_cast<TextBox *>(user_data);
        return self ? static_cast<int>(self->value().size()) : 0;
    }

    detail::TextEditChar TextBox::edit_get_char(void *user_data, int char_idx)
    {
        auto *self = static_cast<TextBox *>(user_data);
        if (!self || char_idx < 0 || char_idx >= static_cast<int>(self->value().size())) return 0;
        return static_cast<unsigned char>(self->value()[char_idx]);
    }

    bool TextBox::edit_insert_chars(void *user_data, int pos, const detail::TextEditChar *text, int text_len)
    {
        return edit_replace_chars(user_data, pos, 0, text, text_len);
    }

    void TextBox::edit_delete_chars(void *user_data, int pos, int count)
    {
        edit_replace_chars(user_data, pos, count, nullptr, 0);
    }

    bool TextBox::edit_replace_chars(void *user_data, int pos, int delete_count, const detail::TextEditChar *text,
                                     int text_len)
    {
        auto *self = static_cast<TextBox *>(user_data);
        if (!self) return false;
        if (self->is_read_only()) return false;

        const acul::string &value = self->value();
        const int len = static_cast<int>(value.size());
        pos = amal::clamp(pos, 0, len);
        delete_count = amal::clamp(delete_count, 0, len - pos);
        if (delete_count <= 0 && (!text || text_len <= 0)) return true;

        const size_t start = static_cast<size_t>(pos);
        const size_t end = static_cast<size_t>(pos + delete_count);
        acul::string out;
        out.reserve(value.size() - static_cast<size_t>(delete_count) + static_cast<size_t>(amal::max(text_len, 0)));
        out.append(value.c_str(), start);
        for (int i = 0; text && i < text_len; ++i) out += static_cast<char>(text[i]);
        out.append(value.c_str() + end, value.size() - end);
        self->_value = out;
        self->sync_text_presentation();
        self->mark_changed();
        return true;
    }

    int TextBox::edit_next_char_index(void *user_data, int idx)
    {
        auto *self = static_cast<TextBox *>(user_data);
        return self ? detail::next_utf8_index(self->value(), idx) : idx + 1;
    }

    int TextBox::edit_prev_char_index(void *user_data, int idx)
    {
        auto *self = static_cast<TextBox *>(user_data);
        return self ? detail::prev_utf8_index(self->value(), idx) : idx - 1;
    }

    MultilineTextBox::MultilineTextBox(u32 id, const acul::string &value, amal::vec2 size, bool can_expand_to_content,
                                       WidgetFlags flags, Widget *parent, TextFlags text_flags,
                                       const acul::string &placeholder, bool read_only)
        : TextBox(id, value, size, flags, parent, AUIK_TAG_TEXTBOX, text_flags, placeholder, read_only,
                  detail::TextVerticalAlign::top, detail::TextWrapMode::word),
          _can_expand_to_content(can_expand_to_content)
    {
        _text.set_trim_trailing_spaces(false);
        if (_placeholder) _placeholder->set_trim_trailing_spaces(false);
        if (_edit) _edit->caret_style.tag_id = AUIK_STYLE_TAG_MULTILINE_CARET;
        detail::text_edit_initialize_state(&_edit_state, false);
    }

    void MultilineTextBox::set_can_expand_to_content(bool value)
    {
        if (_can_expand_to_content == value) return;
        _can_expand_to_content = value;
    }
} // namespace auik
