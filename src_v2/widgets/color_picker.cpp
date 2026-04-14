#include <amal/trigonometric.hpp>
#include <auik/v2/auik.hpp>
#include <auik/v2/detail/depth.hpp>
#include <auik/v2/widgets/color_picker.hpp>

#define AUIK_CIRCLE_WHEEL_FRINGE                  2.0f
#define AUIK_CIRCLE_TESSELLATION_MAX_ERROR         0.30f
#define AUIK_CIRCLE_FRINGE_TESSELLATION_MAX_ERROR 0.15f
#define AUIK_CIRCLE_SEGMENTS_MIN                  4u
#define AUIK_CIRCLE_SEGMENTS_MAX                  512u

namespace auik::v2
{
    namespace detail
    {
        static inline u32 calc_circle_auto_segment_count(f32 radius, f32 max_error = AUIK_CIRCLE_TESSELLATION_MAX_ERROR)
        {
            const f32 r = amal::max(radius, 1e-5f);
            const f32 err = amal::min(max_error, r);
            const f32 arg = amal::clamp(1.0f - err / r, -1.0f, 1.0f);
            const f32 angle_step = amal::acos(arg);
            if (angle_step <= 1e-5f) return AUIK_CIRCLE_SEGMENTS_MAX;

            u32 segments = static_cast<u32>(amal::ceil(amal::pi<f32>() / angle_step));
            if ((segments & 1u) != 0u) ++segments;
            return amal::clamp(segments, AUIK_CIRCLE_SEGMENTS_MIN, AUIK_CIRCLE_SEGMENTS_MAX);
        }

        static inline void fill_circle_grab_instance(const Style &style, const amal::rect &rect, f32 z_order,
                                                     u16 clip_id, const amal::vec4 &background_color,
                                                     QuadsInstanceData &data)
        {
            data.rect = rect;
            data.z_order = z_order;
            fill_quads_instance_by_style(style, clip_id, data);
            data.background_color = pack_rgba8(background_color);
            data.border_color = pack_rgba8(0, 0, 0, 255);
            data.border_thickness = style.has_visible_border() ? style.border_thickness() : 1.0f;
            data.border_radius = amal::max(0.0f, amal::min(rect.size.x, rect.size.y) * 0.5f);
            data.mask |= (static_cast<u32>(AUIK_HAS_BORDER_BIT) << 20u);
        }

        static inline StyleState resolve_grab_visual_state(StyleState state)
        {
            return (state == StyleState::active || state == StyleState::focus) ? state : StyleState::normal;
        }

        static inline amal::vec4 make_circle_color(f32 hue_deg, f32 radius_t)
        {
            const amal::vec4 white = {1.0f, 1.0f, 1.0f, 1.0f};
            const amal::vec4 hue = amal::hsl_to_rgba(hue_deg, 1.0f, 0.5f);
            const f32 t = amal::clamp(radius_t, 0.0f, 1.0f);
            return {white.x + (hue.x - white.x) * t, white.y + (hue.y - white.y) * t, white.z + (hue.z - white.z) * t,
                    1.0f};
        }

        static inline void append_ring_strip_indices(acul::vector<u32> &indices, u32 inner_start, u32 inner_segments,
                                                     u32 outer_start, u32 outer_segments)
        {
            u32 inner_i = 0u;
            u32 outer_i = 0u;
            while (inner_i < inner_segments || outer_i < outer_segments)
            {
                const f32 next_inner_t =
                    inner_i < inner_segments ? static_cast<f32>(inner_i + 1u) / static_cast<f32>(inner_segments) : 1.0f;
                const f32 next_outer_t =
                    outer_i < outer_segments ? static_cast<f32>(outer_i + 1u) / static_cast<f32>(outer_segments) : 1.0f;

                const u32 inner0 = inner_start + inner_i;
                const u32 outer0 = outer_start + outer_i;

                if (outer_i >= outer_segments || (inner_i < inner_segments && next_inner_t < next_outer_t - 1e-6f))
                {
                    const u32 inner1 = inner_start + inner_i + 1u;
                    indices.push_back(inner0);
                    indices.push_back(outer0);
                    indices.push_back(inner1);
                    ++inner_i;
                }
                else if (inner_i >= inner_segments || next_outer_t < next_inner_t - 1e-6f)
                {
                    const u32 outer1 = outer_start + outer_i + 1u;
                    indices.push_back(inner0);
                    indices.push_back(outer0);
                    indices.push_back(outer1);
                    ++outer_i;
                }
                else
                {
                    const u32 inner1 = inner_start + inner_i + 1u;
                    const u32 outer1 = outer_start + outer_i + 1u;
                    indices.push_back(inner0);
                    indices.push_back(outer0);
                    indices.push_back(inner1);
                    indices.push_back(inner1);
                    indices.push_back(outer0);
                    indices.push_back(outer1);
                    ++inner_i;
                    ++outer_i;
                }
            }
        }
    } // namespace detail

    CircleColorPicker::CircleColorPicker(u32 id, amal::vec4 *value, f32 diameter, WidgetFlags widget_flags,
                                         Widget *parent)
        : Widget(id, widget_flags, EventFlagBits::click | EventFlagBits::drag, parent,
                 {{0.0f, 0.0f}, {diameter, diameter}}, AUIK_TAG_CIRCLE_COLOR_PICKER),
          _value(value)
    {
        _grab_hit_rect = detail::make_rect_data(id, AUIK_TAG_CIRCLE_COLOR_PICKER_GRAB);
        _wheel_vertices.reserve(256);
        _wheel_indices.reserve(768);
        if (_value) set_color(*_value);
        else set_hue_radius(0.0f, 0.0f);
    }

    void CircleColorPicker::sync_batch()
    {
        _wheel_batch.vertices = _wheel_vertices.empty() ? nullptr : _wheel_vertices.data();
        _wheel_batch.indices = _wheel_indices.empty() ? nullptr : _wheel_indices.data();
        _wheel_batch.vertex_count = static_cast<u32>(_wheel_vertices.size());
        _wheel_batch.index_count = static_cast<u32>(_wheel_indices.size());
    }

    void CircleColorPicker::set_hue_radius(f32 hue_deg, f32 radius_t)
    {
        f32 hue = std::fmod(hue_deg, 360.0f);
        if (hue < 0.0f) hue += 360.0f;
        _hue_deg = hue;
        _radius_t = amal::clamp(radius_t, 0.0f, 1.0f);
        _resolved_color = detail::make_circle_color(_hue_deg, _radius_t);
        if (_value) *_value = _resolved_color;
    }

    void CircleColorPicker::set_color(const amal::vec4 &color)
    {
        const f32 r = amal::clamp(color.x, 0.0f, 1.0f);
        const f32 g = amal::clamp(color.y, 0.0f, 1.0f);
        const f32 b = amal::clamp(color.z, 0.0f, 1.0f);
        const f32 max_c = amal::max(r, amal::max(g, b));
        const f32 min_c = amal::min(r, amal::min(g, b));
        const f32 delta = max_c - min_c;

        f32 hue = _hue_deg;
        if (delta > 1e-6f)
        {
            if (max_c == r) hue = 60.0f * std::fmod(((g - b) / delta), 6.0f);
            else if (max_c == g) hue = 60.0f * (((b - r) / delta) + 2.0f);
            else hue = 60.0f * (((r - g) / delta) + 4.0f);
            if (hue < 0.0f) hue += 360.0f;
        }
        const f32 radius_t = amal::clamp(1.0f - min_c, 0.0f, 1.0f);
        set_hue_radius(hue, radius_t);
    }

    StyleUpdateFlags CircleColorPicker::update_style()
    {
        const u32 parent_id = parent() ? parent()->id() : 0u;
        StyleUpdateFlags out = StyleUpdateFlagBits::none;
        bool grab_changed = false;
        auto resolve_grab_state = [&](StyleState state) -> StyleUpdateFlags {
            const auto flags = resolve_style_selector(_grab_style, _grab_style.tag_id, parent_id,
                                                      detail::resolve_grab_visual_state(state));
            if (flags & StyleUpdateFlagBits::redraw) grab_changed = true;
            return flags;
        };

        if (_grab_style.id == Theme::STYLE_ID_INVALID) out |= resolve_grab_state(StyleState::normal);

        const auto transition = detail::get_widget_style_selector_transition(id());
        if (transition.prev_tag_id == _grab_hit_rect.tag_id &&
            (transition.current_tag_id != _grab_hit_rect.tag_id || transition.prev_state != transition.current_state))
            out |= resolve_grab_state(StyleState::normal);
        if (transition.current_tag_id == _grab_hit_rect.tag_id) out |= resolve_grab_state(transition.current_state);

        const StyleState widget_grab_state = detail::resolve_grab_visual_state(style_state());
        if (widget_grab_state == StyleState::active || widget_grab_state == StyleState::focus)
            out |= resolve_grab_state(widget_grab_state);

        if (grab_changed) rebuild_grab_visual();
        return out;
    }

    void CircleColorPicker::update_layout_min_size()
    {
        amal::vec2 min_size = size();
        const f32 min_side = 160.0f;
        if (!is_fixed()) min_size.x = 0.0f;
        else if (min_size.x <= 0.0f) min_size.x = min_side;
        if (min_size.y <= 0.0f) min_size.y = min_size.x > 0.0f ? min_size.x : min_side;
        const f32 side = amal::max(min_size.x, min_size.y);
        set_required_size({side, side});
    }

    void CircleColorPicker::update_layout(bool min_size_known)
    {
        if (!min_size_known) update_layout_min_size();
        const amal::vec2 cursor = detail::get_context().screen_cursor;
        const amal::vec2 min_required = required_size();

        f32 side = size().x;
        if (side <= 0.0f) side = min_required.x;
        if (size().y > 0.0f) side = amal::min(side, size().y);
        side = amal::max(side, amal::max(min_required.x, min_required.y));

        set_position(cursor);
        set_size({side, side});
        Widget::update_layout(true);
        inherit_parent_content_clip_rect();
        rebuild_cached_visuals();
        detail::get_context().screen_cursor = {cursor.x, cursor.y + side};
    }

    void CircleColorPicker::translate(const amal::vec2 &delta)
    {
        if (delta.x == 0.0f && delta.y == 0.0f) return;
        Widget::translate(delta);
        detail::get_context().dirty_flags |= DirtyFlagBits::hit_rect_update;
        rebuild_cached_visuals();
    }

    void CircleColorPicker::rebuild_clip_rects()
    {
        inherit_parent_content_clip_rect();
        _wheel_draw_id.hit_id = AUIK_INVALID_DRAW_DATA_ID;
        _grab_draw_id.hit_id = AUIK_INVALID_DRAW_DATA_ID;
        _grab_back_draw_id.hit_id = AUIK_INVALID_DRAW_DATA_ID;
        _grab_hit_rect.clip_id = clip_id();
        rebuild_cached_visuals();
    }

    void CircleColorPicker::update_depth(const amal::vec2 &depth_range)
    {
        Widget::update_depth(depth_range);
        assign_next_depth(this->depth_range(), _track_depth_range);
        assign_next_depth(_track_depth_range, _grab_depth_range);
        rebuild_cached_visuals();
    }

    void CircleColorPicker::draw(DrawCtx &ctx)
    {
        auto *vertex_stream = get_primary_vertex_stream();
        auto *overlay_stream = get_overlay_quads_stream();
        bool hit_pending = ctx.emit_hit_rect;

        if (_wheel_batch.vertex_count > 0 && _wheel_batch.index_count > 0)
        {
            ctx.emit(vertex_stream, _wheel_draw_id, &_wheel_batch, get_rect(), hit_pending);
            hit_pending = false;
        }
        if (_grab_back_visual.rect.size.x > 0.0f && _grab_back_visual.rect.size.y > 0.0f)
            ctx.emit(overlay_stream, _grab_back_draw_id, &_grab_back_visual, _grab_hit_rect, false);
        ctx.emit(overlay_stream, _grab_draw_id, &_grab_visual, _grab_hit_rect, ctx.emit_hit_rect);
    }

    bool CircleColorPicker::has_draw_record() const
    {
        if (_wheel_batch.vertex_count > 0 && _wheel_draw_id.render_id == AUIK_INVALID_DRAW_DATA_ID) return false;
        if (_grab_draw_id.render_id == AUIK_INVALID_DRAW_DATA_ID) return false;
        if (_grab_back_visual.rect.size.x > 0.0f && _grab_back_visual.rect.size.y > 0.0f &&
            _grab_back_draw_id.render_id == AUIK_INVALID_DRAW_DATA_ID)
            return false;
        return true;
    }

    amal::vec2 CircleColorPicker::resolve_grab_size(const Style &grab_style)
    {
        const amal::vec4 grab_padding = grab_style.padding();
        return {amal::max(grab_padding.x + grab_padding.z, 1.0f), amal::max(grab_padding.y + grab_padding.w, 1.0f)};
    }

    void CircleColorPicker::rebuild_wheel_visual()
    {
        _wheel_vertices.clear();
        _wheel_indices.clear();
        sync_batch();

        const amal::vec2 wheel_size = size();
        const f32 wheel_radius_outer = amal::max(amal::min(wheel_size.x, wheel_size.y) * 0.5f, 0.0f);
        const f32 wheel_radius = amal::max(wheel_radius_outer - AUIK_CIRCLE_WHEEL_FRINGE, 0.0f);
        if (wheel_radius <= 1e-4f) return;

        const amal::vec2 center = position() + wheel_size * 0.5f;
        const u32 segments = detail::calc_circle_auto_segment_count(wheel_radius_outer);
        const u32 fringe_segments =
            detail::calc_circle_auto_segment_count(wheel_radius_outer, AUIK_CIRCLE_FRINGE_TESSELLATION_MAX_ERROR);
        constexpr bool use_fringe_aa = true;

        _wheel_vertices.reserve(1u + (segments + 1u) + (use_fringe_aa ? (fringe_segments + 1u) * 2u : 0u));
        _wheel_indices.reserve(segments * 3u + (use_fringe_aa ? (segments + fringe_segments) * 6u : 0u) +
                               (use_fringe_aa ? fringe_segments * 6u : 0u));

        VertexStreamVertex center_v{};
        center_v.position = center;
        center_v.z_order = detail::mid_depth(_track_depth_range);
        center_v.color = detail::pack_rgba8(255, 255, 255, 255);
        center_v.clip_id = clip_id();
        _wheel_vertices.push_back(center_v);

        const f32 two_pi = amal::pi<f32>() * 2.0f;
        const f32 fringe_mid_radius = wheel_radius_outer - AUIK_CIRCLE_WHEEL_FRINGE * 0.5f;
        const u32 color_start = 1u;
        const u32 fringe_mid_start = color_start + (segments + 1u);
        const u32 fringe_outer_start = fringe_mid_start + (fringe_segments + 1u);
        for (u32 i = 0; i <= segments; ++i)
        {
            const f32 t = static_cast<f32>(i) / static_cast<f32>(segments);
            const f32 angle = t * two_pi;
            const f32 hue_deg = t * 360.0f;
            const amal::vec4 hue_color = amal::hsl_to_rgba(hue_deg, 1.0f, 0.5f);

            VertexStreamVertex inner{};
            inner.position = {center.x + amal::cos(angle) * wheel_radius, center.y + amal::sin(angle) * wheel_radius};
            inner.z_order = detail::mid_depth(_track_depth_range);
            inner.color = detail::pack_rgba8(hue_color);
            inner.clip_id = clip_id();
            _wheel_vertices.push_back(inner);
        }

        if (use_fringe_aa)
        {
            for (u32 i = 0; i <= fringe_segments; ++i)
            {
                const f32 t = static_cast<f32>(i) / static_cast<f32>(fringe_segments);
                const f32 angle = t * two_pi;
                const f32 hue_deg = t * 360.0f;
                const amal::vec4 hue_color = amal::hsl_to_rgba(hue_deg, 1.0f, 0.5f);

                VertexStreamVertex mid{};
                mid.position = {center.x + amal::cos(angle) * fringe_mid_radius,
                                center.y + amal::sin(angle) * fringe_mid_radius};
                mid.z_order = detail::mid_depth(_track_depth_range);
                mid.color = detail::pack_rgba8(amal::vec4{hue_color.x, hue_color.y, hue_color.z, 0.35f});
                mid.clip_id = clip_id();
                _wheel_vertices.push_back(mid);
            }

            for (u32 i = 0; i <= fringe_segments; ++i)
            {
                const f32 t = static_cast<f32>(i) / static_cast<f32>(fringe_segments);
                const f32 angle = t * two_pi;
                const f32 hue_deg = t * 360.0f;
                const amal::vec4 hue_color = amal::hsl_to_rgba(hue_deg, 1.0f, 0.5f);

                VertexStreamVertex outer{};
                outer.position = {center.x + amal::cos(angle) * wheel_radius_outer,
                                  center.y + amal::sin(angle) * wheel_radius_outer};
                outer.z_order = detail::mid_depth(_track_depth_range);
                outer.color = detail::pack_rgba8(amal::vec4{hue_color.x, hue_color.y, hue_color.z, 0.0f});
                outer.clip_id = clip_id();
                _wheel_vertices.push_back(outer);
            }
        }

        for (u32 i = 1u; i <= segments; ++i)
        {
            _wheel_indices.push_back(0u);
            _wheel_indices.push_back(color_start + (i - 1u));
            _wheel_indices.push_back(color_start + i);
        }

        if (use_fringe_aa)
        {
            detail::append_ring_strip_indices(_wheel_indices, color_start, segments, fringe_mid_start, fringe_segments);

            for (u32 i = 0u; i < fringe_segments; ++i)
            {
                const u32 mid0 = fringe_mid_start + i;
                const u32 mid1 = fringe_mid_start + i + 1u;
                const u32 out0 = fringe_outer_start + i;
                const u32 out1 = fringe_outer_start + i + 1u;
                _wheel_indices.push_back(mid0);
                _wheel_indices.push_back(out0);
                _wheel_indices.push_back(mid1);
                _wheel_indices.push_back(mid1);
                _wheel_indices.push_back(out0);
                _wheel_indices.push_back(out1);
            }
        }
        sync_batch();
    }

    void CircleColorPicker::rebuild_grab_visual()
    {
        if (_grab_style.id == Theme::STYLE_ID_INVALID) return;
        const auto &grab_style = get_theme()->get_style(_grab_style.id);
        const amal::vec2 grab_size = resolve_grab_size(grab_style);
        const f32 grab_w = amal::max(amal::round(grab_size.x), 3.0f);
        const f32 grab_h = amal::max(amal::round(grab_size.y), 3.0f);

        const amal::vec2 wheel_size = size();
        const amal::vec2 center = position() + wheel_size * 0.5f;
        const f32 wheel_radius =
            amal::max(amal::min(wheel_size.x, wheel_size.y) * 0.5f - AUIK_CIRCLE_WHEEL_FRINGE, 0.0f);
        const f32 angle = (_hue_deg / 180.0f) * amal::pi<f32>();
        const f32 pick_r = wheel_radius * amal::clamp(_radius_t, 0.0f, 1.0f);
        const amal::vec2 pick_pos = {center.x + amal::cos(angle) * pick_r, center.y + amal::sin(angle) * pick_r};

        _grab_hit_rect.bounds.offset = {pick_pos.x - grab_w * 0.5f, pick_pos.y - grab_h * 0.5f};
        _grab_hit_rect.bounds.size = {grab_w, grab_h};
        _grab_hit_rect.depth = detail::mid_depth(_grab_depth_range);
        _grab_hit_rect.clip_id = parent() ? parent()->clip_id() : clip_id();

        const u16 grab_clip = parent() ? parent()->clip_id() : clip_id();
        const f32 grab_z = detail::mid_depth(_grab_depth_range);
        _grab_visual = {};
        _grab_back_visual = {};
        detail::fill_circle_grab_instance(grab_style, _grab_hit_rect.bounds, grab_z, grab_clip, _resolved_color,
                                          _grab_visual);

        if (const Style *border_style = get_theme()->get_desc_style(AUIK_TAG_GRADIENT_SLIDER_GRAB_BORDER))
        {
            const amal::vec2 border_size = resolve_grab_size(*border_style);
            const f32 border_w = amal::max(amal::round(border_size.x), _grab_hit_rect.bounds.size.x);
            const f32 border_h = amal::max(amal::round(border_size.y), _grab_hit_rect.bounds.size.y);
            amal::rect border_rect{};
            border_rect.size = {border_w, border_h};
            border_rect.offset = {pick_pos.x - border_w * 0.5f, pick_pos.y - border_h * 0.5f};
            detail::fill_circle_grab_instance(*border_style, border_rect, grab_z, grab_clip,
                                              border_style->background_color(), _grab_back_visual);
        }
    }

    void CircleColorPicker::rebuild_cached_visuals()
    {
        rebuild_wheel_visual();
        rebuild_grab_visual();
    }

    void CircleColorPicker::update_value_from_mouse()
    {
        const amal::vec2 wheel_size = size();
        const amal::vec2 center = position() + wheel_size * 0.5f;
        const amal::vec2 mouse = get_mouse_pos();
        const f32 dx = mouse.x - center.x;
        const f32 dy = mouse.y - center.y;
        const f32 wheel_radius =
            amal::max(amal::min(wheel_size.x, wheel_size.y) * 0.5f - AUIK_CIRCLE_WHEEL_FRINGE, 1e-5f);
        const f32 dist = amal::sqrt(dx * dx + dy * dy);
        f32 hue = _hue_deg;
        if (dist > 1e-5f)
        {
            hue = amal::atan2(dy, dx) * (180.0f / amal::pi<f32>());
            if (hue < 0.0f) hue += 360.0f;
        }
        const f32 radius_t = amal::clamp(dist / wheel_radius, 0.0f, 1.0f);
        set_hue_radius(hue, radius_t);
        detail::get_context().dirty_flags |= DirtyFlagBits::hit_rect_update;
        rebuild_grab_visual();
    }

    void CircleColorPicker::on_click(MouseKey key, KeyPressState state, u32 click_count)
    {
        (void)click_count;
        if (key != MouseKey::left) return;
        if (state == KeyPressState::press)
        {
            update_value_from_mouse();
            add_render_command<detail::ClickEventTraits>(this, [this]() { redraw_external(has_draw_record()); });
            return;
        }
        if (state == KeyPressState::release)
            add_render_command<detail::ClickEventTraits>(this, [this]() { redraw_external(has_draw_record()); });
    }

    void CircleColorPicker::on_drag(const amal::vec2 &delta, KeyPressState state)
    {
        (void)delta;
        if (state == KeyPressState::press)
        {
            _drag_started = true;
            update_value_from_mouse();
        }
        else if (state == KeyPressState::repeat)
        {
            if (!_drag_started) _drag_started = true;
            update_value_from_mouse();
        }
        else if (state == KeyPressState::release) _drag_started = false;
        else return;

        add_render_command<detail::DragEventTraits>(this, [this]() { redraw_external(has_draw_record()); });
    }
} // namespace auik::v2
