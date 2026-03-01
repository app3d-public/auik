#include <auik/v2/pipelines.hpp>
#include <auik/v2/scrollbar.hpp>

namespace auik::v2
{
    amal::vec4 Scrollbar::get_track_margin() const
    {
        auto *theme = get_theme();
        return theme->get_style(_track_style.id).margin();
    }

    f32 Scrollbar::get_min_track_width() const
    {
        auto *theme = get_theme();
        const auto &track_style = theme->get_style(_track_style.id);
        const auto &thumb_style = theme->get_style(_thumb_style.id);
        const amal::vec4 track_padding = track_style.padding();
        const amal::vec4 thumb_margin = thumb_style.margin();
        const amal::vec4 thumb_padding = thumb_style.padding();
        const f32 desired_thumb_w = amal::max(thumb_padding.x + thumb_padding.z, 1.0f);
        return amal::max(desired_thumb_w + track_padding.x + track_padding.z + thumb_margin.x + thumb_margin.z, 1.0f);
    }

    void Scrollbar::configure(const amal::vec2 &track_pos, const amal::vec2 &track_size, f32 content_size, f32 view_size)
    {
        set_position(track_pos);
        set_size(track_size);

        auto *theme = get_theme();
        const auto &track_style = theme->get_style(_track_style.id);
        const auto &thumb_style = theme->get_style(_thumb_style.id);
        const amal::vec4 track_padding = track_style.padding();
        const amal::vec4 thumb_margin = thumb_style.margin();
        const amal::vec4 thumb_padding = thumb_style.padding();

        const amal::vec2 lane_pos = {track_pos.x + track_padding.x + thumb_margin.x,
                                     track_pos.y + track_padding.y + thumb_margin.y};
        const amal::vec2 lane_size = {
            amal::max(track_size.x - track_padding.x - track_padding.z - thumb_margin.x - thumb_margin.z, 0.0f),
            amal::max(track_size.y - track_padding.y - track_padding.w - thumb_margin.y - thumb_margin.w, 0.0f)};

        const f32 safe_content = amal::max(content_size, 1.0f);
        const f32 safe_view = amal::max(view_size, 0.0f);
        const f32 ratio = amal::clamp(safe_view / safe_content, 0.0f, 1.0f);

        const f32 min_thumb_h = 18.0f;
        const f32 base_thumb_h = amal::max(lane_size.y * ratio, amal::min(min_thumb_h, lane_size.y));
        const f32 thumb_offset_top = thumb_padding.y;
        const f32 thumb_offset_bottom = thumb_padding.w;
        const f32 thumb_h = amal::max(base_thumb_h - thumb_offset_top - thumb_offset_bottom, 0.0f);
        const f32 thumb_range = amal::max(lane_size.y - base_thumb_h, 0.0f);
        const f32 scroll_norm = amal::clamp(_scroll, 0.0f, 1.0f);

        const f32 desired_thumb_w = amal::max(thumb_padding.x + thumb_padding.z, 1.0f);
        const f32 thumb_w = amal::min(desired_thumb_w, lane_size.x);
        const f32 thumb_offset_x = amal::max((lane_size.x - thumb_w) * 0.5f, 0.0f);

        _thumb_pos = {lane_pos.x + thumb_offset_x, lane_pos.y + thumb_range * scroll_norm + thumb_offset_top};
        _thumb_size = {thumb_w, thumb_h};
    }

    void Scrollbar::update_style()
    {
        auto *theme = get_theme();
        const u32 parent_id = parent() ? parent()->id() : 0u;
        _track_style.id = theme->get_resolved_style(_track_style.tag_id, id(), parent_id);
        _thumb_style.id = theme->get_resolved_style(_thumb_style.tag_id, id(), parent_id);
    }

    void Scrollbar::update_layout()
    {
        Widget::update_layout();
        // Scrollbar is an overlay and can be placed outside parent rect.
        set_required_size(size());
    }

    void Scrollbar::draw(DrawCtx &ctx)
    {
        if (!(widget_flags & WidgetFlagBits::visible)) return;
        auto *theme = get_theme();
        auto *quads_stream = get_primary_quad_stream();

        QuadsInstanceData track{};
        track.position = position();
        track.size = size();
        track.z_order = get_z_order();
        fill_quads_instance_by_style(theme->get_style(_track_style.id), track);
        set_quads_clip_rect(track, clip_rect_id());
        ctx.emit(quads_stream, _track_draw_id, &track);

        QuadsInstanceData thumb{};
        thumb.position = _thumb_pos;
        thumb.size = _thumb_size;
        thumb.z_order = next_depth(depth_data());
        fill_quads_instance_by_style(theme->get_style(_thumb_style.id), thumb);
        set_quads_clip_rect(thumb, clip_rect_id());
        ctx.emit(quads_stream, _thumb_draw_id, &thumb);
    }
} // namespace auik::v2
