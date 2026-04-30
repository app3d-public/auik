#include <auik/v2/auik.hpp>
#include <auik/v2/pipelines.hpp>
#include <auik/v2/widgets/radio_button.hpp>

namespace auik::v2
{
    RadioButton::RadioButton(u32 id, bool *value, WidgetFlags widget_flags, Widget *parent)
        : Widget(id, widget_flags, EventFlagBits::click, parent, {{0.0f, 0.0f}, {0.0f, 0.0f}}, AUIK_TAG_RADIO_BUTTON),
          _value(value),
          _indicator_rect(detail::make_rect_data(AUIK_TAG_RADIO_BUTTON_INDICATOR, AUIK_TAG_RADIO_BUTTON_INDICATOR))
    {
    }

    amal::vec2 RadioButton::resolve_indicator_size(const Style &indicator_style) const
    {
        const amal::vec4 padding = indicator_style.padding();
        return {amal::max(padding.x + padding.z, 1.0f), amal::max(padding.y + padding.w, 1.0f)};
    }

    amal::vec2 RadioButton::resolve_background_size(const Style &background_style) const
    {
        const amal::vec4 padding = background_style.padding();
        const f32 side = amal::max(padding.x + padding.z, padding.y + padding.w);
        return {side, side};
    }

    StyleUpdateFlags RadioButton::update_style()
    {
        const u32 parent_id = parent() ? parent()->id() : 0u;
        StyleUpdateFlags out = StyleUpdateFlagBits::none;
        out |= resolve_style_selector(_background_style, id(), parent_id, style_state());
        out |= resolve_style_selector(_indicator_style, _indicator_rect.tag_id, parent_id, style_state());
        return out;
    }

    void RadioButton::update_layout_min_size()
    {
        auto *theme = get_theme();
        const auto &background_style = theme->get_style(_background_style.id);
        const amal::vec4 margin = background_style.margin();
        const amal::vec2 background_size = resolve_background_size(background_style);

        amal::vec2 min_size = size();
        if (!is_fixed()) min_size.x = 0.0f;

        if (min_size.x <= 0.0f) min_size.x = background_size.x;
        else min_size.x = amal::max(min_size.x, background_size.x);
        if (min_size.y <= 0.0f) min_size.y = background_size.y;
        else min_size.y = amal::max(min_size.y, background_size.y);

        set_required_size({min_size.x + margin.x + margin.z, min_size.y + margin.y + margin.w});
    }

    void RadioButton::rebuild_indicator_layout()
    {
        const auto &indicator_style = get_theme()->get_style(_indicator_style.id);
        const amal::vec2 indicator_size = resolve_indicator_size(indicator_style);
        const amal::vec2 indicator_pos = {
            _background_rect.offset.x + amal::max((_background_rect.size.x - indicator_size.x) * 0.5f, 0.0f),
            _background_rect.offset.y + amal::max((_background_rect.size.y - indicator_size.y) * 0.5f, 0.0f)};
        _indicator_rect.bounds = {indicator_pos, indicator_size};
        _indicator_rect.clip_id = clip_id();
        _indicator_rect.depth = next_depth(_indicator_depth_range);
    }

    void RadioButton::update_layout(bool min_size_known)
    {
        if (!min_size_known) update_layout_min_size();

        auto *theme = get_theme();
        const auto &background_style = theme->get_style(_background_style.id);
        const amal::vec4 margin = background_style.margin();
        const amal::vec2 layout_origin = position();
        const amal::vec2 min_required = {amal::max(required_size().x - margin.x - margin.z, 0.0f),
                                         amal::max(required_size().y - margin.y - margin.w, 0.0f)};

        amal::vec2 widget_size = size();
        widget_size.x = amal::max(widget_size.x, min_required.x);
        widget_size.y = amal::max(widget_size.y, min_required.y);

        const amal::vec2 pos = {layout_origin.x + margin.x, layout_origin.y + margin.y};
        set_position(pos);
        set_size(widget_size);
        Widget::update_layout(true);
        assert(parent() && "RadioButton must have parent");
        set_clip_id(parent()->content_clip_id());

        const amal::vec2 background_size = resolve_background_size(background_style);
        const f32 outer_h = widget_size.y + margin.y + margin.w;
        _background_rect.offset = {pos.x + amal::max((widget_size.x - background_size.x) * 0.5f, 0.0f),
                                   layout_origin.y + amal::max((outer_h - background_size.y) * 0.5f, 0.0f)};
        _background_rect.size = background_size;
        rebuild_indicator_layout();
    }

    void RadioButton::translate(const amal::vec2 &delta)
    {
        if (delta.x == 0.0f && delta.y == 0.0f) return;
        Widget::translate(delta);
        _background_rect.offset += delta;
        _indicator_rect.bounds.offset += delta;
    }

    void RadioButton::rebuild_clip_rects()
    {
        assert(parent() && "RadioButton must have parent");
        set_clip_id(parent()->content_clip_id());
        _background_draw.hit_id = AUIK_INVALID_DRAW_DATA_ID;
        _indicator_draw.hit_id = AUIK_INVALID_DRAW_DATA_ID;
        _indicator_rect.clip_id = clip_id();
    }

    void RadioButton::update_depth(const amal::vec2 &depth_range)
    {
        Widget::update_depth(depth_range);
        assign_next_depth(this->depth_range(), _background_depth_range);
        assign_next_depth(_background_depth_range, _indicator_depth_range);
        _indicator_rect.depth = next_depth(_indicator_depth_range);
    }

    void RadioButton::draw(DrawCtx &ctx)
    {
        if (!(widget_flags & WidgetFlagBits::visible)) return;

        auto *quads_stream = get_primary_quads_stream();
        auto *theme = get_theme();

        QuadsInstanceData background{};
        background.rect = _background_rect;
        background.z_order = next_depth(_background_depth_range);
        const bool background_visible =
            fill_quads_instance_by_style(theme->get_style(_background_style.id), clip_id(), background);
        if (should_emit_quads_instance(background_visible, _background_draw, ctx.emit_hit_rect))
        {
            auto hit_rect = get_rect();
            hit_rect.bounds = _background_rect;
            ctx.emit(quads_stream, _background_draw, &background, hit_rect, ctx.emit_hit_rect);
        }

        QuadsInstanceData indicator{};
        indicator.rect = _indicator_rect.bounds;
        indicator.z_order = _indicator_rect.depth;
        const bool indicator_visible =
            value() && fill_quads_instance_by_style(theme->get_style(_indicator_style.id), clip_id(), indicator);
        if (should_emit_quads_instance(indicator_visible, _indicator_draw, false))
            ctx.emit(quads_stream, _indicator_draw, &indicator, _indicator_rect, false);
    }

    bool RadioButton::has_draw_record() const
    {
        if (_background_draw.render_id == AUIK_INVALID_DRAW_DATA_ID) return false;
        if (value() && _indicator_draw.render_id == AUIK_INVALID_DRAW_DATA_ID) return false;
        return true;
    }

    void RadioButton::set_value(bool new_value)
    {
        if (!_value || *_value == new_value) return;
        *_value = new_value;
        redraw_external(has_draw_record());
    }

    void RadioButton::on_click(MouseKey key, KeyPressState state, u32 click_count)
    {
        (void)click_count;
        if (key != MouseKey::left || state != KeyPressState::press || !_value) return;
        *_value = !*_value;
        add_render_command<detail::ClickEventTraits>(this, [this]() { redraw_external(has_draw_record()); });
    }
} // namespace auik::v2
