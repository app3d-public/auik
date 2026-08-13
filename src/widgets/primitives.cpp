#include <auik/auik.hpp>
#include <auik/pipelines.hpp>
#include <auik/widgets/primitives.hpp>
#include "../core/session_stream_utils.hpp"

namespace auik
{
    WLine::WLine(u32 id, amal::axis axis, WidgetFlags flags)
        : Widget(id, flags, EventFlagBits::none, {{0.0f, 0.0f}, AUIK_SIZE_INHERIT}, AUIK_TAG_WLINE), _axis(axis)
    {
        set_requested_size(_axis == amal::axis::x ? amal::vec2{AUIK_SIZE_X_FILL, AUIK_SIZE_Y_FIT}
                                                  : amal::vec2{AUIK_SIZE_X_FIT, AUIK_SIZE_Y_FILL});
    }

    void WLine::set_style_tag(u32 tag_id)
    {
        if (_style.tag_id == tag_id) return;
        _style = {Theme::STYLE_ID_INVALID, tag_id};
    }

    StyleUpdateFlags WLine::update_style()
    {
        const auto flags = resolve_style_selector(_style, id(), parent() ? parent()->id() : 0u, style_state());
        apply_style_layout(get_theme()->get_style(_style.id));
        return flags;
    }

    void WLine::update_layout_min_size_force()
    {
        if (_style.id == Theme::STYLE_ID_INVALID) update_style();
        const auto &style = get_theme()->get_style(_style.id);
        const auto margin = parent() ? style.margin() : amal::vec4{0.0f};
        const auto padding = style.padding();
        const bool has_fixed_cross = _axis == amal::axis::x ? is_height_fixed() : is_width_fixed();
        const f32 style_thickness = _axis == amal::axis::x ? style_size().y : style_size().x;
        const f32 padding_thickness = _axis == amal::axis::x ? padding.y + padding.w : padding.x + padding.z;
        const f32 thickness = has_fixed_cross ? amal::max(style_thickness, 0.0f) : amal::max(padding_thickness, 1.0f);
        const f32 length = _axis == amal::axis::x ? (is_width_fixed() && !fill_width() ? style_size().x : 0.0f)
                                                  : (is_height_fixed() && !fill_height() ? style_size().y : 0.0f);
        const amal::vec2 line_size =
            _axis == amal::axis::x ? amal::vec2{length, thickness} : amal::vec2{thickness, length};
        set_required_size({line_size.x + margin.x + margin.z, line_size.y + margin.y + margin.w});
    }

    void WLine::update_layout(bool min_size_known)
    {
        if (layout_measure_required(min_size_known)) update_layout_min_size_force();
        if (_style.id == Theme::STYLE_ID_INVALID) update_style();
        const auto margin = parent() ? get_theme()->get_style(_style.id).margin() : amal::vec4{0.0f};
        const amal::vec2 min_line_size = {amal::max(required_size().x - margin.x - margin.z, 0.0f),
                                          amal::max(required_size().y - margin.y - margin.w, 0.0f)};
        amal::vec2 line_size = {amal::max(size().x - margin.x - margin.z, 0.0f),
                                amal::max(size().y - margin.y - margin.w, 0.0f)};
        if (fill_width() || is_width_fixed()) line_size.x = amal::max(line_size.x, min_line_size.x);
        else line_size.x = min_line_size.x;
        if (fill_height() || is_height_fixed()) line_size.y = amal::max(line_size.y, min_line_size.y);
        else line_size.y = min_line_size.y;
        if (parent()) set_position(position() + amal::vec2{margin.x, margin.y});
        set_layout_size(line_size);
        Widget::update_layout(true);
        if (parent()) set_clip_id(parent()->content_clip_id());
        else ensure_own_clip_rect({position().x, position().y, size().x, size().y});
    }

    void WLine::translate(const amal::vec2 &delta) { Widget::translate(delta); }

    void WLine::rebuild_clip_rects()
    {
        if (parent()) set_clip_id(parent()->content_clip_id());
        else ensure_own_clip_rect({position().x, position().y, size().x, size().y});
        invalidate_hit_rect(_draw);
    }

    void WLine::reset_draw_records() { _draw = {}; }

    void WLine::draw(DrawCtx &ctx)
    {
        auto *quads_stream =
            (ctx.reason & DrawReasonBits::transient) ? get_overlay_quads_stream() : get_primary_quads_stream();
        QuadsInstanceData data{};
        data.rect = bounds();
        data.z_order = get_z_order();
        const bool visible = fill_quads_instance_by_style(get_theme()->get_style(_style.id), clip_id(), data);
        emit_quads_instance(ctx, quads_stream, _draw, data, get_rect(), visible, false);
    }

    WRect::WRect(u32 id, const amal::rect &bounds, WidgetFlags flags)
        : Widget(id, flags, EventFlagBits::none, bounds, AUIK_TAG_WRECT)
    {
    }

    void WRect::set_style_tag(u32 tag_id)
    {
        if (_style.tag_id == tag_id) return;
        _style = {Theme::STYLE_ID_INVALID, tag_id};
    }

    StyleUpdateFlags WRect::update_style()
    {
        const auto flags = resolve_style_selector(_style, id(), parent() ? parent()->id() : 0u, style_state());
        apply_style_layout(get_theme()->get_style(_style.id));
        return flags;
    }

    void WRect::update_layout_min_size_force()
    {
        if (_style.id == Theme::STYLE_ID_INVALID) update_style();
        const auto &style = get_theme()->get_style(_style.id);
        const auto margin = style.margin();
        const auto padding = style.padding();
        const f32 width = is_width_fixed() ? style_size().x : size().x + padding.x + padding.z;
        const f32 height = is_height_fixed() ? style_size().y : size().y + padding.y + padding.w;
        set_required_size({width + margin.x + margin.z, height + margin.y + margin.w});
    }

    void WRect::update_layout(bool min_size_known)
    {
        if (layout_measure_required(min_size_known)) update_layout_min_size_force();
        if (_style.id == Theme::STYLE_ID_INVALID) update_style();
        const auto &style = get_theme()->get_style(_style.id);
        const auto margin = style.margin();
        const amal::vec2 min_rect_size = {amal::max(required_size().x - margin.x - margin.z, 0.0f),
                                          amal::max(required_size().y - margin.y - margin.w, 0.0f)};
        amal::vec2 rect_size = {amal::max(size().x - margin.x - margin.z, 0.0f),
                                amal::max(size().y - margin.y - margin.w, 0.0f)};
        if (fill_width() || is_width_fixed()) rect_size.x = amal::max(rect_size.x, min_rect_size.x);
        else rect_size.x = min_rect_size.x;
        if (fill_height() || is_height_fixed()) rect_size.y = amal::max(rect_size.y, min_rect_size.y);
        else rect_size.y = min_rect_size.y;
        if (parent()) set_position(position() + amal::vec2{margin.x, margin.y});
        set_layout_size(rect_size);
        Widget::update_layout(true);
        if (parent()) set_clip_id(parent()->content_clip_id());
        else ensure_own_clip_rect({position().x, position().y, size().x, size().y});
    }

    void WRect::rebuild_clip_rects()
    {
        if (parent()) set_clip_id(parent()->content_clip_id());
        else ensure_own_clip_rect({position().x, position().y, size().x, size().y});
        invalidate_hit_rect(_draw);
    }

    void WRect::reset_draw_records() { _draw = {}; }

    void WRect::draw(DrawCtx &ctx)
    {
        auto *quads_stream =
            (ctx.reason & DrawReasonBits::transient) ? get_overlay_quads_stream() : get_primary_quads_stream();
        QuadsInstanceData data{};
        data.rect = bounds();
        data.z_order = get_z_order();
        const bool visible = fill_quads_instance_by_style(get_theme()->get_style(_style.id), clip_id(), data);
        emit_quads_instance(ctx, quads_stream, _draw, data, get_rect(), visible, can_emit_hit(ctx));
    }

    namespace
    {
        void write_w_line(acul::bin_stream &stream, umbf::Block *block)
        {
            auto *widget = static_cast<WLine *>(block);
            detail::write_widget_common_data(stream, *widget);
            stream.write(static_cast<u8>(widget->axis())).write(widget->style_tag());
        }

        umbf::Block *read_w_line(acul::bin_stream &stream)
        {
            const auto common = detail::read_widget_common_data(stream);
            u8 axis = static_cast<u8>(amal::axis::x);
            u32 style_tag = AUIK_STYLE_TAG_SEPARATOR;
            stream.read(axis).read(style_tag);

            auto *widget =
                acul::alloc<WLine>(common.id, static_cast<amal::axis>(axis), WidgetFlags(common.widget_flags));
            widget->set_style_tag(style_tag);
            detail::apply_widget_common_data(widget, common);
            return widget;
        }

        void write_w_rect(acul::bin_stream &stream, umbf::Block *block)
        {
            auto *widget = static_cast<WRect *>(block);
            detail::write_widget_common_data(stream, *widget);
            stream.write(widget->style_tag());
        }

        umbf::Block *read_w_rect(acul::bin_stream &stream)
        {
            const auto common = detail::read_widget_common_data(stream);
            u32 style_tag = AUIK_STYLE_TAG_SEPARATOR;
            stream.read(style_tag);

            auto *widget = acul::alloc<WRect>(common.id, common.bounds, WidgetFlags(common.widget_flags));
            widget->set_style_tag(style_tag);
            detail::apply_widget_common_data(widget, common);
            return widget;
        }
    } // namespace

    namespace streams
    {
        AUIK_EXPORT const umbf::streams::Stream w_line{read_w_line, write_w_line};
        AUIK_EXPORT const umbf::streams::Stream w_rect{read_w_rect, write_w_rect};
    } // namespace streams
} // namespace auik
