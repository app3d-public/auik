#include <auik/auik.hpp>
#include <auik/pipelines.hpp>
#include <auik/widgets/rubber_band.hpp>

namespace auik
{
    RubberBand::RubberBand(u32 id, WidgetFlags widget_flags)
        : Widget(id, widget_flags, EventFlagBits::drag, {{0.0f, 0.0f}, {0.0f, 0.0f}}, AUIK_TAG_RUBBER_BAND)
    {
    }

    amal::rect RubberBand::make_rect_from_points(const amal::vec2 &a, const amal::vec2 &b)
    {
        const f32 left = amal::min(a.x, b.x);
        const f32 top = amal::min(a.y, b.y);
        const f32 right = amal::max(a.x, b.x);
        const f32 bottom = amal::max(a.y, b.y);
        return {{left, top}, {right - left, bottom - top}};
    }

    static amal::rect clamp_rect_to_rect(const amal::rect &rect, const amal::rect &bounds)
    {
        const f32 left =
            amal::clamp(amal::get_rect_left(rect), amal::get_rect_left(bounds), amal::get_rect_right(bounds));
        const f32 top =
            amal::clamp(amal::get_rect_top(rect), amal::get_rect_top(bounds), amal::get_rect_bottom(bounds));
        const f32 right =
            amal::clamp(amal::get_rect_right(rect), amal::get_rect_left(bounds), amal::get_rect_right(bounds));
        const f32 bottom =
            amal::clamp(amal::get_rect_bottom(rect), amal::get_rect_top(bounds), amal::get_rect_bottom(bounds));
        return {{left, top}, {amal::max(right - left, 0.0f), amal::max(bottom - top, 0.0f)}};
    }

    static amal::rect rect_from_vec4(const amal::vec4 &rect) { return {{rect.x, rect.y}, {rect.z, rect.w}}; }

    void RubberBand::filter_elements(RubberBandSearch &search) const
    {
        if (!search.widgets || search.count == 0) return;

        auto matches = [](const amal::rect &selection, const amal::rect &item, RubberBandMatchMode mode) {
            if (amal::is_rect_empty(item)) return false;
            switch (mode)
            {
                case RubberBandMatchMode::contains:
                    return amal::is_rect_contains(selection, item);
                case RubberBandMatchMode::overlap:
                default:
                    return amal::is_rects_overlap(selection, item);
            }
        };

        u32 write = 0;
        for (u32 read = 0; read < search.count; ++read)
        {
            Widget *widget = search.widgets[read];
            if (!widget || !widget->is_visible()) continue;
            if (!matches(search.rect, widget->bounds(), search.mode)) continue;
            search.widgets[write++] = widget;
        }
        search.count = write;
    }

    StyleUpdateFlags RubberBand::update_style()
    {
        const u32 parent_id = parent() ? parent()->id() : 0u;
        return resolve_style_selector(_style, id(), parent_id, style_state());
    }

    void RubberBand::update_layout_min_size_force() { set_required_size({0.0f, 0.0f}); }

    void RubberBand::update_layout(bool min_size_known)
    {
        if (layout_measure_required(min_size_known)) update_layout_min_size_force();

        if (parent())
        {
            const amal::vec4 parent_content = parent()->get_content_clip_rect();
            const amal::rect overlay_rect = {{parent_content.x, parent_content.y},
                                             {parent_content.z, parent_content.w}};
            set_position(overlay_rect.offset);
            set_layout_size(overlay_rect.size);
            set_clip_id(parent()->content_clip_id());
        }

        Widget::update_layout(true);
    }

    void RubberBand::translate(const amal::vec2 &delta)
    {
        if (delta.x == 0.0f && delta.y == 0.0f) return;
        Widget::translate(delta);
        _start += delta;
        _end += delta;
        _selection_rect.offset += delta;
    }

    void RubberBand::rebuild_clip_rects()
    {
        if (parent()) set_clip_id(parent()->content_clip_id());
        invalidate_hit_rect(_draw_id);
    }

    void RubberBand::reset_draw_records()
    {
        Widget::reset_draw_records();
        _draw_id = {};
    }

    void RubberBand::update_depth(const amal::vec2 &depth_range)
    {
        Widget::update_depth(depth_range);
        _rect.depth = next_depth(this->depth_range());
        _rect.hit_depth = _rect.depth;
    }

    void RubberBand::back_hit_depth()
    {
        Widget::back_hit_depth();
        _rect.hit_depth = get_rect().hit_depth;
    }

    void RubberBand::restore_hit_depth()
    {
        Widget::restore_hit_depth();
        _rect.hit_depth = _rect.depth;
    }

    void RubberBand::draw(DrawCtx &ctx)
    {
        auto *quads_stream = get_overlay_quads_stream();
        QuadsInstanceData data{};
        data.rect = _active ? _selection_rect : amal::rect{};
        data.z_order = get_z_order();

        const bool visible = _active && !amal::is_rect_empty(_selection_rect) &&
                             fill_quads_instance_by_style(get_theme()->get_style(_style.id), clip_id(), data);
        if (!visible)
        {
            data.background_color = 0u;
            data.border_color = 0u;
            data.border_radius = 0.0f;
            data.border_thickness = 0.0f;
            data.mask = static_cast<u32>(clip_id());
        }

        emit_context_draw(ctx, quads_stream, _draw_id, &data, get_rect(), false);
    }

    void RubberBand::on_drag(const amal::vec2 &delta, KeyPressState state)
    {
        (void)delta;
        if (state == KeyPressState::press)
        {
            set_visible();
            sync_widget_flags();
            _start = detail::get_context().io.last_click_pos;
            _end = get_mouse_pos();
            update_selection_rect();
            _active = true;
            _committed = false;
            redraw_band();
            return;
        }

        if (state == KeyPressState::release)
        {
            if (!_active) return;
            _end = get_mouse_pos();
            update_selection_rect();
            _active = false;
            _committed = true;
            redraw_band();
            unset_visible();
            sync_widget_flags();
            mark_changed();
            return;
        }

        if (!_active) return;
        _end = get_mouse_pos();
        update_selection_rect();
        redraw_band();
    }

    void RubberBand::update_selection_rect()
    {
        amal::rect clip_bounds = bounds();
        if (parent())
        {
            const amal::vec4 parent_content = parent()->get_content_clip_rect();
            clip_bounds = rect_from_vec4(parent_content);
            set_position(clip_bounds.offset);
            set_layout_size(clip_bounds.size);
            set_clip_id(parent()->content_clip_id());
        }
        _selection_rect = clamp_rect_to_rect(make_rect_from_points(_start, _end), clip_bounds);
    }

    void RubberBand::redraw_band()
    {
        redraw_external(has_draw_record(), DrawReasonBits::external | DrawReasonBits::transient);
        mark_host_refresh_request();
    }

    namespace
    {
        void write_rubber_band(acul::bin_stream &stream, umbf::Block *block)
        {
            auto *widget = static_cast<RubberBand *>(block);
            stream.write(widget->id()).write(static_cast<u32>(widget->widget_flags));
        }

        umbf::Block *read_rubber_band(acul::bin_stream &stream)
        {
            u32 id = 0u;
            u32 widget_flags = 0u;
            stream.read(id).read(widget_flags);
            return acul::alloc<RubberBand>(id, WidgetFlags(widget_flags));
        }
    } // namespace

    namespace streams
    {
        AUIK_EXPORT const umbf::streams::Stream rubber_band{read_rubber_band, write_rubber_band};
    } // namespace streams
} // namespace auik
