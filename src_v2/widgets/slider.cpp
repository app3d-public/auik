#include <auik/v2/auik.hpp>
#include <auik/v2/detail/depth.hpp>
#include <auik/v2/widgets/slider.hpp>

#define SLIDER_TRACK_HIT_PAD 4.0f

namespace auik::v2
{
    namespace detail
    {
        static inline void fill_border_only_instance(const Style &style, const amal::rect &rect, f32 z_order,
                                                     u16 clip_id, QuadsInstanceData &data)
        {
            data.rect = rect;
            data.background_color = pack_rgba8(0, 0, 0, 0);
            data.border_color = style.border_color();
            data.border_radius = style.border_radius();
            data.border_thickness = style.has_visible_border() ? style.border_thickness() : 0.0f;
            data.z_order = z_order;
            u32 flags = 0u;
            if (data.border_thickness > 0.0f) flags |= AUIK_HAS_BORDER_BIT;
            if (data.border_radius > 0.0f) flags |= AUIK_HAS_RADIUS_BIT;
            data.mask = static_cast<u32>(clip_id) | ((style.corner_mask() & 0xFu) << 16u) | (flags << 20u);
        }

        static inline amal::rect resolve_slider_track_rect(const amal::rect &bounds, const Style &track_style)
        {
            const amal::vec4 track_padding = track_style.padding();
            const f32 min_track_h =
                amal::max(6.0f, track_style.border_radius() > 0.0f ? track_style.border_radius() * 2.0f : 0.0f);
            const f32 padded_h = track_padding.y + track_padding.w;
            const f32 desired_track_h = padded_h > 0.0f ? padded_h : min_track_h;
            const f32 track_h = amal::min(bounds.size.y, desired_track_h);
            amal::rect out = bounds;
            out.offset.y += (bounds.size.y - track_h) * 0.5f;
            out.size.y = track_h;
            return out;
        }

        static inline void fill_gradient_grab_instance(const Style &style, const amal::rect &rect, f32 z_order,
                                                       u16 clip_id, u32 background_color, f32 border_thickness,
                                                       QuadsInstanceData &data)
        {
            data.rect = rect;
            data.z_order = z_order;
            fill_quads_instance_by_style(style, clip_id, data);
            data.background_color = background_color;
            data.border_color = style.border_color();
            data.border_thickness = border_thickness;
            data.border_radius = amal::max(0.0f, amal::min(rect.size.x, rect.size.y) * 0.5f);
            data.mask |= (static_cast<u32>(AUIK_HAS_BORDER_BIT) << 20u);
        }

        static inline f32 apply_edge_grab_bias(f32 center_x, f32 factor)
        {
            constexpr f32 edge_bias = 2.0f;
            constexpr f32 edge_epsilon = 1e-4f;
            if (factor <= edge_epsilon) return center_x - edge_bias;
            if (factor >= 1.0f - edge_epsilon) return center_x + edge_bias;
            return center_x;
        }

        static inline StyleState resolve_grab_visual_state(StyleState state)
        {
            return state == StyleState::active ? StyleState::active : StyleState::normal;
        }

        static inline StyleUpdateFlags resolve_style_and_mark_redraw(StyleSelector &selector, u32 self_id,
                                                                     u32 parent_id, StyleState state,
                                                                     bool &redraw_changed)
        {
            const auto flags = resolve_style_selector(selector, self_id, parent_id, state);
            if (flags & StyleUpdateFlagBits::redraw) redraw_changed = true;
            return flags;
        }

        static inline StyleUpdateFlags resolve_selector_style_and_mark_redraw(StyleSelector &selector, u32 self_id,
                                                                            u32 parent_id, StyleState state,
                                                                            bool &redraw_changed)
        {
            const auto flags = resolve_style_selector(selector, self_id, parent_id, state);
            if (flags == StyleUpdateFlagBits::none) return StyleUpdateFlagBits::none;
            redraw_changed = true;
            return StyleUpdateFlagBits::redraw;
        }

        static inline bool is_slider_grab_hit(const ElementID &id, u32 widget_id, u32 tag_id)
        {
            return id.widget_id == widget_id && id.tag_id == tag_id;
        }

        static inline f32 resolve_drag_mouse_offset_x(const amal::rect &grab_rect)
        {
            return get_mouse_pos().x - (grab_rect.offset.x + grab_rect.size.x * 0.5f);
        }

        static inline amal::rect resolve_slider_grab_rect(const Style &style, f32 center_x, f32 center_y,
                                                          const amal::vec2 &visual_size)
        {
            const amal::vec4 margin = style.margin();
            const amal::vec2 outer_size = {visual_size.x + margin.x + margin.z, visual_size.y + margin.y + margin.w};
            return {{center_x - outer_size.x * 0.5f + margin.x, amal::round(center_y - outer_size.y * 0.5f + margin.y)},
                    visual_size};
        }

        static inline f32 resolve_slider_grab_outer_half_width(const Style &style, f32 visual_width)
        {
            const amal::vec4 margin = style.margin();
            return (visual_width + margin.x + margin.z) * 0.5f;
        }

        static inline f32 resolve_slider_track_hit_depth(const amal::vec2 &widget_depth_range)
        {
            const amal::vec2 work_range = get_depth_workzone_range(widget_depth_range);
            return depth_zone_range(work_range, DepthZone::background).x;
        }

        static inline RectData make_slider_track_hit_rect(u32 widget_id, u32 tag_id, const amal::rect &track_rect,
                                                          u16 clip_id, f32 depth)
        {
            amal::rect hit_rect = track_rect;
            hit_rect.offset.y -= SLIDER_TRACK_HIT_PAD;
            hit_rect.size.y += SLIDER_TRACK_HIT_PAD * 2.0f;
            return make_rect_data(widget_id, tag_id, hit_rect, clip_id, depth);
        }

        void build_quad_slider_track_visual(SliderTrackVisual &visual, const Style &style, const amal::rect &track_rect,
                                            const amal::vec2 &depth_range, u16 clip_id)
        {
            visual.clear_payload();
            visual.background.rect = track_rect;
            visual.background.z_order = next_depth(depth_range);
            if (fill_quads_instance_by_style(style, clip_id, visual.background))
                visual.set_layer(SliderTrackVisual::LayerBits::background);
        }

        void build_gradient_slider_track_visual(SliderTrackVisual &visual, GradientTrackVisual &gradient_visual,
                                                const Style &style, const amal::rect &track_rect,
                                                const amal::vec2 &depth_range, u16 clip_id, const amal::vec4 *colors,
                                                u32 color_count)
        {
            visual.clear_payload();
            gradient_visual.clear_payload();
            if (!colors || color_count == 0 || amal::is_rect_empty(track_rect)) return;

            gradient_visual.valid =
                build_gradient_rect_vertex_data(gradient_visual.data, track_rect, next_depth(depth_range), clip_id,
                                                colors, color_count, style.border_radius(), style.corner_mask(), 1.0f);

            if (style.has_visible_border())
            {
                visual.set_layer(SliderTrackVisual::LayerBits::border);
                amal::vec2 border_range{};
                assign_next_depth(depth_range, border_range);
                fill_border_only_instance(style, track_rect, next_depth(border_range), clip_id, visual.border);
            }
        }
    } // namespace detail

    Slider::Slider(u32 id, f32 *value, f32 min_value, f32 max_value, f32 width, f32 *range_start_value,
                   WidgetFlags widget_flags, Widget *parent)
        : Widget(id, widget_flags, EventFlagBits::click | EventFlagBits::drag, parent, {{0.0f, 0.0f}, {width, 0.0f}},
                 AUIK_TAG_SLIDER),
          _value(value),
          _range_start_value(range_start_value),
          _min_value(min_value),
          _max_value(max_value)
    {
        _track_style.tag_id = AUIK_TAG_SLIDER;
        _fill_style.tag_id = AUIK_TAG_SLIDER;
        _grab_style.tag_id = AUIK_TAG_SLIDER_GRAB;
        _grab_hit_rect = detail::make_rect_data(id, _grab_style.tag_id);
        if (_max_value < _min_value) std::swap(_min_value, _max_value);
        set_value(value ? *value : _min_value);
    }

    StyleUpdateFlags Slider::update_style()
    {
        const u32 parent_id = parent() ? parent()->id() : 0u;

        StyleUpdateFlags out = StyleUpdateFlagBits::none;
        bool track_or_fill_changed = false;
        bool grab_changed = false;
        if (_track_style.id == Theme::STYLE_ID_INVALID)
            out |= detail::resolve_style_and_mark_redraw(_track_style, _track_style.tag_id, parent_id,
                                                         StyleState::normal, track_or_fill_changed);
        if (_fill_style.id == Theme::STYLE_ID_INVALID)
            out |= detail::resolve_style_and_mark_redraw(_fill_style, _fill_style.tag_id, parent_id, StyleState::active,
                                                         track_or_fill_changed);
        if (_grab_style.id == Theme::STYLE_ID_INVALID)
            out |= detail::resolve_selector_style_and_mark_redraw(_grab_style, _grab_style.tag_id, parent_id,
                                                                StyleState::normal, grab_changed);

        const auto transition = detail::get_widget_style_selector_transition(id());
        if (transition.prev_id.tag_id == _grab_style.tag_id &&
            (transition.current_id.tag_id != _grab_style.tag_id || transition.prev_state != transition.current_state))
            out |= detail::resolve_selector_style_and_mark_redraw(_grab_style, _grab_style.tag_id, parent_id,
                                                                StyleState::normal, grab_changed);
        if (transition.current_id.tag_id == _grab_style.tag_id)
            out |= detail::resolve_selector_style_and_mark_redraw(_grab_style, _grab_style.tag_id, parent_id,
                                                                transition.current_state, grab_changed);

        const StyleState widget_grab_state = detail::resolve_grab_visual_state(style_state());
        if (widget_grab_state == StyleState::active || widget_grab_state == StyleState::focus)
            out |= detail::resolve_selector_style_and_mark_redraw(_grab_style, _grab_style.tag_id, parent_id,
                                                                widget_grab_state, grab_changed);
        else if (transition.current_id.tag_id != _grab_style.tag_id)
            out |= detail::resolve_selector_style_and_mark_redraw(_grab_style, _grab_style.tag_id, parent_id,
                                                                StyleState::normal, grab_changed);

        if (track_or_fill_changed) rebuild_track_visuals();
        if (grab_changed) rebuild_grab_visual();
        return out;
    }

    amal::vec2 Slider::resolve_grab_size(const Style &grab_style) const
    {
        const amal::vec4 grab_padding = grab_style.padding();
        return {amal::max(grab_padding.x + grab_padding.z, 1.0f), amal::max(grab_padding.y + grab_padding.w, 1.0f)};
    }

    bool Slider::resolve_range_values(f32 &out_start, f32 &out_end) const
    {
        if (!_value) return false;
        if (_range_start_value)
        {
            if (!amal::isfinite(*_range_start_value)) return false;
            out_start = amal::clamp(*_range_start_value, _min_value, _max_value);
        }
        else out_start = _min_value;
        out_end = amal::clamp(*_value, _min_value, _max_value);
        if (out_end < out_start) std::swap(out_start, out_end);
        return true;
    }

    void Slider::update_layout_min_size()
    {
        const auto &track_style = get_theme()->get_style(_track_style.id);
        const amal::vec4 margin = track_style.margin();
        const amal::vec4 padding = track_style.padding();

        amal::vec2 min_size = size();
        if (!is_fixed()) min_size.x = 0.0f;
        else if (min_size.x <= 0.0f) min_size.x = 160.0f;

        const f32 min_track_h =
            amal::max(6.0f, track_style.border_radius() > 0.0f ? track_style.border_radius() * 2.0f : 0.0f);
        const f32 padded_h = padding.y + padding.w;
        const f32 total_h = padded_h > 0.0f ? padded_h : min_track_h;

        if (min_size.y <= 0.0f) min_size.y = total_h;
        else min_size.y = amal::max(min_size.y, total_h);
        if (min_size.x > 0.0f) min_size.x = amal::max(min_size.x, 24.0f + margin.x + margin.z);
        set_required_size({min_size.x + margin.x + margin.z, min_size.y + margin.y + margin.w});
    }

    void Slider::update_layout(bool min_size_known)
    {
        if (!min_size_known) update_layout_min_size();

        const auto &style = get_theme()->get_style(_track_style.id);
        const amal::vec2 layout_origin = position();
        const amal::vec4 margin = style.margin();
        const amal::vec2 min_required = required_size();
        amal::vec2 slider_size = size();
        if (!is_fixed())
            slider_size.x = amal::max(slider_size.x - margin.x - margin.z, min_required.x - margin.x - margin.z);
        else slider_size.x = amal::max(slider_size.x, min_required.x - margin.x - margin.z);
        slider_size.y = amal::max(slider_size.y, min_required.y - margin.y - margin.w);

        const amal::vec2 pos = {layout_origin.x + margin.x, layout_origin.y + margin.y};
        set_position(pos);
        set_size(slider_size);
        Widget::update_layout(true);
        assert(parent() && "Slider must have parent");
        set_clip_id(parent()->content_clip_id());
        _grab_hit_rect.clip_id = clip_id();

        rebuild_track_visuals();
        rebuild_grab_visual();
    }

    void Slider::translate(const amal::vec2 &delta)
    {
        if (delta.x == 0.0f && delta.y == 0.0f) return;
        Widget::translate(delta);
        detail::get_context().dirty_flags |= DirtyFlagBits::hit_rect_update;
        _track_rect.offset += delta;
        _grab_rect.offset += delta;
        _grab_hit_rect.bounds.offset += delta;
        rebuild_track_visuals();
        rebuild_grab_visual();
    }

    void Slider::rebuild_clip_rects()
    {
        assert(parent() && "Slider must have parent");
        set_clip_id(parent()->content_clip_id());
        _track_visual.background_draw_id.hit_id = AUIK_INVALID_DRAW_DATA_ID;
        _track_visual.fill_draw_id.hit_id = AUIK_INVALID_DRAW_DATA_ID;
        _track_visual.border_draw_id.hit_id = AUIK_INVALID_DRAW_DATA_ID;
        _grab_draw_id.hit_id = AUIK_INVALID_DRAW_DATA_ID;
        _grab_hit_rect.clip_id = clip_id();
        rebuild_track_visuals();
        rebuild_grab_visual();
    }

    void Slider::update_depth(const amal::vec2 &depth_range)
    {
        Widget::update_depth(depth_range);
        assign_next_depth(this->depth_range(), _track_depth_range);
        assign_next_depth(_track_depth_range, _grab_depth_range);
        rebuild_track_visuals();
        rebuild_grab_visual();
    }

    void Slider::draw(DrawCtx &ctx)
    {
        auto *quad_stream = get_primary_quads_stream();
        bool hit_pending = ctx.emit_hit_rect;
        const auto track_hit_rect = detail::make_slider_track_hit_rect(
            id(), _track_style.tag_id, _track_rect, clip_id(), detail::resolve_slider_track_hit_depth(depth_range()));

        if (ctx.is_recording() ||
            should_emit_quads_instance(_track_visual.has_layer(detail::SliderTrackVisual::LayerBits::background),
                                       _track_visual.background_draw_id, hit_pending))
        {
            ctx.emit(quad_stream, _track_visual.background_draw_id, &_track_visual.background, track_hit_rect,
                     hit_pending);
            hit_pending = false;
        }
        if (ctx.is_recording() ||
            should_emit_quads_instance(_track_visual.has_layer(detail::SliderTrackVisual::LayerBits::fill),
                                       _track_visual.fill_draw_id, hit_pending))
        {
            ctx.emit(quad_stream, _track_visual.fill_draw_id, &_track_visual.fill, track_hit_rect, hit_pending);
            hit_pending = false;
        }
        if (ctx.is_recording() ||
            should_emit_quads_instance(_track_visual.has_layer(detail::SliderTrackVisual::LayerBits::border),
                                       _track_visual.border_draw_id, hit_pending))
        {
            ctx.emit(quad_stream, _track_visual.border_draw_id, &_track_visual.border, track_hit_rect, hit_pending);
            hit_pending = false;
        }
        ctx.emit(quad_stream, _grab_draw_id, &_grab_visual, _grab_hit_rect, ctx.emit_hit_rect);
    }

    bool Slider::has_draw_record() const
    {
        if (_grab_draw_id.render_id == AUIK_INVALID_DRAW_DATA_ID) return false;
        if (_track_visual.background_draw_id.render_id == AUIK_INVALID_DRAW_DATA_ID) return false;
        if (_track_visual.fill_draw_id.render_id == AUIK_INVALID_DRAW_DATA_ID) return false;
        if (_track_visual.border_draw_id.render_id == AUIK_INVALID_DRAW_DATA_ID) return false;
        return true;
    }

    void Slider::set_value(f32 new_value)
    {
        if (!_value) return;
        f32 clamped = amal::clamp(new_value, _min_value, _max_value);
        if (_step > 0.0f)
        {
            const f32 normalized = (clamped - _min_value) / _step;
            clamped = _min_value + amal::round(normalized) * _step;
            clamped = amal::clamp(clamped, _min_value, _max_value);
        }
        if (*_value == clamped) return;
        *_value = clamped;
        dispatch_change();
        detail::get_context().dirty_flags |= DirtyFlagBits::hit_rect_update;
        rebuild_track_fill_visual();
        rebuild_grab_visual();
    }

    void Slider::set_range_start_value_ptr(f32 *value_ptr)
    {
        _range_start_value = value_ptr;
        rebuild_track_fill_visual();
    }

    void Slider::set_step(f32 step)
    {
        _step = amal::max(step, 0.0f);
        if (_value) set_value(*_value);
    }

    void Slider::update_value_from_mouse()
    {
        if (!_value) return;
        const f32 width = amal::max(_track_rect.size.x, 1e-5f);
        const f32 mouse_x = get_mouse_pos().x - _drag_grab_offset_x;
        const f32 t = amal::clamp((mouse_x - _track_rect.offset.x) / width, 0.0f, 1.0f);
        set_value(_min_value + (_max_value - _min_value) * t);
    }

    void Slider::on_click(MouseKey key, KeyPressState state, u32 click_count)
    {
        (void)click_count;
        if (key != MouseKey::left) return;

        if (state == KeyPressState::press)
        {
            if (detail::is_slider_grab_hit(detail::get_context().io.clicked_id, id(), _grab_style.tag_id))
            {
                add_render_command<detail::ClickEventTraits>(this, [this]() { redraw_external(has_draw_record()); });
                return;
            }
            update_value_from_mouse();
            add_render_command<detail::ClickEventTraits>(this, [this]() { redraw_external(has_draw_record()); });
            return;
        }

        if (state == KeyPressState::release)
            add_render_command<detail::ClickEventTraits>(this, [this]() { redraw_external(has_draw_record()); });
    }

    void Slider::on_drag(const amal::vec2 &delta, KeyPressState state)
    {
        (void)delta;
        if (state == KeyPressState::press)
        {
            const auto drag_id = detail::get_context().io.drag_id;
            if (detail::is_slider_grab_hit(drag_id, id(), _grab_style.tag_id))
                _drag_grab_offset_x = detail::resolve_drag_mouse_offset_x(_grab_rect);
            else
            {
                _drag_grab_offset_x = 0.0f;
                update_value_from_mouse();
            }
        }
        else if (state == KeyPressState::repeat) update_value_from_mouse();
        else if (state == KeyPressState::release) _drag_grab_offset_x = 0.0f;
        else return;

        add_render_command<detail::DragEventTraits>(this, [this]() { redraw_external(has_draw_record()); });
    }

    void Slider::rebuild_track_fill_visual()
    {
        if (_fill_style.id == Theme::STYLE_ID_INVALID)
        {
            _track_visual.clear_layer(detail::SliderTrackVisual::LayerBits::fill);
            return;
        }
        f32 range_start = 0.0f;
        f32 range_end = 0.0f;
        if (!resolve_range_values(range_start, range_end))
        {
            _track_visual.clear_layer(detail::SliderTrackVisual::LayerBits::fill);
            return;
        }

        const f32 full = amal::max(_max_value - _min_value, 1e-5f);
        const f32 start_t = amal::clamp((range_start - _min_value) / full, 0.0f, 1.0f);
        const f32 end_t = amal::clamp((range_end - _min_value) / full, 0.0f, 1.0f);
        const f32 left_t = amal::min(start_t, end_t);
        const f32 right_t = amal::max(start_t, end_t);
        const f32 left_x = _track_rect.offset.x + _track_rect.size.x * left_t;
        const f32 right_x = _track_rect.offset.x + _track_rect.size.x * right_t;
        const f32 fill_w = amal::max(right_x - left_x, 0.0f);

        const auto &fill_style = get_theme()->get_style(_fill_style.id);
        _track_visual.fill.rect = _track_rect;
        _track_visual.fill.rect.offset.x = left_x;
        _track_visual.fill.rect.size.x = fill_w > 0.0f ? fill_w : 1.0f;
        _track_visual.fill.z_order = next_depth(_track_depth_range);
        const bool fill_visible = fill_quads_instance_by_style(fill_style, clip_id(), _track_visual.fill) &&
                                  fill_w > 0.0f && !amal::is_rect_empty(_track_rect);
        if (!fill_visible)
        {
            _track_visual.clear_layer(detail::SliderTrackVisual::LayerBits::fill);
            _track_visual.fill.background_color = 0;
            _track_visual.fill.border_color = 0;
            _track_visual.fill.border_thickness = 0.0f;
            _track_visual.fill.mask &= ~(static_cast<u32>(AUIK_HAS_BORDER_BIT) << 20u);
            return;
        }
        _track_visual.set_layer(detail::SliderTrackVisual::LayerBits::fill);

        if (!fill_style.has_visible_border())
        {
            _track_visual.fill.border_thickness = 0.0f;
            _track_visual.fill.mask &= ~(static_cast<u32>(AUIK_HAS_BORDER_BIT) << 20u);
        }
    }

    void Slider::rebuild_track_visuals()
    {
        if (_track_style.id == Theme::STYLE_ID_INVALID || _grab_style.id == Theme::STYLE_ID_INVALID) return;
        const auto &track_style = get_theme()->get_style(_track_style.id);
        _track_rect = detail::resolve_slider_track_rect(bounds(), track_style);
        detail::build_quad_slider_track_visual(_track_visual, track_style, _track_rect, _track_depth_range, clip_id());
        rebuild_track_fill_visual();
    }

    void Slider::rebuild_grab_visual()
    {
        if (_track_style.id == Theme::STYLE_ID_INVALID || _grab_style.id == Theme::STYLE_ID_INVALID) return;

        const auto &grab_style = get_theme()->get_style(_grab_style.id);
        const amal::vec2 grab_size = resolve_grab_size(grab_style);
        const f32 grab_w = amal::max(amal::round(grab_size.x), 3.0f);
        const f32 grab_h = amal::max(amal::round(grab_size.y), 3.0f);
        const f32 range = amal::max(_max_value - _min_value, 1e-5f);
        const f32 factor = amal::clamp((value() - _min_value) / range, 0.0f, 1.0f);
        const f32 outer_half_w = detail::resolve_slider_grab_outer_half_width(grab_style, grab_w);
        const f32 min_center_x = _track_rect.offset.x + outer_half_w;
        const f32 max_center_x = _track_rect.offset.x + _track_rect.size.x - outer_half_w;
        f32 center_x = _track_rect.offset.x + _track_rect.size.x * factor;
        if (max_center_x > min_center_x) center_x = min_center_x + (max_center_x - min_center_x) * factor;
        else center_x = _track_rect.offset.x + _track_rect.size.x * 0.5f;
        const f32 center_y = _track_rect.offset.y + _track_rect.size.y * 0.5f;
        _grab_rect = detail::resolve_slider_grab_rect(grab_style, center_x, center_y, {grab_w, grab_h});
        _grab_hit_rect.bounds = _grab_rect;
        _grab_hit_rect.depth = next_depth(_grab_depth_range);

        const u16 grab_clip_id = clip_id();
        const f32 grab_z = next_depth(_grab_depth_range);
        _grab_visual.rect = _grab_rect;
        _grab_visual.z_order = grab_z;
        fill_quads_instance_by_style(grab_style, grab_clip_id, _grab_visual);
        if (!grab_style.has_visible_border())
        {
            _grab_visual.border_thickness = 0.0f;
            _grab_visual.mask &= ~(static_cast<u32>(AUIK_HAS_BORDER_BIT) << 20u);
        }
    }

    void Slider::rebuild_cached_visuals()
    {
        rebuild_track_visuals();
        rebuild_grab_visual();
    }

    GradientSlider::GradientSlider(u32 id, f32 *value, f32 min_value, f32 max_value, f32 width,
                                   const amal::vec4 *colors, u32 color_count, WidgetFlags widget_flags, Widget *parent)
        : Widget(id, widget_flags, EventFlagBits::click | EventFlagBits::drag, parent, {{0.0f, 0.0f}, {width, 0.0f}},
                 AUIK_TAG_GRADIENT_SLIDER),
          _value(value),
          _min_value(min_value),
          _max_value(max_value)
    {
        _track_style.tag_id = AUIK_TAG_GRADIENT_SLIDER;
        _grab_style.tag_id = AUIK_TAG_GRADIENT_SLIDER_GRAB;
        _grab_hit_rect = detail::make_rect_data(id, _grab_style.tag_id);
        _colors.reserve(color_count);
        for (u32 i = 0; i < color_count; ++i) _colors.push_back(colors[i]);
        if (_max_value < _min_value) std::swap(_min_value, _max_value);
        set_value(value ? *value : _min_value);
    }

    GradientSlider::~GradientSlider() {}

    StyleUpdateFlags GradientSlider::update_style()
    {
        const u32 parent_id = parent() ? parent()->id() : 0u;

        StyleUpdateFlags out = StyleUpdateFlagBits::none;
        bool track_changed = false;
        bool grab_changed = false;

        auto resolve_track_state = [&](StyleState state) -> StyleUpdateFlags {
            const auto flags = resolve_style_selector(_track_style, _track_style.tag_id, parent_id, state);
            if (flags & StyleUpdateFlagBits::redraw) track_changed = true;
            return flags;
        };

        auto resolve_grab_state = [&](StyleState state) -> StyleUpdateFlags {
            const auto flags = detail::resolve_selector_style_and_mark_redraw(_grab_style, _grab_style.tag_id, parent_id,
                                                                            state, grab_changed);
            if (flags & StyleUpdateFlagBits::redraw) grab_changed = true;
            return flags;
        };

        if (_track_style.id == Theme::STYLE_ID_INVALID) out |= resolve_track_state(StyleState::normal);
        if (_grab_style.id == Theme::STYLE_ID_INVALID) out |= resolve_grab_state(StyleState::normal);

        const auto transition = detail::get_widget_style_selector_transition(id());
        if (transition.prev_id.tag_id == _grab_style.tag_id &&
            (transition.current_id.tag_id != _grab_style.tag_id || transition.prev_state != transition.current_state))
            out |= resolve_grab_state(StyleState::normal);
        if (transition.current_id.tag_id == _grab_style.tag_id) out |= resolve_grab_state(transition.current_state);

        const StyleState widget_grab_state = detail::resolve_grab_visual_state(style_state());
        if (widget_grab_state == StyleState::active || widget_grab_state == StyleState::focus)
            out |= resolve_grab_state(widget_grab_state);
        else if (transition.current_id.tag_id != _grab_style.tag_id) out |= resolve_grab_state(StyleState::normal);

        if (track_changed) rebuild_track_visuals();
        if (grab_changed) rebuild_grab_visual();
        return out;
    }

    amal::vec2 GradientSlider::resolve_grab_size(const Style &grab_style) const
    {
        const amal::vec4 grab_padding = grab_style.padding();
        return {amal::max(grab_padding.x + grab_padding.z, 1.0f), amal::max(grab_padding.y + grab_padding.w, 1.0f)};
    }

    amal::vec4 GradientSlider::resolve_active_color(f32 factor) const
    {
        return detail::sample_gradient_color(_colors.data(), static_cast<u32>(_colors.size()), factor);
    }

    void GradientSlider::update_layout_min_size()
    {
        const auto &track_style = get_theme()->get_style(_track_style.id);
        const amal::vec4 margin = track_style.margin();
        const amal::vec4 padding = track_style.padding();

        amal::vec2 min_size = size();
        if (!is_fixed()) min_size.x = 0.0f;
        else if (min_size.x <= 0.0f) min_size.x = 160.0f;

        const f32 min_track_h =
            amal::max(6.0f, track_style.border_radius() > 0.0f ? track_style.border_radius() * 2.0f : 0.0f);
        const f32 padded_h = padding.y + padding.w;
        const f32 total_h = padded_h > 0.0f ? padded_h : min_track_h;

        if (min_size.y <= 0.0f) min_size.y = total_h;
        else min_size.y = amal::max(min_size.y, total_h);
        if (min_size.x > 0.0f) min_size.x = amal::max(min_size.x, 24.0f + margin.x + margin.z);
        set_required_size({min_size.x + margin.x + margin.z, min_size.y + margin.y + margin.w});
    }

    void GradientSlider::update_layout(bool min_size_known)
    {
        if (!min_size_known) update_layout_min_size();

        const auto &style = get_theme()->get_style(_track_style.id);
        const amal::vec2 layout_origin = position();
        const amal::vec4 margin = style.margin();
        const amal::vec2 min_required = required_size();
        amal::vec2 slider_size = size();
        if (!is_fixed())
            slider_size.x = amal::max(slider_size.x - margin.x - margin.z, min_required.x - margin.x - margin.z);
        else slider_size.x = amal::max(slider_size.x, min_required.x - margin.x - margin.z);
        slider_size.y = amal::max(slider_size.y, min_required.y - margin.y - margin.w);

        const amal::vec2 pos = {layout_origin.x + margin.x, layout_origin.y + margin.y};
        set_position(pos);
        set_size(slider_size);
        Widget::update_layout(true);
        assert(parent() && "GradientSlider must have parent");
        set_clip_id(parent()->content_clip_id());
        _grab_hit_rect.clip_id = clip_id();

        rebuild_track_visuals();
        rebuild_grab_visual();
    }

    void GradientSlider::translate(const amal::vec2 &delta)
    {
        if (delta.x == 0.0f && delta.y == 0.0f) return;
        Widget::translate(delta);
        detail::get_context().dirty_flags |= DirtyFlagBits::hit_rect_update;
        _track_rect.offset += delta;
        _grab_rect.offset += delta;
        _grab_hit_rect.bounds.offset += delta;
        rebuild_track_visuals();
        rebuild_grab_visual();
    }

    void GradientSlider::rebuild_clip_rects()
    {
        assert(parent() && "GradientSlider must have parent");
        set_clip_id(parent()->content_clip_id());
        _track_visual.background_draw_id.hit_id = AUIK_INVALID_DRAW_DATA_ID;
        _track_visual.fill_draw_id.hit_id = AUIK_INVALID_DRAW_DATA_ID;
        _track_visual.border_draw_id.hit_id = AUIK_INVALID_DRAW_DATA_ID;
        _gradient_visual.draw_id.hit_id = AUIK_INVALID_DRAW_DATA_ID;
        _grab_draw_id.hit_id = AUIK_INVALID_DRAW_DATA_ID;
        _grab_back_draw_id.hit_id = AUIK_INVALID_DRAW_DATA_ID;
        _grab_hit_rect.clip_id = clip_id();
        rebuild_track_visuals();
        rebuild_grab_visual();
    }

    void GradientSlider::update_depth(const amal::vec2 &depth_range)
    {
        Widget::update_depth(depth_range);
        assign_next_depth(this->depth_range(), _track_depth_range);
        assign_next_depth(_track_depth_range, _grab_depth_range);
        rebuild_track_visuals();
        rebuild_grab_visual();
    }

    void GradientSlider::draw(DrawCtx &ctx)
    {
        auto *quad_stream = get_primary_quads_stream();
        auto *overlay_quad_stream = get_overlay_quads_stream();
        auto *vertex_stream = get_primary_vertex_stream();
        bool hit_pending = ctx.emit_hit_rect;
        const auto track_hit_rect = detail::make_slider_track_hit_rect(
            id(), _track_style.tag_id, _track_rect, clip_id(), detail::resolve_slider_track_hit_depth(depth_range()));

        if (ctx.is_recording() ||
            should_emit_quads_instance(_track_visual.has_layer(detail::SliderTrackVisual::LayerBits::background),
                                       _track_visual.background_draw_id, hit_pending))
        {
            ctx.emit(quad_stream, _track_visual.background_draw_id, &_track_visual.background, track_hit_rect,
                     hit_pending);
            hit_pending = false;
        }
        if (_gradient_visual.valid || (ctx.is_invalidating() &&
                                       _gradient_visual.draw_id.render_id != AUIK_INVALID_DRAW_DATA_ID))
        {
            ctx.emit(vertex_stream, _gradient_visual.draw_id, &_gradient_visual.data.batch, track_hit_rect,
                     hit_pending);
            hit_pending = false;
        }
        if (ctx.is_recording() ||
            should_emit_quads_instance(_track_visual.has_layer(detail::SliderTrackVisual::LayerBits::border),
                                       _track_visual.border_draw_id, hit_pending))
        {
            ctx.emit(quad_stream, _track_visual.border_draw_id, &_track_visual.border, track_hit_rect, hit_pending);
            hit_pending = false;
        }
        const bool grab_back_visible = _grab_back_visual.rect.size.x > 0.0f && _grab_back_visual.rect.size.y > 0.0f;
        if (ctx.is_recording() || grab_back_visible || _grab_back_draw_id.render_id != AUIK_INVALID_DRAW_DATA_ID)
            ctx.emit(overlay_quad_stream, _grab_back_draw_id, &_grab_back_visual, _grab_hit_rect, false);
        ctx.emit(overlay_quad_stream, _grab_draw_id, &_grab_visual, _grab_hit_rect, ctx.emit_hit_rect);
    }

    bool GradientSlider::has_draw_record() const
    {
        if (_grab_draw_id.render_id == AUIK_INVALID_DRAW_DATA_ID) return false;
        if (_grab_back_draw_id.render_id == AUIK_INVALID_DRAW_DATA_ID) return false;
        if (_track_visual.background_draw_id.render_id == AUIK_INVALID_DRAW_DATA_ID) return false;
        if (_track_visual.border_draw_id.render_id == AUIK_INVALID_DRAW_DATA_ID) return false;
        if (_gradient_visual.valid && _gradient_visual.draw_id.render_id == AUIK_INVALID_DRAW_DATA_ID) return false;
        return true;
    }

    void GradientSlider::set_value(f32 new_value)
    {
        if (!_value) return;
        f32 clamped = amal::clamp(new_value, _min_value, _max_value);
        if (_step > 0.0f)
        {
            const f32 normalized = (clamped - _min_value) / _step;
            clamped = _min_value + amal::round(normalized) * _step;
            clamped = amal::clamp(clamped, _min_value, _max_value);
        }
        if (*_value == clamped) return;
        *_value = clamped;
        dispatch_change();
        detail::get_context().dirty_flags |= DirtyFlagBits::hit_rect_update;
        rebuild_grab_visual();
    }

    void GradientSlider::set_step(f32 step)
    {
        _step = amal::max(step, 0.0f);
        if (_value) set_value(*_value);
    }

    void GradientSlider::update_value_from_mouse()
    {
        if (!_value) return;
        const f32 width = amal::max(_track_rect.size.x, 1e-5f);
        const f32 mouse_x = get_mouse_pos().x - _drag_grab_offset_x;
        const f32 t = amal::clamp((mouse_x - _track_rect.offset.x) / width, 0.0f, 1.0f);
        set_value(_min_value + (_max_value - _min_value) * t);
    }

    void GradientSlider::on_click(MouseKey key, KeyPressState state, u32 click_count)
    {
        (void)click_count;
        if (key != MouseKey::left) return;

        if (state == KeyPressState::press)
        {
            if (detail::is_slider_grab_hit(detail::get_context().io.clicked_id, id(), _grab_style.tag_id))
            {
                add_render_command<detail::ClickEventTraits>(this, [this]() { redraw_external(has_draw_record()); });
                return;
            }
            update_value_from_mouse();
            add_render_command<detail::ClickEventTraits>(this, [this]() { redraw_external(has_draw_record()); });
            return;
        }

        if (state == KeyPressState::release)
            add_render_command<detail::ClickEventTraits>(this, [this]() { redraw_external(has_draw_record()); });
    }

    void GradientSlider::on_drag(const amal::vec2 &delta, KeyPressState state)
    {
        (void)delta;
        if (state == KeyPressState::press)
        {
            const auto drag_id = detail::get_context().io.drag_id;
            if (detail::is_slider_grab_hit(drag_id, id(), _grab_style.tag_id))
                _drag_grab_offset_x = detail::resolve_drag_mouse_offset_x(_grab_rect);
            else
            {
                _drag_grab_offset_x = 0.0f;
                update_value_from_mouse();
            }
        }
        else if (state == KeyPressState::repeat) update_value_from_mouse();
        else if (state == KeyPressState::release) _drag_grab_offset_x = 0.0f;
        else return;

        add_render_command<detail::DragEventTraits>(this, [this]() { redraw_external(has_draw_record()); });
    }

    void GradientSlider::rebuild_track_visuals()
    {
        if (_track_style.id == Theme::STYLE_ID_INVALID || _grab_style.id == Theme::STYLE_ID_INVALID) return;
        const auto &track_style = get_theme()->get_style(_track_style.id);
        _track_rect = detail::resolve_slider_track_rect(bounds(), track_style);
        detail::build_gradient_slider_track_visual(_track_visual, _gradient_visual, track_style, _track_rect,
                                                   _track_depth_range, clip_id(), _colors.data(),
                                                   static_cast<u32>(_colors.size()));
    }

    void GradientSlider::rebuild_grab_visual()
    {
        if (_track_style.id == Theme::STYLE_ID_INVALID || _grab_style.id == Theme::STYLE_ID_INVALID) return;

        const auto &grab_style = get_theme()->get_style(_grab_style.id);
        const amal::vec2 grab_size = resolve_grab_size(grab_style);
        const f32 grab_w = amal::max(amal::round(grab_size.x), 3.0f);
        const f32 grab_h = amal::max(amal::round(grab_size.y), 3.0f);
        const f32 range = amal::max(_max_value - _min_value, 1e-5f);
        const f32 factor = amal::clamp((value() - _min_value) / range, 0.0f, 1.0f);
        const f32 outer_half_w = detail::resolve_slider_grab_outer_half_width(grab_style, grab_w);
        const f32 min_center_x = _track_rect.offset.x + outer_half_w;
        const f32 max_center_x = _track_rect.offset.x + _track_rect.size.x - outer_half_w;
        f32 center_x = _track_rect.offset.x + _track_rect.size.x * factor;
        if (max_center_x > min_center_x) center_x = min_center_x + (max_center_x - min_center_x) * factor;
        else center_x = _track_rect.offset.x + _track_rect.size.x * 0.5f;
        center_x = detail::apply_edge_grab_bias(center_x, factor);
        const f32 center_y = _track_rect.offset.y + _track_rect.size.y * 0.5f;
        _grab_rect = detail::resolve_slider_grab_rect(grab_style, center_x, center_y, {grab_w, grab_h});
        _grab_hit_rect.bounds = _grab_rect;
        _grab_hit_rect.depth = next_depth(_grab_depth_range);

        const u16 grab_clip_id = clip_id();
        const f32 grab_z = next_depth(_grab_depth_range);
        _grab_visual.rect = _grab_rect;
        _grab_visual.z_order = grab_z;
        _grab_back_visual = {};

        if (const Style *border_style = get_theme()->get_desc_style(AUIK_TAG_GRADIENT_SLIDER_GRAB_BORDER))
        {
            const amal::vec2 border_size = resolve_grab_size(*border_style);
            const f32 border_w = amal::max(amal::round(border_size.x), _grab_rect.size.x);
            const f32 border_h = amal::max(amal::round(border_size.y), _grab_rect.size.y);
            const amal::rect border_rect =
                detail::resolve_slider_grab_rect(*border_style, center_x, center_y, {border_w, border_h});
            detail::fill_gradient_grab_instance(*border_style, border_rect, grab_z, grab_clip_id,
                                                border_style->background_color(), border_style->border_thickness(),
                                                _grab_back_visual);
        }

        detail::fill_gradient_grab_instance(grab_style, _grab_rect, grab_z, grab_clip_id,
                                            detail::pack_rgba8(resolve_active_color(factor)),
                                            grab_style.border_thickness(), _grab_visual);
    }

    void GradientSlider::rebuild_cached_visuals()
    {
        rebuild_track_visuals();
        rebuild_grab_visual();
    }

    TransparencySlider::TransparencySlider(u32 id, f32 *value, f32 min_value, f32 max_value, f32 width,
                                           const amal::vec4 &color, WidgetFlags widget_flags, Widget *parent)
        : Widget(id, widget_flags, EventFlagBits::click | EventFlagBits::drag, parent, {{0.0f, 0.0f}, {width, 0.0f}},
                 AUIK_TAG_GRADIENT_SLIDER),
          _value(value),
          _min_value(min_value),
          _max_value(max_value),
          _color(color)
    {
        _track_style.tag_id = AUIK_TAG_GRADIENT_SLIDER;
        _grab_style.tag_id = AUIK_TAG_GRADIENT_SLIDER_GRAB;
        _grab_hit_rect = detail::make_rect_data(id, _grab_style.tag_id);
        _colors.resize(2u);
        rebuild_gradient_colors();
        if (_max_value < _min_value) std::swap(_min_value, _max_value);
        set_value(value ? *value : _min_value);
    }

    StyleUpdateFlags TransparencySlider::update_style()
    {
        const u32 parent_id = parent() ? parent()->id() : 0u;

        StyleUpdateFlags out = StyleUpdateFlagBits::none;
        bool track_changed = false;
        bool grab_changed = false;

        auto resolve_track_state = [&](StyleState state) -> StyleUpdateFlags {
            const auto flags = resolve_style_selector(_track_style, _track_style.tag_id, parent_id, state);
            if (flags & StyleUpdateFlagBits::redraw) track_changed = true;
            return flags;
        };

        auto resolve_grab_state = [&](StyleState state) -> StyleUpdateFlags {
            const auto flags = detail::resolve_selector_style_and_mark_redraw(_grab_style, _grab_style.tag_id, parent_id,
                                                                            state, grab_changed);
            if (flags & StyleUpdateFlagBits::redraw) grab_changed = true;
            return flags;
        };

        if (_track_style.id == Theme::STYLE_ID_INVALID) out |= resolve_track_state(StyleState::normal);
        if (_grab_style.id == Theme::STYLE_ID_INVALID) out |= resolve_grab_state(StyleState::normal);

        const auto transition = detail::get_widget_style_selector_transition(id());
        if (transition.prev_id.tag_id == _grab_style.tag_id &&
            (transition.current_id.tag_id != _grab_style.tag_id || transition.prev_state != transition.current_state))
            out |= resolve_grab_state(StyleState::normal);
        if (transition.current_id.tag_id == _grab_style.tag_id) out |= resolve_grab_state(transition.current_state);

        const StyleState widget_grab_state = detail::resolve_grab_visual_state(style_state());
        if (widget_grab_state == StyleState::active || widget_grab_state == StyleState::focus)
            out |= resolve_grab_state(widget_grab_state);
        else if (transition.current_id.tag_id != _grab_style.tag_id) out |= resolve_grab_state(StyleState::normal);

        if (track_changed) rebuild_track_visuals();
        if (grab_changed) rebuild_grab_visual();
        return out;
    }

    amal::vec2 TransparencySlider::resolve_grab_size(const Style &grab_style) const
    {
        const amal::vec4 grab_padding = grab_style.padding();
        return {amal::max(grab_padding.x + grab_padding.z, 1.0f), amal::max(grab_padding.y + grab_padding.w, 1.0f)};
    }

    amal::vec4 TransparencySlider::resolve_active_color(f32 factor) const
    {
        return {_color.r, _color.g, _color.b, factor};
    }

    void TransparencySlider::rebuild_gradient_colors()
    {
        if (_colors.size() < 2u) _colors.resize(2u);
        _colors[0] = {_color.r, _color.g, _color.b, 0.0f};
        _colors[1] = {_color.r, _color.g, _color.b, 1.0f};
    }

    void TransparencySlider::update_layout_min_size()
    {
        const auto &track_style = get_theme()->get_style(_track_style.id);
        const amal::vec4 margin = track_style.margin();
        const amal::vec4 padding = track_style.padding();

        amal::vec2 min_size = size();
        if (!is_fixed()) min_size.x = 0.0f;
        else if (min_size.x <= 0.0f) min_size.x = 160.0f;

        const f32 min_track_h =
            amal::max(6.0f, track_style.border_radius() > 0.0f ? track_style.border_radius() * 2.0f : 0.0f);
        const f32 padded_h = padding.y + padding.w;
        const f32 total_h = padded_h > 0.0f ? padded_h : min_track_h;

        if (min_size.y <= 0.0f) min_size.y = total_h;
        else min_size.y = amal::max(min_size.y, total_h);
        if (min_size.x > 0.0f) min_size.x = amal::max(min_size.x, 24.0f + margin.x + margin.z);
        set_required_size({min_size.x + margin.x + margin.z, min_size.y + margin.y + margin.w});
    }

    void TransparencySlider::update_layout(bool min_size_known)
    {
        if (!min_size_known) update_layout_min_size();

        const auto &style = get_theme()->get_style(_track_style.id);
        const amal::vec2 layout_origin = position();
        const amal::vec4 margin = style.margin();
        const amal::vec2 min_required = required_size();
        amal::vec2 slider_size = size();
        if (!is_fixed())
            slider_size.x = amal::max(slider_size.x - margin.x - margin.z, min_required.x - margin.x - margin.z);
        else slider_size.x = amal::max(slider_size.x, min_required.x - margin.x - margin.z);
        slider_size.y = amal::max(slider_size.y, min_required.y - margin.y - margin.w);

        const amal::vec2 pos = {layout_origin.x + margin.x, layout_origin.y + margin.y};
        set_position(pos);
        set_size(slider_size);
        Widget::update_layout(true);
        assert(parent() && "TransparencySlider must have parent");
        set_clip_id(parent()->content_clip_id());
        _grab_hit_rect.clip_id = clip_id();

        rebuild_track_visuals();
        rebuild_grab_visual();
    }

    void TransparencySlider::translate(const amal::vec2 &delta)
    {
        if (delta.x == 0.0f && delta.y == 0.0f) return;
        Widget::translate(delta);
        detail::get_context().dirty_flags |= DirtyFlagBits::hit_rect_update;
        _track_rect.offset += delta;
        _grab_rect.offset += delta;
        _grab_hit_rect.bounds.offset += delta;
        rebuild_track_visuals();
        rebuild_grab_visual();
    }

    void TransparencySlider::rebuild_clip_rects()
    {
        assert(parent() && "TransparencySlider must have parent");
        set_clip_id(parent()->content_clip_id());
        _track_visual.background_draw_id.hit_id = AUIK_INVALID_DRAW_DATA_ID;
        _track_visual.fill_draw_id.hit_id = AUIK_INVALID_DRAW_DATA_ID;
        _track_visual.border_draw_id.hit_id = AUIK_INVALID_DRAW_DATA_ID;
        _gradient_visual.draw_id.hit_id = AUIK_INVALID_DRAW_DATA_ID;
        _grab_draw_id.hit_id = AUIK_INVALID_DRAW_DATA_ID;
        _grab_back_draw_id.hit_id = AUIK_INVALID_DRAW_DATA_ID;
        _grab_hit_rect.clip_id = clip_id();
        rebuild_track_visuals();
        rebuild_grab_visual();
    }

    void TransparencySlider::update_depth(const amal::vec2 &depth_range)
    {
        Widget::update_depth(depth_range);
        assign_next_depth(this->depth_range(), _track_depth_range);
        assign_next_depth(_track_depth_range, _grab_depth_range);
        rebuild_track_visuals();
        rebuild_grab_visual();
    }

    void TransparencySlider::draw(DrawCtx &ctx)
    {
        auto *quad_stream = get_primary_quads_stream();
        auto *overlay_quad_stream = get_overlay_quads_stream();
        auto *vertex_stream = get_primary_vertex_stream();
        bool hit_pending = ctx.emit_hit_rect;
        const auto track_hit_rect = detail::make_slider_track_hit_rect(
            id(), _track_style.tag_id, _track_rect, clip_id(), detail::resolve_slider_track_hit_depth(depth_range()));

        if (ctx.is_recording() ||
            should_emit_quads_instance(_track_visual.has_layer(detail::SliderTrackVisual::LayerBits::background),
                                       _track_visual.background_draw_id, hit_pending))
        {
            ctx.emit(quad_stream, _track_visual.background_draw_id, &_track_visual.background, track_hit_rect,
                     hit_pending);
            hit_pending = false;
        }
        if (_gradient_visual.valid || (ctx.is_invalidating() &&
                                       _gradient_visual.draw_id.render_id != AUIK_INVALID_DRAW_DATA_ID))
        {
            ctx.emit(vertex_stream, _gradient_visual.draw_id, &_gradient_visual.data.batch, track_hit_rect,
                     hit_pending);
            hit_pending = false;
        }
        if (ctx.is_recording() ||
            should_emit_quads_instance(_track_visual.has_layer(detail::SliderTrackVisual::LayerBits::border),
                                       _track_visual.border_draw_id, hit_pending))
        {
            ctx.emit(quad_stream, _track_visual.border_draw_id, &_track_visual.border, track_hit_rect, hit_pending);
            hit_pending = false;
        }
        const bool grab_back_visible = _grab_back_visual.rect.size.x > 0.0f && _grab_back_visual.rect.size.y > 0.0f;
        if (ctx.is_recording() || grab_back_visible || _grab_back_draw_id.render_id != AUIK_INVALID_DRAW_DATA_ID)
            ctx.emit(overlay_quad_stream, _grab_back_draw_id, &_grab_back_visual, _grab_hit_rect, false);
        ctx.emit(overlay_quad_stream, _grab_draw_id, &_grab_visual, _grab_hit_rect, ctx.emit_hit_rect);
    }

    bool TransparencySlider::has_draw_record() const
    {
        if (_grab_draw_id.render_id == AUIK_INVALID_DRAW_DATA_ID) return false;
        if (_grab_back_draw_id.render_id == AUIK_INVALID_DRAW_DATA_ID) return false;
        if (_track_visual.background_draw_id.render_id == AUIK_INVALID_DRAW_DATA_ID) return false;
        if (_track_visual.border_draw_id.render_id == AUIK_INVALID_DRAW_DATA_ID) return false;
        if (_gradient_visual.valid && _gradient_visual.draw_id.render_id == AUIK_INVALID_DRAW_DATA_ID) return false;
        return true;
    }

    void TransparencySlider::set_value(f32 new_value)
    {
        if (!_value) return;
        f32 clamped = amal::clamp(new_value, _min_value, _max_value);
        if (_step > 0.0f)
        {
            const f32 normalized = (clamped - _min_value) / _step;
            clamped = _min_value + amal::round(normalized) * _step;
            clamped = amal::clamp(clamped, _min_value, _max_value);
        }
        if (*_value == clamped) return;
        *_value = clamped;
        dispatch_change();
        detail::get_context().dirty_flags |= DirtyFlagBits::hit_rect_update;
        rebuild_grab_visual();
    }

    void TransparencySlider::set_step(f32 step)
    {
        _step = amal::max(step, 0.0f);
        if (_value) set_value(*_value);
    }

    void TransparencySlider::set_color(const amal::vec4 &color)
    {
        if (_color == color) return;
        _color = color;
        rebuild_gradient_colors();
        rebuild_track_visuals();
        rebuild_grab_visual();
    }

    void TransparencySlider::update_value_from_mouse()
    {
        if (!_value) return;
        const f32 width = amal::max(_track_rect.size.x, 1e-5f);
        const f32 mouse_x = get_mouse_pos().x - _drag_grab_offset_x;
        const f32 t = amal::clamp((mouse_x - _track_rect.offset.x) / width, 0.0f, 1.0f);
        set_value(_min_value + (_max_value - _min_value) * t);
    }

    void TransparencySlider::on_click(MouseKey key, KeyPressState state, u32 click_count)
    {
        (void)click_count;
        if (key != MouseKey::left) return;

        if (state == KeyPressState::press)
        {
            if (detail::is_slider_grab_hit(detail::get_context().io.clicked_id, id(), _grab_style.tag_id))
            {
                add_render_command<detail::ClickEventTraits>(this, [this]() { redraw_external(has_draw_record()); });
                return;
            }
            update_value_from_mouse();
            add_render_command<detail::ClickEventTraits>(this, [this]() { redraw_external(has_draw_record()); });
            return;
        }

        if (state == KeyPressState::release)
            add_render_command<detail::ClickEventTraits>(this, [this]() { redraw_external(has_draw_record()); });
    }

    void TransparencySlider::on_drag(const amal::vec2 &delta, KeyPressState state)
    {
        (void)delta;
        if (state == KeyPressState::press)
        {
            const auto drag_id = detail::get_context().io.drag_id;
            if (detail::is_slider_grab_hit(drag_id, id(), _grab_style.tag_id))
                _drag_grab_offset_x = detail::resolve_drag_mouse_offset_x(_grab_rect);
            else
            {
                _drag_grab_offset_x = 0.0f;
                update_value_from_mouse();
            }
        }
        else if (state == KeyPressState::repeat) update_value_from_mouse();
        else if (state == KeyPressState::release) _drag_grab_offset_x = 0.0f;
        else return;

        add_render_command<detail::DragEventTraits>(this, [this]() { redraw_external(has_draw_record()); });
    }

    void TransparencySlider::rebuild_track_visuals()
    {
        if (_track_style.id == Theme::STYLE_ID_INVALID || _grab_style.id == Theme::STYLE_ID_INVALID) return;
        const auto &track_style = get_theme()->get_style(_track_style.id);
        _track_rect = detail::resolve_slider_track_rect(bounds(), track_style);
        detail::build_quad_slider_track_visual(_track_visual, track_style, _track_rect, _track_depth_range, clip_id());
        _track_visual.background.mask |= (static_cast<u32>(AUIK_HAS_CHECKER_BIT) << 20u);
        _track_visual.background.background_color = 0;

        _gradient_visual.clear_payload();
        if (!amal::is_rect_empty(_track_rect) && _colors.size() >= 2u)
        {
            _gradient_visual.valid = build_gradient_rect_vertex_data(
                _gradient_visual.data, _track_rect, next_depth(_track_depth_range), clip_id(), _colors.data(),
                static_cast<u32>(_colors.size()), track_style.border_radius(), track_style.corner_mask(), 1.0f);
        }

        if (track_style.has_visible_border())
        {
            _track_visual.set_layer(detail::SliderTrackVisual::LayerBits::border);
            amal::vec2 border_range{};
            assign_next_depth(_track_depth_range, border_range);
            detail::fill_border_only_instance(track_style, _track_rect, next_depth(border_range), clip_id(),
                                              _track_visual.border);
        }
        else _track_visual.clear_layer(detail::SliderTrackVisual::LayerBits::border);
    }

    void TransparencySlider::rebuild_grab_visual()
    {
        if (_track_style.id == Theme::STYLE_ID_INVALID || _grab_style.id == Theme::STYLE_ID_INVALID) return;

        const auto &grab_style = get_theme()->get_style(_grab_style.id);
        const amal::vec2 grab_size = resolve_grab_size(grab_style);
        const f32 grab_w = amal::max(amal::round(grab_size.x), 3.0f);
        const f32 grab_h = amal::max(amal::round(grab_size.y), 3.0f);
        const f32 range = amal::max(_max_value - _min_value, 1e-5f);
        const f32 factor = amal::clamp((value() - _min_value) / range, 0.0f, 1.0f);
        const f32 outer_half_w = detail::resolve_slider_grab_outer_half_width(grab_style, grab_w);
        const f32 min_center_x = _track_rect.offset.x + outer_half_w;
        const f32 max_center_x = _track_rect.offset.x + _track_rect.size.x - outer_half_w;
        f32 center_x = _track_rect.offset.x + _track_rect.size.x * factor;
        if (max_center_x > min_center_x) center_x = min_center_x + (max_center_x - min_center_x) * factor;
        else center_x = _track_rect.offset.x + _track_rect.size.x * 0.5f;
        center_x = detail::apply_edge_grab_bias(center_x, factor);
        const f32 center_y = _track_rect.offset.y + _track_rect.size.y * 0.5f;
        _grab_rect = detail::resolve_slider_grab_rect(grab_style, center_x, center_y, {grab_w, grab_h});
        _grab_hit_rect.bounds = _grab_rect;
        _grab_hit_rect.depth = next_depth(_grab_depth_range);

        const u16 grab_clip_id = clip_id();
        const f32 grab_z = next_depth(_grab_depth_range);
        _grab_visual.rect = _grab_rect;
        _grab_visual.z_order = grab_z;
        _grab_back_visual = {};

        if (const Style *border_style = get_theme()->get_desc_style(AUIK_TAG_GRADIENT_SLIDER_GRAB_BORDER))
        {
            const amal::vec2 border_size = resolve_grab_size(*border_style);
            const f32 border_w = amal::max(amal::round(border_size.x), _grab_rect.size.x);
            const f32 border_h = amal::max(amal::round(border_size.y), _grab_rect.size.y);
            const amal::rect border_rect =
                detail::resolve_slider_grab_rect(*border_style, center_x, center_y, {border_w, border_h});
            detail::fill_gradient_grab_instance(*border_style, border_rect, grab_z, grab_clip_id,
                                                border_style->background_color(), border_style->border_thickness(),
                                                _grab_back_visual);
        }

        detail::fill_gradient_grab_instance(grab_style, _grab_rect, grab_z, grab_clip_id,
                                            detail::pack_rgba8(resolve_active_color(factor)),
                                            grab_style.border_thickness(), _grab_visual);
    }

    void TransparencySlider::rebuild_cached_visuals()
    {
        rebuild_track_visuals();
        rebuild_grab_visual();
    }

    RangeSlider::RangeSlider(u32 id, f32 *from_value, f32 *to_value, f32 min_value, f32 max_value, f32 width,
                             WidgetFlags widget_flags, Widget *parent)
        : Widget(id, widget_flags, EventFlagBits::click | EventFlagBits::drag, parent, {{0.0f, 0.0f}, {width, 0.0f}},
                 AUIK_TAG_RANGE_SLIDER),
          _from_value(from_value),
          _to_value(to_value),
          _min_value(min_value),
          _max_value(max_value)
    {
        _track_style.tag_id = AUIK_TAG_SLIDER;
        _fill_style.tag_id = AUIK_TAG_SLIDER;
        _from_grab_style.tag_id = AUIK_TAG_SLIDER_GRAB;
        _to_grab_style.tag_id = AUIK_TAG_SLIDER_GRAB;
        _from_hit_rect = detail::make_rect_data(id, _from_grab_style.tag_id, {}, 0xFFFFu, 0.0f, 0u,
                                                static_cast<u32>(ActiveGrab::from));
        _to_hit_rect =
            detail::make_rect_data(id, _to_grab_style.tag_id, {}, 0xFFFFu, 0.0f, 0u, static_cast<u32>(ActiveGrab::to));
        if (_max_value < _min_value) std::swap(_min_value, _max_value);
        set_values(from_value ? *from_value : _min_value, to_value ? *to_value : _max_value);
    }

    StyleUpdateFlags RangeSlider::update_style()
    {
        const u32 parent_id = parent() ? parent()->id() : 0u;

        StyleUpdateFlags out = StyleUpdateFlagBits::none;
        bool track_or_fill_changed = false;
        bool grab_changed = false;

        auto resolve_track_state = [&](StyleState state) -> StyleUpdateFlags {
            const auto flags = resolve_style_selector(_track_style, _track_style.tag_id, parent_id, state);
            if (flags & StyleUpdateFlagBits::redraw) track_or_fill_changed = true;
            return flags;
        };

        auto resolve_fill_state = [&](StyleState state) -> StyleUpdateFlags {
            const auto flags = resolve_style_selector(_fill_style, _fill_style.tag_id, parent_id, state);
            if (flags & StyleUpdateFlagBits::redraw) track_or_fill_changed = true;
            return flags;
        };

        auto resolve_grab_state = [&](StyleSelector &selector, StyleState state) -> StyleUpdateFlags {
            const auto flags =
                detail::resolve_selector_style_and_mark_redraw(selector, selector.tag_id, parent_id, state, grab_changed);
            if (flags & StyleUpdateFlagBits::redraw) grab_changed = true;
            return flags;
        };

        if (_track_style.id == Theme::STYLE_ID_INVALID) out |= resolve_track_state(StyleState::normal);
        if (_fill_style.id == Theme::STYLE_ID_INVALID) out |= resolve_fill_state(StyleState::active);
        if (_from_grab_style.id == Theme::STYLE_ID_INVALID)
            out |= resolve_grab_state(_from_grab_style, StyleState::normal);
        if (_to_grab_style.id == Theme::STYLE_ID_INVALID) out |= resolve_grab_state(_to_grab_style, StyleState::normal);

        const auto transition = detail::get_widget_style_selector_transition(id());
        const bool prev_is_grab = transition.prev_id.tag_id == _from_grab_style.tag_id;
        const bool curr_is_grab = transition.current_id.tag_id == _from_grab_style.tag_id;
        const bool curr_from = curr_is_grab && transition.current_id.element_id == static_cast<u32>(ActiveGrab::from);
        const bool curr_to = curr_is_grab && transition.current_id.element_id == static_cast<u32>(ActiveGrab::to);
        if (prev_is_grab && (!curr_is_grab || transition.current_id.element_id != transition.prev_id.element_id ||
                             transition.current_state != transition.prev_state))
        {
            if (transition.prev_id.element_id == static_cast<u32>(ActiveGrab::from))
                out |= resolve_grab_state(_from_grab_style, StyleState::normal);
            else if (transition.prev_id.element_id == static_cast<u32>(ActiveGrab::to))
                out |= resolve_grab_state(_to_grab_style, StyleState::normal);
        }
        if (curr_from) out |= resolve_grab_state(_from_grab_style, transition.current_state);
        if (curr_to) out |= resolve_grab_state(_to_grab_style, transition.current_state);

        if (_active_grab != ActiveGrab::none)
        {
            const bool widget_active = detail::get_context().active_id == id();
            if (_active_grab == ActiveGrab::from)
                out |= resolve_grab_state(_from_grab_style, widget_active ? StyleState::active : StyleState::normal);
            else out |= resolve_grab_state(_to_grab_style, widget_active ? StyleState::active : StyleState::normal);
        }
        else
        {
            if (!curr_from) out |= resolve_grab_state(_from_grab_style, StyleState::normal);
            if (!curr_to) out |= resolve_grab_state(_to_grab_style, StyleState::normal);
        }

        if (track_or_fill_changed) rebuild_track_visuals();
        if (grab_changed) rebuild_grab_visuals();
        return out;
    }

    void RangeSlider::update_layout_min_size()
    {
        const auto &track_style = get_theme()->get_style(_track_style.id);
        const amal::vec4 margin = track_style.margin();
        const amal::vec4 padding = track_style.padding();

        amal::vec2 min_size = size();
        if (!is_fixed()) min_size.x = 0.0f;
        else if (min_size.x <= 0.0f) min_size.x = 160.0f;

        const f32 min_track_h =
            amal::max(6.0f, track_style.border_radius() > 0.0f ? track_style.border_radius() * 2.0f : 0.0f);
        const f32 padded_h = padding.y + padding.w;
        const f32 total_h = padded_h > 0.0f ? padded_h : min_track_h;

        if (min_size.y <= 0.0f) min_size.y = total_h;
        else min_size.y = amal::max(min_size.y, total_h);
        if (min_size.x > 0.0f) min_size.x = amal::max(min_size.x, 24.0f + margin.x + margin.z);
        set_required_size({min_size.x + margin.x + margin.z, min_size.y + margin.y + margin.w});
    }
    void RangeSlider::update_layout(bool min_size_known)
    {
        if (!min_size_known) update_layout_min_size();

        const auto &style = get_theme()->get_style(_track_style.id);
        const amal::vec2 layout_origin = position();
        const amal::vec4 margin = style.margin();
        const amal::vec2 min_required = required_size();
        amal::vec2 slider_size = size();
        if (!is_fixed())
            slider_size.x = amal::max(slider_size.x - margin.x - margin.z, min_required.x - margin.x - margin.z);
        else slider_size.x = amal::max(slider_size.x, min_required.x - margin.x - margin.z);
        slider_size.y = amal::max(slider_size.y, min_required.y - margin.y - margin.w);

        const amal::vec2 pos = {layout_origin.x + margin.x, layout_origin.y + margin.y};
        set_position(pos);
        set_size(slider_size);
        Widget::update_layout(true);
        assert(parent() && "RangeSlider must have parent");
        set_clip_id(parent()->content_clip_id());
        _from_hit_rect.clip_id = clip_id();
        _to_hit_rect.clip_id = clip_id();

        rebuild_track_visuals();
        rebuild_grab_visuals();
    }

    void RangeSlider::translate(const amal::vec2 &delta)
    {
        if (delta.x == 0.0f && delta.y == 0.0f) return;
        Widget::translate(delta);
        detail::get_context().dirty_flags |= DirtyFlagBits::hit_rect_update;
        _track_rect.offset += delta;
        _from_rect.offset += delta;
        _to_rect.offset += delta;
        _from_hit_rect.bounds.offset += delta;
        _to_hit_rect.bounds.offset += delta;
        rebuild_track_visuals();
        rebuild_grab_visuals();
    }

    void RangeSlider::rebuild_clip_rects()
    {
        assert(parent() && "RangeSlider must have parent");
        set_clip_id(parent()->content_clip_id());
        _track_visual.background_draw_id.hit_id = AUIK_INVALID_DRAW_DATA_ID;
        _track_visual.fill_draw_id.hit_id = AUIK_INVALID_DRAW_DATA_ID;
        _from_draw_id.hit_id = AUIK_INVALID_DRAW_DATA_ID;
        _to_draw_id.hit_id = AUIK_INVALID_DRAW_DATA_ID;
        _from_hit_rect.clip_id = clip_id();
        _to_hit_rect.clip_id = clip_id();
        rebuild_track_visuals();
        rebuild_grab_visuals();
    }

    void RangeSlider::update_depth(const amal::vec2 &depth_range)
    {
        Widget::update_depth(depth_range);
        assign_next_depth(this->depth_range(), _track_depth_range);
        assign_next_depth(_track_depth_range, _grab_depth_range);
        rebuild_track_visuals();
        rebuild_grab_visuals();
    }

    void RangeSlider::draw(DrawCtx &ctx)
    {
        auto *quad_stream = get_primary_quads_stream();
        bool hit_pending = ctx.emit_hit_rect;
        const auto track_hit_rect = detail::make_slider_track_hit_rect(
            id(), _track_style.tag_id, _track_rect, clip_id(), detail::resolve_slider_track_hit_depth(depth_range()));

        if (ctx.is_recording() ||
            should_emit_quads_instance(_track_visual.has_layer(detail::SliderTrackVisual::LayerBits::background),
                                       _track_visual.background_draw_id, hit_pending))
        {
            ctx.emit(quad_stream, _track_visual.background_draw_id, &_track_visual.background, track_hit_rect,
                     hit_pending);
            hit_pending = false;
        }
        if (ctx.is_recording() ||
            should_emit_quads_instance(_track_visual.has_layer(detail::SliderTrackVisual::LayerBits::fill),
                                       _track_visual.fill_draw_id, hit_pending))
        {
            ctx.emit(quad_stream, _track_visual.fill_draw_id, &_track_visual.fill, track_hit_rect, hit_pending);
            hit_pending = false;
        }

        ctx.emit(quad_stream, _from_draw_id, &_from_visual, _from_hit_rect, ctx.emit_hit_rect);
        ctx.emit(quad_stream, _to_draw_id, &_to_visual, _to_hit_rect, ctx.emit_hit_rect);
    }

    bool RangeSlider::has_draw_record() const
    {
        if (_from_draw_id.render_id == AUIK_INVALID_DRAW_DATA_ID) return false;
        if (_to_draw_id.render_id == AUIK_INVALID_DRAW_DATA_ID) return false;
        if (_track_visual.background_draw_id.render_id == AUIK_INVALID_DRAW_DATA_ID) return false;
        if (_track_visual.fill_draw_id.render_id == AUIK_INVALID_DRAW_DATA_ID) return false;
        return true;
    }

    f32 RangeSlider::clamped_value(f32 value) const
    {
        f32 out = amal::clamp(value, _min_value, _max_value);
        if (_step > 0.0f)
        {
            const f32 normalized = (out - _min_value) / _step;
            out = _min_value + amal::round(normalized) * _step;
            out = amal::clamp(out, _min_value, _max_value);
        }
        return out;
    }

    void RangeSlider::set_values(f32 from_value, f32 to_value)
    {
        if (!_from_value || !_to_value) return;
        f32 from_clamped = clamped_value(from_value);
        f32 to_clamped = clamped_value(to_value);
        if (from_clamped > to_clamped) std::swap(from_clamped, to_clamped);
        const bool changed = (*_from_value != from_clamped) || (*_to_value != to_clamped);
        *_from_value = from_clamped;
        *_to_value = to_clamped;
        if (!changed) return;
        dispatch_change();
        detail::get_context().dirty_flags |= DirtyFlagBits::hit_rect_update;
        rebuild_track_visuals();
        rebuild_grab_visuals();
    }

    void RangeSlider::set_step(f32 step)
    {
        _step = amal::max(step, 0.0f);
        if (_from_value && _to_value) set_values(*_from_value, *_to_value);
    }

    amal::vec2 RangeSlider::resolve_grab_size(const Style &grab_style) const
    {
        const amal::vec4 grab_padding = grab_style.padding();
        return {amal::max(grab_padding.x + grab_padding.z, 1.0f), amal::max(grab_padding.y + grab_padding.w, 1.0f)};
    }

    f32 RangeSlider::resolve_grab_center_x(f32 value, f32 outer_half_grab_w) const
    {
        const f32 range = amal::max(_max_value - _min_value, 1e-5f);
        const f32 factor = amal::clamp((value - _min_value) / range, 0.0f, 1.0f);
        const f32 min_center_x = _track_rect.offset.x + outer_half_grab_w;
        const f32 max_center_x = _track_rect.offset.x + _track_rect.size.x - outer_half_grab_w;
        if (max_center_x > min_center_x) return min_center_x + (max_center_x - min_center_x) * factor;
        return _track_rect.offset.x + _track_rect.size.x * 0.5f;
    }

    void RangeSlider::rebuild_track_visuals()
    {
        if (_track_style.id == Theme::STYLE_ID_INVALID || _fill_style.id == Theme::STYLE_ID_INVALID) return;

        const auto &track_style = get_theme()->get_style(_track_style.id);
        _track_rect = detail::resolve_slider_track_rect(bounds(), track_style);

        _track_visual.clear_payload();
        _track_visual.background.rect = _track_rect;
        _track_visual.background.z_order = next_depth(_track_depth_range);
        if (fill_quads_instance_by_style(track_style, clip_id(), _track_visual.background))
            _track_visual.set_layer(detail::SliderTrackVisual::LayerBits::background);

        if (_from_value && _to_value)
        {
            const f32 from_clamped = amal::clamp(*_from_value, _min_value, _max_value);
            const f32 to_clamped = amal::clamp(*_to_value, _min_value, _max_value);
            const f32 left_value = amal::min(from_clamped, to_clamped);
            const f32 right_value = amal::max(from_clamped, to_clamped);
            const f32 full = amal::max(_max_value - _min_value, 1e-5f);
            const f32 left_t = amal::clamp((left_value - _min_value) / full, 0.0f, 1.0f);
            const f32 right_t = amal::clamp((right_value - _min_value) / full, 0.0f, 1.0f);
            const f32 left_x = _track_rect.offset.x + _track_rect.size.x * left_t;
            const f32 right_x = _track_rect.offset.x + _track_rect.size.x * right_t;
            const f32 fill_w = amal::max(right_x - left_x, 0.0f);
            if (fill_w > 0.0f)
            {
                const auto &fill_style = get_theme()->get_style(_fill_style.id);
                _track_visual.fill.rect = _track_rect;
                _track_visual.fill.rect.offset.x = left_x;
                _track_visual.fill.rect.size.x = fill_w;
                _track_visual.fill.z_order = next_depth(_track_depth_range);
                if (fill_quads_instance_by_style(fill_style, clip_id(), _track_visual.fill))
                    _track_visual.set_layer(detail::SliderTrackVisual::LayerBits::fill);
                if (!fill_style.has_visible_border())
                {
                    _track_visual.fill.border_thickness = 0.0f;
                    _track_visual.fill.mask &= ~(static_cast<u32>(AUIK_HAS_BORDER_BIT) << 20u);
                }
            }
        }
    }

    void RangeSlider::rebuild_grab_visuals()
    {
        if (!_from_value || !_to_value) return;
        if (_from_grab_style.id == Theme::STYLE_ID_INVALID || _to_grab_style.id == Theme::STYLE_ID_INVALID)
        {
            const u32 parent_id = parent() ? parent()->id() : 0u;
            if (_from_grab_style.id == Theme::STYLE_ID_INVALID)
                resolve_style_selector(_from_grab_style, _from_grab_style.tag_id, parent_id, StyleState::normal);
            if (_to_grab_style.id == Theme::STYLE_ID_INVALID)
                resolve_style_selector(_to_grab_style, _to_grab_style.tag_id, parent_id, StyleState::normal);
            if (_from_grab_style.id == Theme::STYLE_ID_INVALID || _to_grab_style.id == Theme::STYLE_ID_INVALID) return;
        }

        const auto &from_style = get_theme()->get_style(_from_grab_style.id);
        const auto &to_style = get_theme()->get_style(_to_grab_style.id);
        const amal::vec2 from_size = resolve_grab_size(from_style);
        const amal::vec2 to_size = resolve_grab_size(to_style);
        const f32 from_w = amal::max(amal::round(from_size.x), 3.0f);
        const f32 from_h = amal::max(amal::round(from_size.y), 3.0f);
        const f32 to_w = amal::max(amal::round(to_size.x), 3.0f);
        const f32 to_h = amal::max(amal::round(to_size.y), 3.0f);
        const f32 from_outer_half_w = detail::resolve_slider_grab_outer_half_width(from_style, from_w);
        const f32 to_outer_half_w = detail::resolve_slider_grab_outer_half_width(to_style, to_w);
        const f32 center_y = _track_rect.offset.y + _track_rect.size.y * 0.5f;
        const u16 grab_clip_id = clip_id();

        const f32 from_center_x = resolve_grab_center_x(*_from_value, from_outer_half_w);
        const f32 to_center_x = resolve_grab_center_x(*_to_value, to_outer_half_w);

        _from_rect = detail::resolve_slider_grab_rect(from_style, from_center_x, center_y, {from_w, from_h});
        _from_hit_rect.bounds = _from_rect;
        _from_hit_rect.depth = next_depth(_grab_depth_range);
        _from_visual.rect = _from_rect;
        _from_visual.z_order = next_depth(_grab_depth_range);
        fill_quads_instance_by_style(from_style, grab_clip_id, _from_visual);

        _to_rect = detail::resolve_slider_grab_rect(to_style, to_center_x, center_y, {to_w, to_h});
        _to_hit_rect.bounds = _to_rect;
        _to_hit_rect.depth = next_depth(_grab_depth_range);
        _to_visual.rect = _to_rect;
        _to_visual.z_order = next_depth(_grab_depth_range);
        fill_quads_instance_by_style(to_style, grab_clip_id, _to_visual);

        if (!from_style.has_visible_border())
        {
            _from_visual.border_thickness = 0.0f;
            _from_visual.mask &= ~(static_cast<u32>(AUIK_HAS_BORDER_BIT) << 20u);
        }
        if (!to_style.has_visible_border())
        {
            _to_visual.border_thickness = 0.0f;
            _to_visual.mask &= ~(static_cast<u32>(AUIK_HAS_BORDER_BIT) << 20u);
        }
    }

    void RangeSlider::rebuild_cached_visuals()
    {
        rebuild_track_visuals();
        rebuild_grab_visuals();
    }

    void RangeSlider::update_active_grab_from_mouse()
    {
        const auto hover_id = detail::get_context().hover_id;
        if (hover_id.widget_id == id() && hover_id.tag_id == _from_grab_style.tag_id &&
            hover_id.element_id == static_cast<u32>(ActiveGrab::from))
        {
            _active_grab = ActiveGrab::from;
            return;
        }
        if (hover_id.widget_id == id() && hover_id.tag_id == _to_grab_style.tag_id &&
            hover_id.element_id == static_cast<u32>(ActiveGrab::to))
        {
            _active_grab = ActiveGrab::to;
            return;
        }

        const f32 mouse_x = get_mouse_pos().x;
        const f32 track_mid_x = _track_rect.offset.x + _track_rect.size.x * 0.5f;
        _active_grab = mouse_x > track_mid_x ? ActiveGrab::to : ActiveGrab::from;
    }

    void RangeSlider::update_active_value_from_mouse()
    {
        if (!_from_value || !_to_value) return;
        if (_active_grab == ActiveGrab::none) return;
        const f32 width = amal::max(_track_rect.size.x, 1e-5f);
        const f32 mouse_x = get_mouse_pos().x - _drag_grab_offset_x;
        const f32 t = amal::clamp((mouse_x - _track_rect.offset.x) / width, 0.0f, 1.0f);
        const f32 target = _min_value + (_max_value - _min_value) * t;

        if (_active_grab == ActiveGrab::from) set_values(amal::min(target, *_to_value), *_to_value);
        else set_values(*_from_value, amal::max(target, *_from_value));
    }

    void RangeSlider::on_click(MouseKey key, KeyPressState state, u32 click_count)
    {
        (void)click_count;
        if (key != MouseKey::left) return;

        if (state == KeyPressState::press)
        {
            update_active_grab_from_mouse();
            const auto clicked_id = detail::get_context().io.clicked_id;
            const bool clicked_grab = clicked_id.widget_id == id() && clicked_id.tag_id == _from_grab_style.tag_id &&
                                      (clicked_id.element_id == static_cast<u32>(ActiveGrab::from) ||
                                       clicked_id.element_id == static_cast<u32>(ActiveGrab::to));
            if (!clicked_grab) update_active_value_from_mouse();
            add_render_command<detail::ClickEventTraits>(this, [this]() { redraw_external(has_draw_record()); });
            return;
        }

        if (state == KeyPressState::release)
        {
            _active_grab = ActiveGrab::none;
            add_render_command<detail::ClickEventTraits>(this, [this]() { redraw_external(has_draw_record()); });
        }
    }

    void RangeSlider::on_drag(const amal::vec2 &delta, KeyPressState state)
    {
        (void)delta;
        if (state == KeyPressState::press)
        {
            update_active_grab_from_mouse();
            const auto drag_id = detail::get_context().io.drag_id;
            const bool drag_from = drag_id.widget_id == id() && drag_id.tag_id == _from_grab_style.tag_id &&
                                   drag_id.element_id == static_cast<u32>(ActiveGrab::from);
            const bool drag_to = drag_id.widget_id == id() && drag_id.tag_id == _to_grab_style.tag_id &&
                                 drag_id.element_id == static_cast<u32>(ActiveGrab::to);
            if (drag_from) _drag_grab_offset_x = detail::resolve_drag_mouse_offset_x(_from_rect);
            else if (drag_to) _drag_grab_offset_x = detail::resolve_drag_mouse_offset_x(_to_rect);
            else
            {
                _drag_grab_offset_x = 0.0f;
                update_active_value_from_mouse();
            }
        }
        else if (state == KeyPressState::repeat)
        {
            if (_active_grab == ActiveGrab::none) update_active_grab_from_mouse();
            update_active_value_from_mouse();
        }
        else if (state == KeyPressState::release)
        {
            _active_grab = ActiveGrab::none;
            _drag_grab_offset_x = 0.0f;
        }
        else return;

        add_render_command<detail::DragEventTraits>(this, [this]() { redraw_external(has_draw_record()); });
    }
} // namespace auik::v2
