#include <auik/v2/auik.hpp>
#include <auik/v2/detail/depth.hpp>
#include <auik/v2/widgets/slider.hpp>

#define AUIK_TAG_RANGE_SLIDER_GRAB_FROM 0x0A9E4D31u
#define AUIK_TAG_RANGE_SLIDER_GRAB_TO   0x7D5C28B4u

namespace auik::v2
{
    namespace detail
    {
        static inline f32 mid_depth(const amal::vec2 &range) { return (range.x + range.y) * 0.5f; }

        static inline bool has_visible_border(const Style &style)
        {
            return style.border_thickness() > 0.0f && style.border_color().w > 0.0f;
        }

        static inline void fill_border_only_instance(const Style &style, const amal::rect &rect, f32 z_order,
                                                     u16 clip_id, QuadsInstanceData &data)
        {
            data.rect = rect;
            data.background_color = pack_rgba8(0, 0, 0, 0);
            data.border_color = style.border_color_packed();
            data.border_radius = style.border_radius();
            data.border_thickness = has_visible_border(style) ? style.border_thickness() : 0.0f;
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
                                                       u16 clip_id, const amal::vec4 &background_color,
                                                       f32 border_thickness, QuadsInstanceData &data)
        {
            data.rect = rect;
            data.z_order = z_order;
            fill_quads_instance_by_style(style, clip_id, data);
            data.background_color = pack_rgba8(background_color);
            data.border_color = pack_rgba8(0, 0, 0, 255);
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
            return (state == StyleState::active || state == StyleState::focus) ? state : StyleState::normal;
        }

        static inline StyleUpdateFlags resolve_style_and_mark_redraw(StyleSelector &selector, u32 self_id,
                                                                     u32 parent_id, StyleState state,
                                                                     bool &redraw_changed)
        {
            const auto flags = resolve_style_selector(selector, self_id, parent_id, state);
            if (flags & StyleUpdateFlagBits::redraw) redraw_changed = true;
            return flags;
        }

        void build_quad_slider_track_visual(SliderTrackVisual &visual, const Style &style, const amal::rect &track_rect,
                                            const amal::vec2 &depth_range, u16 clip_id)
        {
            visual.clear_payload();
            visual.set_layer(SliderTrackVisual::LayerBits::background);
            visual.background.rect = track_rect;
            visual.background.z_order = mid_depth(depth_range);
            fill_quads_instance_by_style(style, clip_id, visual.background);
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
                build_gradient_rect_vertex_data(gradient_visual.data, track_rect, mid_depth(depth_range), clip_id,
                                                colors, color_count, style.border_radius(), style.corner_mask(), 1.0f);

            if (has_visible_border(style))
            {
                visual.set_layer(SliderTrackVisual::LayerBits::border);
                amal::vec2 border_range{};
                assign_next_depth(depth_range, border_range);
                fill_border_only_instance(style, track_rect, mid_depth(border_range), clip_id, visual.border);
            }
        }
    } // namespace detail

    Slider::Slider(u32 id, f32 *value, f32 min_value, f32 max_value, f32 width, f32 *range_start_value,
                   WidgetFlags widget_flags, Widget *parent)
        : Widget(id, widget_flags, EventFlagBits::hover | EventFlagBits::click | EventFlagBits::drag, parent,
                 {{0.0f, 0.0f}, {width, 0.0f}}, AUIK_TAG_SLIDER),
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
            out |= detail::resolve_style_and_mark_redraw(_grab_style, _grab_style.tag_id, parent_id, StyleState::normal,
                                                         grab_changed);

        const auto transition = detail::get_widget_style_selector_transition(id());
        if (transition.prev_tag_id == _grab_style.tag_id &&
            (transition.current_tag_id != _grab_style.tag_id || transition.prev_state != transition.current_state))
            out |= detail::resolve_style_and_mark_redraw(_grab_style, _grab_style.tag_id, parent_id, StyleState::normal,
                                                         grab_changed);
        if (transition.current_tag_id == _grab_style.tag_id)
            out |= detail::resolve_style_and_mark_redraw(_grab_style, _grab_style.tag_id, parent_id,
                                                         transition.current_state, grab_changed);

        const StyleState widget_grab_state = detail::resolve_grab_visual_state(style_state());
        if (widget_grab_state == StyleState::active || widget_grab_state == StyleState::focus)
            out |= detail::resolve_style_and_mark_redraw(_grab_style, _grab_style.tag_id, parent_id, widget_grab_state,
                                                         grab_changed);

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
        const amal::vec2 cursor = detail::get_context().screen_cursor;
        const amal::vec4 margin = style.margin();
        const amal::vec2 min_required = required_size();
        amal::vec2 slider_size = size();
        if (!is_fixed())
            slider_size.x = amal::max(slider_size.x - margin.x - margin.z, min_required.x - margin.x - margin.z);
        else slider_size.x = amal::max(slider_size.x, min_required.x - margin.x - margin.z);
        slider_size.y = amal::max(slider_size.y, min_required.y - margin.y - margin.w);

        const amal::vec2 pos = {cursor.x + margin.x, cursor.y + margin.y};
        set_position(pos);
        set_size(slider_size);
        Widget::update_layout(true);
        inherit_parent_content_clip_rect();

        rebuild_track_visuals();
        rebuild_grab_visual();
        detail::get_context().screen_cursor = {cursor.x, pos.y + slider_size.y + margin.w};
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
        inherit_parent_content_clip_rect();
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

        if (_track_visual.has_layer(detail::SliderTrackVisual::LayerBits::background))
        {
            ctx.emit(quad_stream, _track_visual.background_draw_id, &_track_visual.background, get_rect(), hit_pending);
            hit_pending = false;
        }
        if (_track_visual.has_layer(detail::SliderTrackVisual::LayerBits::fill))
        {
            ctx.emit(quad_stream, _track_visual.fill_draw_id, &_track_visual.fill, get_rect(), hit_pending);
            hit_pending = false;
        }
        if (_track_visual.has_layer(detail::SliderTrackVisual::LayerBits::border))
        {
            ctx.emit(quad_stream, _track_visual.border_draw_id, &_track_visual.border, get_rect(), hit_pending);
            hit_pending = false;
        }
        ctx.emit(quad_stream, _grab_draw_id, &_grab_visual, _grab_hit_rect, ctx.emit_hit_rect);
    }

    bool Slider::has_draw_record() const
    {
        if (_grab_draw_id.render_id == AUIK_INVALID_DRAW_DATA_ID) return false;
        if (_track_visual.has_layer(detail::SliderTrackVisual::LayerBits::background) &&
            _track_visual.background_draw_id.render_id == AUIK_INVALID_DRAW_DATA_ID)
            return false;
        if (_track_visual.has_layer(detail::SliderTrackVisual::LayerBits::fill) &&
            _track_visual.fill_draw_id.render_id == AUIK_INVALID_DRAW_DATA_ID)
            return false;
        if (_track_visual.has_layer(detail::SliderTrackVisual::LayerBits::border) &&
            _track_visual.border_draw_id.render_id == AUIK_INVALID_DRAW_DATA_ID)
            return false;
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
        const f32 t = amal::clamp((get_mouse_pos().x - _track_rect.offset.x) / width, 0.0f, 1.0f);
        set_value(_min_value + (_max_value - _min_value) * t);
    }

    void Slider::on_hover(HoverState state) { (void)state; }

    void Slider::on_click(MouseKey key, KeyPressState state, u32 click_count)
    {
        (void)click_count;
        if (key != MouseKey::left) return;

        if (state == KeyPressState::press)
        {
            update_value_from_mouse();
            add_render_command<detail::ClickEventTraits>(this, [this]() {
                if (has_draw_record()) update_draw_commands(DrawReasonBits::external);
                else record_draw_commands(DrawReasonBits::external);
                detail::get_context().dirty_flags |= DirtyFlagBits::redraw;
            });
            return;
        }

        if (state == KeyPressState::release)
        {
            add_render_command<detail::ClickEventTraits>(this, [this]() {
                if (has_draw_record()) update_draw_commands(DrawReasonBits::external);
                else record_draw_commands(DrawReasonBits::external);
                detail::get_context().dirty_flags |= DirtyFlagBits::redraw;
            });
        }
    }

    void Slider::on_drag(const amal::vec2 &delta, KeyPressState state)
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
        else if (state == KeyPressState::release) { _drag_started = false; }
        else return;

        add_render_command<detail::DragEventTraits>(this, [this]() {
            if (has_draw_record()) update_draw_commands(DrawReasonBits::external);
            else record_draw_commands(DrawReasonBits::external);
            detail::get_context().dirty_flags |= DirtyFlagBits::redraw;
        });
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
        _track_visual.set_layer(detail::SliderTrackVisual::LayerBits::fill);
        _track_visual.fill.rect = _track_rect;
        _track_visual.fill.rect.offset.x = left_x;
        _track_visual.fill.rect.size.x = fill_w > 0.0f ? fill_w : 1.0f;
        _track_visual.fill.z_order = detail::mid_depth(_track_depth_range);
        fill_quads_instance_by_style(fill_style, clip_id(), _track_visual.fill);
        if (fill_w <= 0.0f || amal::is_rect_empty(_track_rect))
        {
            _track_visual.fill.background_color = detail::pack_rgba8(0, 0, 0, 0);
            _track_visual.fill.border_color = detail::pack_rgba8(0, 0, 0, 0);
            _track_visual.fill.border_thickness = 0.0f;
            _track_visual.fill.mask &= ~(static_cast<u32>(AUIK_HAS_BORDER_BIT) << 20u);
            return;
        }

        if (!detail::has_visible_border(fill_style))
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
        const f32 half_grab_w = grab_w * 0.5f;
        const f32 half_grab_h = grab_h * 0.5f;
        const f32 min_center_x = _track_rect.offset.x + half_grab_w;
        const f32 max_center_x = _track_rect.offset.x + _track_rect.size.x - half_grab_w;
        f32 center_x = _track_rect.offset.x + _track_rect.size.x * factor;
        if (max_center_x > min_center_x) center_x = min_center_x + (max_center_x - min_center_x) * factor;
        else center_x = _track_rect.offset.x + _track_rect.size.x * 0.5f;
        const f32 center_y = _track_rect.offset.y + _track_rect.size.y * 0.5f;
        f32 grab_x = center_x - half_grab_w;
        _grab_rect.offset.x = grab_x;
        _grab_rect.offset.y = amal::round(center_y - half_grab_h);
        _grab_rect.size.x = grab_w;
        _grab_rect.size.y = grab_h;
        _grab_hit_rect.bounds = _grab_rect;
        _grab_hit_rect.depth = detail::mid_depth(_grab_depth_range);
        _grab_hit_rect.clip_id = parent() ? parent()->clip_id() : clip_id();

        const u16 grab_clip_id = parent() ? parent()->clip_id() : clip_id();
        const f32 grab_z = detail::mid_depth(_grab_depth_range);
        _grab_visual.rect = _grab_rect;
        _grab_visual.z_order = grab_z;
        fill_quads_instance_by_style(grab_style, grab_clip_id, _grab_visual);
        if (!detail::has_visible_border(grab_style))
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
                                   const amal::vec4 *colors, u32 color_count, WidgetFlags widget_flags, Widget *parent,
                                   GradientTrackKind gradient_kind)
        : Widget(id, widget_flags, EventFlagBits::hover | EventFlagBits::click | EventFlagBits::drag, parent,
                 {{0.0f, 0.0f}, {width, 0.0f}}, AUIK_TAG_GRADIENT_SLIDER),
          _value(value),
          _min_value(min_value),
          _max_value(max_value),
          _gradient_kind(gradient_kind)
    {
        _track_style.tag_id = AUIK_TAG_GRADIENT_SLIDER;
        _grab_style.tag_id = AUIK_TAG_GRADIENT_SLIDER_GRAB;
        _grab_hit_rect = detail::make_rect_data(id, _grab_style.tag_id);
        _colors.reserve(color_count);
        for (u32 i = 0; i < color_count; ++i) _colors.push_back(colors[i]);
        if (_gradient_kind == GradientTrackKind::hsl)
        {
            constexpr u32 width_hsl = 360u;
            _hsl_cache.resize(width_hsl);
            for (u32 i = 0; i < width_hsl; ++i)
                _hsl_cache[i] = amal::hsl_to_rgba(static_cast<f32>(i), 1.0f, 0.5f);
        }
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
            const auto flags = resolve_style_selector(_grab_style, _grab_style.tag_id, parent_id, state);
            if (flags & StyleUpdateFlagBits::redraw) grab_changed = true;
            return flags;
        };

        if (_track_style.id == Theme::STYLE_ID_INVALID) out |= resolve_track_state(StyleState::normal);
        if (_grab_style.id == Theme::STYLE_ID_INVALID) out |= resolve_grab_state(StyleState::normal);

        const auto transition = detail::get_widget_style_selector_transition(id());
        if (transition.prev_tag_id == _grab_style.tag_id &&
            (transition.current_tag_id != _grab_style.tag_id || transition.prev_state != transition.current_state))
            out |= resolve_grab_state(StyleState::normal);
        if (transition.current_tag_id == _grab_style.tag_id) out |= resolve_grab_state(transition.current_state);

        const StyleState widget_grab_state = detail::resolve_grab_visual_state(style_state());
        if (widget_grab_state == StyleState::active || widget_grab_state == StyleState::focus)
            out |= resolve_grab_state(widget_grab_state);

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
        amal::vec4 color = detail::sample_gradient_color(_colors.data(), static_cast<u32>(_colors.size()), factor);
        if (_gradient_kind != GradientTrackKind::hsl) return color;
        if (_hsl_cache.empty()) return color;
        const u32 index = amal::clamp(static_cast<u32>(amal::round(factor * 359.0f)), 0u, 359u);
        return _hsl_cache[index];
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
        const amal::vec2 cursor = detail::get_context().screen_cursor;
        const amal::vec4 margin = style.margin();
        const amal::vec2 min_required = required_size();
        amal::vec2 slider_size = size();
        if (!is_fixed())
            slider_size.x = amal::max(slider_size.x - margin.x - margin.z, min_required.x - margin.x - margin.z);
        else slider_size.x = amal::max(slider_size.x, min_required.x - margin.x - margin.z);
        slider_size.y = amal::max(slider_size.y, min_required.y - margin.y - margin.w);

        const amal::vec2 pos = {cursor.x + margin.x, cursor.y + margin.y};
        set_position(pos);
        set_size(slider_size);
        Widget::update_layout(true);
        inherit_parent_content_clip_rect();

        rebuild_track_visuals();
        rebuild_grab_visual();
        detail::get_context().screen_cursor = {cursor.x, pos.y + slider_size.y + margin.w};
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
        inherit_parent_content_clip_rect();
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

        if (_track_visual.has_layer(detail::SliderTrackVisual::LayerBits::background))
        {
            ctx.emit(quad_stream, _track_visual.background_draw_id, &_track_visual.background, get_rect(), hit_pending);
            hit_pending = false;
        }
        if (_gradient_visual.valid)
        {
            ctx.emit(vertex_stream, _gradient_visual.draw_id, &_gradient_visual.data.batch, get_rect(), hit_pending);
            hit_pending = false;
        }
        if (_track_visual.has_layer(detail::SliderTrackVisual::LayerBits::border))
        {
            ctx.emit(quad_stream, _track_visual.border_draw_id, &_track_visual.border, get_rect(), hit_pending);
            hit_pending = false;
        }
        if (_grab_back_visual.rect.size.x > 0.0f && _grab_back_visual.rect.size.y > 0.0f)
            ctx.emit(overlay_quad_stream, _grab_back_draw_id, &_grab_back_visual, _grab_hit_rect, false);
        ctx.emit(overlay_quad_stream, _grab_draw_id, &_grab_visual, _grab_hit_rect, ctx.emit_hit_rect);
    }

    bool GradientSlider::has_draw_record() const
    {
        if (_grab_draw_id.render_id == AUIK_INVALID_DRAW_DATA_ID) return false;
        if (_grab_back_visual.rect.size.x > 0.0f && _grab_back_visual.rect.size.y > 0.0f &&
            _grab_back_draw_id.render_id == AUIK_INVALID_DRAW_DATA_ID)
            return false;
        if (_track_visual.has_layer(detail::SliderTrackVisual::LayerBits::background) &&
            _track_visual.background_draw_id.render_id == AUIK_INVALID_DRAW_DATA_ID)
            return false;
        if (_track_visual.has_layer(detail::SliderTrackVisual::LayerBits::border) &&
            _track_visual.border_draw_id.render_id == AUIK_INVALID_DRAW_DATA_ID)
            return false;
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
        const f32 t = amal::clamp((get_mouse_pos().x - _track_rect.offset.x) / width, 0.0f, 1.0f);
        set_value(_min_value + (_max_value - _min_value) * t);
    }

    void GradientSlider::on_hover(HoverState state) { (void)state; }

    void GradientSlider::on_click(MouseKey key, KeyPressState state, u32 click_count)
    {
        (void)click_count;
        if (key != MouseKey::left) return;

        if (state == KeyPressState::press)
        {
            update_value_from_mouse();
            add_render_command<detail::ClickEventTraits>(this, [this]() {
                if (has_draw_record()) update_draw_commands(DrawReasonBits::external);
                else record_draw_commands(DrawReasonBits::external);
                detail::get_context().dirty_flags |= DirtyFlagBits::redraw;
            });
            return;
        }

        if (state == KeyPressState::release)
        {
            add_render_command<detail::ClickEventTraits>(this, [this]() {
                if (has_draw_record()) update_draw_commands(DrawReasonBits::external);
                else record_draw_commands(DrawReasonBits::external);
                detail::get_context().dirty_flags |= DirtyFlagBits::redraw;
            });
        }
    }

    void GradientSlider::on_drag(const amal::vec2 &delta, KeyPressState state)
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
        else if (state == KeyPressState::release) { _drag_started = false; }
        else return;

        add_render_command<detail::DragEventTraits>(this, [this]() {
            if (has_draw_record()) update_draw_commands(DrawReasonBits::external);
            else record_draw_commands(DrawReasonBits::external);
            detail::get_context().dirty_flags |= DirtyFlagBits::redraw;
        });
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
        const f32 half_grab_w = grab_w * 0.5f;
        const f32 half_grab_h = grab_h * 0.5f;
        const f32 min_center_x = _track_rect.offset.x + half_grab_w;
        const f32 max_center_x = _track_rect.offset.x + _track_rect.size.x - half_grab_w;
        f32 center_x = _track_rect.offset.x + _track_rect.size.x * factor;
        if (max_center_x > min_center_x) center_x = min_center_x + (max_center_x - min_center_x) * factor;
        else center_x = _track_rect.offset.x + _track_rect.size.x * 0.5f;
        center_x = detail::apply_edge_grab_bias(center_x, factor);
        const f32 center_y = _track_rect.offset.y + _track_rect.size.y * 0.5f;
        _grab_rect.offset.x = center_x - half_grab_w;
        _grab_rect.offset.y = amal::round(center_y - half_grab_h);
        _grab_rect.size.x = grab_w;
        _grab_rect.size.y = grab_h;
        _grab_hit_rect.bounds = _grab_rect;
        _grab_hit_rect.depth = detail::mid_depth(_grab_depth_range);
        _grab_hit_rect.clip_id = parent() ? parent()->clip_id() : clip_id();

        const u16 grab_clip_id = parent() ? parent()->clip_id() : clip_id();
        const f32 grab_z = detail::mid_depth(_grab_depth_range);
        _grab_visual.rect = _grab_rect;
        _grab_visual.z_order = grab_z;
        _grab_back_visual = {};

        if (const Style *border_style = get_theme()->get_desc_style(AUIK_TAG_GRADIENT_SLIDER_GRAB_BORDER))
        {
            const amal::vec2 border_size = resolve_grab_size(*border_style);
            const f32 border_w = amal::max(amal::round(border_size.x), _grab_rect.size.x);
            const f32 border_h = amal::max(amal::round(border_size.y), _grab_rect.size.y);
            amal::rect border_rect{};
            border_rect.size = {border_w, border_h};
            border_rect.offset.x = center_x - border_w * 0.5f;
            border_rect.offset.y = amal::round(center_y - border_h * 0.5f);
            detail::fill_gradient_grab_instance(*border_style, border_rect, grab_z, grab_clip_id,
                                                border_style->background_color(), border_style->border_thickness(),
                                                _grab_back_visual);
        }

        detail::fill_gradient_grab_instance(grab_style, _grab_rect, grab_z, grab_clip_id, resolve_active_color(factor),
                                            grab_style.border_thickness(), _grab_visual);
    }

    void GradientSlider::rebuild_cached_visuals()
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
        _grab_style.tag_id = AUIK_TAG_RANGE_SLIDER_GRAB;
        _from_hit_rect = detail::make_rect_data(id, AUIK_TAG_RANGE_SLIDER_GRAB_FROM);
        _to_hit_rect = detail::make_rect_data(id, AUIK_TAG_RANGE_SLIDER_GRAB_TO);
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

        auto resolve_grab_state = [&](u32 grab_tag, StyleState state) -> StyleUpdateFlags {
            const auto flags = resolve_style_selector(_grab_style, grab_tag, parent_id,
                                                      detail::resolve_grab_visual_state(state));
            if (flags & StyleUpdateFlagBits::redraw) grab_changed = true;
            return flags;
        };

        if (_track_style.id == Theme::STYLE_ID_INVALID) out |= resolve_track_state(StyleState::normal);
        if (_fill_style.id == Theme::STYLE_ID_INVALID) out |= resolve_fill_state(StyleState::active);
        if (_grab_style.id == Theme::STYLE_ID_INVALID)
            out |= resolve_grab_state(AUIK_TAG_RANGE_SLIDER_GRAB_FROM, StyleState::normal);

        const auto transition = detail::get_widget_style_selector_transition(id());
        const bool prev_is_grab = transition.prev_tag_id == AUIK_TAG_RANGE_SLIDER_GRAB_FROM ||
                                  transition.prev_tag_id == AUIK_TAG_RANGE_SLIDER_GRAB_TO;
        const bool curr_is_grab = transition.current_tag_id == AUIK_TAG_RANGE_SLIDER_GRAB_FROM ||
                                  transition.current_tag_id == AUIK_TAG_RANGE_SLIDER_GRAB_TO;
        if (prev_is_grab &&
            (!curr_is_grab || transition.current_tag_id != transition.prev_tag_id ||
             transition.current_state != transition.prev_state))
            out |= resolve_grab_state(transition.prev_tag_id, StyleState::normal);
        if (curr_is_grab) out |= resolve_grab_state(transition.current_tag_id, transition.current_state);

        if (_active_grab != ActiveGrab::none)
        {
            const u32 active_tag =
                (_active_grab == ActiveGrab::to) ? AUIK_TAG_RANGE_SLIDER_GRAB_TO : AUIK_TAG_RANGE_SLIDER_GRAB_FROM;
            out |= resolve_grab_state(active_tag, style_state());
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
        const amal::vec2 cursor = detail::get_context().screen_cursor;
        const amal::vec4 margin = style.margin();
        const amal::vec2 min_required = required_size();
        amal::vec2 slider_size = size();
        if (!is_fixed())
            slider_size.x = amal::max(slider_size.x - margin.x - margin.z, min_required.x - margin.x - margin.z);
        else slider_size.x = amal::max(slider_size.x, min_required.x - margin.x - margin.z);
        slider_size.y = amal::max(slider_size.y, min_required.y - margin.y - margin.w);

        const amal::vec2 pos = {cursor.x + margin.x, cursor.y + margin.y};
        set_position(pos);
        set_size(slider_size);
        Widget::update_layout(true);
        inherit_parent_content_clip_rect();

        rebuild_track_visuals();
        rebuild_grab_visuals();
        detail::get_context().screen_cursor = {cursor.x, pos.y + slider_size.y + margin.w};
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
        inherit_parent_content_clip_rect();
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

        if (_track_visual.has_layer(detail::SliderTrackVisual::LayerBits::background))
        {
            ctx.emit(quad_stream, _track_visual.background_draw_id, &_track_visual.background, get_rect(), hit_pending);
            hit_pending = false;
        }
        if (_track_visual.has_layer(detail::SliderTrackVisual::LayerBits::fill))
        {
            ctx.emit(quad_stream, _track_visual.fill_draw_id, &_track_visual.fill, get_rect(), hit_pending);
            hit_pending = false;
        }

        ctx.emit(quad_stream, _from_draw_id, &_from_visual, _from_hit_rect, ctx.emit_hit_rect);
        ctx.emit(quad_stream, _to_draw_id, &_to_visual, _to_hit_rect, ctx.emit_hit_rect);
    }

    bool RangeSlider::has_draw_record() const
    {
        if (_from_draw_id.render_id == AUIK_INVALID_DRAW_DATA_ID) return false;
        if (_to_draw_id.render_id == AUIK_INVALID_DRAW_DATA_ID) return false;
        if (_track_visual.has_layer(detail::SliderTrackVisual::LayerBits::background) &&
            _track_visual.background_draw_id.render_id == AUIK_INVALID_DRAW_DATA_ID)
            return false;
        if (_track_visual.has_layer(detail::SliderTrackVisual::LayerBits::fill) &&
            _track_visual.fill_draw_id.render_id == AUIK_INVALID_DRAW_DATA_ID)
            return false;
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

    f32 RangeSlider::resolve_grab_center_x(f32 value, f32 half_grab_w) const
    {
        const f32 range = amal::max(_max_value - _min_value, 1e-5f);
        const f32 factor = amal::clamp((value - _min_value) / range, 0.0f, 1.0f);
        const f32 min_center_x = _track_rect.offset.x + half_grab_w;
        const f32 max_center_x = _track_rect.offset.x + _track_rect.size.x - half_grab_w;
        if (max_center_x > min_center_x) return min_center_x + (max_center_x - min_center_x) * factor;
        return _track_rect.offset.x + _track_rect.size.x * 0.5f;
    }

    void RangeSlider::rebuild_track_visuals()
    {
        if (_track_style.id == Theme::STYLE_ID_INVALID || _fill_style.id == Theme::STYLE_ID_INVALID) return;

        const auto &track_style = get_theme()->get_style(_track_style.id);
        _track_rect = detail::resolve_slider_track_rect(bounds(), track_style);

        _track_visual.clear_payload();
        _track_visual.set_layer(detail::SliderTrackVisual::LayerBits::background);
        _track_visual.background.rect = _track_rect;
        _track_visual.background.z_order = detail::mid_depth(_track_depth_range);
        fill_quads_instance_by_style(track_style, clip_id(), _track_visual.background);

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
                _track_visual.set_layer(detail::SliderTrackVisual::LayerBits::fill);
                _track_visual.fill.rect = _track_rect;
                _track_visual.fill.rect.offset.x = left_x;
                _track_visual.fill.rect.size.x = fill_w;
                _track_visual.fill.z_order = detail::mid_depth(_track_depth_range);
                fill_quads_instance_by_style(fill_style, clip_id(), _track_visual.fill);
                if (!detail::has_visible_border(fill_style))
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
        if (_grab_style.id == Theme::STYLE_ID_INVALID)
        {
            const u32 parent_id = parent() ? parent()->id() : 0u;
            resolve_style_selector(_grab_style, AUIK_TAG_RANGE_SLIDER_GRAB_FROM, parent_id, StyleState::normal);
            if (_grab_style.id == Theme::STYLE_ID_INVALID) return;
        }

        const auto &grab_style = get_theme()->get_style(_grab_style.id);
        const amal::vec2 grab_size = resolve_grab_size(grab_style);
        const f32 grab_w = amal::max(amal::round(grab_size.x), 3.0f);
        const f32 grab_h = amal::max(amal::round(grab_size.y), 3.0f);
        const f32 half_grab_w = grab_w * 0.5f;
        const f32 half_grab_h = grab_h * 0.5f;
        const f32 center_y = _track_rect.offset.y + _track_rect.size.y * 0.5f;
        const u16 grab_clip_id = parent() ? parent()->clip_id() : clip_id();

        const f32 from_center_x = resolve_grab_center_x(*_from_value, half_grab_w);
        const f32 to_center_x = resolve_grab_center_x(*_to_value, half_grab_w);

        f32 from_x = from_center_x - half_grab_w;
        _from_rect.offset.x = from_x;
        _from_rect.offset.y = amal::round(center_y - half_grab_h);
        _from_rect.size.x = grab_w;
        _from_rect.size.y = grab_h;
        _from_hit_rect.bounds = _from_rect;
        _from_hit_rect.depth = detail::mid_depth(_grab_depth_range);
        _from_hit_rect.clip_id = grab_clip_id;
        _from_visual.rect = _from_rect;
        _from_visual.z_order = detail::mid_depth(_grab_depth_range);
        fill_quads_instance_by_style(grab_style, grab_clip_id, _from_visual);

        f32 to_x = to_center_x - half_grab_w;
        _to_rect.offset.x = to_x;
        _to_rect.offset.y = amal::round(center_y - half_grab_h);
        _to_rect.size.x = grab_w;
        _to_rect.size.y = grab_h;
        _to_hit_rect.bounds = _to_rect;
        _to_hit_rect.depth = detail::mid_depth(_grab_depth_range);
        _to_hit_rect.clip_id = grab_clip_id;
        _to_visual.rect = _to_rect;
        _to_visual.z_order = detail::mid_depth(_grab_depth_range);
        fill_quads_instance_by_style(grab_style, grab_clip_id, _to_visual);

        if (!detail::has_visible_border(grab_style))
        {
            _from_visual.border_thickness = 0.0f;
            _from_visual.mask &= ~(static_cast<u32>(AUIK_HAS_BORDER_BIT) << 20u);
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
        const u32 tag = detail::get_context().hover_id.tag_id;
        if (tag == AUIK_TAG_RANGE_SLIDER_GRAB_FROM)
        {
            _active_grab = ActiveGrab::from;
            return;
        }
        if (tag == AUIK_TAG_RANGE_SLIDER_GRAB_TO)
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
        const f32 t = amal::clamp((get_mouse_pos().x - _track_rect.offset.x) / width, 0.0f, 1.0f);
        const f32 target = _min_value + (_max_value - _min_value) * t;

        if (_active_grab == ActiveGrab::from) set_values(amal::min(target, *_to_value), *_to_value);
        else set_values(*_from_value, amal::max(target, *_from_value));
    }

    void RangeSlider::on_hover(HoverState state) { (void)state; }

    void RangeSlider::on_click(MouseKey key, KeyPressState state, u32 click_count)
    {
        (void)click_count;
        if (key != MouseKey::left) return;

        if (state == KeyPressState::press)
        {
            update_active_grab_from_mouse();
            update_active_value_from_mouse();
            add_render_command<detail::ClickEventTraits>(this, [this]() {
                if (has_draw_record()) update_draw_commands(DrawReasonBits::external);
                else record_draw_commands(DrawReasonBits::external);
                detail::get_context().dirty_flags |= DirtyFlagBits::redraw;
            });
            return;
        }

        if (state == KeyPressState::release)
        {
            _active_grab = ActiveGrab::none;
            add_render_command<detail::ClickEventTraits>(this, [this]() {
                if (has_draw_record()) update_draw_commands(DrawReasonBits::external);
                else record_draw_commands(DrawReasonBits::external);
                detail::get_context().dirty_flags |= DirtyFlagBits::redraw;
            });
        }
    }

    void RangeSlider::on_drag(const amal::vec2 &delta, KeyPressState state)
    {
        if (state == KeyPressState::press)
        {
            update_active_grab_from_mouse();
            update_active_value_from_mouse();
        }
        else if (state == KeyPressState::repeat)
        {
            if (_active_grab == ActiveGrab::none) update_active_grab_from_mouse();
            update_active_value_from_mouse();
        }
        else if (state == KeyPressState::release) _active_grab = ActiveGrab::none;
        else return;

        add_render_command<detail::DragEventTraits>(this, [this]() {
            if (has_draw_record()) update_draw_commands(DrawReasonBits::external);
            else record_draw_commands(DrawReasonBits::external);
            detail::get_context().dirty_flags |= DirtyFlagBits::redraw;
        });
    }
} // namespace auik::v2
