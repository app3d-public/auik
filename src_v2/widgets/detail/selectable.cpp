#include <auik/v2/pipelines.hpp>
#include <auik/v2/widgets/detail/selectable.hpp>

namespace auik::v2::detail
{
    StyleUpdateFlags Selectable::update_style()
    {
        const u32 parent_id = parent() ? parent()->id() : 0u;
        const StyleState current_state = style_state();
        auto flags = resolve_style_selector(_style, id(), parent_id, current_state);
        flags |= resolve_style_selector(_selected_style, id(), parent_id,
                                        _selected ? current_state : _selected_style_state);
        const auto &style = get_theme()->get_style(_style.id);
        _layout_config.size_px = round_font_px(style.text_size());
        const u32 text_color = style.text_color();
        if (_render_config.tint_color != text_color)
        {
            _render_config.tint_color = text_color;
            _instances_gpu_dirty = true;
        }
        return flags;
    }

    void Selectable::rebuild_clip_rects()
    {
        update_content_clip_rect();
        _bg.hit_id = AUIK_INVALID_DRAW_DATA_ID;
        _selected_bg.hit_id = AUIK_INVALID_DRAW_DATA_ID;
    }

    void Selectable::update_layout(bool min_size_known)
    {
        if (!is_fixed())
        {
            const auto &style = get_theme()->get_style(_style.id);
            const amal::vec4 margin = style.margin();
            set_size({amal::max(size().x - margin.x - margin.z, 0.0f), size().y});
        }
        Text::update_layout(min_size_known);
        update_content_clip_rect();
    }

    void Selectable::translate(const amal::vec2 &delta)
    {
        Text::translate(delta);
        update_content_clip_rect();
    }

    void Selectable::reset_draw_records()
    {
        Text::reset_draw_records();
        _bg = {};
        _selected_bg = {};
    }

    void Selectable::update_depth(const amal::vec2 &depth_range)
    {
        Widget::update_depth(depth_range);
        amal::vec2 selected_bg_range{};
        amal::vec2 bg_range{};
        amal::vec2 text_range{};
        assign_next_depth(this->depth_range(), selected_bg_range);
        assign_next_depth(selected_bg_range, bg_range);
        assign_next_depth(bg_range, text_range);
        _selected_bg_z = next_depth(selected_bg_range);
        _bg_z = next_depth(bg_range);
        const f32 text_z = next_depth(text_range);
        if (_rect.depth != text_z)
        {
            _rect.depth = text_z;
            _instances_gpu_dirty = true;
        }
    }

    void Selectable::draw(DrawCtx &ctx)
    {
        auto *quads_stream = get_primary_quads_stream();
        if (ctx.is_invalidating())
        {
            ctx.emit(quads_stream, _selected_bg, nullptr, get_rect(), false);
            ctx.emit(quads_stream, _bg, nullptr, get_rect(), false);
            DrawCtx text_ctx = ctx;
            text_ctx.emit_hit_rect = false;
            Text::draw(text_ctx);
            return;
        }

        const u16 bg_clip_id = clip_id();
        if (bg_clip_id == 0xFFFFu)
        {
            if (!ctx.is_recording())
            {
                DrawCtx invalidate_ctx = ctx;
                invalidate_ctx.emit_fn = &emit_draw_invalidate;
                invalidate_ctx.emit_hit_rect = false;
                draw(invalidate_ctx);
            }
            return;
        }
        QuadsInstanceData selected_bg{};
        selected_bg.rect = bounds();
        selected_bg.z_order = _selected_bg_z;
        const bool selected_visible =
            _selected && fill_quads_instance_by_style(get_theme()->get_style(_selected_style.id), bg_clip_id, selected_bg);
        emit_quads_instance(ctx, quads_stream, _selected_bg, selected_bg, get_rect(), selected_visible, false);

        QuadsInstanceData bg{};
        bg.rect = bounds();
        bg.z_order = _bg_z;
        const bool draw_state_bg = !_selected;
        const bool bg_visible =
            draw_state_bg && fill_quads_instance_by_style(get_theme()->get_style(_style.id), bg_clip_id, bg);
        emit_quads_instance(ctx, quads_stream, _bg, bg, get_rect(), bg_visible, ctx.emit_hit_rect);

        DrawCtx text_ctx = ctx;
        text_ctx.emit_hit_rect = false;
        set_clip_id(bg_clip_id);
        Text::draw(text_ctx);
    }

    void Selectable::update_content_clip_rect()
    {
        const u16 next_clip_id = parent() ? parent()->content_clip_id() : clip_id();
        if (next_clip_id == 0xFFFFu) return;
        set_clip_id(next_clip_id);
    }
} // namespace auik::v2::detail
