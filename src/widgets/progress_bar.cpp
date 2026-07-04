#include <auik/auik.hpp>
#include <auik/detail/depth.hpp>
#include <auik/widgets/progress_bar.hpp>
#include "../core/session_stream_utils.hpp"

#define AUIK_PROGRESS_BAR_SCALE_DURATION 0.18

namespace auik
{
    namespace
    {
        static inline amal::vec2 make_progress_bar_style_size(f32 length, amal::axis axis)
        {
            return axis == amal::axis::y ? amal::vec2{0.0f, length} : amal::vec2{length, 0.0f};
        }

        static inline amal::rect resolve_progress_bar_track_rect(const amal::rect &bounds, const Style &style,
                                                                 amal::axis axis)
        {
            const amal::vec4 padding = style.padding();
            const f32 min_track_size =
                amal::max(6.0f, style.border_radius() > 0.0f ? style.border_radius() * 2.0f : 0.0f);
            amal::rect out = bounds;
            if (axis == amal::axis::y)
            {
                const f32 padded_w = padding.x + padding.z;
                const f32 desired_w = padded_w > 0.0f ? padded_w : min_track_size;
                const f32 track_w = amal::min(bounds.size.x, desired_w);
                out.offset.x += (bounds.size.x - track_w) * 0.5f;
                out.size.x = track_w;
            }
            else
            {
                const f32 padded_h = padding.y + padding.w;
                const f32 desired_h = padded_h > 0.0f ? padded_h : min_track_size;
                const f32 track_h = amal::min(bounds.size.y, desired_h);
                out.offset.y += (bounds.size.y - track_h) * 0.5f;
                out.size.y = track_h;
            }
            return out;
        }

        static inline amal::vec2 resolve_progress_bar_min_body_size(const Widget &widget, const Style &style,
                                                                    amal::axis axis)
        {
            const amal::vec4 margin = style.margin();
            const amal::vec4 padding = style.padding();
            amal::vec2 min_size = {is_size_concrete(widget.style_size().x) ? widget.style_size().x : 0.0f,
                                   is_size_concrete(widget.style_size().y) ? widget.style_size().y : 0.0f};
            const f32 min_track_size =
                amal::max(6.0f, style.border_radius() > 0.0f ? style.border_radius() * 2.0f : 0.0f);
            const f32 padded_cross = axis == amal::axis::y ? padding.x + padding.z : padding.y + padding.w;
            const f32 cross_size = padded_cross > 0.0f ? padded_cross : min_track_size;

            if (axis == amal::axis::y)
            {
                if (!widget.is_height_fixed() && !widget.fill_height()) min_size.y = 0.0f;
                else if (min_size.y <= 0.0f) min_size.y = 160.0f;
                if (widget.fill_width()) min_size.x = 0.0f;
                if (min_size.x <= 0.0f) min_size.x = cross_size;
                else min_size.x = amal::max(min_size.x, cross_size);
                if (min_size.y > 0.0f) min_size.y = amal::max(min_size.y, 24.0f + margin.y + margin.w);
            }
            else
            {
                if (!widget.is_width_fixed() && !widget.fill_width()) min_size.x = 0.0f;
                else if (min_size.x <= 0.0f) min_size.x = 160.0f;
                if (widget.fill_height()) min_size.y = 0.0f;
                if (min_size.y <= 0.0f) min_size.y = cross_size;
                else min_size.y = amal::max(min_size.y, cross_size);
                if (min_size.x > 0.0f) min_size.x = amal::max(min_size.x, 24.0f + margin.x + margin.z);
            }
            return min_size;
        }
    } // namespace

    ProgressBar::ProgressBar(u32 id, f32 value, f32 min_value, f32 max_value, f32 size, amal::axis axis,
                             WidgetFlags widget_flags)
        : Widget(id, widget_flags, EventFlagBits::change,
                 {{0.0f, 0.0f}, make_progress_bar_style_size(size, axis)}, AUIK_STYLE_TAG_PROGRESS_BAR),
          _min_value(min_value),
          _max_value(max_value),
          _axis(axis)
    {
        _track_style.tag_id = AUIK_STYLE_TAG_PROGRESS_BAR;
        _active_style.tag_id = AUIK_STYLE_TAG_PROGRESS_BAR_ACTIVE;
        if (_max_value < _min_value) std::swap(_min_value, _max_value);
        _value = clamped_value(value);
        _display_value = _value;
        _change_from_value = _value;
    }

    ProgressBar::ProgressBar(u32 id, ModelBinding *binding, f32 min_value, f32 max_value, f32 size,
                             amal::axis axis, WidgetFlags widget_flags)
        : ProgressBar(id, 0.0f, min_value, max_value, size, axis, widget_flags)
    {
        set_model_binding(binding);
    }

    ProgressBar::~ProgressBar()
    {
        _animation.clear(this);
        erase_widget_from_transient_cache(this);
        cancel_delayed_tasks(id());
    }

    StyleUpdateFlags ProgressBar::update_style()
    {
        const u32 parent_id = parent() ? parent()->id() : 0u;
        StyleUpdateFlags out = StyleUpdateFlagBits::none;
        bool redraw_changed = false;
        if (_track_style.id == Theme::STYLE_ID_INVALID)
        {
            const auto flags = resolve_style_selector(_track_style, _track_style.tag_id, parent_id,
                                                      StyleState::normal);
            if (flags & StyleUpdateFlagBits::redraw) redraw_changed = true;
            out |= flags;
        }
        if (_active_style.id == Theme::STYLE_ID_INVALID)
        {
            const auto flags = resolve_style_selector(_active_style, _active_style.tag_id, parent_id,
                                                      StyleState::normal);
            if (flags & StyleUpdateFlagBits::redraw) redraw_changed = true;
            out |= flags;
        }
        if (redraw_changed) rebuild_cached_visuals();
        return out;
    }

    void ProgressBar::update_layout_min_size()
    {
        const auto &track_style = get_theme()->get_style(_track_style.id);
        const amal::vec4 margin = track_style.margin();
        const amal::vec2 min_size = resolve_progress_bar_min_body_size(*this, track_style, _axis);
        set_required_size({min_size.x + margin.x + margin.z, min_size.y + margin.y + margin.w});
    }

    void ProgressBar::update_layout(bool min_size_known)
    {
        if (!min_size_known) update_layout_min_size();

        const auto &style = get_theme()->get_style(_track_style.id);
        const amal::vec2 layout_origin = position();
        const amal::vec4 margin = style.margin();
        const amal::vec2 min_required = required_size();
        amal::vec2 body_size = {amal::max(size().x - margin.x - margin.z, 0.0f),
                                amal::max(size().y - margin.y - margin.w, 0.0f)};
        if (fill_width()) body_size.x = amal::max(body_size.x, min_required.x - margin.x - margin.z);
        else if (!is_width_fixed())
            body_size.x = amal::max(body_size.x, min_required.x - margin.x - margin.z);
        else body_size.x = amal::max(body_size.x, min_required.x - margin.x - margin.z);
        if (!fill_height() && !is_height_fixed()) body_size.y = min_required.y - margin.y - margin.w;
        else body_size.y = amal::max(body_size.y, min_required.y - margin.y - margin.w);

        set_position({layout_origin.x + margin.x, layout_origin.y + margin.y});
        set_layout_size(body_size);
        Widget::update_layout(true);
        assert(parent() && "ProgressBar must have parent");
        set_clip_id(parent()->content_clip_id());
        rebuild_cached_visuals();
    }

    void ProgressBar::translate(const amal::vec2 &delta)
    {
        if (delta.x == 0.0f && delta.y == 0.0f) return;
        Widget::translate(delta);
        _track_rect.offset += delta;
        _track_visual.rect.offset += delta;
        _active_visual.rect.offset += delta;
    }

    void ProgressBar::rebuild_clip_rects()
    {
        assert(parent() && "ProgressBar must have parent");
        set_clip_id(parent()->content_clip_id());
        rebuild_cached_visuals();
    }

    void ProgressBar::reset_draw_records()
    {
        _track_draw_id = {};
        _active_draw_id = {};
    }

    void ProgressBar::update_depth(const amal::vec2 &depth_range)
    {
        Widget::update_depth(depth_range);
        assign_next_depth(this->depth_range(), _track_depth_range);
        assign_next_depth(_track_depth_range, _active_depth_range);
        rebuild_cached_visuals();
    }

    void ProgressBar::draw(DrawCtx &ctx)
    {
        auto *stream = get_primary_quads_stream();
        if (!stream) return;

        if ((ctx.reason & DrawReasonBits::transient) && _animation.active())
        {
            f32 current_factor = value_factor(_value);
            get_animation_state_data(_animation, &current_factor);
            QuadsInstanceData active = _active_visual;
            active.rect = resolve_active_rect(current_factor);
            active.z_order = next_depth(_active_depth_range);
            emit_context_draw(ctx, stream, _active_draw_id, &active, get_rect(), false);
            return;
        }

        emit_context_draw(ctx, stream, _track_draw_id, &_track_visual, get_rect(), false);
        if (_animation.active()) return;
        emit_context_draw(ctx, stream, _active_draw_id, &_active_visual, get_rect(), false);
    }

    void ProgressBar::on_change(ChangeEvent &event)
    {
        (void)event;
        if (_change_from_value == _value) return;
        if (!detail::g_context) return;
        f32 from_factor = value_factor(_change_from_value);
        if (_animation.active()) get_animation_state_data(_animation, &from_factor);
        const f32 to_factor = value_factor(_value);
        configure_scale_animation(_animation, AUIK_PROGRESS_BAR_SCALE_DURATION, from_factor, to_factor);
        start_animation(_animation, this);
        update_draw_commands(DrawReasonBits::external);
        detail::get_context().dirty_flags |= DirtyFlagBits::redraw;
        detail::mark_host_refresh_request();
    }

    f32 ProgressBar::clamped_value(f32 value) const
    {
        return amal::clamp(value, _min_value, _max_value);
    }

    f32 ProgressBar::value_factor(f32 value) const
    {
        const f32 range = amal::max(_max_value - _min_value, 1e-5f);
        return amal::clamp((value - _min_value) / range, 0.0f, 1.0f);
    }

    amal::rect ProgressBar::resolve_active_rect(f32 factor) const
    {
        amal::rect rect = _track_rect;
        if (_axis == amal::axis::y)
        {
            const f32 height = rect.size.y * factor;
            rect.offset.y += rect.size.y - height;
            rect.size.y = height;
        }
        else rect.size.x *= factor;
        return rect;
    }

    void ProgressBar::rebuild_track_visual()
    {
        if (_track_style.id == Theme::STYLE_ID_INVALID) return;
        const auto &style = get_theme()->get_style(_track_style.id);
        _track_rect = resolve_progress_bar_track_rect(bounds(), style, _axis);
        _track_visual.rect = _track_rect;
        _track_visual.z_order = next_depth(_track_depth_range);
        fill_quads_instance_by_style(style, clip_id(), _track_visual);
    }

    void ProgressBar::rebuild_active_visual(f32 factor)
    {
        if (_active_style.id == Theme::STYLE_ID_INVALID) return;
        const auto &style = get_theme()->get_style(_active_style.id);
        _active_visual.rect = resolve_active_rect(factor);
        _active_visual.z_order = next_depth(_active_depth_range);
        fill_quads_instance_by_style(style, clip_id(), _active_visual);
    }

    void ProgressBar::rebuild_cached_visuals()
    {
        rebuild_track_visual();
        rebuild_active_visual(value_factor(_display_value));
    }

    void ProgressBar::set_value(f32 value)
    {
        const f32 next = clamped_value(value);
        if (_value == next) return;
        _change_from_value = _value;
        _value = next;
        _display_value = next;
        rebuild_active_visual(value_factor(_display_value));
        if (_track_style.id != Theme::STYLE_ID_INVALID && _active_style.id != Theme::STYLE_ID_INVALID)
            redraw_external(has_draw_record());
    }

    void ProgressBar::set_model_binding(ModelBinding *binding)
    {
        if (_model_binding) _model_binding->on_field_change = nullptr;
        _model_binding = binding;
        if (!_model_binding) return;
        _model_binding->on_field_change = [this](ModelRecordID, ModelFieldID) {
            f32 value = 0.0f;
            if (read_model_binding_value(*_model_binding, value)) set_value(value);
        };
        attach_model_binding(*_model_binding);
        f32 value = 0.0f;
        if (read_model_binding_value(*_model_binding, value)) set_value(value);
    }

    void ProgressBar::set_range(f32 min_value, f32 max_value)
    {
        _min_value = min_value;
        _max_value = max_value;
        if (_max_value < _min_value) std::swap(_min_value, _max_value);
        const f32 next = clamped_value(_value);
        _change_from_value = _value;
        _value = next;
        _display_value = next;
        rebuild_active_visual(value_factor(_display_value));
        if (_track_style.id != Theme::STYLE_ID_INVALID && _active_style.id != Theme::STYLE_ID_INVALID)
            redraw_external(has_draw_record());
    }

    void ProgressBar::set_axis(amal::axis axis)
    {
        if (_axis == axis) return;
        _axis = axis;
        rebuild_cached_visuals();
        if (_track_style.id != Theme::STYLE_ID_INVALID && _active_style.id != Theme::STYLE_ID_INVALID)
            redraw_external(has_draw_record());
    }

    void ProgressBar::set_style_tags(u32 track_tag_id, u32 active_tag_id)
    {
        _track_style = {Theme::STYLE_ID_INVALID, track_tag_id};
        _active_style = {Theme::STYLE_ID_INVALID, active_tag_id};
        set_rect_tag_id(track_tag_id);
        rebuild_cached_visuals();
    }

    bool ProgressBar::has_draw_record() const
    {
        return detail::has_render_record(_track_draw_id) || detail::has_render_record(_active_draw_id);
    }

    namespace
    {
        void write_progress_bar(acul::bin_stream &stream, umbf::Block *block)
        {
            auto *widget = static_cast<ProgressBar *>(block);
            detail::write_widget_common_data(stream, *widget);
            stream.write(widget->value())
                .write(widget->min_value())
                .write(widget->max_value())
                .write(static_cast<u8>(widget->axis()))
                .write(widget->track_style_tag())
                .write(widget->active_style_tag());
        }

        umbf::Block *read_progress_bar(acul::bin_stream &stream)
        {
            const auto common = detail::read_widget_common_data(stream);
            f32 value = 0.0f;
            f32 min_value = 0.0f;
            f32 max_value = 1.0f;
            u8 axis = static_cast<u8>(amal::axis::x);
            u32 track_style_tag = AUIK_STYLE_TAG_PROGRESS_BAR;
            u32 active_style_tag = AUIK_STYLE_TAG_PROGRESS_BAR_ACTIVE;
            stream.read(value).read(min_value).read(max_value).read(axis).read(track_style_tag).read(active_style_tag);

            const f32 size = amal::axis(axis) == amal::axis::y ? common.inline_size.y : common.inline_size.x;
            auto *widget = acul::alloc<ProgressBar>(common.id, value, min_value, max_value, size, amal::axis(axis),
                                                    WidgetFlags(common.widget_flags));
            widget->set_style_tags(track_style_tag, active_style_tag);
            detail::apply_widget_common_data(widget, common);
            return widget;
        }
    } // namespace

    namespace streams
    {
        AUIK_EXPORT const umbf::streams::Stream progress_bar{read_progress_bar, write_progress_bar};
    } // namespace streams
} // namespace auik
