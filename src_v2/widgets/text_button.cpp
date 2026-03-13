#include <auik/v2/auik.hpp>
#include <auik/v2/pipelines.hpp>
#include <auik/v2/widgets/text_button.hpp>

namespace auik::v2
{
    void TextButton::update_style()
    {
        auto *theme = get_theme();
        const u32 parent_id = parent() ? parent()->id() : 0u;
        _style.id = theme->get_resolved_style(_style.tag_id, id(), parent_id, style_state());
    }

    void TextButton::update_layout()
    {
        auto *theme = get_theme();
        const auto &style = theme->get_style(_style.id);
        const amal::vec2 cursor = detail::get_context().screen_cursor;
        const amal::vec4 margin = style.margin();
        const amal::vec4 padding = style.padding();

        amal::vec2 button_size = size();
        if (!is_fixed()) button_size.x = amal::max(button_size.x - margin.x - margin.z, 0.0f);
        else if (button_size.x <= 0.0f) button_size.x = 120.0f;
        if (button_size.y <= 0.0f) button_size.y = style.text_size() + padding.y + padding.w;

        const amal::vec2 pos = {cursor.x + margin.x, cursor.y + margin.y};
        set_position(pos);
        set_size(button_size);
        Widget::update_layout();
        set_required_size({button_size.x + margin.x + margin.z, button_size.y + margin.y + margin.w});
        inherit_parent_content_clip_rect();

        detail::get_context().screen_cursor = {cursor.x, pos.y + button_size.y + margin.w};
    }

    void TextButton::rebuild_clip_rects()
    {
        inherit_parent_content_clip_rect();
        _bg.hit_id = AUIK_INVALID_DRAW_DATA_ID;
    }

    void TextButton::on_hover(HoverState state, u32 prev_tag_id)
    {
        (void)prev_tag_id;
        if (style_state() == StyleState::active || state == HoverState::active) return;

        const StyleState next_state = (state == HoverState::leave) ? StyleState::normal : StyleState::hover;
        if (!set_style_state(next_state)) return;

        add_render_command(this, [this]() {
            update_style();
            if (_bg.render_id != AUIK_INVALID_DRAW_DATA_ID) update_draw_commands();
            else record_draw_commands();
            detail::get_context().dirty_flags |= DirtyFlagBits::redraw;
        });
        detail::mark_host_refresh_request();
    }

    void TextButton::on_click(MouseKey key, KeyPressState state, u32 click_count)
    {
        (void)click_count;
        if (key != MouseKey::left) return;

        StyleState next_state = style_state();
        if (state == KeyPressState::press) next_state = StyleState::active;
        else if (state == KeyPressState::release)
        {
            const auto &ctx = detail::get_context();
            next_state = (ctx.hover_id.widget_id == id()) ? StyleState::hover : StyleState::normal;
        }
        else return;

        if (!set_style_state(next_state)) return;
        add_render_command(this, [this]() {
            update_style();
            if (_bg.render_id != AUIK_INVALID_DRAW_DATA_ID) update_draw_commands();
            else record_draw_commands();
            detail::get_context().dirty_flags |= DirtyFlagBits::redraw;
        });
        detail::mark_host_refresh_request();
    }

    void TextButton::draw(DrawCtx &ctx)
    {
        auto *theme = get_theme();
        auto *quads_stream = get_primary_quad_stream();

        QuadsInstanceData bg_data{};
        bg_data.position = position();
        bg_data.size = size();
        bg_data.z_order = get_z_order();
        fill_quads_instance_by_style(theme->get_style(_style.id), clip_id(), bg_data);
        ctx.emit(quads_stream, _bg, &bg_data, get_rect(), ctx.emit_hit_rect);
    }
} // namespace auik::v2
