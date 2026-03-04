#include <auik/v2/pipelines.hpp>
#include <auik/v2/text_button.hpp>

namespace auik::v2
{
    static inline amal::vec4 intersect_rect(const amal::vec4 &a, const amal::vec4 &b)
    {
        const amal::vec2 a_min = {a.x, a.y};
        const amal::vec2 a_max = {a.x + a.z, a.y + a.w};
        const amal::vec2 b_min = {b.x, b.y};
        const amal::vec2 b_max = {b.x + b.z, b.y + b.w};

        const amal::vec2 out_min = {amal::max(a_min.x, b_min.x), amal::max(a_min.y, b_min.y)};
        const amal::vec2 out_max = {amal::min(a_max.x, b_max.x), amal::min(a_max.y, b_max.y)};
        const amal::vec2 out_size = {amal::max(out_max.x - out_min.x, 0.0f), amal::max(out_max.y - out_min.y, 0.0f)};
        return {out_min, out_size};
    }

    void TextButton::update_style()
    {
        auto *theme = get_theme();
        const u32 parent_id = parent() ? parent()->id() : 0u;
        _style.id = theme->get_resolved_style(_style.tag_id, id(), parent_id);
    }

    void TextButton::update_layout()
    {
        auto *theme = get_theme();
        const auto &style = theme->get_style(_style.id);
        const amal::vec2 cursor = detail::get_context().screen_cursor;
        const amal::vec4 margin = style.margin();
        const amal::vec4 padding = style.padding();

        amal::vec2 button_size = size();
        if (button_size.x <= 0.0f) button_size.x = 120.0f;
        if (button_size.y <= 0.0f) button_size.y = style.text_size() + padding.y + padding.w;

        const amal::vec2 pos = {cursor.x + margin.x, cursor.y + margin.y};
        set_position(pos);
        set_size(button_size);
        set_required_size({button_size.x + margin.x + margin.z, button_size.y + margin.y + margin.w});
        Widget::update_layout();

        if (parent())
        {
            const amal::vec4 parent_clip = get_clip_rect(parent()->clip_rect_id());
            const amal::vec4 self_clip = get_clip_rect(clip_rect_id());
            get_clip_rect(clip_rect_id()) = intersect_rect(self_clip, parent_clip);
        }

        detail::get_context().screen_cursor = {cursor.x, pos.y + button_size.y + margin.w};
    }

    void TextButton::rebuild_clip_rects()
    {
        Widget::rebuild_clip_rects();
        _bg.hit_id = AUIK_INVALID_DRAW_DATA_ID;
    }

    void TextButton::draw(DrawCtx &ctx)
    {
        auto *theme = get_theme();
        auto *quads_stream = get_primary_quad_stream();

        QuadsInstanceData bg_data{};
        bg_data.position = position();
        bg_data.size = size();
        bg_data.z_order = get_z_order();
        fill_quads_instance_by_style(theme->get_style(_style.id), clip_rect_id(), bg_data);
        ctx.emit(quads_stream, _bg, &bg_data, get_rect());
    }
} // namespace auik::v2
