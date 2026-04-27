#include <auik/v2/auik.hpp>
#include <auik/v2/pipelines.hpp>
#include <auik/v2/widgets/textbox.hpp>

#define AUIK_TEXTBOX_CARET_ON_TIME                    0.80
#define AUIK_TEXTBOX_CARET_PERIOD                     1.20
#define AUIK_TEXTBOX_CARET_BLINK_TASK_SCHEDULED_BIT   0x01u
#define AUIK_TEXTBOX_SELECTION_DRAG_PRESS_PENDING_BIT 0x02u
#define AUIK_TEXTBOX_SELECTION_DRAG_ACTIVE_BIT        0x04u
#define AUIK_TEXTBOX_SELECTION_DRAG_DOT_COUNT         5u

namespace auik::v2
{
    static WidgetFlags resolve_textbox_widget_flags(WidgetFlags flags, bool read_only)
    {
        if (read_only) flags &= ~WidgetFlagBits::hittable;
        else flags |= WidgetFlagBits::hittable;
        return flags;
    }

    static EventFlags resolve_textbox_event_flags(bool read_only)
    {
        if (read_only) return EventFlagBits::none;
        return EventFlagBits::hover | EventFlagBits::click | EventFlagBits::drag | EventFlagBits::focus |
               EventFlagBits::key_input | EventFlagBits::char_input | EventFlagBits::shortcut;
    }

    static detail::TextEditKey map_key(Key key, KeyMode mods, bool multiline)
    {
        detail::TextEditKey out = 0;
        switch (key)
        {
            case Key::left:
                out = (mods & KeyModeBits::control) ? AUIK_TEXT_EDIT_KEY_WORD_LEFT : AUIK_TEXT_EDIT_KEY_LEFT;
                break;
            case Key::right:
                out = (mods & KeyModeBits::control) ? AUIK_TEXT_EDIT_KEY_WORD_RIGHT : AUIK_TEXT_EDIT_KEY_RIGHT;
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

        const f32 dot_size = amal::max(amal::round(line_h * 0.14f), 1.5f);
        const f32 dot_step = line_h > dot_size ? (line_h - dot_size) / 4.0f : dot_size;
        const u32 fill = visible ? style.background_color() : 0;
        const u32 border = visible ? style.border_color() : 0;
        QuadsInstanceData dots[AUIK_TEXTBOX_SELECTION_DRAG_DOT_COUNT]{};
        for (u32 i = 0; i < AUIK_TEXTBOX_SELECTION_DRAG_DOT_COUNT; ++i)
        {
            auto &dot = dots[i];
            dot.rect = {{drop_pos.x - dot_size * 0.5f, drop_pos.y + static_cast<f32>(i) * dot_step},
                        {dot_size, dot_size}};
            dot.background_color = fill;
            dot.border_color = border;
            dot.border_radius = dot_size * 0.5f;
            dot.border_thickness = visible ? style.border_thickness() : 0.0f;
            dot.z_order = z_order;
            u32 flags = 0u;
            if (dot.border_radius > 0.0f) flags |= AUIK_HAS_RADIUS_BIT;
            if (dot.border_thickness > 0.0f) flags |= AUIK_HAS_BORDER_BIT;
            dot.mask = static_cast<u32>(clip_id) | (flags << 20u);
        }

        if (ctx.is_recording())
            push_data_batch_to_stream(stream, dots, AUIK_TEXTBOX_SELECTION_DRAG_DOT_COUNT, draw_ids);
        else update_data_batch_in_stream(stream, draw_ids, dots, AUIK_TEXTBOX_SELECTION_DRAG_DOT_COUNT);
    }

    struct TextBoxEditData
    {
        acul::vector<DrawDataID> selections;
        acul::vector<DrawDataID> carets;
        DrawDataID selection_drag_dots[AUIK_TEXTBOX_SELECTION_DRAG_DOT_COUNT]{};
        StyleSelector caret_style{Theme::STYLE_ID_INVALID, AUIK_TAG_CARET};
        StyleSelector selection_style{Theme::STYLE_ID_INVALID, AUIK_TAG_SELECTION};
        StyleSelector drag_icon_style{Theme::STYLE_ID_INVALID, AUIK_TAG_TEXT_DRAG_ICON};
        f64 caret_anim_reset_time = 0.0;
        u8 flags = 0;
    };

    TextBox::TextBox(u32 id, const acul::string &value, amal::vec2 size, WidgetFlags flags, Widget *parent,
                     u32 style_tag_id, TextFlags text_flags, const acul::string &placeholder, bool read_only)
        : Widget(id, resolve_textbox_widget_flags(flags, read_only), resolve_textbox_event_flags(read_only), parent,
                 {{0.0f, 0.0f}, size}, style_tag_id),
          _text(AUIK_TAG_TEXT, value, amal::vec2{0.0f, 0.0f},
                WidgetFlagBits::visible |
                    ((flags & WidgetFlagBits::fixed) ? WidgetFlagBits::fixed : WidgetFlagBits::none),
                this, AUIK_TAG_NO_PAD),
          _placeholder(placeholder.empty()
                           ? nullptr
                           : acul::alloc<Text>(
                                 AUIK_TAG_TEXT, placeholder, amal::vec2{0.0f, 0.0f},
                                 WidgetFlagBits::visible |
                                     ((flags & WidgetFlagBits::fixed) ? WidgetFlagBits::fixed : WidgetFlagBits::none),
                                 this, AUIK_TAG_PLACEHOLDER)),
          _edit(read_only ? nullptr : acul::alloc<TextBoxEditData>()),
          _read_only(read_only)
    {
        this->text_flags = text_flags;
        _text.set_overflow_mode(detail::TextOverflowMode::clip);
        _text.set_vertical_align(detail::TextVerticalAlign::center);
        if (_placeholder)
        {
            _placeholder->set_overflow_mode(detail::TextOverflowMode::clip);
            _placeholder->set_vertical_align(detail::TextVerticalAlign::center);
        }
        detail::text_edit_initialize_state(&_edit_state, true);
        if (is_read_only()) return;
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
        _text.update_style();
        if (_placeholder) _placeholder->update_style();
        return flags;
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

        amal::vec2 min_size = size();
        if (min_size.x <= 0.0f) min_size.x = 160.0f;
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
        if (box_size.y <= 0.0f || should_resize_to_content()) box_size.y = required_size().y - margin.y - margin.w;
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
        _text.set_position(_content_pos);
        _text.set_size(_content_size);
        _text.update_layout(true);
        _text.set_clip_id(clip_id());
        if (_placeholder)
        {
            _placeholder->set_position(_content_pos);
            _placeholder->set_size(_content_size);
            _placeholder->update_layout(true);
            _placeholder->set_clip_id(clip_id());
        }
    }

    void TextBox::translate(const amal::vec2 &delta)
    {
        if (delta.x == 0.0f && delta.y == 0.0f) return;
        Widget::translate(delta);
        _content_pos += delta;
        _text.translate(delta);
        if (_placeholder) _placeholder->translate(delta);
    }

    void TextBox::rebuild_clip_rects()
    {
        assert(parent() && "TextBox must have parent");
        set_clip_id(parent()->content_clip_id());
        _bg.hit_id = AUIK_INVALID_DRAW_DATA_ID;
        _text.set_clip_id(clip_id());
        _text.rebuild_clip_rects();
        if (_placeholder)
        {
            _placeholder->set_clip_id(clip_id());
            _placeholder->rebuild_clip_rects();
        }
    }

    void TextBox::update_depth(const amal::vec2 &depth_range)
    {
        Widget::update_depth(depth_range);
        amal::vec2 text_range{};
        assign_next_depth(this->depth_range(), text_range);
        _text.update_depth(text_range);
        if (_placeholder) _placeholder->update_depth(text_range);
    }

    void TextBox::draw(DrawCtx &ctx)
    {
        const bool transient = ctx.reason & DrawReasonBits::transient;
        const bool draw_transient_payload = transient || ctx.is_recording();
        const f32 selection_z = next_depth(depth_range());
        if (!transient)
        {
            auto *quads_stream = get_primary_quads_stream();
            QuadsInstanceData bg{};
            bg.rect = bounds();
            bg.z_order = get_z_order();
            const bool bg_visible = fill_quads_instance_by_style(get_theme()->get_style(_style.id), clip_id(), bg);
            if (should_emit_quads_instance(bg_visible, _bg, ctx.emit_hit_rect))
                ctx.emit(quads_stream, _bg, &bg, get_rect(), ctx.emit_hit_rect);

            if (_edit)
            {
                const auto &selection_style = get_theme()->get_style(_edit->selection_style.id);
                const f32 selection_h = amal::max(get_theme()->get_style(_style.id).text_size(), 1.0f);
                const u32 caret_count = amal::max(caret_draw_slots(), static_cast<u32>(_edit->selections.size()));
                if (_edit->selections.size() < caret_count) _edit->selections.resize(caret_count);
                for (u32 i = 0; i < caret_count; ++i)
                {
                    QuadsInstanceData selection{};
                    selection.rect = {_content_pos, {0.0f, selection_h}};
                    selection.z_order = selection_z;
                    selection.background_color = 0;
                    selection.mask = clip_id();

                    if (i < _edit_state.cursors.size() && _edit_state.cursors[i].has_selection())
                    {
                        const auto &cursor = _edit_state.cursors[i];
                        const int start =
                            cursor.select_start < cursor.select_end ? cursor.select_start : cursor.select_end;
                        const int end =
                            cursor.select_start < cursor.select_end ? cursor.select_end : cursor.select_start;
                        const amal::vec2 start_pos = cursor_screen_pos(start);
                        const amal::vec2 end_pos = cursor_screen_pos(end);
                        selection.rect = {{start_pos.x, start_pos.y},
                                          {amal::max(end_pos.x - start_pos.x, 0.0f), selection_h}};
                        selection.background_color = selection_style.background_color();
                    }

                    ctx.emit(quads_stream, _edit->selections[i], &selection,
                             detail::make_rect_data(id(), AUIK_TAG_SELECTION, selection.rect), false);
                }
            }

            DrawCtx text_ctx = ctx;
            text_ctx.emit_hit_rect = false;
            Text *display_text = show_placeholder() ? _placeholder : &_text;
            if (display_text) display_text->draw(text_ctx);
        }

        if (!_edit) return;
        if (!draw_transient_payload) return;

        auto *overlay_stream = get_overlay_quads_stream();
        if (!overlay_stream) return;

        const bool active = detail::get_context().focus_id == id();
        bool blink_on = active;
        if (active)
        {
            detail::update_window_time(detail::get_context().window_ctx);
            const f64 elapsed = detail::get_context().window_ctx->time - _edit->caret_anim_reset_time;
            blink_on = std::fmod(elapsed, AUIK_TEXTBOX_CARET_PERIOD) <= AUIK_TEXTBOX_CARET_ON_TIME;
        }

        const auto &style = get_theme()->get_style(_style.id);
        const auto &caret_style = get_theme()->get_style(_edit->caret_style.id);
        const auto caret_padding = caret_style.padding();
        const u32 caret_count = amal::max(caret_draw_slots(), static_cast<u32>(_edit->carets.size()));
        if (_edit->carets.size() < caret_count) _edit->carets.resize(caret_count);
        for (u32 i = 0; i < caret_count; ++i)
        {
            const bool visible = blink_on && i < _edit_state.cursors.size();
            const auto caret_pos = visible ? cursor_screen_pos(_edit_state.cursors[i].cursor) : _content_pos;
            QuadsInstanceData caret{};
            const f32 caret_base_h = amal::max(style.text_size(), 1.0f);
            const f32 caret_overflow = amal::max(caret_padding.y, 0.0f);
            const f32 caret_w = amal::max(caret_padding.x, 1.0f);
            const f32 caret_h = amal::max(caret_base_h + caret_overflow * 2.0f, 1.0f);
            caret.rect = {{caret_pos.x, caret_pos.y - caret_overflow}, {caret_w, caret_h}};
            caret.z_order = next_depth(depth_range());
            caret.background_color = visible ? caret_style.background_color() : 0;
            caret.mask = clip_id();
            ctx.emit(overlay_stream, _edit->carets[i], &caret, detail::make_rect_data(id(), AUIK_TAG_CARET, caret.rect),
                     false);
        }

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
                            (selection_z + _text.get_z_order()) * 0.5f, clip_id(), drag_icon_style, show_drag_icon);
    }

    void TextBox::on_focus(bool focused)
    {
        if (!focused && _edit) _edit->flags &= ~AUIK_TEXTBOX_CARET_BLINK_TASK_SCHEDULED_BIT;

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
            redraw_all_commands();
        });
    }

    void TextBox::on_hover(HoverState state)
    {
        if (is_read_only()) return;
        auto &ctx = detail::get_context();
        detail::set_window_cursor(state == HoverState::leave ? detail::CursorID::arrow : detail::CursorID::ibeam,
                                  ctx.window_ctx);
    }

    void TextBox::on_click(MouseKey key, KeyPressState state, u32 click_count)
    {
        (void)click_count;
        if (is_read_only()) return;
        if (key != MouseKey::left || state != KeyPressState::press) return;
        if (_edit &&
            (_edit->flags & (AUIK_TEXTBOX_SELECTION_DRAG_PRESS_PENDING_BIT | AUIK_TEXTBOX_SELECTION_DRAG_ACTIVE_BIT)))
            return;
        set_style_state(StyleState::focus);
        collapse_cursor_at_point(get_mouse_pos());
        add_render_command<detail::ClickEventTraits>(this, [this]() {
            update_style();
            apply_render_update(false);
        });
    }

    void TextBox::on_drag(const amal::vec2 &delta, KeyPressState state)
    {
        if (is_read_only()) return;
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
        if (_edit_state.cursors.empty()) return false;
        const auto &cursor = _edit_state.primary_cursor();
        if (!cursor.has_selection()) return false;

        const int start = cursor.select_start < cursor.select_end ? cursor.select_start : cursor.select_end;
        const int end = cursor.select_start < cursor.select_end ? cursor.select_end : cursor.select_start;
        const auto start_pos = cursor_screen_pos(start);
        const auto end_pos = cursor_screen_pos(end);
        const auto &style = get_theme()->get_style(_style.id);
        const f32 selection_h = amal::max(style.text_size(), 1.0f);
        return point.x >= start_pos.x && point.x <= end_pos.x && point.y >= start_pos.y &&
               point.y <= start_pos.y + selection_h;
    }

    bool TextBox::begin_selection_drag_press()
    {
        if (!_edit) return false;
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
        _edit_state.cursors.resize(1);
        reset_caret_blink();
        schedule_caret_blink();
    }

    void TextBox::select_all_text()
    {
        if (is_read_only()) return;
        const auto &text = value();
        if (text.empty()) return;

        auto &cursor = _edit_state.primary_cursor();
        cursor.cursor = static_cast<int>(text.size());
        cursor.select_start = 0;
        cursor.select_end = cursor.cursor;
        cursor.has_preferred_x = 0;
        _edit_state.cursors.resize(1);
        reset_caret_blink();
        schedule_caret_blink();
        add_render_command<detail::KeyEventTraits>(this, [this]() { apply_render_update(false); });
    }

    void TextBox::copy_selection_to_clipboard()
    {
        if (is_read_only()) return;
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
        _text.set_text(text);
        cursor.cursor = start;
        cursor.select_start = start;
        cursor.select_end = start;
        cursor.has_preferred_x = 0;
        _edit_state.cursors.resize(1);
        reset_caret_blink();
        schedule_caret_blink();
        add_render_command<detail::KeyEventTraits>(this, [this]() { apply_render_update(true); });
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
        _text.set_text(moved);

        cursor.cursor = drop_cursor + static_cast<int>(selection.size());
        cursor.select_start = drop_cursor;
        cursor.select_end = cursor.cursor;
        cursor.has_preferred_x = 0;
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
        if (layout_dirty) add_render_command<detail::DragEventTraits>(this, [this]() { apply_render_update(true); });
    }

    void TextBox::on_key(Key key, KeyPressState state, KeyMode mods)
    {
        if (is_read_only()) return;
        if (state == KeyPressState::release) return;
        if (key == Key::enter && accepts_newline())
        {
            detail::TextEditChar newline = '\n';
            auto edit_string = make_text_edit_string();
            detail::text_edit_text(&edit_string, &_edit_state, &newline, 1);
            commit_text_edit();
            add_render_command<detail::KeyEventTraits>(this, [this]() { apply_render_update(true); });
            return;
        }

        const auto edit_key = map_key(key, mods, accepts_newline());
        if (!edit_key) return;

        const acul::string before_text = value();
        auto edit_string = make_text_edit_string();
        detail::text_edit_key(&edit_string, &_edit_state, edit_key);
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
        commit_text_edit();
        add_render_command<detail::CharEventTraits>(this, [this]() { apply_render_update(true); });
    }

    void TextBox::set_value(const acul::string &value)
    {
        _text.set_text(value);
        auto &cursor = _edit_state.primary_cursor();
        cursor.cursor = static_cast<int>(this->value().size());
        cursor.select_start = cursor.cursor;
        cursor.select_end = cursor.cursor;
        detail::get_context().dirty_flags |= DirtyFlagBits::layout;
        detail::mark_host_refresh_request();
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
            _placeholder->set_position(_content_pos);
            _placeholder->set_size(_content_size);
            _placeholder->set_clip_id(clip_id());
        }
        else
        {
            _placeholder = acul::alloc<Text>(
                AUIK_TAG_TEXT, value, amal::vec2{0.0f, 0.0f},
                WidgetFlagBits::visible |
                    ((widget_flags & WidgetFlagBits::fixed) ? WidgetFlagBits::fixed : WidgetFlagBits::none),
                this, AUIK_TAG_PLACEHOLDER);
            _placeholder->set_overflow_mode(detail::TextOverflowMode::clip);
            _placeholder->set_vertical_align(detail::TextVerticalAlign::center);
            _placeholder->set_position(_content_pos);
            _placeholder->set_size(_content_size);
            _placeholder->set_clip_id(clip_id());
            _placeholder->update_style();
            _placeholder->update_depth(_text.depth_range());
        }
        detail::get_context().dirty_flags |= DirtyFlagBits::layout;
        detail::mark_host_refresh_request();
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
        const f64 remaining = phase <= AUIK_TEXTBOX_CARET_ON_TIME ? (AUIK_TEXTBOX_CARET_ON_TIME - phase)
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

    void TextBox::update_transient_draw_commands()
    {
        if (!_edit) return;
        if (detail::get_context().focus_id != id()) return;
        if (_edit->carets.empty() || _edit->carets[0].render_id == AUIK_INVALID_DRAW_DATA_ID) return;
        update_draw_commands(DrawReasonBits::transient);
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

        const f32 local_x = point.x - _content_pos.x;
        if (local_x <= 0.0f) return 0;
        if (local_x >= layout.size.x) return static_cast<int>(value().size());

        const auto &line = layout.lines[0];
        const auto &text = value();
        int best = static_cast<int>(line.text_start);
        f32 best_dist = 3.4e38f;
        for (u32 i = 0; i < line.glyph_count; ++i)
        {
            const auto &glyph = layout.glyphs[line.glyph_offset + i];
            const int glyph_start = static_cast<int>(glyph.cluster);
            const int glyph_end = detail::next_utf8_index(text, glyph_start);
            const f32 start_dist = amal::abs(local_x - glyph.pen.x);
            if (start_dist < best_dist)
            {
                best_dist = start_dist;
                best = glyph_start;
            }
            const f32 end_dist = amal::abs(local_x - (glyph.pen.x + glyph.advance.x));
            if (end_dist < best_dist)
            {
                best_dist = end_dist;
                best = glyph_end;
            }
        }
        return best;
    }

    amal::vec2 TextBox::cursor_screen_pos(int cursor) const
    {
        const auto &layout = _text.layout_result();
        const auto &style = get_theme()->get_style(_style.id);
        const f32 caret_h = amal::max(style.text_size(), 1.0f);
        if (layout.lines.empty())
            return {_content_pos.x, _content_pos.y + amal::max((_content_size.y - caret_h) * 0.5f, 0.0f)};
        const auto &line = layout.lines[0];
        f32 x = 0.0f;
        for (u32 i = 0; i < line.glyph_count; ++i)
        {
            const auto &glyph = layout.glyphs[line.glyph_offset + i];
            if (static_cast<int>(glyph.cluster) >= cursor) break;
            x = glyph.pen.x + glyph.advance.x;
        }
        if (cursor >= static_cast<int>(line.text_end)) x = line.width;

        const f32 line_h = amal::max(layout.ascender - layout.descender, 0.0f);
        const f32 align_h = line_h > 0.0f ? line_h : layout.size.y;
        const f32 align_y = _content_pos.y + amal::max((_content_size.y - align_h) * 0.5f, 0.0f);
        const f32 y = align_y + amal::max((align_h - caret_h) * 0.5f, 0.0f);
        return {_content_pos.x + x, y};
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
        self->_text.set_text(out);
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

    MultilineTextBox::MultilineTextBox(u32 id, const acul::string &value, amal::vec2 size, bool resize_to_content,
                                       WidgetFlags flags, Widget *parent, TextFlags text_flags,
                                       const acul::string &placeholder, bool read_only)
        : TextBox(id, value, size, flags, parent, AUIK_TAG_TEXTBOX, text_flags, placeholder, read_only),
          _resize_to_content(resize_to_content)
    {
        _text.set_multiline(true);
        detail::text_edit_initialize_state(&_edit_state, false);
    }

    void MultilineTextBox::set_resize_to_content(bool value)
    {
        if (_resize_to_content == value) return;
        _resize_to_content = value;
        detail::get_context().dirty_flags |= DirtyFlagBits::layout;
        detail::mark_host_refresh_request();
    }
} // namespace auik::v2
