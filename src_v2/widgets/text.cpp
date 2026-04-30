#include <auik/v2/auik.hpp>
#include <auik/v2/detail/context.hpp>
#include <auik/v2/pipelines.hpp>
#include <auik/v2/widgets/text.hpp>

#define AUIK_TOOLTIP_SHOW_DELAY 0.35

namespace auik::v2
{
    static amal::vec2 resolve_text_size(const Text &widget, const amal::vec2 &measured_size)
    {
        amal::vec2 out = widget.size();
        if (out.x <= 0.0f) out.x = measured_size.x;
        if (out.y <= 0.0f) out.y = measured_size.y;
        return out;
    }

    static f32 resolve_layout_height_for_widget(const Text &widget, const detail::TextLayoutResult &layout)
    {
        if (!widget.tight_content_height()) return layout.size.y;

        bool has_visible_glyph = false;
        f32 min_y = 0.0f;
        f32 max_y = 0.0f;
        for (const auto &glyph : layout.glyphs)
        {
            if (!glyph.visible()) continue;
            const f32 glyph_min = glyph.rect.offset.y;
            const f32 glyph_max = glyph.rect.offset.y + glyph.rect.size.y;
            if (!has_visible_glyph)
            {
                min_y = glyph_min;
                max_y = glyph_max;
                has_visible_glyph = true;
                continue;
            }
            min_y = amal::min(min_y, glyph_min);
            max_y = amal::max(max_y, glyph_max);
        }
        if (has_visible_glyph) return amal::max(max_y - min_y, 0.0f);

        const f32 metrics_height = amal::max(layout.ascender - layout.descender, 0.0f);
        if (layout.lines.size() <= 1 && metrics_height > 0.0f) return metrics_height;
        return layout.size.y;
    }

    StyleUpdateFlags Text::update_style()
    {
        const u32 parent_id = parent() ? parent()->id() : 0u;
        const auto flags = resolve_style_selector(_style, id(), parent_id, style_state());
        const auto &style = get_theme()->get_style(_style.id);
        _layout_config.size_px = round_font_px(style.text_size());
        _render_config.tint_color = style.text_color();
        return flags;
    }

    void Text::update_layout_min_size()
    {
        _layout_result.clear();
        _content_bounds = {position(), {0.0f, 0.0f}};
        _instances.clear();
        _instances_gpu_dirty = true;
        const auto &style = get_theme()->get_style(_style.id);
        const amal::vec4 margin = style.margin();
        const amal::vec4 padding = style.padding();

        auto *font = get_theme()->get_style(_style.id).font();
        const bool allow_empty_layout = multiline();
        if (!font || (!allow_empty_layout && _text.empty()) || _layout_config.size_px == 0)
        {
            const amal::vec2 content_size = is_fixed() ? size() : amal::vec2{0.0f, 0.0f};
            set_required_size({content_size.x + padding.x + padding.z + margin.x + margin.z,
                               content_size.y + padding.y + padding.w + margin.y + margin.w});
            return;
        }

        auto measure_config = _layout_config;
        if (_layout_config.width_mode == detail::TextLayoutWidthMode::bounds && size().x > 0.0f &&
            (is_fixed() || multiline()))
            measure_config.max_width = size().x;

        const bool is_ok = multiline() ? detail::layout_multiline(*font, _text, measure_config, _layout_result)
                                       : detail::layout_single_line(*font, _text, measure_config, _layout_result);
        if (!is_ok)
        {
            _layout_result.clear();
            const amal::vec2 content_size = is_fixed() ? size() : amal::vec2{0.0f, 0.0f};
            set_required_size({content_size.x + padding.x + padding.z + margin.x + margin.z,
                               content_size.y + padding.y + padding.w + margin.y + margin.w});
            return;
        }

        amal::vec2 min_size = _layout_result.size;
        min_size.y = resolve_layout_height_for_widget(*this, _layout_result);
        if (is_fixed())
        {
            if (size().x > 0.0f) min_size.x = size().x;
            if (size().y > 0.0f) min_size.y = size().y;
        }
        else if (size().y > 0.0f) min_size.y = amal::max(min_size.y, size().y);

        set_required_size({min_size.x + padding.x + padding.z + margin.x + margin.z,
                           min_size.y + padding.y + padding.w + margin.y + margin.w});
    }

    void Text::update_layout(bool min_size_known)
    {
        if (!min_size_known) update_layout_min_size();

        const auto &style = get_theme()->get_style(_style.id);
        const amal::vec4 margin = style.margin();
        const amal::vec4 padding = style.padding();
        const amal::vec2 required_outer = required_size();
        const amal::vec2 required_inner = {amal::max(required_outer.x - margin.x - margin.z, 0.0f),
                                           amal::max(required_outer.y - margin.y - margin.w, 0.0f)};
        const bool auto_width = size().x <= 0.0f;
        const bool auto_height = size().y <= 0.0f;
        const amal::vec2 layout_origin = position();
        const amal::vec2 outer_pos = {layout_origin.x + margin.x, layout_origin.y + margin.y};
        set_position(outer_pos);

        amal::vec2 outer_size = resolve_text_size(*this, required_inner);
        set_size(outer_size);
        Widget::update_layout(true);
        assert(parent() && "Text must have parent");
        set_clip_id(parent()->content_clip_id());

        const amal::vec2 inner_size = {amal::max(outer_size.x - padding.x - padding.z, 0.0f),
                                       amal::max(outer_size.y - padding.y - padding.w, 0.0f)};
        set_position(outer_pos + amal::vec2{padding.x, padding.y});
        rebuild_text_buffers(inner_size);
        update_content_bounds();
        set_position(outer_pos);

        if (!is_fixed())
        {
            const f32 resolved_height = resolve_layout_height_for_widget(*this, _layout_result);
            if (auto_width) outer_size.x = _layout_result.size.x + padding.x + padding.z;
            if (auto_height) outer_size.y = resolved_height + padding.y + padding.w;
            set_size(outer_size);
            set_required_size({_layout_result.size.x + padding.x + padding.z + margin.x + margin.z,
                               resolved_height + padding.y + padding.w + margin.y + margin.w});
        }
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

    void Text::draw(DrawCtx &ctx)
    {
        if (ctx.emit_hit_rect && is_hittable())
        {
            detail::RectData hit_rect = get_rect();
            hit_rect.bounds = _content_bounds;
            if (ctx.is_recording()) _hit_id = AUIK_INVALID_DRAW_DATA_ID;
            const bool force_update =
                ctx.is_recording() || (detail::get_context().dirty_flags & DirtyFlagBits::hit_rect_update);
            update_hit_rect(_hit_id, hit_rect, force_update);
        }

        const u16 current_clip = clip_id();
        const f32 current_z = get_z_order();
        auto *textured_quads_stream = get_primary_textured_quads_stream();
        if (!textured_quads_stream)
        {
            _draw_ids.clear();
            _instances_gpu_dirty = true;
            return;
        }

        if (ctx.is_invalidating())
        {
            for (auto &draw_id : _draw_ids) ctx.emit(textured_quads_stream, draw_id, nullptr, get_rect(), false);
            _instances_gpu_dirty = true;
            return;
        }

        if (_instances.empty())
        {
            assert((ctx.is_recording() || _draw_ids.empty()) && "Text instance shrink requires draw record rebuild");
            _draw_ids.clear();
            _applied_clip_id = current_clip;
            _instances_gpu_dirty = false;
            return;
        }

        const bool draw_state_changed = (_applied_clip_id != current_clip);
        const bool instances_changed = _instances_gpu_dirty;
        if (draw_state_changed || instances_changed)
        {
            for (auto &instance : _instances) instance.clip_id = current_clip;
        }
        if (draw_state_changed || instances_changed)
        {
            for (auto &instance : _instances)
            {
                instance.z_order = current_z;
            instance.tint_color = _render_config.tint_color;
            }
        }

        if (ctx.is_recording())
        {
            _draw_ids.resize(_instances.size());
            push_textured_quads_batch_to_stream(textured_quads_stream, _instances.data(),
                                                static_cast<u32>(_instances.size()), _draw_ids.data());
            _applied_clip_id = current_clip;
            _instances_gpu_dirty = false;
            return;
        }

        if (!draw_state_changed && !instances_changed) return;

        const size_t old_count = _draw_ids.size();
        const size_t new_count = _instances.size();
        assert(old_count <= new_count && "Text instance shrink requires draw record rebuild");
        const size_t update_count = amal::min(old_count, new_count);
        if (update_count > 0)
            update_textured_quads_batch_in_stream(textured_quads_stream, _draw_ids.data(), _instances.data(),
                                                  static_cast<u32>(update_count));

        if (new_count > old_count)
        {
            const size_t append_count = new_count - old_count;
            _draw_ids.resize(new_count);
            push_textured_quads_batch_to_stream(textured_quads_stream, _instances.data() + old_count,
                                                static_cast<u32>(append_count), _draw_ids.data() + old_count);
        }

        _applied_clip_id = current_clip;
        _instances_gpu_dirty = false;
    }

    void Text::set_text(const acul::string &text)
    {
        if (_text == text) return;
        _text = text;
        mark_layout_dirty();
    }

    void Text::set_multiline(bool value)
    {
        const auto next = value ? detail::TextWrapMode::word : detail::TextWrapMode::none;
        if (_layout_config.wrap == next) return;
        _layout_config.wrap = next;
        mark_layout_dirty();
    }

    void Text::set_overflow_mode(detail::TextOverflowMode value)
    {
        if (_layout_config.overflow == value) return;
        _layout_config.overflow = value;
        mark_layout_dirty();
    }

    void Text::set_trim_trailing_spaces(bool value)
    {
        if (_layout_config.trim_trailing_spaces == value) return;
        _layout_config.trim_trailing_spaces = value;
        mark_layout_dirty();
    }

    void Text::set_width_mode(detail::TextLayoutWidthMode value)
    {
        if (_layout_config.width_mode == value) return;
        _layout_config.width_mode = value;
        mark_layout_dirty();
    }

    void Text::set_horizontal_align(detail::TextHorizontalAlign value)
    {
        if (_render_config.horizontal_align == value) return;
        _render_config.horizontal_align = value;
        mark_layout_dirty();
    }

    void Text::set_vertical_align(detail::TextVerticalAlign value)
    {
        if (_render_config.vertical_align == value) return;
        _render_config.vertical_align = value;
        mark_layout_dirty();
    }

    void Text::set_max_lines(u32 value)
    {
        if (_layout_config.max_lines == value) return;
        _layout_config.max_lines = value;
        mark_layout_dirty();
    }

    void Text::set_tight_content_height(bool value)
    {
        if (_tight_content_height == value) return;
        _tight_content_height = value;
        mark_layout_dirty();
    }

    bool Text::rebuild_text_buffers(const amal::vec2 &bounds_size)
    {
        _instances.clear();
        _layout_result.clear();
        _instances_gpu_dirty = true;

        auto *font = get_theme()->get_style(_style.id).font();
        const bool allow_empty_layout = multiline();
        if (!font || (!allow_empty_layout && _text.empty()) || _layout_config.size_px == 0) return false;

        auto render_config = _render_config;
        render_config.bounds = {position(), bounds_size};
        render_config.z_order = get_z_order();
        render_config.clip_id = clip_id();

        return multiline() ? detail::build_multiline_instances(*font, _text, _layout_config, render_config, _instances,
                                                               &_layout_result)
                           : detail::build_single_line_instances(*font, _text, _layout_config, render_config,
                                                                 _instances, &_layout_result);
    }

    void Text::mark_layout_dirty()
    {
        auto &ctx = detail::get_context();
        const bool layout_pending = ctx.dirty_flags & DirtyFlagBits::layout;
        ctx.dirty_flags |= DirtyFlagBits::layout;
        if (!layout_pending) detail::mark_host_refresh_request();
    }

    TextWithTooltip::~TextWithTooltip()
    {
        cancel_delayed_tasks(id());
        clear_tooltip_if_source(&_tooltip_text);
    }

    void TextWithTooltip::on_hover(HoverState state)
    {
        const u32 wid = id();
        const auto current_hover = detail::get_context().hover_id;
        const bool is_same_hover_session = current_hover.widget_id == wid;
        if (_tooltip_text.empty())
        {
            if (state == HoverState::leave)
            {
                cancel_delayed_tasks(wid);
                add_render_command<detail::HoverEventTraits>(this, []() { hide_tooltip(); });
            }
            return;
        }

        if (state == HoverState::leave)
        {
            cancel_delayed_tasks(wid);
            add_render_command<detail::HoverEventTraits>(this, [this, is_same_hover_session]() {
                hide_tooltip();
                if (!is_same_hover_session) clear_tooltip_if_source(&_tooltip_text);
            });
            return;
        }

        if (state != HoverState::enter) return;
        const f32 anchor_x = get_mouse_pos().x;
        cancel_delayed_tasks(wid);
        detail::update_window_time(detail::get_context().window_ctx);
        const f64 due_time = detail::get_context().window_ctx->time + AUIK_TOOLTIP_SHOW_DELAY;
        schedule_delayed_host_task(wid, due_time, [this, anchor_x]() { show_tooltip(anchor_x, &_tooltip_text); });
    }
} // namespace auik::v2
