#include <auik/v2/pipelines.hpp>
#include <auik/v2/widgets/detail/selectable.hpp>

namespace auik::v2::detail
{
    StyleUpdateFlags Selectable::update_style()
    {
        const u32 parent_id = parent() ? parent()->id() : 0u;
        auto flags = resolve_style_selector(_style, id(), parent_id, style_state());
        flags |= resolve_style_selector(_selected_style, id(), parent_id, _selected_style_state);
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
        set_clip_id(parent() ? parent()->content_clip_id() : clip_id());
        Text::rebuild_clip_rects();
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
        const u16 bg_clip_id = parent() ? parent()->content_clip_id() : clip_id();
        QuadsInstanceData selected_bg{};
        selected_bg.rect = bounds();
        selected_bg.z_order = _selected_bg_z;
        const bool selected_visible =
            _selected &&
            fill_quads_instance_by_style(get_theme()->get_style(_selected_style.id), bg_clip_id, selected_bg);
        if (selected_visible || ctx.is_recording() || _selected_bg.render_id != AUIK_INVALID_DRAW_DATA_ID)
            ctx.emit(quads_stream, _selected_bg, &selected_bg, get_rect(), false);

        QuadsInstanceData bg{};
        bg.rect = bounds();
        bg.z_order = _bg_z;
        const bool draw_state_bg = !_selected || style_state() != StyleState::normal;
        const bool bg_visible =
            draw_state_bg && fill_quads_instance_by_style(get_theme()->get_style(_style.id), bg_clip_id, bg);
        if (should_emit_quads_instance(bg_visible, _bg, ctx.emit_hit_rect))
            ctx.emit(quads_stream, _bg, &bg, get_rect(), ctx.emit_hit_rect);

        DrawCtx text_ctx = ctx;
        text_ctx.emit_hit_rect = false;
        set_clip_id(bg_clip_id);
        Text::draw(text_ctx);
    }
} // namespace auik::v2::detail
