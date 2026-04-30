#include <auik/v2/auik.hpp>
#include <auik/v2/pipelines.hpp>
#include <auik/v2/widgets/switch_button.hpp>

namespace auik::v2
{
    SwitchButton::SwitchButton(u32 id, bool *value, WidgetFlags widget_flags, Widget *parent)
        : Widget(id, widget_flags, EventFlagBits::click, parent, {{0.0f, 0.0f}, {0.0f, 0.0f}}, AUIK_TAG_SWITCH_BUTTON),
          _value(value),
          _grab_rect(detail::make_rect_data(AUIK_TAG_SWITCH_BUTTON_GRAB, AUIK_TAG_SWITCH_BUTTON_GRAB))
    {
        sync_track_tag();
    }

    amal::vec2 SwitchButton::resolve_grab_size(const Style &grab_style) const
    {
        const amal::vec4 padding = grab_style.padding();
        return {amal::max(padding.x + padding.z, 1.0f), amal::max(padding.y + padding.w, 1.0f)};
    }

    amal::vec2 SwitchButton::resolve_track_size(const Style &track_style, const Style &grab_style) const
    {
        const amal::vec4 padding = track_style.padding();
        const amal::vec2 grab_size = resolve_grab_size(grab_style);
        return {amal::max(grab_size.x * 2.0f + padding.x + padding.z, 1.0f),
                amal::max(grab_size.y + padding.y + padding.w, 1.0f)};
    }

    u32 SwitchButton::track_tag() const { return value() ? AUIK_TAG_SWITCH_BUTTON_ON : AUIK_TAG_SWITCH_BUTTON; }

    void SwitchButton::sync_track_tag()
    {
        const u32 tag = track_tag();
        if (_track_style.tag_id == tag) return;
        _track_style.tag_id = tag;
        set_rect_tag_id(tag);
    }

    StyleUpdateFlags SwitchButton::update_style()
    {
        sync_track_tag();
        const u32 parent_id = parent() ? parent()->id() : 0u;
        StyleUpdateFlags out = StyleUpdateFlagBits::none;
        out |= resolve_style_selector(_track_style, id(), parent_id, style_state());
        out |= resolve_style_selector(_grab_style, _grab_rect.tag_id, parent_id, style_state());
        return out;
    }

    void SwitchButton::update_layout_min_size()
    {
        auto *theme = get_theme();
        const auto &track_style = theme->get_style(_track_style.id);
        const auto &grab_style = theme->get_style(_grab_style.id);
        const amal::vec4 margin = track_style.margin();
        const amal::vec2 track_size = resolve_track_size(track_style, grab_style);

        amal::vec2 min_size = size();
        if (!is_fixed()) min_size.x = 0.0f;

        if (min_size.x <= 0.0f) min_size.x = track_size.x;
        else min_size.x = amal::max(min_size.x, track_size.x);
        if (min_size.y <= 0.0f) min_size.y = track_size.y;
        else min_size.y = amal::max(min_size.y, track_size.y);

        set_required_size({min_size.x + margin.x + margin.z, min_size.y + margin.y + margin.w});
    }

    void SwitchButton::rebuild_grab_layout()
    {
        auto *theme = get_theme();
        const auto &track_style = theme->get_style(_track_style.id);
        const auto &grab_style = theme->get_style(_grab_style.id);
        const amal::vec4 track_padding = track_style.padding();
        const amal::vec2 grab_size = resolve_grab_size(grab_style);
        const amal::vec2 lane_pos = {_track_rect.offset.x + track_padding.x, _track_rect.offset.y + track_padding.y};
        const amal::vec2 lane_size = {amal::max(_track_rect.size.x - track_padding.x - track_padding.z, 0.0f),
                                      amal::max(_track_rect.size.y - track_padding.y - track_padding.w, 0.0f)};
        const f32 grab_x = value() ? lane_pos.x + amal::max(lane_size.x - grab_size.x, 0.0f) : lane_pos.x;
        const f32 grab_y = lane_pos.y + amal::max((lane_size.y - grab_size.y) * 0.5f, 0.0f);

        _grab_rect.bounds = {{grab_x, grab_y}, grab_size};
        _grab_rect.clip_id = clip_id();
        _grab_rect.depth = next_depth(_grab_depth_range);
    }

    void SwitchButton::update_layout(bool min_size_known)
    {
        if (!min_size_known) update_layout_min_size();

        auto *theme = get_theme();
        const auto &track_style = theme->get_style(_track_style.id);
        const auto &grab_style = theme->get_style(_grab_style.id);
        const amal::vec4 margin = track_style.margin();
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
        assert(parent() && "SwitchButton must have parent");
        set_clip_id(parent()->content_clip_id());

        const amal::vec2 track_size = resolve_track_size(track_style, grab_style);
        const f32 outer_h = widget_size.y + margin.y + margin.w;
        _track_rect.offset = {pos.x + amal::max((widget_size.x - track_size.x) * 0.5f, 0.0f),
                              layout_origin.y + amal::max((outer_h - track_size.y) * 0.5f, 0.0f)};
        _track_rect.size = track_size;
        rebuild_grab_layout();
    }

    void SwitchButton::translate(const amal::vec2 &delta)
    {
        if (delta.x == 0.0f && delta.y == 0.0f) return;
        Widget::translate(delta);
        _track_rect.offset += delta;
        _grab_rect.bounds.offset += delta;
    }

    void SwitchButton::rebuild_clip_rects()
    {
        assert(parent() && "SwitchButton must have parent");
        set_clip_id(parent()->content_clip_id());
        _track_draw.hit_id = AUIK_INVALID_DRAW_DATA_ID;
        _grab_draw.hit_id = AUIK_INVALID_DRAW_DATA_ID;
        _grab_rect.clip_id = clip_id();
    }

    void SwitchButton::update_depth(const amal::vec2 &depth_range)
    {
        Widget::update_depth(depth_range);
        assign_next_depth(this->depth_range(), _track_depth_range);
        assign_next_depth(_track_depth_range, _grab_depth_range);
        _grab_rect.depth = next_depth(_grab_depth_range);
    }

    void SwitchButton::draw(DrawCtx &ctx)
    {
        if (!(widget_flags & WidgetFlagBits::visible)) return;

        auto *quads_stream = get_primary_quads_stream();
        auto *theme = get_theme();
        const auto &track_style = theme->get_style(_track_style.id);
        const auto &grab_style = theme->get_style(_grab_style.id);

        QuadsInstanceData track{};
        track.rect = _track_rect;
        track.z_order = next_depth(_track_depth_range);
        const bool track_visible = fill_quads_instance_by_style(track_style, clip_id(), track);
        if (should_emit_quads_instance(track_visible, _track_draw, ctx.emit_hit_rect))
        {
            auto hit_rect = get_rect();
            hit_rect.bounds = _track_rect;
            ctx.emit(quads_stream, _track_draw, &track, hit_rect, ctx.emit_hit_rect);
        }

        QuadsInstanceData grab{};
        grab.rect = _grab_rect.bounds;
        grab.z_order = _grab_rect.depth;
        const bool grab_visible = fill_quads_instance_by_style(grab_style, clip_id(), grab);
        if (should_emit_quads_instance(grab_visible, _grab_draw, false))
            ctx.emit(quads_stream, _grab_draw, &grab, _grab_rect, false);
    }

    bool SwitchButton::has_draw_record() const
    {
        return _track_draw.render_id != AUIK_INVALID_DRAW_DATA_ID && _grab_draw.render_id != AUIK_INVALID_DRAW_DATA_ID;
    }

    void SwitchButton::set_value(bool new_value)
    {
        if (!_value || *_value == new_value) return;
        *_value = new_value;
        sync_track_tag();
        update_style();
        rebuild_grab_layout();
        redraw_external(has_draw_record());
    }

    void SwitchButton::on_click(MouseKey key, KeyPressState state, u32 click_count)
    {
        (void)click_count;
        if (key != MouseKey::left || state != KeyPressState::press || !_value) return;
        *_value = !*_value;
        sync_track_tag();
        update_style();
        rebuild_grab_layout();
        add_render_command<detail::ClickEventTraits>(this, [this]() { redraw_external(has_draw_record()); });
    }
} // namespace auik::v2
