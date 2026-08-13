#include <acul/hash/utils.hpp>
#include <auik/auik.hpp>
#include <auik/detail/context.hpp>
#include <auik/pipelines.hpp>
#include <auik/widgets/text.hpp>
#include "../core/session_stream_utils.hpp"

#define AUIK_TOOLTIP_SHOW_DELAY             0.35
#define AUIK_TEXT_SHRINK_RECORD_RATIO       8
#define AUIK_TEXT_SHRINK_RECORD_MIN_REMOVED 64
#define AUIK_ETEXT_CARET_ON_TIME            0.80
#define AUIK_ETEXT_CARET_PERIOD             1.20
#define AUIK_ETEXT_CARET_TASK_BIT           0x01u

namespace auik
{
    static inline bool text_style_has(const Text &widget, const Style &style,
                                      detail::StylePropertiesBits::enum_type property)
    {
        return (widget.style_mask() & style.mask() & property) != 0u;
    }

    static inline amal::vec4 text_style_margin(const Text &widget, const Style &style)
    {
        return text_style_has(widget, style, detail::StylePropertiesBits::margin) ? style.margin() : amal::vec4{};
    }

    static inline amal::vec4 text_style_padding(const Text &widget, const Style &style)
    {
        return text_style_has(widget, style, detail::StylePropertiesBits::padding) ? style.padding() : amal::vec4{};
    }

    static inline Font *text_style_font(const Text &widget, const Style &style)
    {
        return text_style_has(widget, style, detail::StylePropertiesBits::font) ? style.font() : nullptr;
    }

    static inline f32 text_style_size(const Text &widget, const Style &style)
    {
        return text_style_has(widget, style, detail::StylePropertiesBits::text_size) ? style.text_size() : 0.0f;
    }

    static inline u32 text_style_color(const Text &widget, const Style &style)
    {
        return text_style_has(widget, style, detail::StylePropertiesBits::text_color) ? style.text_color() : 0u;
    }

    static inline f32 text_style_min_width(const Text &widget, const Style &style)
    {
        return text_style_has(widget, style, detail::StylePropertiesBits::min_width) ? style.min_width() : 0.0f;
    }

    static inline f32 text_style_min_height(const Text &widget, const Style &style)
    {
        return text_style_has(widget, style, detail::StylePropertiesBits::min_height) ? style.min_height() : 0.0f;
    }

    static inline u32 make_presented_model_text_id(const ModelBinding &binding, const ModelRecord &record,
                                                   ModelFieldID field_id)
    {
        size_t seed = AUIK_TAG_TEXT;
        acul::hash_combine(seed, binding.model_id);
        acul::hash_combine(seed, record.id);
        acul::hash_combine(seed, field_id);
        return static_cast<u32>(seed != 0u ? seed : AUIK_TAG_TEXT);
    }

    AUIK_EXPORT void present_model_text_record(ModelBinding *binding, ModelRecord &record, u32 record_index,
                                               Widget **widgets, u32 widget_count, void *data)
    {
        (void)record_index;
        (void)data;
        if (!binding || !widgets) return;
        const u32 count = amal::min(widget_count, static_cast<u32>(binding->presenter.field_ids.size()));
        for (u32 index = 0u; index < count; ++index)
        {
            const ModelFieldID field_id = binding->presenter.field_ids[index];
            ModelFieldAccess access{};
            if (!access_model_binding_field(*binding, record.id, field_id, access)) continue;
            const auto *value =
                static_cast<const acul::string *>(get_model_field_access_data(binding->db, binding->model_id, access));
            widgets[index] = make_text(make_presented_model_text_id(*binding, record, field_id),
                                       value ? StringView{*value} : StringView{});
        }
    }

    static bool should_record_text_shrink(size_t old_count, size_t new_count)
    {
        if (old_count <= new_count) return false;
        if (old_count - new_count < AUIK_TEXT_SHRINK_RECORD_MIN_REMOVED) return false;
        if (new_count == 0) return true;
        return old_count >= new_count * AUIK_TEXT_SHRINK_RECORD_RATIO;
    }

    static amal::vec2 resolve_text_size(const Text &widget, const amal::vec2 &measured_size, const amal::vec4 &margin)
    {
        amal::vec2 out = {amal::max(widget.size().x - margin.x - margin.z, 0.0f),
                          amal::max(widget.size().y - margin.y - margin.w, 0.0f)};
        if (!is_size_concrete(out.x) || out.x <= 0.0f) out.x = measured_size.x;
        if (!is_size_concrete(out.y) || out.y <= 0.0f) out.y = measured_size.y;
        return out;
    }

    struct TextVisualYBounds
    {
        f32 min = 0.0f;
        f32 max = 0.0f;
        bool valid = false;
    };

    static TextVisualYBounds resolve_visual_y_bounds(const detail::TextLayoutResult &layout)
    {
        TextVisualYBounds out{};
        for (const auto &glyph : layout.glyphs)
        {
            if (amal::is_rect_empty(glyph.rect)) continue;
            const f32 glyph_min = glyph.rect.offset.y;
            const f32 glyph_max = glyph.rect.offset.y + glyph.rect.size.y;
            if (!out.valid)
            {
                out.min = glyph_min;
                out.max = glyph_max;
                out.valid = true;
                continue;
            }
            out.min = amal::min(out.min, glyph_min);
            out.max = amal::max(out.max, glyph_max);
        }
        return out;
    }

    static f32 resolve_layout_height_for_widget(const Text &widget, const detail::TextLayoutResult &layout)
    {
        if (!widget.tight_content_height()) return layout.size.y;

        const auto visual_bounds = resolve_visual_y_bounds(layout);
        if (visual_bounds.valid) return amal::max(visual_bounds.max - visual_bounds.min, 0.0f);

        const f32 metrics_height = amal::max(layout.ascender - layout.descender, 0.0f);
        if (layout.lines.size() <= 1 && metrics_height > 0.0f) return metrics_height;
        return layout.size.y;
    }

    static f32 resolve_text_align_y_offset(const Text &widget, const Style &style,
                                           const detail::TextLayoutResult &layout, const amal::vec2 &bounds_size)
    {
        const auto *align =
            text_style_has(widget, style, detail::StylePropertiesBits::extra) ? style.align_settings() : nullptr;
        if (!align) return 0.0f;

        const f32 metrics_height = amal::max(layout.ascender - layout.descender, 0.0f);
        const f32 layout_h = layout.lines.size() <= 1 && metrics_height > 0.0f ? metrics_height : layout.size.y;
        const ChildLayoutFlags flags{align->flags};
        if (flags & ChildLayoutFlagBits::vcenter) return amal::max((bounds_size.y - layout_h) * 0.5f, 0.0f);
        if (flags & ChildLayoutFlagBits::bottom) return amal::max(bounds_size.y - layout_h, 0.0f);
        return 0.0f;
    }

    static amal::vec2 resolve_text_widget_render_origin(const Text &widget, const Style &style,
                                                        const detail::TextLayoutResult *layout = nullptr,
                                                        const amal::vec2 *bounds_size = nullptr)
    {
        amal::vec2 origin = widget.position();
        if (layout && widget.tight_content_height())
        {
            const auto visual_bounds = resolve_visual_y_bounds(*layout);
            if (visual_bounds.valid) origin.y -= visual_bounds.min;
        }
        if (layout && bounds_size) origin.y += resolve_text_align_y_offset(widget, style, *layout, *bounds_size);
        return origin;
    }

    static void invalidate_text_draw_ids(DrawStream *stream, acul::vector<DrawDataID> &draw_ids, size_t first)
    {
        if (!stream || first >= draw_ids.size()) return;
        invalidate_data_batch_in_stream(stream, draw_ids.data() + first, static_cast<u32>(draw_ids.size() - first));
    }

    StyleUpdateFlags Text::update_style()
    {
        const u32 parent_id = parent() ? parent()->id() : 0u;
        auto *theme = get_theme();
        const bool invalidate_previous = detail::get_context().dirty_flags & DirtyFlagBits::styles;
        const StyleID prev_style_id = invalidate_previous ? Theme::STYLE_ID_INVALID : _style.id;
        const StyleID next_style_id = theme->get_resolved_style(_style.tag_id, id(), parent_id, style_state());
        _style.id = next_style_id;
        StyleUpdateFlags flags{};
        if (prev_style_id == Theme::STYLE_ID_INVALID)
        {
            const Style empty_style{};
            flags = make_style_update_flags(empty_style, theme->get_style(next_style_id), _style_mask);
        }
        else if (prev_style_id != next_style_id)
            flags =
                make_style_update_flags(theme->get_style(prev_style_id), theme->get_style(next_style_id), _style_mask);
        const auto &style = get_theme()->get_style(_style.id);
        apply_style_layout(style, _style_mask);
        // Glyph instances cache the resolved tint. A style-only redraw does not
        // rebuild text layout, so make sure draw() still uploads the new tint.
        if (flags & StyleUpdateFlagBits::redraw) _instances_gpu_dirty = true;
        if (const auto *text =
                text_style_has(*this, style, detail::StylePropertiesBits::extra) ? style.text_settings() : nullptr)
        {
            set_multiline(text->wrap == TextWrapMode::word);
            set_overflow_mode(text->overflow);
        }
        if (flags & (StyleUpdateFlagBits::layout | StyleUpdateFlagBits::parent_layout)) invalidate_layout_measure();
        return flags;
    }

    Text *Text::clone(u32 id) const
    {
        auto *out = acul::alloc<Text>(id, source_text(), style_size(), widget_flags, _layout_config.flags);
        out->set_style_tag(_style.tag_id);
        out->_style_mask = _style_mask;
        out->text_flags = text_flags;
        out->set_max_lines(_layout_config.max_lines);
        out->set_tight_content_height(_tight_content_height);
        struct RefUserData
        {
            u32 tag = 0u;
            void *handle = nullptr;
        };
        acul::vector<RefUserData> refs;
        for (auto *node = user_data_head(); node; node = node->pNext)
        {
            if (node->destroy || !node->handle || node->tag_id == AUIK_UD_LOCALE_LITERAL) continue;
            refs.push_back({node->tag_id, node->handle});
        }
        for (size_t i = refs.size(); i > 0u; --i)
            out->emplace_user_data_ref_head(refs[i - 1u].tag, refs[i - 1u].handle);
        return out;
    }

    void Text::set_text(StringView text)
    {
        if (text.is_translated)
        {
            set_translated_text_literal(text.str);
            return;
        }
        set_text(acul::string(text.str ? text.str : ""));
    }

    void Text::set_model_binding(ModelBinding *binding)
    {
        if (_model_value_binding == binding)
        {
            apply_model_binding_value();
            return;
        }

        if (_model_value_binding)
        {
            _model_value_binding->on_field_change = nullptr;
            detach_model_binding(*_model_value_binding);
        }
        _model_value_binding = binding;
        if (!_model_value_binding) return;

        _model_value_binding->on_field_change = [this](ModelRecordID, ModelFieldID) { apply_model_binding_value(); };
        attach_model_binding(*_model_value_binding);
        apply_model_binding_value();
    }

    bool Text::apply_model_binding_value()
    {
        acul::string next_text{};
        if (_model_value_binding)
        {
            if (!read_model_binding_value(*_model_value_binding, next_text)) return false;
        }
        else return false;

        if (_text == next_text) return false;
        set_text(next_text);
        if (mark_changed()) return true;

        auto &ctx = detail::get_context();
        const u32 widget_id = id();
        const auto it = ctx.id_map.find(widget_id);
        if (it == ctx.id_map.end() || it->second != this) return true;

        Widget *expected_widget = this;
        add_render_command([widget_id, expected_widget]() {
            auto &ctx = detail::get_context();
            const auto it = ctx.id_map.find(widget_id);
            if (it == ctx.id_map.end() || it->second != expected_widget) return;
            auto *self = static_cast<Text *>(it->second);

            if (self->_style.id == Theme::STYLE_ID_INVALID) self->update_style_invalidated();

            const amal::vec2 prev_required_size = self->required_size();
            self->update_layout_min_size();
            const bool needs_parent_layout = prev_required_size != self->required_size();
            if (needs_parent_layout)
            {
                if (Widget *target = resolve_parent_layout_update_target(self))
                {
                    target->update_layout(false);
                    target->update_draw_commands(DrawReasonBits::layout);
                }
            }
            else
            {
                self->update_layout(true);
                self->update_draw_commands(DrawReasonBits::layout);
            }

            ctx.dirty_flags |= DirtyFlagBits::redraw | DirtyFlagBits::hit_rect_update;
        });
        return true;
    }

    void Text::set_translated_text_literal(const char *literal)
    {
        _translated_text = true;
        _translation_literal = literal;
        emplace_user_data_ref_after_head(AUIK_UD_LOCALE_LITERAL, const_cast<char *>(_translation_literal.c_str()));
        _text = _translation_literal;
        update_translated_text();
    }

    void Text::clear_translated_text_literal()
    {
        _translated_text = false;
        _translation_literal.clear();
        emplace_user_data_ref_head(AUIK_UD_LOCALE_LITERAL, nullptr);
    }

    const char *Text::translated_text_literal() const
    {
        return static_cast<const char *>(get_user_data(AUIK_UD_LOCALE_LITERAL));
    }

    bool Text::update_translated_text()
    {
        if (!_translated_text) return false;
        const char *literal = translated_text_literal();
        if (!literal) return false;
        StringView value{literal, true};
        const acul::string next = translate_string(value);
        if (_text == next) return false;
        _text = next;
        invalidate_layout_measure();
        _layout_result.clear();
        _instances_gpu_dirty = true;
        invalidate_draw_records();
        return true;
    }

    void Text::update_layout_min_size_force()
    {
        assert(_style.id != Theme::STYLE_ID_INVALID && "Text style must be resolved before layout update");
        if (detail::get_context().dirty_flags & DirtyFlagBits::locale) update_translated_text();
        _layout_result.clear();
        _content_bounds = {position(), {0.0f, 0.0f}};
        _instances.clear();
        _instances_gpu_dirty = true;
        const auto &style = get_theme()->get_style(_style.id);
        const amal::vec4 margin = text_style_margin(*this, style);
        const amal::vec4 padding = text_style_padding(*this, style);
        const f32 min_width = text_style_min_width(*this, style);
        const f32 min_height = text_style_min_height(*this, style);
        auto *font = text_style_font(*this, style);
        const u32 font_size_px = round_font_px(text_style_size(*this, style));
        const bool allow_empty_layout = multiline();
        if (!font || (!allow_empty_layout && _text.empty()) || font_size_px == 0)
        {
            const amal::vec2 content_size = {
                amal::max(is_width_fixed() && !fill_width() ? style_size().x : 0.0f, min_width),
                amal::max(is_height_fixed() && !fill_height() ? style_size().y : 0.0f, min_height)};
            set_required_size({content_size.x + padding.x + padding.z + margin.x + margin.z,
                               content_size.y + padding.y + padding.w + margin.y + margin.w});
            return;
        }

        auto measure_config = _layout_config;
        if (width_mode() == TextLayoutWidthMode::bounds && is_size_concrete(size().x) && size().x > 0.0f &&
            (is_width_fixed() || multiline()))
            measure_config.max_width = amal::max(size().x - margin.x - margin.z - padding.x - padding.z, 0.0f);

        const bool is_ok = multiline()
                               ? detail::layout_multiline(*font, font_size_px, _text, measure_config, _layout_result)
                               : detail::layout_single_line(*font, font_size_px, _text, measure_config, _layout_result);
        if (!is_ok)
        {
            _layout_result.clear();
            const amal::vec2 content_size = {
                amal::max(is_width_fixed() && !fill_width() ? style_size().x : 0.0f, min_width),
                amal::max(is_height_fixed() && !fill_height() ? style_size().y : 0.0f, min_height)};
            set_required_size({content_size.x + padding.x + padding.z + margin.x + margin.z,
                               content_size.y + padding.y + padding.w + margin.y + margin.w});
            return;
        }

        amal::vec2 min_size = _layout_result.size;
        min_size.y = resolve_layout_height_for_widget(*this, _layout_result);
        if (is_width_fixed() && !fill_width()) min_size.x = style_size().x;
        else if (is_size_concrete(size().x) && size().x > 0.0f && multiline())
            min_size.x = amal::max(min_size.x, size().x);

        if (is_height_fixed() && !fill_height()) min_size.y = style_size().y;
        min_size.x = amal::max(min_size.x, min_width);
        min_size.y = amal::max(min_size.y, min_height);

        set_required_size({min_size.x + padding.x + padding.z + margin.x + margin.z,
                           min_size.y + padding.y + padding.w + margin.y + margin.w});
    }

    void Text::update_layout(bool min_size_known)
    {
        assert(_style.id != Theme::STYLE_ID_INVALID && "Text style must be resolved before layout update");
        const bool measure_required = layout_measure_required(min_size_known);
        if (measure_required) update_layout_min_size_force();

        const auto &style = get_theme()->get_style(_style.id);
        const amal::vec4 margin = text_style_margin(*this, style);
        const amal::vec4 padding = text_style_padding(*this, style);
        const amal::vec2 measured_bounds = size();
        const bool measure_matches_bounds = layout_measure_was_updated() &&
                                            width_mode() == TextLayoutWidthMode::bounds &&
                                            _layout_config.max_width <= 0.0f && is_size_concrete(measured_bounds.x) &&
                                            measured_bounds.x > 0.0f && (is_width_fixed() || multiline());
        const f32 min_width = text_style_min_width(*this, style);
        const f32 min_height = text_style_min_height(*this, style);
        const amal::vec2 required_outer = required_size();
        const amal::vec2 required_inner = {amal::max(required_outer.x - margin.x - margin.z, 0.0f),
                                           amal::max(required_outer.y - margin.y - margin.w, 0.0f)};
        const bool auto_width = !is_size_concrete(size().x) || size().x <= 0.0f;
        const bool auto_height = !is_size_concrete(size().y) || size().y <= 0.0f;
        const amal::vec2 layout_origin = position();
        const amal::vec2 outer_pos = {layout_origin.x + margin.x, layout_origin.y + margin.y};
        set_position(outer_pos);

        amal::vec2 outer_size = resolve_text_size(*this, required_inner, margin);
        set_layout_size(outer_size);
        const bool width_changed = layout_width_dirty();
        assert(parent() && "Text must have parent");
        set_clip_id(parent()->content_clip_id());
        const amal::vec2 inner_size = {amal::max(outer_size.x - padding.x - padding.z, 0.0f),
                                       amal::max(outer_size.y - padding.y - padding.w, 0.0f)};
        set_position(outer_pos + amal::vec2{padding.x, padding.y});
        const auto width_mode = text_width_mode(_layout_config.flags);
        f32 effective_max_width = _layout_config.max_width;
        if (width_mode == TextLayoutWidthMode::bounds && effective_max_width <= 0.0f)
            effective_max_width = inner_size.x;
        const bool single_line_measure_fits = !multiline() && !_layout_result.truncated && effective_max_width > 0.0f &&
                                              _layout_result.size.x <= effective_max_width;
        const bool reuse_layout = !width_changed || width_mode != TextLayoutWidthMode::bounds ||
                                  _layout_config.max_width > 0.0f || measure_matches_bounds || single_line_measure_fits;
        const bool buffers_built = rebuild_text_buffers(inner_size, reuse_layout);
        update_content_bounds();
        set_position(outer_pos);

        if (!is_width_fixed() || !is_height_fixed())
        {
            const f32 resolved_height = amal::max(resolve_layout_height_for_widget(*this, _layout_result), min_height);
            const f32 resolved_width = auto_width ? amal::max(_layout_result.size.x, min_width)
                                                  : amal::max(_layout_result.size.x, inner_size.x);
            if (auto_width) outer_size.x = resolved_width + padding.x + padding.z;
            if (auto_height) outer_size.y = resolved_height + padding.y + padding.w;
            set_layout_size(outer_size);
        }
        Widget::update_layout(true);
        if (!buffers_built) invalidate_layout_arrange();
    }

    void Text::translate(const amal::vec2 &delta)
    {
        if (delta.x == 0.0f && delta.y == 0.0f) return;
        Widget::translate(delta);
        for (auto &instance : _instances) instance.rect.offset += delta;
        _content_bounds.offset += delta;
        if (!_instances.empty()) _instances_gpu_dirty = true;
    }

    void Text::update_content_bounds()
    {
        if (_instances.empty())
        {
            _content_bounds = {position(), {0.0f, 0.0f}};
            return;
        }

        amal::vec2 min_pos = _instances[0].rect.offset;
        amal::vec2 max_pos = _instances[0].rect.offset + _instances[0].rect.size;
        for (size_t i = 1; i < _instances.size(); ++i)
        {
            const auto &rect = _instances[i].rect;
            min_pos.x = amal::min(min_pos.x, rect.offset.x);
            min_pos.y = amal::min(min_pos.y, rect.offset.y);
            max_pos.x = amal::max(max_pos.x, rect.offset.x + rect.size.x);
            max_pos.y = amal::max(max_pos.y, rect.offset.y + rect.size.y);
        }

        _content_bounds = {min_pos, {amal::max(max_pos.x - min_pos.x, 0.0f), amal::max(max_pos.y - min_pos.y, 0.0f)}};
    }

    void Text::rebuild_clip_rects()
    {
        // Text uses parent's clip to avoid stale per-text clip rects on parent translate.
        assert(parent() && "Text must have parent");
        set_clip_id(parent()->content_clip_id());
    }

    void Text::update_depth(const amal::vec2 &depth_range)
    {
        const f32 prev_z = get_z_order();
        Widget::update_depth(depth_range);
        if (prev_z != get_z_order()) _instances_gpu_dirty = true;
    }

    void Text::reset_draw_records()
    {
        _draw_ids.clear();
        _hit_id = AUIK_INVALID_DRAW_DATA_ID;
        _applied_clip_id = 0xFFFFu;
        _applied_post_fx_chain = nullptr;
        _instances_gpu_dirty = true;
    }

    void Text::invalidate_draw_records()
    {
        if (!detail::g_context || !detail::get_context().streams.default_streams) return;
        auto *stream = get_primary_textured_quads_stream();
        if (!stream || !stream->runtime_data) return;
        invalidate_text_draw_ids(stream, _draw_ids, 0);
        reset_draw_records();
    }

    void Text::draw(DrawCtx &ctx)
    {
        const u16 current_clip = clip_id();
        if (current_clip == 0xFFFFu)
        {
            assert(_instances.empty() && "Text draw requires a valid clip rect");
            if (!_instances.empty()) return;
        }

        if (ctx.is_hit_allowed && is_hittable())
        {
            detail::RectData hit_rect = get_rect();
            hit_rect.bounds = _content_bounds;
            if ((ctx.reason & DrawReasonBits::record)) _hit_id = AUIK_INVALID_DRAW_DATA_ID;
            const bool force_update = (ctx.reason & DrawReasonBits::record) ||
                                      (detail::get_context().dirty_flags & DirtyFlagBits::hit_rect_update);
            update_hit_rect(_hit_id, hit_rect, force_update);
        }
        else if (_hit_id != AUIK_INVALID_DRAW_DATA_ID &&
                 ((ctx.reason & DrawReasonBits::invalidate) ||
                  (detail::get_context().dirty_flags & DirtyFlagBits::hit_rect_update)))
        {
            detail::RectData hit_rect = get_rect();
            hit_rect.bounds = _content_bounds;
            hit_rect.bounds.size = {0.0f, 0.0f};
            update_hit_rect(_hit_id, hit_rect, true);
        }

        auto *textured_quads_stream = get_primary_textured_quads_stream();
        if (!textured_quads_stream)
        {
            _draw_ids.clear();
            _instances_gpu_dirty = true;
            return;
        }

        if ((ctx.reason & DrawReasonBits::invalidate))
        {
            invalidate_text_draw_ids(textured_quads_stream, _draw_ids, 0);
            reset_draw_records();
            return;
        }

        if (_instances.empty())
        {
            if (!(ctx.reason & DrawReasonBits::record))
            {
                if (should_record_text_shrink(_draw_ids.size(), 0))
                {
                    redraw_all_commands();
                    return;
                }
                invalidate_text_draw_ids(textured_quads_stream, _draw_ids, 0);
            }
            _draw_ids.clear();
            _applied_clip_id = current_clip;
            _applied_post_fx_chain = ctx.post_fx_chain;
            _instances_gpu_dirty = false;
            return;
        }

        const f32 current_z = get_z_order();
        const bool draw_state_changed =
            _applied_clip_id != current_clip || _applied_post_fx_chain != ctx.post_fx_chain || ctx.post_fx_chain;
        const bool force_instance_update = ctx.reason & DrawReasonBits::full_redraw;
        const bool instances_changed = _instances_gpu_dirty || force_instance_update;
        if (draw_state_changed || instances_changed)
        {
            for (auto &instance : _instances) instance.clip_id = current_clip;
        }
        if (draw_state_changed || instances_changed)
        {
            const u32 tint_color = get_theme()->get_style(_style.id).text_color();
            for (auto &instance : _instances)
            {
                instance.z_order = current_z;
                instance.tint_color = tint_color;
            }
        }

        if ((ctx.reason & DrawReasonBits::record))
        {
            _draw_ids.resize(_instances.size());
            emit_context_draw_batch(ctx, textured_quads_stream, _draw_ids.data(), _instances.data(),
                                    static_cast<u32>(_instances.size()));
            _applied_clip_id = current_clip;
            _applied_post_fx_chain = ctx.post_fx_chain;
            _instances_gpu_dirty = false;
            return;
        }

        if (!draw_state_changed && !instances_changed) return;

        const size_t old_count = _draw_ids.size();
        const size_t new_count = _instances.size();
        if (should_record_text_shrink(old_count, new_count))
        {
            redraw_all_commands();
            return;
        }

        const size_t update_count = amal::min(old_count, new_count);
        if (update_count > 0)
            emit_context_draw_batch(ctx, textured_quads_stream, _draw_ids.data(), _instances.data(),
                                    static_cast<u32>(update_count));

        if (new_count < old_count)
        {
            invalidate_text_draw_ids(textured_quads_stream, _draw_ids, new_count);
            _draw_ids.resize(new_count);
        }

        if (new_count > old_count)
        {
            const size_t append_count = new_count - old_count;
            _draw_ids.resize(new_count);
            emit_context_draw_batch(ctx, textured_quads_stream, _draw_ids.data() + old_count,
                                    _instances.data() + old_count, static_cast<u32>(append_count));
        }

        _applied_clip_id = current_clip;
        _applied_post_fx_chain = ctx.post_fx_chain;
        _instances_gpu_dirty = false;
    }

    bool Text::rebuild_text_buffers(const amal::vec2 &bounds_size, bool reuse_layout)
    {
        _instances.clear();
        _instances_gpu_dirty = true;

        const auto &style = get_theme()->get_style(_style.id);
        auto *font = text_style_font(*this, style);
        const u32 font_size_px = round_font_px(text_style_size(*this, style));
        const u32 tint_color = text_style_color(*this, style);
        const bool allow_empty_layout = multiline();
        if (!font || (!allow_empty_layout && _text.empty()) || font_size_px == 0) return false;

        auto render_config = _render_config;
        render_config.size = bounds_size;
        render_config.z_order = get_z_order();
        render_config.clip_id = clip_id();

        if (reuse_layout)
        {
            render_config.origin =
                resolve_text_widget_render_origin(*this, style, &_layout_result, &render_config.size);
            return detail::build_text_instances_from_layout(*font, font_size_px, _layout_result, render_config,
                                                            tint_color, _instances);
        }

        render_config.origin = resolve_text_widget_render_origin(*this, style);
        const bool built =
            multiline() ? detail::build_multiline_instances(*font, font_size_px, _text, _layout_config, render_config,
                                                            tint_color, _instances, &_layout_result)
                        : detail::build_single_line_instances(*font, font_size_px, _text, _layout_config, render_config,
                                                              tint_color, _instances, &_layout_result);
        return built;
    }

    static EventFlags resolve_etext_event_flags()
    {
        return EventFlagBits::hover | EventFlagBits::click | EventFlagBits::drag | EventFlagBits::focus |
               EventFlagBits::key_input | EventFlagBits::char_input;
    }

    static u64 etext_tooltip_task_owner(u32 id)
    {
        return (static_cast<u64>(AUIK_TAG_TEXT) << 32u) | static_cast<u64>(id);
    }

    static detail::TextEditKey map_etext_key(Key key, KeyMode mods, bool multiline, bool read_only)
    {
        detail::TextEditKey out = 0;
        if (read_only && (key == Key::backspace || key == Key::del)) return out;
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

    static int etext_edit_string_len(void *user_data)
    {
        auto *self = static_cast<EText *>(user_data);
        return self ? static_cast<int>(self->text().size()) : 0;
    }

    static detail::TextEditChar etext_edit_get_char(void *user_data, int char_idx)
    {
        auto *self = static_cast<EText *>(user_data);
        if (!self || char_idx < 0 || char_idx >= static_cast<int>(self->text().size())) return 0;
        return static_cast<unsigned char>(self->text()[char_idx]);
    }

    static bool etext_edit_replace_chars(void *user_data, int pos, int delete_count, const detail::TextEditChar *text,
                                         int text_len)
    {
        auto *self = static_cast<EText *>(user_data);
        if (!self || self->is_read_only()) return false;
        const acul::string &value = self->text();
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
        self->set_text(out);
        self->mark_changed();
        return true;
    }

    static bool etext_edit_insert_chars(void *user_data, int pos, const detail::TextEditChar *text, int text_len)
    {
        return etext_edit_replace_chars(user_data, pos, 0, text, text_len);
    }

    static void etext_edit_delete_chars(void *user_data, int pos, int count)
    {
        etext_edit_replace_chars(user_data, pos, count, nullptr, 0);
    }

    static int etext_edit_next_char_index(void *user_data, int idx)
    {
        auto *self = static_cast<EText *>(user_data);
        return self ? detail::next_utf8_index(self->text(), idx) : idx + 1;
    }

    static int etext_edit_prev_char_index(void *user_data, int idx)
    {
        auto *self = static_cast<EText *>(user_data);
        return self ? detail::prev_utf8_index(self->text(), idx) : idx - 1;
    }

    struct ETextEditData
    {
        f64 caret_anim_reset_time = 0.0;
        u8 flags = 0;
    };

    EText::EText(u32 id, acul::string text, amal::vec2 max_size, WidgetFlags flags, TextLayoutFlags layout_flags)
        : Text(id, std::move(text), max_size, flags, layout_flags),
          _edit((flags & WidgetFlagBits::hittable) ? acul::alloc<ETextEditData>() : nullptr)
    {
        set_rect_tag_id(AUIK_TAG_ETEXT);
        if (_edit)
        {
            set_post_data(_edit);
            requested_event_flags = resolve_etext_event_flags();
            event_flags = is_disabled() ? EventFlagBits::none : requested_event_flags;
            detail::text_edit_initialize_state(&_edit_state, text_wrap_mode(layout_flags) != TextWrapMode::word);
            register_shortcut(this, Shortcut{.mods = KeyModeBits::control, .keys = {Key::c}},
                              [this]() { copy_selection_to_clipboard(); });
            register_shortcut(this, Shortcut{.mods = KeyModeBits::control, .keys = {Key::a}},
                              [this]() { select_all_text(); });
        }
    }

    EText::EText(u32 id, StringView text, amal::vec2 max_size, WidgetFlags flags, TextLayoutFlags layout_flags)
        : EText(id, acul::string(text.str ? text.str : ""), max_size, flags, layout_flags)
    {
        if (text.is_translated) set_translated_text_literal(text.str);
    }

    EText::~EText()
    {
        cancel_delayed_tasks(id());
        cancel_delayed_tasks(etext_tooltip_task_owner(id()));
        clear_tooltip_if_source(&_text);
        acul::release(_edit);
    }

    void EText::on_disabled_changed(bool disabled)
    {
        if (!disabled || !_edit) return;
        cancel_delayed_tasks(id());
        _edit->flags &= ~AUIK_ETEXT_CARET_TASK_BIT;
        erase_widget_from_transient_cache(this);
    }

    void EText::sync_widget_flags()
    {
        Widget::sync_widget_flags(is_disabled() ? EventFlagBits::none : requested_event_flags);
    }

    StyleUpdateFlags EText::update_style()
    {
        const u32 parent_id = parent() ? parent()->id() : 0u;
        auto flags = Text::update_style();
        if (_edit)
        {
            flags |= resolve_style_selector(_caret_style, id(), parent_id, StyleState::normal);
            flags |= resolve_style_selector(_selection_style, id(), parent_id, StyleState::normal);
        }
        return flags;
    }

    void EText::update_layout_min_size_force()
    {
        Text::update_layout_min_size_force();

        const auto &style = get_theme()->get_style(_style.id);
        const amal::vec4 margin = text_style_margin(*this, style);
        const amal::vec4 padding = text_style_padding(*this, style);
        const f32 chrome_w = margin.x + margin.z + padding.x + padding.z;
        const f32 chrome_h = margin.y + margin.w + padding.y + padding.w;
        const f32 natural_w = _layout_result.size.x;
        const f32 natural_h = resolve_layout_height_for_widget(*this, _layout_result);

        f32 required_w = 0.0f;
        if (is_size_fit(style_size().x)) required_w = natural_w;
        else if (is_size_fill(style_size().x)) required_w = 0.0f;
        else if (is_size_concrete(style_size().x) && style_size().x > 0.0f) required_w = style_size().x;

        f32 required_h = natural_h;
        if (is_size_fill(style_size().y)) required_h = 0.0f;
        else if (is_size_concrete(style_size().y) && style_size().y > 0.0f) required_h = style_size().y;

        required_w = amal::max(required_w, text_style_min_width(*this, style));
        required_h = amal::max(required_h, text_style_min_height(*this, style));
        set_required_size({required_w + chrome_w, required_h + chrome_h});
    }

    void EText::translate(const amal::vec2 &delta)
    {
        Text::translate(delta);
        for (auto &rect : _selection_rects) rect.offset += delta;
    }

    void EText::reset_draw_records()
    {
        Text::reset_draw_records();
        _caret = {};
        _selection_draw_ids.clear();
    }

    bool EText::should_show_lazy_tooltip() const
    {
        return overflow_mode() == TextOverflowMode::ellipsis && _layout_result.truncated && !_text.empty();
    }

    void EText::schedule_lazy_tooltip()
    {
        if (!should_show_lazy_tooltip()) return;
        const u32 wid = id();
        const f32 anchor_x = get_mouse_pos().x;
        detail::update_window_time(detail::get_context().window_ctx);
        const f64 due_time = detail::get_context().window_ctx->time + AUIK_TOOLTIP_SHOW_DELAY;
        schedule_delayed_host_task(etext_tooltip_task_owner(wid), due_time, [this, wid, anchor_x]() {
            if (detail::get_context().hover_id.widget_id != wid) return;
            if (!should_show_lazy_tooltip()) return;
            show_tooltip(anchor_x, &_text);
        });
    }

    void EText::clear_lazy_tooltip(bool clear_source)
    {
        cancel_delayed_tasks(etext_tooltip_task_owner(id()));
        add_render_command<detail::HoverEventTraits>(this, [this, clear_source]() {
            hide_tooltip();
            if (clear_source) clear_tooltip_if_source(&_text);
        });
    }

    void EText::on_hover(HoverState state)
    {
        detail::set_window_cursor(state == HoverState::leave ? detail::CursorID::arrow : detail::CursorID::ibeam,
                                  detail::get_context().window_ctx);
        if (state == HoverState::leave)
        {
            clear_lazy_tooltip(true);
            return;
        }
        if (state == HoverState::enter) schedule_lazy_tooltip();
    }

    void EText::on_focus(bool focused)
    {
        if (!focused)
        {
            for (auto &cursor : _edit_state.cursors)
            {
                cursor.select_start = cursor.cursor;
                cursor.select_end = cursor.cursor;
            }
        }

        add_render_command<detail::FocusEventTraits>(this, [this, focused]() {
            if (focused) push_widget_to_transient_cache(this);
            else erase_widget_from_transient_cache(this);
            reset_caret_blink();
            if (focused) schedule_caret_blink();
            rebuild_selection_rect_cache();
            update_draw_commands(DrawReasonBits::transient);
            detail::get_context().dirty_flags |= DirtyFlagBits::redraw;
        });
    }

    void EText::on_click(MouseKey key, KeyPressState state, u32 click_count)
    {
        if (key != MouseKey::left || state != KeyPressState::press) return;
        set_style_state(StyleState::focus);
        if (click_count >= 3) select_all_text();
        else if (click_count == 2) select_word_at_point(get_mouse_pos());
        else collapse_cursor_at_point(get_mouse_pos());
        add_render_command<detail::ClickEventTraits>(this, [this]() { apply_edit_render_update(false); });
    }

    void EText::on_drag(const amal::vec2 &, KeyPressState state)
    {
        if (state == KeyPressState::press)
        {
            _drag_anchor = cursor_from_point(get_mouse_pos());
            _dragging_selection = true;
            return;
        }
        if (!_dragging_selection) return;
        if (state == KeyPressState::release) _dragging_selection = false;

        select_text_range(_drag_anchor, cursor_from_point(get_mouse_pos()));
        add_render_command<detail::DragEventTraits>(this, [this]() { apply_edit_render_update(false); });
    }

    void EText::on_key(Key key, KeyPressState state, KeyMode mods)
    {
        if (state == KeyPressState::release) return;
        if ((key == Key::enter || key == Key::kp_enter) && !is_read_only() && multiline())
        {
            detail::TextEditChar newline = '\n';
            auto edit_string = make_text_edit_string();
            detail::text_edit_text(&edit_string, &_edit_state, &newline, 1);
            reset_caret_blink();
            add_render_command<detail::KeyEventTraits>(this, [this]() { apply_edit_render_update(true); });
            return;
        }

        const auto edit_key = map_etext_key(key, mods, multiline(), is_read_only());
        if (!edit_key) return;
        const acul::string before_text = text();
        auto edit_string = make_text_edit_string();
        detail::text_edit_key(&edit_string, &_edit_state, edit_key);
        reset_caret_blink();
        add_render_command<detail::KeyEventTraits>(
            this, [this, layout_dirty = before_text != text()]() { apply_edit_render_update(layout_dirty); });
    }

    void EText::on_char_input(u32 char_code, u32 count)
    {
        if (is_read_only() || count == 0) return;
        if (char_code < 32 && char_code != '\t') return;
        const acul::string encoded = detail::encode_utf8_codepoint(char_code, text_flags);
        if (encoded.empty()) return;

        count = amal::min(count, 1024u);
        acul::vector<detail::TextEditChar> chars;
        chars.reserve(encoded.size() * static_cast<size_t>(count));
        for (u32 repeat = 0; repeat < count; ++repeat)
            for (size_t i = 0; i < encoded.size(); ++i) chars.push_back(static_cast<unsigned char>(encoded[i]));

        auto edit_string = make_text_edit_string();
        detail::text_edit_text(&edit_string, &_edit_state, chars.data(), static_cast<int>(chars.size()));
        reset_caret_blink();
        add_render_command<detail::CharEventTraits>(this, [this]() { apply_edit_render_update(true); });
    }

    void EText::draw(DrawCtx &ctx)
    {
        const bool transient = ctx.reason & DrawReasonBits::transient;
        if (!_edit)
        {
            if (!transient) Text::draw(ctx);
            return;
        }

        if ((ctx.reason & DrawReasonBits::invalidate))
        {
            Text::draw(ctx);
            _selection_draw_ids.clear();
            _caret = {};
            return;
        }

        if (!transient)
        {
            auto *selection_stream = get_primary_quads_stream();
            const f32 selection_z = (depth_range().x + get_z_order()) * 0.5f;
            if (selection_stream)
            {
                const auto &selection_style = get_theme()->get_style(_selection_style.id);
                const f32 selection_h =
                    amal::max(_layout_result.line_height, get_theme()->get_style(_style.id).text_size());
                const u32 selection_slots =
                    amal::max(static_cast<u32>(_selection_rects.size()), static_cast<u32>(_selection_draw_ids.size()));
                if (_selection_draw_ids.size() < selection_slots) _selection_draw_ids.resize(selection_slots);

                u32 selection_slot = 0;
                auto emit_selection = [&](const amal::rect &rect, bool visible) {
                    if (selection_slot >= _selection_draw_ids.size()) return;
                    QuadsInstanceData selection{};
                    selection.rect = visible ? rect : amal::rect{position(), {0.0f, selection_h}};
                    selection.z_order = selection_z;
                    selection.background_color = visible ? selection_style.background_color() : 0;
                    selection.mask = clip_id();
                    emit_context_draw(ctx, selection_stream, _selection_draw_ids[selection_slot], &selection,
                                      detail::make_rect_data(id(), AUIK_STYLE_TAG_SELECTION, selection.rect), false);
                    ++selection_slot;
                };

                for (const auto &rect : _selection_rects)
                    emit_selection(rect, rect.size.x > 0.0f && rect.size.y > 0.0f);
                while (selection_slot < _selection_draw_ids.size())
                    emit_selection({position(), {0.0f, selection_h}}, false);
            }

            Text::draw(ctx);
        }

        auto *quads_stream = get_overlay_quads_stream();
        if (!quads_stream) return;
        const bool active = detail::get_context().focus_id == id();
        bool blink_on = active;
        if (active)
        {
            detail::update_window_time(detail::get_context().window_ctx);
            const f64 elapsed = detail::get_context().window_ctx->time - _edit->caret_anim_reset_time;
            blink_on = std::fmod(elapsed, AUIK_ETEXT_CARET_PERIOD) < AUIK_ETEXT_CARET_ON_TIME;
        }

        const auto &caret_style = get_theme()->get_style(_caret_style.id);
        const auto caret_padding = caret_style.padding();
        const bool visible = blink_on && !_edit_state.cursors.empty() && !_edit_state.has_selection();
        const f32 caret_w = amal::max(amal::round(caret_padding.x), 1.0f);
        QuadsInstanceData caret{};
        if (visible)
        {
            const int cursor = _edit_state.primary_cursor().cursor;
            const u32 line_index = line_index_from_cursor(cursor);
            caret.rect = caret_rect(line_index, cursor_x_on_line(line_index, cursor), caret_w);
        }
        else caret.rect = {position(), {caret_w, 0.0f}};
        caret.z_order = next_depth(depth_range());
        caret.background_color = visible ? caret_style.background_color() : 0;
        caret.mask = clip_id();
        emit_context_draw(ctx, quads_stream, _caret, &caret,
                          detail::make_rect_data(id(), _caret_style.tag_id, caret.rect), false);
    }

    void EText::reset_caret_blink()
    {
        if (!_edit) return;
        detail::update_window_time(detail::get_context().window_ctx);
        _edit->caret_anim_reset_time = detail::get_context().window_ctx->time;
        _edit->flags &= ~AUIK_ETEXT_CARET_TASK_BIT;
    }

    void EText::schedule_caret_blink()
    {
        if (!_edit) return;
        if (detail::get_context().focus_id != id()) return;
        if (_edit->flags & AUIK_ETEXT_CARET_TASK_BIT) return;

        detail::update_window_time(detail::get_context().window_ctx);
        const f64 now = detail::get_context().window_ctx->time;
        const f64 elapsed = now - _edit->caret_anim_reset_time;
        const f64 phase = std::fmod(elapsed, AUIK_ETEXT_CARET_PERIOD);
        const f64 remaining =
            phase < AUIK_ETEXT_CARET_ON_TIME ? (AUIK_ETEXT_CARET_ON_TIME - phase) : (AUIK_ETEXT_CARET_PERIOD - phase);
        _edit->flags |= AUIK_ETEXT_CARET_TASK_BIT;
        schedule_delayed_host_task(id(), now + (remaining > 0.001 ? remaining : 0.001),
                                   [this]() { update_transient_draw_commands(); });
    }

    void EText::update_transient_draw_commands()
    {
        if (!_edit) return;
        _edit->flags &= ~AUIK_ETEXT_CARET_TASK_BIT;
        if (detail::get_context().focus_id != id()) return;
        update_draw_commands(DrawReasonBits::transient);
        detail::get_context().dirty_flags |= DirtyFlagBits::redraw;
        schedule_caret_blink();
    }

    void EText::apply_edit_render_update(bool layout_dirty)
    {
        if (layout_dirty)
        {
            update_layout(false);
            rebuild_selection_rect_cache();
            update_draw_commands(DrawReasonBits::layout);
        }
        else
        {
            rebuild_selection_rect_cache();
            if (edit_draw_slots_need_record()) redraw_all_commands();
            else update_draw_commands(DrawReasonBits::transient);
        }
        detail::get_context().dirty_flags |= DirtyFlagBits::redraw | DirtyFlagBits::hit_rect_update;
        schedule_caret_blink();
    }

    bool EText::edit_draw_slots_need_record() const
    {
        if (_selection_draw_ids.size() < _selection_rects.size()) return true;
        if (!is_read_only() && _caret.render_id == AUIK_INVALID_DRAW_DATA_ID) return true;
        return false;
    }

    int EText::cursor_from_point(const amal::vec2 &point) const
    {
        if (_layout_result.lines.empty())
            return point.x < position().x + size().x * 0.5f ? 0 : static_cast<int>(text().size());

        const f32 local_x = point.x - position().x;
        const f32 local_y = point.y - position().y;
        u32 line_index = 0;
        f32 best_dist = 3.4e38f;
        for (u32 i = 0; i < _layout_result.lines.size(); ++i)
        {
            const auto &line = _layout_result.lines[i];
            f32 line_min_y = line.glyph_count > 0 ? 3.4e38f : static_cast<f32>(i) * _layout_result.line_height;
            f32 line_max_y = line.glyph_count > 0 ? -3.4e38f : line_min_y + _layout_result.line_height;
            for (u32 glyph_i = 0; glyph_i < line.glyph_count; ++glyph_i)
            {
                const auto &glyph = _layout_result.glyphs[line.glyph_offset + glyph_i];
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

    int EText::cursor_from_line_x(u32 line_index, f32 x) const
    {
        if (_layout_result.lines.empty()) return x < size().x * 0.5f ? 0 : static_cast<int>(text().size());
        if (line_index >= _layout_result.lines.size()) line_index = static_cast<u32>(_layout_result.lines.size() - 1);

        const auto &line = _layout_result.lines[line_index];
        if (x <= 0.0f) return static_cast<int>(line.text_start);
        if (x >= line.width) return static_cast<int>(line.text_end);

        int best = static_cast<int>(line.text_start);
        f32 best_dist = 3.4e38f;
        for (u32 i = 0; i < line.glyph_count; ++i)
        {
            const auto &glyph = _layout_result.glyphs[line.glyph_offset + i];
            const int glyph_start = static_cast<int>(glyph.cluster);
            const int glyph_end = detail::next_utf8_index(text(), glyph_start);
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

    f32 EText::cursor_x_on_line(u32 line_index, int cursor) const
    {
        if (_layout_result.lines.empty()) return 0.0f;
        if (line_index >= _layout_result.lines.size()) line_index = static_cast<u32>(_layout_result.lines.size() - 1);

        const auto &line = _layout_result.lines[line_index];
        if (cursor <= static_cast<int>(line.text_start)) return 0.0f;
        if (cursor >= static_cast<int>(line.text_end)) return line.width;

        f32 x = 0.0f;
        for (u32 i = 0; i < line.glyph_count; ++i)
        {
            const auto &glyph = _layout_result.glyphs[line.glyph_offset + i];
            if (static_cast<int>(glyph.cluster) >= cursor) break;
            x = glyph.pen.x + glyph.advance.x;
        }
        return x;
    }

    u32 EText::line_index_from_cursor(int cursor) const
    {
        if (_layout_result.lines.empty()) return 0;

        u32 best = 0;
        for (u32 i = 0; i < _layout_result.lines.size(); ++i)
        {
            const auto &line = _layout_result.lines[i];
            if (cursor < static_cast<int>(line.text_start)) return best;
            best = i;
            if (cursor <= static_cast<int>(line.text_end)) return i;
        }
        return best;
    }

    amal::vec2 EText::cursor_screen_pos(int cursor) const
    {
        const u32 line_index = line_index_from_cursor(cursor);
        const f32 y =
            line_index < _layout_result.lines.size() && _layout_result.lines[line_index].glyph_count > 0
                ? _layout_result.glyphs[_layout_result.lines[line_index].glyph_offset].pen.y - _layout_result.ascender
                : static_cast<f32>(line_index) * _layout_result.line_height;
        return {position().x + cursor_x_on_line(line_index, cursor), position().y + y};
    }

    amal::rect EText::caret_rect(u32 line_index, f32 x, f32 width) const
    {
        const auto &style = get_theme()->get_style(_style.id);
        const f32 resolved_text_size = text_style_size(*this, style);
        const f32 line_h = amal::max(_layout_result.line_height, resolved_text_size);
        const f32 caret_h = amal::min(resolved_text_size, amal::max(size().y, resolved_text_size));
        f32 line_y = static_cast<f32>(line_index) * line_h + amal::max((line_h - caret_h) * 0.5f, 0.0f);
        if (!multiline()) line_y = amal::max((size().y - caret_h) * 0.5f, 0.0f);
        return {{amal::round(position().x + x), amal::round(position().y + line_y)}, {width, caret_h}};
    }

    amal::rect EText::line_selection_rect(u32 line_index, f32 x0, f32 x1) const
    {
        const auto &style = get_theme()->get_style(_style.id);
        const f32 line_h = amal::max(_layout_result.line_height, text_style_size(*this, style));
        f32 line_y = static_cast<f32>(line_index) * line_h;
        if (line_index < _layout_result.lines.size() && _layout_result.lines[line_index].glyph_count > 0)
            line_y =
                _layout_result.glyphs[_layout_result.lines[line_index].glyph_offset].pen.y - _layout_result.ascender;
        if (!multiline()) line_y = amal::max((size().y - line_h) * 0.5f, 0.0f);
        return {{position().x + x0, position().y + line_y}, {amal::max(x1 - x0, 0.0f), line_h}};
    }

    void EText::collapse_cursor_at_point(const amal::vec2 &point)
    {
        auto &cursor = _edit_state.primary_cursor();
        cursor.cursor = cursor_from_point(point);
        cursor.select_start = cursor.cursor;
        cursor.select_end = cursor.cursor;
        cursor.has_preferred_x = 0;
        cursor.cursor_at_end_of_line = 0;
        _edit_state.cursors.resize(1);
        reset_caret_blink();
    }

    void EText::select_text_range(int start, int end)
    {
        const int len = static_cast<int>(text().size());
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
    }

    void EText::select_word_at_point(const amal::vec2 &point)
    {
        const int pos = cursor_from_point(point);
        const auto &value = text();
        if (value.empty())
        {
            collapse_cursor_at_point(point);
            return;
        }

        int start = amal::clamp(pos, 0, static_cast<int>(value.size()));
        if (start == static_cast<int>(value.size()) && start > 0) start = detail::prev_utf8_index(value, start);
        const auto is_word_space = [&](int index) {
            return index >= 0 && index < static_cast<int>(value.size()) &&
                   std::isspace(static_cast<unsigned char>(value[index]));
        };
        const bool space_word = is_word_space(start);
        int end = space_word ? detail::next_utf8_index(value, start) : start;
        while (start > 0 && is_word_space(detail::prev_utf8_index(value, start)) == space_word)
            start = detail::prev_utf8_index(value, start);
        while (end < static_cast<int>(value.size()) && is_word_space(end) == space_word)
            end = detail::next_utf8_index(value, end);
        select_text_range(start, end);
    }

    void EText::select_all_text()
    {
        if (text().empty()) return;
        select_text_range(0, static_cast<int>(text().size()));
        add_render_command<detail::KeyEventTraits>(this, [this]() { apply_edit_render_update(false); });
    }

    void EText::copy_selection_to_clipboard()
    {
        if (_edit_state.cursors.empty()) return;
        const auto &cursor = _edit_state.primary_cursor();
        if (!cursor.has_selection()) return;
        int start = cursor.select_start < cursor.select_end ? cursor.select_start : cursor.select_end;
        int end = cursor.select_start < cursor.select_end ? cursor.select_end : cursor.select_start;
        start = amal::clamp(start, 0, static_cast<int>(text().size()));
        end = amal::clamp(end, 0, static_cast<int>(text().size()));
        if (end <= start) return;
        detail::set_clipboard_string(detail::get_context().window_ctx,
                                     text().substr(static_cast<size_t>(start), static_cast<size_t>(end - start)));
    }

    void EText::delete_selection()
    {
        if (is_read_only() || _edit_state.cursors.empty()) return;
        auto &cursor = _edit_state.primary_cursor();
        if (!cursor.has_selection()) return;
        int start = cursor.select_start < cursor.select_end ? cursor.select_start : cursor.select_end;
        int end = cursor.select_start < cursor.select_end ? cursor.select_end : cursor.select_start;
        acul::string value = text();
        start = amal::clamp(start, 0, static_cast<int>(value.size()));
        end = amal::clamp(end, 0, static_cast<int>(value.size()));
        if (end <= start) return;
        value.erase(static_cast<size_t>(start), static_cast<size_t>(end - start));
        set_text(value);
        cursor.cursor = start;
        cursor.select_start = start;
        cursor.select_end = start;
        mark_changed();
    }

    void EText::rebuild_selection_rect_cache()
    {
        _selection_rects.clear();
        if (_edit_state.cursors.empty()) return;
        const auto &cursor = _edit_state.primary_cursor();
        if (!cursor.has_selection()) return;

        const int start = cursor.select_start < cursor.select_end ? cursor.select_start : cursor.select_end;
        const int end = cursor.select_start < cursor.select_end ? cursor.select_end : cursor.select_start;
        for (u32 line_i = 0; line_i < _layout_result.lines.size(); ++line_i)
        {
            const auto &line = _layout_result.lines[line_i];
            const int line_start = static_cast<int>(line.text_start);
            const int line_end = static_cast<int>(line.text_end);
            if (end < line_start || start > line_end) continue;
            const int segment_start = amal::max(start, line_start);
            const int segment_end = amal::min(end, line_end);
            if (segment_end < segment_start) continue;
            const f32 x0 = cursor_x_on_line(line_i, segment_start);
            const f32 x1 = cursor_x_on_line(line_i, segment_end);
            if (x1 <= x0) continue;
            _selection_rects.push_back(line_selection_rect(line_i, x0, x1));
        }
    }

    detail::TextEditString EText::make_text_edit_string()
    {
        detail::TextEditString out{};
        out.user_data = this;
        out.string_len = etext_edit_string_len;
        out.get_char = etext_edit_get_char;
        out.insert_chars = etext_edit_insert_chars;
        out.delete_chars = etext_edit_delete_chars;
        out.replace_chars = etext_edit_replace_chars;
        out.next_char_index = etext_edit_next_char_index;
        out.prev_char_index = etext_edit_prev_char_index;
        return out;
    }

    namespace
    {
        void write_text_like(acul::bin_stream &stream, const Text &widget)
        {
            const bool translated = widget.is_translated_text();
            const char *literal = translated ? widget.translated_text_literal() : nullptr;
            detail::write_localized_string(stream, translated ? acul::string(literal ? literal : "") : widget.text(),
                                           translated);
            stream.write(static_cast<u32>(widget.text_flags))
                .write(widget.style_tag())
                .write(static_cast<u32>(widget.layout_flags()))
                .write(widget.max_lines())
                .write(widget.tight_content_height());
        }

        struct TextPayload
        {
            acul::string text;
            bool translated = false;
            u32 text_flags = 0u;
            u32 style_tag = AUIK_STYLE_TAG_NO_PAD;
            u32 layout_flags = static_cast<u32>(default_text_layout_flags());
            u32 max_lines = 0u;
            bool tight_content_height = false;
        };

        TextPayload read_text_payload(acul::bin_stream &stream)
        {
            TextPayload out{};
            const auto text = detail::read_localized_string(stream);
            out.text = std::move(text.text);
            out.translated = text.translated;
            stream.read(out.text_flags)
                .read(out.style_tag)
                .read(out.layout_flags)
                .read(out.max_lines)
                .read(out.tight_content_height);
            return out;
        }

        void apply_text_payload(Text &widget, const TextPayload &payload)
        {
            widget.text_flags = TextFlags(payload.text_flags);
            widget.set_max_lines(payload.max_lines);
            widget.set_tight_content_height(payload.tight_content_height);
        }

        void write_text(acul::bin_stream &stream, umbf::Block *block)
        {
            auto *widget = static_cast<Text *>(block);
            detail::write_widget_common_data(stream, *widget);
            write_text_like(stream, *widget);
        }

        umbf::Block *read_text(acul::bin_stream &stream)
        {
            const auto common = detail::read_widget_common_data(stream);
            const auto payload = read_text_payload(stream);
            auto *widget = acul::alloc<Text>(common.id, payload.text, common.inline_size,
                                             WidgetFlags(common.widget_flags), TextLayoutFlags(payload.layout_flags));
            widget->set_style_tag(payload.style_tag);
            detail::apply_widget_common_data(widget, common);
            if (payload.translated) widget->set_translated_text_literal(payload.text.c_str());
            apply_text_payload(*widget, payload);
            return widget;
        }

        umbf::Block *read_etext(acul::bin_stream &stream)
        {
            const auto common = detail::read_widget_common_data(stream);
            const auto payload = read_text_payload(stream);
            auto *widget = acul::alloc<EText>(common.id, payload.text, common.inline_size,
                                              WidgetFlags(common.widget_flags), TextLayoutFlags(payload.layout_flags));
            widget->set_style_tag(payload.style_tag);
            detail::apply_widget_common_data(widget, common);
            if (payload.translated) widget->set_translated_text_literal(payload.text.c_str());
            apply_text_payload(*widget, payload);
            return widget;
        }
    } // namespace

    namespace streams
    {
        AUIK_EXPORT const umbf::streams::Stream text{read_text, write_text};
        AUIK_EXPORT const umbf::streams::Stream etext{read_etext, write_text};
    } // namespace streams

} // namespace auik
