#include <auik/detail/depth.hpp>
#include <auik/pipelines.hpp>
#include <auik/widgets/detail/scrollbar.hpp>

namespace auik::detail
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
            const bool changed = _behavior.scroll_offset() != old_offset;
            if (changed) update_thumb_rect();
            return changed;
        }

        const f32 track_min = position().x;
        const f32 track_len = amal::max(size().x, 1e-6f);
        const f32 t = amal::clamp((mouse_pos.x - track_min) / track_len, 0.0f, 1.0f);
        const f32 old_offset = _behavior.scroll_offset();
        _behavior.set_scroll_normalized(t);
        const bool changed = _behavior.scroll_offset() != old_offset;
        if (changed) update_thumb_rect();
        return changed;
    }

    bool Scrollbar::is_point_on_thumb(const amal::vec2 &mouse_pos) const
    {
        const auto &r = _thumb_rect.bounds;
        return mouse_pos.x >= r.offset.x && mouse_pos.y >= r.offset.y && mouse_pos.x < (r.offset.x + r.size.x) &&
               mouse_pos.y < (r.offset.y + r.size.y);
    }

    void Scrollbar::begin_thumb_drag(const amal::vec2 &mouse_pos)
    {
        _thumb_drag_grab_offset =
            (_behavior.axis == amal::axis::y) ? mouse_pos.y - _thumb_rect.bounds.offset.y
                                              : mouse_pos.x - _thumb_rect.bounds.offset.x;
    }

    bool Scrollbar::scroll_thumb_to_mouse_pos(const amal::vec2 &mouse_pos)
    {
        const f32 max_scroll_px = _behavior.max_scroll();
        if (max_scroll_px <= 0.0f) return false;

        auto *theme = get_theme();
        const auto &track_style = theme->get_style(_track_style.id);
        const auto &thumb_style = theme->get_style(_thumb_style.id);
        const amal::vec4 track_padding = axis_reverse_offsets(track_style.padding(), _behavior.axis);
        const amal::vec4 thumb_margin = axis_reverse_offsets(thumb_style.margin(), _behavior.axis);
        const amal::vec4 thumb_padding = axis_reverse_offsets(thumb_style.padding(), _behavior.axis);

        const f32 safe_content = amal::max(_content_size, 1.0f);
        const f32 safe_view = amal::max(_view_size, 0.0f);
        const f32 ratio = amal::clamp(safe_view / safe_content, 0.0f, 1.0f);
        const f32 old_offset = _behavior.scroll_offset();

        if (_behavior.axis == amal::axis::y)
        {
            const f32 lane_y = position().y + track_padding.y + thumb_margin.y;
            const f32 lane_h =
                amal::max(size().y - track_padding.y - track_padding.w - thumb_margin.y - thumb_margin.w, 0.0f);
            const f32 min_thumb_h = 18.0f;
            const f32 base_thumb_h = amal::max(lane_h * ratio, amal::min(min_thumb_h, lane_h));
            const f32 thumb_range = amal::max(lane_h - base_thumb_h, 0.0f);
            if (thumb_range <= 0.0f) return false;

            const f32 thumb_y = mouse_pos.y - _thumb_drag_grab_offset;
            const f32 normalized = (thumb_y - lane_y - thumb_padding.y) / thumb_range;
            _behavior.set_scroll_normalized(normalized);
            const bool changed = _behavior.scroll_offset() != old_offset;
            if (changed) update_thumb_rect();
            return changed;
        }

        const f32 lane_x = position().x + track_padding.x + thumb_margin.x;
        const f32 lane_w =
            amal::max(size().x - track_padding.x - track_padding.z - thumb_margin.x - thumb_margin.z, 0.0f);
        const f32 min_thumb_w = 18.0f;
        const f32 base_thumb_w = amal::max(lane_w * ratio, amal::min(min_thumb_w, lane_w));
        const f32 thumb_range = amal::max(lane_w - base_thumb_w, 0.0f);
        if (thumb_range <= 0.0f) return false;

        const f32 thumb_x = mouse_pos.x - _thumb_drag_grab_offset;
        const f32 normalized = (thumb_x - lane_x - thumb_padding.x) / thumb_range;
        _behavior.set_scroll_normalized(normalized);
        const bool changed = _behavior.scroll_offset() != old_offset;
        if (changed) update_thumb_rect();
        return changed;
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
        const bool changed = _behavior.scroll_by_pixels(scroll_delta_px);
        if (changed) update_thumb_rect();
        return changed;
    }

    void Scrollbar::set_scroll_normalized(f32 value)
    {
        const f32 old_offset = _behavior.scroll_offset();
        _behavior.set_scroll_normalized(value);
        if (_behavior.scroll_offset() != old_offset) update_thumb_rect();
    }

    void Scrollbar::set_scroll_offset(f32 offset_px)
    {
        const f32 old_offset = _behavior.scroll_offset();
        _behavior.set_scroll_offset(offset_px);
        if (_behavior.scroll_offset() != old_offset) update_thumb_rect();
    }

    bool Scrollbar::scroll_by_pixels(f32 delta_px)
    {
        const bool changed = _behavior.scroll_by_pixels(delta_px);
        if (changed) update_thumb_rect();
        return changed;
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
        _behavior.set_metrics(content_size, view_size);
        set_position(track_pos);
        set_layout_size(track_size);
        update_thumb_rect();
    }

    void Scrollbar::update_thumb_rect()
    {
        auto *theme = get_theme();
        const auto &track_style = theme->get_style(_track_style.id);
        const auto &thumb_style = theme->get_style(_thumb_style.id);
        const amal::vec4 track_padding = axis_reverse_offsets(track_style.padding(), _behavior.axis);
        const amal::vec4 thumb_margin = axis_reverse_offsets(thumb_style.margin(), _behavior.axis);
        const amal::vec4 thumb_padding = axis_reverse_offsets(thumb_style.padding(), _behavior.axis);

        const f32 safe_content = amal::max(_content_size, 1.0f);
        const f32 safe_view = amal::max(_view_size, 0.0f);
        const f32 ratio = amal::clamp(safe_view / safe_content, 0.0f, 1.0f);
        const f32 scroll_norm = amal::clamp(_behavior.normalized, 0.0f, 1.0f);

        if (_behavior.axis == amal::axis::y)
        {
            const amal::vec2 lane_pos = {position().x + track_padding.x + thumb_margin.x,
                                         position().y + track_padding.y + thumb_margin.y};
            const amal::vec2 lane_size = {
                amal::max(size().x - track_padding.x - track_padding.z - thumb_margin.x - thumb_margin.z, 0.0f),
                amal::max(size().y - track_padding.y - track_padding.w - thumb_margin.y - thumb_margin.w, 0.0f)};

            const f32 min_thumb_h = 18.0f;
            const f32 base_thumb_h = amal::max(lane_size.y * ratio, amal::min(min_thumb_h, lane_size.y));
            const f32 thumb_offset_top = thumb_padding.y;
            const f32 thumb_offset_bottom = thumb_padding.w;
            const f32 thumb_h = amal::max(base_thumb_h - thumb_offset_top - thumb_offset_bottom, 0.0f);
            const f32 thumb_range = amal::max(lane_size.y - base_thumb_h, 0.0f);

            const f32 desired_thumb_w = amal::max(thumb_padding.x + thumb_padding.z, 1.0f);
            const f32 thumb_w = amal::min(desired_thumb_w, lane_size.x);
            const f32 thumb_offset_x = amal::max((lane_size.x - thumb_w) * 0.5f, 0.0f);

            _thumb_rect.bounds = {lane_pos.x + thumb_offset_x,
                                  lane_pos.y + thumb_range * scroll_norm + thumb_offset_top, thumb_w, thumb_h};
            _thumb_rect.clip_id = clip_id();
            return;
        }

        const amal::vec2 lane_pos = {position().x + track_padding.x + thumb_margin.x,
                                     position().y + track_padding.y + thumb_margin.y};
        const amal::vec2 lane_size = {
            amal::max(size().x - track_padding.x - track_padding.z - thumb_margin.x - thumb_margin.z, 0.0f),
            amal::max(size().y - track_padding.y - track_padding.w - thumb_margin.y - thumb_margin.w, 0.0f)};
        _thumb_rect.clip_id = clip_id();

        const f32 min_thumb_w = 18.0f;
        const f32 base_thumb_w = amal::max(lane_size.x * ratio, amal::min(min_thumb_w, lane_size.x));
        const f32 thumb_offset_left = thumb_padding.x;
        const f32 thumb_offset_right = thumb_padding.z;
        const f32 thumb_w = amal::max(base_thumb_w - thumb_offset_left - thumb_offset_right, 0.0f);
        const f32 thumb_range = amal::max(lane_size.x - base_thumb_w, 0.0f);

        const f32 desired_thumb_h = amal::max(thumb_padding.y + thumb_padding.w, 1.0f);
        const f32 thumb_h = amal::min(desired_thumb_h, lane_size.y);
        const f32 thumb_offset_y = amal::max((lane_size.y - thumb_h) * 0.5f, 0.0f);

        _thumb_rect.bounds = {lane_pos.x + thumb_range * scroll_norm + thumb_offset_left, lane_pos.y + thumb_offset_y,
                              thumb_w, thumb_h};
    }

    StyleUpdateFlags Scrollbar::update_style()
    {
        const u32 parent_id = parent() ? parent()->id() : 0u;
        StyleUpdateFlags out = StyleUpdateFlagBits::none;
        const auto transition = detail::get_widget_style_selector_transition(parent_id);
        StyleState track_state = StyleState::normal;
        StyleState thumb_state = StyleState::normal;
        if (transition.current_id.tag_id == _rect.id.tag_id) track_state = transition.current_state;
        else if (transition.current_id.tag_id == _thumb_rect.id.tag_id) thumb_state = transition.current_state;
        out |= resolve_style_selector(_track_style, _rect.id.tag_id, parent_id, track_state);
        out |= resolve_style_selector(_thumb_style, _thumb_rect.id.tag_id, parent_id, thumb_state);
        return out;
    }

    void Scrollbar::translate(const amal::vec2 &delta)
    {
        if (delta.x == 0.0f && delta.y == 0.0f) return;
        Widget::translate(delta);
        _thumb_rect.bounds.offset += delta;
    }

    void Scrollbar::rebuild_clip_rects()
    {
        _thumb_rect.clip_id = clip_id();
        DrawDataID *hit_ids[] = {&_track_draw_id, &_thumb_draw_id};
        invalidate_hit_rect_batch(hit_ids, 2);
    }

    void Scrollbar::back_hit_depth()
    {
        Widget::back_hit_depth();
        _thumb_rect.hit_depth = get_root_depth_zone_range(DepthZone::background).x;
        get_context().dirty_flags |= DirtyFlagBits::hit_rect_update;
    }

    void Scrollbar::restore_hit_depth()
    {
        Widget::restore_hit_depth();
        _thumb_rect.hit_depth = _thumb_rect.depth;
        get_context().dirty_flags |= DirtyFlagBits::hit_rect_update;
    }

    void Scrollbar::draw(DrawCtx &ctx)
    {
        if (!(widget_flags & WidgetFlagBits::visible)) return;
        auto *theme = get_theme();
        auto *quads_stream = get_primary_quads_stream();

        QuadsInstanceData track{};
        track.rect = bounds();
        track.z_order = get_z_order();
        const bool track_visible = fill_quads_instance_by_style(theme->get_style(_track_style.id), clip_id(), track);
        emit_quads_instance(ctx, quads_stream, _track_draw_id, track, get_rect(), track_visible, can_emit_hit(ctx));

        QuadsInstanceData thumb{};
        thumb.rect = _thumb_rect.bounds;
        thumb.z_order = next_depth(depth_range());
        _thumb_rect.depth = thumb.z_order;
        _thumb_rect.hit_depth = _thumb_rect.depth;
        _thumb_rect.clip_id = clip_id();
        const bool thumb_visible = fill_quads_instance_by_style(theme->get_style(_thumb_style.id), clip_id(), thumb);
        emit_quads_instance(ctx, quads_stream, _thumb_draw_id, thumb, _thumb_rect, thumb_visible, can_emit_hit(ctx));
    }

    bool Scrollbar::has_draw_record() const
    {
        if (!is_visible()) return true;
        return _track_draw_id.render_id != AUIK_INVALID_DRAW_DATA_ID &&
               _thumb_draw_id.render_id != AUIK_INVALID_DRAW_DATA_ID;
    }
} // namespace auik::detail
