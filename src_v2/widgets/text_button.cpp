#include <auik/v2/auik.hpp>
#include <auik/v2/pipelines.hpp>
#include <auik/v2/widgets/text_button.hpp>

namespace auik::v2
{
    StyleUpdateFlags TextButton::update_style()
    {
        const u32 parent_id = parent() ? parent()->id() : 0u;
        return resolve_style_selector(_style, id(), parent_id, style_state());
        ;
    }

    void TextButton::update_layout_min_size()
    {
        auto *theme = get_theme();
        const auto &style = theme->get_style(_style.id);
        const amal::vec4 margin = style.margin();
        const amal::vec4 padding = style.padding();

        amal::vec2 min_size = size();
        if (!is_fixed()) min_size.x = 0.0f;
        else if (min_size.x <= 0.0f) min_size.x = 120.0f;
        if (min_size.y <= 0.0f) min_size.y = style.text_size() + padding.y + padding.w;
        set_required_size({min_size.x + margin.x + margin.z, min_size.y + margin.y + margin.w});
    }

    void TextButton::update_layout(bool min_size_known)
    {
        if (!min_size_known) update_layout_min_size();
        auto *theme = get_theme();
        const auto &style = theme->get_style(_style.id);
        const amal::vec2 cursor = detail::get_context().screen_cursor;
        const amal::vec4 margin = style.margin();

        const amal::vec2 min_required = required_size();
        const amal::vec2 min_button = {amal::max(min_required.x - margin.x - margin.z, 0.0f),
                                       amal::max(min_required.y - margin.y - margin.w, 0.0f)};
        amal::vec2 button_size = size();
        if (!is_fixed()) button_size.x = amal::max(button_size.x - margin.x - margin.z, min_button.x);
        else button_size.x = amal::max(button_size.x, min_button.x);
        button_size.y = amal::max(button_size.y, min_button.y);

        const amal::vec2 pos = {cursor.x + margin.x, cursor.y + margin.y};
        set_position(pos);
        set_size(button_size);
        Widget::update_layout(true);
        inherit_parent_content_clip_rect();

        detail::get_context().screen_cursor = {cursor.x, pos.y + button_size.y + margin.w};
    }

    void TextButton::rebuild_clip_rects()
    {
        inherit_parent_content_clip_rect();
        _bg.hit_id = AUIK_INVALID_DRAW_DATA_ID;
    }

    void TextButton::draw(DrawCtx &ctx)
    {
        auto *theme = get_theme();
        auto *quads_stream = get_primary_quad_stream();

        QuadsInstanceData bg_data{};
        bg_data.rect = bounds();
        bg_data.z_order = get_z_order();
        fill_quads_instance_by_style(theme->get_style(_style.id), clip_id(), bg_data);
        ctx.emit(quads_stream, _bg, &bg_data, get_rect(), ctx.emit_hit_rect);
    }
} // namespace auik::v2
