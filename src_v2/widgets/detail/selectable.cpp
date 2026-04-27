#include <auik/v2/pipelines.hpp>
#include <auik/v2/widgets/detail/selectable.hpp>

namespace auik::v2::detail
{
    StyleUpdateFlags Selectable::update_style()
    {
        const u32 parent_id = parent() ? parent()->id() : 0u;
        const auto flags = resolve_style_selector(_style, id(), parent_id, style_state());
        const auto &style = get_theme()->get_style(_style.id);
        _layout_config.size_px = round_font_px(style.text_size());
        _render_config.tint_color = style.text_color();
        return flags;
    }

    void Selectable::rebuild_clip_rects()
    {
        set_clip_id(parent() ? parent()->content_clip_id() : clip_id());
        Text::rebuild_clip_rects();
        _bg.hit_id = AUIK_INVALID_DRAW_DATA_ID;
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
        amal::vec2 bg_range{};
        amal::vec2 text_range{};
        assign_next_depth(this->depth_range(), bg_range);
        assign_next_depth(bg_range, text_range);
        _bg_z = next_depth(bg_range);
        _rect.depth = next_depth(text_range);
    }

    void Selectable::draw(DrawCtx &ctx)
    {
        auto *quads_stream = get_primary_quads_stream();
        const u16 bg_clip_id = parent() ? parent()->content_clip_id() : clip_id();
        QuadsInstanceData bg{};
        bg.rect = bounds();
        bg.z_order = _bg_z;
        const bool bg_visible = fill_quads_instance_by_style(get_theme()->get_style(_style.id), bg_clip_id, bg);
        if (should_emit_quads_instance(bg_visible, _bg, ctx.emit_hit_rect))
            ctx.emit(quads_stream, _bg, &bg, get_rect(), ctx.emit_hit_rect);

        DrawCtx text_ctx = ctx;
        text_ctx.emit_hit_rect = false;
        set_clip_id(bg_clip_id);
        Text::draw(text_ctx);
    }
} // namespace auik::v2::detail
