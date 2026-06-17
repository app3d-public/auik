#include <auik/auik.hpp>
#include <auik/pipelines.hpp>
#include <auik/widgets/primitives.hpp>
#include "../core/session_stream_utils.hpp"

namespace auik
{
    WLine::WLine(u32 id, amal::axis axis, WidgetFlags flags, Widget *parent, u32 style_tag)
        : Widget(id, flags, EventFlagBits::none, parent, {}, AUIK_TAG_WLINE),
          _axis(axis),
          _style({Theme::STYLE_ID_INVALID, style_tag})
    {
        set_size(_axis == amal::axis::x ? amal::vec2{AUIK_SIZE_X_FILL, AUIK_SIZE_Y_FIT}
                                        : amal::vec2{AUIK_SIZE_X_FIT, AUIK_SIZE_Y_FILL});
    }

    void WLine::set_style_tag(u32 tag_id)
    {
        if (_style.tag_id == tag_id) return;
        _style = {Theme::STYLE_ID_INVALID, tag_id};
    }

    StyleUpdateFlags WLine::update_style()
    {
        return resolve_style_selector(_style, id(), parent() ? parent()->id() : 0u, style_state());
    }

    void WLine::update_layout_min_size()
    {
        if (_style.id == Theme::STYLE_ID_INVALID) update_style();
        const auto padding = get_theme()->get_style(_style.id).padding();
        const f32 thickness = _axis == amal::axis::x ? amal::max(padding.y + padding.w, 1.0f)
                                                     : amal::max(padding.x + padding.z, 1.0f);
        const f32 length = _axis == amal::axis::x
                               ? (is_width_fixed() && !fill_width() ? requested_size().x : 0.0f)
                               : (is_height_fixed() && !fill_height() ? requested_size().y : 0.0f);
        set_required_size(_axis == amal::axis::x ? amal::vec2{length, thickness}
                                                 : amal::vec2{thickness, length});
    }

    void WLine::update_layout(bool min_size_known)
    {
        if (!min_size_known) update_layout_min_size();
        set_layout_size(amal::max(size(), required_size()));
        Widget::update_layout(true);
        if (parent()) set_clip_id(parent()->content_clip_id());
        else ensure_own_clip_rect({position().x, position().y, size().x, size().y});
    }

    void WLine::translate(const amal::vec2 &delta) { Widget::translate(delta); }

    void WLine::rebuild_clip_rects()
    {
        if (parent()) set_clip_id(parent()->content_clip_id());
        else ensure_own_clip_rect({position().x, position().y, size().x, size().y});
        _draw.hit_id = AUIK_INVALID_DRAW_DATA_ID;
    }

    void WLine::reset_draw_records() { _draw = {}; }

    void WLine::draw(DrawCtx &ctx)
    {
        auto *quads_stream = get_primary_quads_stream();
        QuadsInstanceData data{};
        data.rect = bounds();
        data.z_order = get_z_order();
        const bool visible = fill_quads_instance_by_style(get_theme()->get_style(_style.id), clip_id(), data);
        emit_quads_instance(ctx, quads_stream, _draw, data, get_rect(), visible, false);
    }

    WRect::WRect(u32 id, const amal::rect &bounds, WidgetFlags flags, Widget *parent, u32 style_tag)
        : Widget(id, flags, EventFlagBits::none, parent, bounds, AUIK_TAG_WRECT),
          _style({Theme::STYLE_ID_INVALID, style_tag})
    {
    }

    void WRect::set_style_tag(u32 tag_id)
    {
        if (_style.tag_id == tag_id) return;
        _style = {Theme::STYLE_ID_INVALID, tag_id};
    }

    StyleUpdateFlags WRect::update_style()
    {
        return resolve_style_selector(_style, id(), parent() ? parent()->id() : 0u, style_state());
    }

    void WRect::update_layout_min_size()
    {
        if (_style.id == Theme::STYLE_ID_INVALID) update_style();
        const auto &style = get_theme()->get_style(_style.id);
        const auto margin = style.margin();
        const auto padding = style.padding();
        set_required_size({size().x + margin.x + margin.z + padding.x + padding.z,
                           size().y + margin.y + margin.w + padding.y + padding.w});
    }

    void WRect::update_layout(bool min_size_known)
    {
        if (!min_size_known) update_layout_min_size();
        if (_style.id == Theme::STYLE_ID_INVALID) update_style();
        const auto &style = get_theme()->get_style(_style.id);
        const auto margin = style.margin();
        const auto padding = style.padding();
        if (parent()) set_position(position() + amal::vec2{margin.x, margin.y});
        set_layout_size({amal::max(size().x, required_size().x - margin.x - margin.z - padding.x - padding.z),
                         amal::max(size().y, required_size().y - margin.y - margin.w - padding.y - padding.w)});
        Widget::update_layout(true);
        if (parent()) set_clip_id(parent()->content_clip_id());
        else ensure_own_clip_rect({position().x, position().y, size().x, size().y});
    }

    void WRect::rebuild_clip_rects()
    {
        if (parent()) set_clip_id(parent()->content_clip_id());
        else ensure_own_clip_rect({position().x, position().y, size().x, size().y});
        _draw.hit_id = AUIK_INVALID_DRAW_DATA_ID;
    }

    void WRect::reset_draw_records() { _draw = {}; }

    void WRect::draw(DrawCtx &ctx)
    {
        auto *quads_stream = get_primary_quads_stream();
        QuadsInstanceData data{};
        data.rect = bounds();
        data.z_order = get_z_order();
        const bool visible = fill_quads_instance_by_style(get_theme()->get_style(_style.id), clip_id(), data);
        emit_quads_instance(ctx, quads_stream, _draw, data, get_rect(), visible, false);
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
                acul::alloc<WLine>(common.id, static_cast<amal::axis>(axis), WidgetFlags(common.widget_flags), nullptr,
                                   style_tag);
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

            auto *widget = acul::alloc<WRect>(common.id, common.bounds, WidgetFlags(common.widget_flags), nullptr,
                                              style_tag);
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
