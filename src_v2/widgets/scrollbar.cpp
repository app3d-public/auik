#include <auik/v2/pipelines.hpp>
#include <auik/v2/scrollbar.hpp>

namespace auik::v2
{
    void Scrollbar::set_metrics(f32 content_size, f32 view_size)
    {
        const f32 safe_view = amal::max(view_size, 0.0f);
        _max_scroll = amal::max(content_size - safe_view, 0.0f);
        if (_max_scroll <= 0.0f)
        {
            _scroll = 0.0f;
            return;
        }
        _scroll = amal::clamp(_scroll, 0.0f, 1.0f);
    }

    void Scrollbar::set_scroll_offset(f32 offset_px)
    {
        if (_max_scroll <= 0.0f)
        {
            _scroll = 0.0f;
            return;
        }
        _scroll = amal::clamp(offset_px / _max_scroll, 0.0f, 1.0f);
    }

    bool Scrollbar::scroll_by_pixels(f32 delta_px)
    {
        if (_max_scroll <= 0.0f || delta_px == 0.0f) return false;
        const f32 old = scroll_offset();
        const f32 next = amal::clamp(old + delta_px, 0.0f, _max_scroll);
        if (next == old) return false;
        set_scroll_offset(next);
        return true;
    }

    amal::vec4 Scrollbar::get_track_margin() const
    {
        auto *theme = get_theme();
        return theme->get_style(_track_style.id).margin();
    }

    f32 Scrollbar::get_min_track_thickness() const
    {
        auto *theme = get_theme();
        const auto &track_style = theme->get_style(_track_style.id);
        const auto &thumb_style = theme->get_style(_thumb_style.id);
        const amal::vec4 track_padding = track_style.padding();
        const amal::vec4 thumb_margin = thumb_style.margin();
        const amal::vec4 thumb_padding = thumb_style.padding();
        if (_axis == amal::axis::y)
        {
            const f32 desired_thumb_w = amal::max(thumb_padding.x + thumb_padding.z, 1.0f);
            return amal::max(desired_thumb_w + track_padding.x + track_padding.z + thumb_margin.x + thumb_margin.z,
                             1.0f);
        }

        const f32 desired_thumb_h = amal::max(thumb_padding.y + thumb_padding.w, 1.0f);
        return amal::max(desired_thumb_h + track_padding.y + track_padding.w + thumb_margin.y + thumb_margin.w, 1.0f);
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

        const f32 safe_content = amal::max(content_size, 1.0f);
        const f32 safe_view = amal::max(view_size, 0.0f);
        const f32 ratio = amal::clamp(safe_view / safe_content, 0.0f, 1.0f);
        const f32 scroll_norm = amal::clamp(_scroll, 0.0f, 1.0f);

        if (_axis == amal::axis::y)
        {
            const amal::vec2 lane_pos = {track_pos.x + track_padding.x + thumb_margin.x,
                                         track_pos.y + track_padding.y + thumb_margin.y};
            const amal::vec2 lane_size = {
                amal::max(track_size.x - track_padding.x - track_padding.z - thumb_margin.x - thumb_margin.z, 0.0f),
                amal::max(track_size.y - track_padding.y - track_padding.w - thumb_margin.y - thumb_margin.w, 0.0f)};

            const f32 min_thumb_h = 18.0f;
            const f32 base_thumb_h = amal::max(lane_size.y * ratio, amal::min(min_thumb_h, lane_size.y));
            const f32 thumb_offset_top = thumb_padding.y;
            const f32 thumb_offset_bottom = thumb_padding.w;
            const f32 thumb_h = amal::max(base_thumb_h - thumb_offset_top - thumb_offset_bottom, 0.0f);
            const f32 thumb_range = amal::max(lane_size.y - base_thumb_h, 0.0f);

            const f32 desired_thumb_w = amal::max(thumb_padding.x + thumb_padding.z, 1.0f);
            const f32 thumb_w = amal::min(desired_thumb_w, lane_size.x);
            const f32 thumb_offset_x = amal::max((lane_size.x - thumb_w) * 0.5f, 0.0f);

            _thumb_rect.position = {lane_pos.x + thumb_offset_x,
                                    lane_pos.y + thumb_range * scroll_norm + thumb_offset_top};
            _thumb_rect.size = {thumb_w, thumb_h};
            return;
        }

        const amal::vec2 lane_pos = {track_pos.x + track_padding.x + thumb_margin.x,
                                     track_pos.y + track_padding.y + thumb_margin.y};
        const amal::vec2 lane_size = {
            amal::max(track_size.x - track_padding.x - track_padding.z - thumb_margin.x - thumb_margin.z, 0.0f),
            amal::max(track_size.y - track_padding.y - track_padding.w - thumb_margin.y - thumb_margin.w, 0.0f)};

        const f32 min_thumb_w = 18.0f;
        const f32 base_thumb_w = amal::max(lane_size.x * ratio, amal::min(min_thumb_w, lane_size.x));
        const f32 thumb_offset_left = thumb_padding.x;
        const f32 thumb_offset_right = thumb_padding.z;
        const f32 thumb_w = amal::max(base_thumb_w - thumb_offset_left - thumb_offset_right, 0.0f);
        const f32 thumb_range = amal::max(lane_size.x - base_thumb_w, 0.0f);

        const f32 desired_thumb_h = amal::max(thumb_padding.y + thumb_padding.w, 1.0f);
        const f32 thumb_h = amal::min(desired_thumb_h, lane_size.y);
        const f32 thumb_offset_y = amal::max((lane_size.y - thumb_h) * 0.5f, 0.0f);

        _thumb_rect.position = {lane_pos.x + thumb_range * scroll_norm + thumb_offset_left, lane_pos.y + thumb_offset_y};
        _thumb_rect.size = {thumb_w, thumb_h};
    }

    void Scrollbar::update_style()
    {
        auto *theme = get_theme();
        const u32 parent_id = parent() ? parent()->id() : 0u;
        _track_style.id = theme->get_resolved_style(_track_style.tag_id, id(), parent_id);
        _thumb_style.id = theme->get_resolved_style(_thumb_style.tag_id, id(), parent_id);
    }

    void Scrollbar::rebuild_clip_rects()
    {
        _thumb_rect.clip_rect_id = clip_rect_id();
        _track_draw_id.hit_id = AUIK_INVALID_DRAW_DATA_ID;
        _thumb_draw_id.hit_id = AUIK_INVALID_DRAW_DATA_ID;
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
        fill_quads_instance_by_style(theme->get_style(_track_style.id), clip_rect_id(), track);
        ctx.emit(quads_stream, _track_draw_id, &track, get_rect());

        QuadsInstanceData thumb{};
        thumb.position = _thumb_rect.position;
        thumb.size = _thumb_rect.size;
        thumb.z_order = next_depth(depth_range());
        _thumb_rect.depth = thumb.z_order;
        fill_quads_instance_by_style(theme->get_style(_thumb_style.id), clip_rect_id(), thumb);
        ctx.emit(quads_stream, _thumb_draw_id, &thumb, _thumb_rect);
    }
} // namespace auik::v2
