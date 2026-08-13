#include <auik/pipelines.hpp>
#include <auik/widgets/separator.hpp>

namespace auik
{
    static inline amal::vec4 orient_offsets(const amal::vec4 &v, amal::axis axis)
    {
        if (axis == amal::axis::y) return {v.y, v.x, v.w, v.z};
        return v;
    }

    static inline amal::vec2 orient_size(const amal::vec2 &v, amal::axis axis)
    {
        if (axis == amal::axis::y) return {v.y, v.x};
        return v;
    }

    static inline amal::vec2 unorient_size(const amal::vec2 &v, amal::axis axis)
    {
        if (axis == amal::axis::y) return {v.y, v.x};
        return v;
    }

    static inline amal::rect unorient_rect(const amal::vec2 &origin, const amal::rect &r, amal::axis axis)
    {
        if (axis == amal::axis::y) return {{origin.x + r.offset.y, origin.y + r.offset.x}, {r.size.y, r.size.x}};
        return {{origin.x + r.offset.x, origin.y + r.offset.y}, r.size};
    }

    Separator::Separator(amal::axis axis, WidgetFlags widget_flags, u32 style_tag)
        : Widget(AUIK_TAG_SEPARATOR, widget_flags, EventFlagBits::none, {{0.0f, 0.0f}, AUIK_SIZE_INHERIT}, style_tag),
          _axis(axis),
          _style({Theme::STYLE_ID_INVALID, style_tag})
    {
    }

    StyleUpdateFlags Separator::update_style()
    {
        return resolve_style_selector(_style, id(), parent() ? parent()->id() : 0u, style_state());
    }

    void Separator::update_layout_min_size_force()
    {
        const auto &style = get_theme()->get_style(_style.id);
        const amal::vec4 margin = orient_offsets(style.margin(), _axis);
        const amal::vec4 padding = orient_offsets(style.padding(), _axis);
        const f32 main_size = padding.x + padding.z;
        const f32 cross_size = padding.y + padding.w;
        set_required_size(unorient_size({main_size + margin.x + margin.z, cross_size + margin.y + margin.w}, _axis));
    }

    void Separator::update_layout(bool min_size_known)
    {
        if (layout_measure_required(min_size_known)) update_layout_min_size_force();
        const auto &style = get_theme()->get_style(_style.id);
        const amal::vec4 margin = orient_offsets(style.margin(), _axis);
        const amal::vec4 padding = orient_offsets(style.padding(), _axis);
        const amal::vec2 layout_origin = position();
        const amal::vec2 oriented_size = orient_size(size(), _axis);
        const amal::vec2 oriented_required = orient_size(required_size(), _axis);
        const amal::vec2 cell_size = {amal::max(oriented_size.x, oriented_required.x),
                                      oriented_size.y > 0.0f ? amal::max(oriented_size.y, oriented_required.y)
                                                             : oriented_required.y};
        const amal::vec2 inner_size = {amal::max(cell_size.x - margin.x - margin.z, 0.0f),
                                       amal::max(cell_size.y - margin.y - margin.w, 0.0f)};
        set_position(layout_origin + unorient_size({margin.x, margin.y}, _axis));
        set_layout_size(unorient_size(inner_size, _axis));
        Widget::update_layout(true);
        set_clip_id(parent() ? parent()->content_clip_id() : clip_id());

        const f32 cross_thickness = amal::max(inner_size.y, padding.y + padding.w);
        const amal::vec2 line_pos = {padding.x, 0.0f};
        const amal::vec2 line_size = {amal::max(inner_size.x - padding.x - padding.z, 0.0f),
                                      amal::min(cross_thickness, amal::max(inner_size.y, 0.0f))};
        _line_rect = unorient_rect(position(), {line_pos, line_size}, _axis);
    }

    void Separator::rebuild_clip_rects()
    {
        set_clip_id(parent() ? parent()->content_clip_id() : clip_id());
        invalidate_hit_rect(_line);
    }

    void Separator::translate(const amal::vec2 &delta)
    {
        if (delta.x == 0.0f && delta.y == 0.0f) return;
        Widget::translate(delta);
        _line_rect.offset += delta;
    }

    void Separator::update_depth(const amal::vec2 &depth_range) { Widget::update_depth(depth_range); }

    void Separator::draw(DrawCtx &ctx)
    {
        auto *quads_stream = get_primary_quads_stream();
        QuadsInstanceData line{};
        line.rect = _line_rect;
        line.z_order = get_z_order();
        const bool visible = fill_quads_instance_by_style(get_theme()->get_style(_style.id), clip_id(), line);
        emit_quads_instance(ctx, quads_stream, _line, line, get_rect(), visible, false);
    }
} // namespace auik
