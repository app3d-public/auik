#include <auik/v2/pipelines.hpp>
#include <auik/v2/widgets/detail/scrollbar.hpp>

namespace auik::v2::detail
{
    static inline amal::vec4 axis_reverse_offsets(const amal::vec4 &v, amal::axis axis)
    {
        if (axis == amal::axis::x) return {v.y, v.x, v.w, v.z};
        return v;
    }

    void ScrollBehavior::set_metrics(f32 content_size, f32 view_size)
    {
        const f32 safe_view = amal::max(view_size, 0.0f);
        max_scroll_px = amal::max(content_size - safe_view, 0.0f);
        if (max_scroll_px <= 0.0f)
        {
            normalized = 0.0f;
            return;
        }
        normalized = amal::clamp(normalized, 0.0f, 1.0f);
    }

    void ScrollBehavior::set_scroll_offset(f32 offset_px)
    {
        if (max_scroll_px <= 0.0f)
        {
            normalized = 0.0f;
            return;
        }
        normalized = amal::clamp(offset_px / max_scroll_px, 0.0f, 1.0f);
    }

    bool ScrollBehavior::scroll_by_pixels(f32 delta_px)
    {
        if (max_scroll_px <= 0.0f || delta_px == 0.0f) return false;
        const f32 old = scroll_offset();
        const f32 next = amal::clamp(old + delta_px, 0.0f, max_scroll_px);
        if (next == old) return false;
        set_scroll_offset(next);
        return true;
    }

    bool Scrollbar::scroll_to_track_click(const amal::vec2 &mouse_pos)
    {
        const f32 max_scroll_px = _behavior.max_scroll();
        if (max_scroll_px <= 0.0f) return false;

        if (_behavior.axis == amal::axis::y)
        {
            const f32 track_min = position().y;
            const f32 track_len = amal::max(size().y, 1e-6f);
            const f32 t = amal::clamp((mouse_pos.y - track_min) / track_len, 0.0f, 1.0f);
            const f32 old_offset = _behavior.scroll_offset();
            _behavior.set_scroll_normalized(t);
            return _behavior.scroll_offset() != old_offset;
        }

        const f32 track_min = position().x;
        const f32 track_len = amal::max(size().x, 1e-6f);
        const f32 t = amal::clamp((mouse_pos.x - track_min) / track_len, 0.0f, 1.0f);
        const f32 old_offset = _behavior.scroll_offset();
        _behavior.set_scroll_normalized(t);
        return _behavior.scroll_offset() != old_offset;
    }

    bool Scrollbar::is_point_on_thumb(const amal::vec2 &mouse_pos) const
    {
        const auto &r = _thumb_rect;
        return mouse_pos.x >= r.position.x && mouse_pos.y >= r.position.y && mouse_pos.x < (r.position.x + r.size.x) &&
               mouse_pos.y < (r.position.y + r.size.y);
    }

    bool Scrollbar::scroll_thumb_by_drag_delta(const amal::vec2 &delta)
    {
        const f32 max_scroll_px = _behavior.max_scroll();
        if (max_scroll_px <= 0.0f) return false;

        auto *theme = get_theme();
        const auto &track_style = theme->get_style(_track_style.id);
        const auto &thumb_style = theme->get_style(_thumb_style.id);
        const amal::vec4 track_padding = axis_reverse_offsets(track_style.padding(), _behavior.axis);
        const amal::vec4 thumb_margin = axis_reverse_offsets(thumb_style.margin(), _behavior.axis);

        const f32 safe_content = amal::max(_content_size, 1.0f);
        const f32 safe_view = amal::max(_view_size, 0.0f);
        const f32 ratio = amal::clamp(safe_view / safe_content, 0.0f, 1.0f);

        f32 thumb_range = 0.0f;
        f32 delta_axis = 0.0f;
        if (_behavior.axis == amal::axis::y)
        {
            const f32 lane_h =
                amal::max(size().y - track_padding.y - track_padding.w - thumb_margin.y - thumb_margin.w, 0.0f);
            const f32 min_thumb_h = 18.0f;
            const f32 base_thumb_h = amal::max(lane_h * ratio, amal::min(min_thumb_h, lane_h));
            thumb_range = amal::max(lane_h - base_thumb_h, 0.0f);
            delta_axis = delta.y;
        }
        else
        {
            const f32 lane_w =
                amal::max(size().x - track_padding.x - track_padding.z - thumb_margin.x - thumb_margin.z, 0.0f);
            const f32 min_thumb_w = 18.0f;
            const f32 base_thumb_w = amal::max(lane_w * ratio, amal::min(min_thumb_w, lane_w));
            thumb_range = amal::max(lane_w - base_thumb_w, 0.0f);
            delta_axis = delta.x;
        }

        if (thumb_range <= 0.0f || delta_axis == 0.0f) return false;
        const f32 scroll_delta_px = delta_axis * (max_scroll_px / thumb_range);
        return _behavior.scroll_by_pixels(scroll_delta_px);
    }

    amal::vec4 Scrollbar::get_track_margin() const
    {
        auto *theme = get_theme();
        return axis_reverse_offsets(theme->get_style(_track_style.id).margin(), _behavior.axis);
    }

    f32 Scrollbar::get_min_track_thickness() const
    {
        auto *theme = get_theme();
        const auto &track_style = theme->get_style(_track_style.id);
        const auto &thumb_style = theme->get_style(_thumb_style.id);
        const amal::vec4 track_padding = axis_reverse_offsets(track_style.padding(), _behavior.axis);
        const amal::vec4 thumb_margin = axis_reverse_offsets(thumb_style.margin(), _behavior.axis);
        const amal::vec4 thumb_padding = axis_reverse_offsets(thumb_style.padding(), _behavior.axis);
        if (_behavior.axis == amal::axis::y)
        {
            const f32 desired_thumb_w = amal::max(thumb_padding.x + thumb_padding.z, 1.0f);
            return amal::max(desired_thumb_w + track_padding.x + track_padding.z + thumb_margin.x + thumb_margin.z,
                             1.0f);
        }

        const f32 desired_thumb_h = amal::max(thumb_padding.y + thumb_padding.w, 1.0f);
        return amal::max(desired_thumb_h + track_padding.y + track_padding.w + thumb_margin.y + thumb_margin.w, 1.0f);
    }

    void Scrollbar::configure(const amal::vec2 &track_pos, const amal::vec2 &track_size, f32 content_size,
                              f32 view_size)
    {
        _content_size = content_size;
        _view_size = view_size;
        set_position(track_pos);
        set_size(track_size);

        auto *theme = get_theme();
        const auto &track_style = theme->get_style(_track_style.id);
        const auto &thumb_style = theme->get_style(_thumb_style.id);
        const amal::vec4 track_padding = axis_reverse_offsets(track_style.padding(), _behavior.axis);
        const amal::vec4 thumb_margin = axis_reverse_offsets(thumb_style.margin(), _behavior.axis);
        const amal::vec4 thumb_padding = axis_reverse_offsets(thumb_style.padding(), _behavior.axis);

        const f32 safe_content = amal::max(content_size, 1.0f);
        const f32 safe_view = amal::max(view_size, 0.0f);
        const f32 ratio = amal::clamp(safe_view / safe_content, 0.0f, 1.0f);
        const f32 scroll_norm = amal::clamp(_behavior.normalized, 0.0f, 1.0f);

        if (_behavior.axis == amal::axis::y)
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

        _thumb_rect.position = {lane_pos.x + thumb_range * scroll_norm + thumb_offset_left,
                                lane_pos.y + thumb_offset_y};
        _thumb_rect.size = {thumb_w, thumb_h};
    }

    StyleUpdateFlags Scrollbar::update_style()
    {
        const u32 parent_id = parent() ? parent()->id() : 0u;
        StyleUpdateFlags out = StyleUpdateFlagBits::none;
        out |= resolve_style_selector(_track_style, id(), parent_id, StyleState::normal);
        out |= resolve_style_selector(_thumb_style, id(), parent_id, style_state());
        return out;
    }

    void Scrollbar::rebuild_clip_rects()
    {
        _thumb_rect.clip_id = clip_id();
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
        fill_quads_instance_by_style(theme->get_style(_track_style.id), clip_id(), track);
        ctx.emit(quads_stream, _track_draw_id, &track, get_rect(), ctx.emit_hit_rect);

        QuadsInstanceData thumb{};
        thumb.position = _thumb_rect.position;
        thumb.size = _thumb_rect.size;
        thumb.z_order = next_depth(depth_range());
        _thumb_rect.depth = thumb.z_order;
        fill_quads_instance_by_style(theme->get_style(_thumb_style.id), clip_id(), thumb);
        ctx.emit(quads_stream, _thumb_draw_id, &thumb, _thumb_rect, ctx.emit_hit_rect);
    }
} // namespace auik::v2::detail
