#include <auik/v2/auik.hpp>
#include <auik/v2/pipelines.hpp>
#include <auik/v2/widgets/text_button.hpp>

namespace auik::v2
{
    static inline bool is_style_only_draw_update(const DrawCtx &ctx)
    {
        if (!ctx.is_updating()) return false;
        if (!(ctx.reason & DrawReasonBits::style)) return false;
        if (ctx.reason & DrawReasonBits::layout) return false;
        if (ctx.reason & DrawReasonBits::full_redraw) return false;
        return true;
    }

    StyleUpdateFlags TextButton::update_style()
    {
        const u32 parent_id = parent() ? parent()->id() : 0u;
        const auto flags = resolve_style_selector(_style, id(), parent_id, style_state());
        const auto &style = get_theme()->get_style(_style.id);
        _text->update_style();
        if (_resolved_text_color != style.text_color())
        {
            _resolved_text_color = style.text_color();
            _text_draw_dirty = true;
        }
        return flags;
    }

    void TextButton::update_layout_min_size()
    {
        auto *theme = get_theme();
        const auto &style = theme->get_style(_style.id);
        const amal::vec4 margin = style.margin();
        const amal::vec4 padding = style.padding();

        _text->update_layout_min_size();
        const amal::vec2 text_size = _text->required_size();

        amal::vec2 min_size = size();
        if (!is_fixed()) min_size.x = 0.0f;
        else if (min_size.x <= 0.0f) min_size.x = amal::max(120.0f, text_size.x + padding.x + padding.z);
        if (min_size.y <= 0.0f) min_size.y = amal::max(style.text_size(), text_size.y) + padding.y + padding.w;
        if (min_size.x > 0.0f) min_size.x = amal::max(min_size.x, text_size.x + padding.x + padding.z);
        set_required_size({min_size.x + margin.x + margin.z, min_size.y + margin.y + margin.w});
    }

    void TextButton::update_layout(bool min_size_known)
    {
        if (!min_size_known) update_layout_min_size();
        auto *theme = get_theme();
        const auto &style = theme->get_style(_style.id);
        const amal::vec2 layout_origin = position();
        const amal::vec4 margin = style.margin();

        const amal::vec2 min_required = required_size();
        const amal::vec2 min_button = {amal::max(min_required.x - margin.x - margin.z, 0.0f),
                                       amal::max(min_required.y - margin.y - margin.w, 0.0f)};
        amal::vec2 button_size = size();
        if (!is_fixed()) button_size.x = amal::max(button_size.x - margin.x - margin.z, min_button.x);
        else button_size.x = amal::max(button_size.x, min_button.x);
        button_size.y = amal::max(button_size.y, min_button.y);

        const amal::vec2 pos = {layout_origin.x + margin.x, layout_origin.y + margin.y};
        set_position(pos);
        set_size(button_size);
        Widget::update_layout(true);
        assert(parent() && "TextButton must have parent");
        set_clip_id(parent()->content_clip_id());

        const amal::vec4 padding = style.padding();
        const amal::vec2 content_pos = {pos.x + padding.x, pos.y + padding.y};
        const amal::vec2 content_size = {amal::max(button_size.x - padding.x - padding.z, 0.0f),
                                         amal::max(button_size.y - padding.y - padding.w, 0.0f)};
        _text->set_position(content_pos);
        _text->set_size(content_size);
        _text->update_layout(true);
    }

    void TextButton::translate(const amal::vec2 &delta)
    {
        if (delta.x == 0.0f && delta.y == 0.0f) return;
        Widget::translate(delta);
        _text->translate(delta);
    }

    void TextButton::rebuild_clip_rects()
    {
        assert(parent() && "TextButton must have parent");
        set_clip_id(parent()->content_clip_id());
        _bg.hit_id = AUIK_INVALID_DRAW_DATA_ID;
        _text->set_clip_id(clip_id());
        _text->rebuild_clip_rects();
        _text_draw_dirty = true;
    }

    void TextButton::update_depth(const amal::vec2 &depth_range)
    {
        Widget::update_depth(depth_range);
        amal::vec2 text_range{};
        assign_next_depth(this->depth_range(), text_range);
        _text->update_depth(text_range);
    }

    void TextButton::draw(DrawCtx &ctx)
    {
        auto *theme = get_theme();
        auto *quads_stream = get_primary_quads_stream();

        QuadsInstanceData bg_data{};
        bg_data.rect = bounds();
        bg_data.z_order = get_z_order();
        const bool bg_visible = fill_quads_instance_by_style(theme->get_style(_style.id), clip_id(), bg_data);
        if (should_emit_quads_instance(bg_visible, _bg, ctx.emit_hit_rect))
            ctx.emit(quads_stream, _bg, &bg_data, get_rect(), ctx.emit_hit_rect);

        if (is_style_only_draw_update(ctx) && !_text_draw_dirty) return;
        _text->draw(ctx);
        _text_draw_dirty = false;
    }
} // namespace auik::v2
