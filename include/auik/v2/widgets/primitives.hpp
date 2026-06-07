#pragma once

#include <amal/geometric.hpp>
#include <auik/v2/auik.hpp>
#include <auik/v2/pipelines.hpp>
#include "widget.hpp"

#define AUIK_TAG_WLINE 0x8CE9217Du
#define AUIK_TAG_WRECT 0xD4816E02u

namespace auik::v2
{
    class WLine final : public Widget
    {
    public:
        explicit WLine(u32 id, amal::axis axis = amal::axis::x, WidgetFlags flags = WidgetFlagBits::visible,
                       Widget *parent = nullptr, u32 style_tag = AUIK_STYLE_TAG_SEPARATOR)
            : Widget(id, flags, EventFlagBits::none, parent, {}, AUIK_TAG_WLINE),
              _axis(axis),
              _style({Theme::STYLE_ID_INVALID, style_tag})
        {
        }

        StyleUpdateFlags update_style() override
        {
            return resolve_style_selector(_style, id(), parent() ? parent()->id() : 0u, style_state());
        }

        void update_layout_min_size() override
        {
            if (_style.id == Theme::STYLE_ID_INVALID) update_style();
            const auto padding = get_theme()->get_style(_style.id).padding();
            const f32 thickness = _axis == amal::axis::x ? amal::max(padding.y + padding.w, 1.0f)
                                                         : amal::max(padding.x + padding.z, 1.0f);
            set_required_size(_axis == amal::axis::x ? amal::vec2{size().x, thickness}
                                                     : amal::vec2{thickness, size().y});
        }

        void update_layout(bool min_size_known) override
        {
            if (!min_size_known) update_layout_min_size();
            set_layout_size(amal::max(size(), required_size()));
            Widget::update_layout(true);
            if (parent()) set_clip_id(parent()->content_clip_id());
            else ensure_own_clip_rect({position().x, position().y, size().x, size().y});
        }

        void translate(const amal::vec2 &delta) override { Widget::translate(delta); }

        void rebuild_clip_rects() override
        {
            if (parent()) set_clip_id(parent()->content_clip_id());
            else ensure_own_clip_rect({position().x, position().y, size().x, size().y});
            _draw.hit_id = AUIK_INVALID_DRAW_DATA_ID;
        }

        void draw(DrawCtx &ctx) override
        {
            auto *quads_stream = get_primary_quads_stream();
            QuadsInstanceData data{};
            data.rect = bounds();
            data.z_order = get_z_order();
            const bool visible = fill_quads_instance_by_style(get_theme()->get_style(_style.id), clip_id(), data);
            emit_quads_instance(ctx, quads_stream, _draw, data, get_rect(), visible, false);
        }

    private:
        amal::axis _axis = amal::axis::x;
        StyleSelector _style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_SEPARATOR};
        DrawDataID _draw{};
    };

    class WRect final : public Widget
    {
    public:
        explicit WRect(u32 id, const amal::rect &bounds = {}, WidgetFlags flags = WidgetFlagBits::visible,
                       Widget *parent = nullptr, u32 style_tag = AUIK_STYLE_TAG_SEPARATOR)
            : Widget(id, flags, EventFlagBits::none, parent, bounds, AUIK_TAG_WRECT),
              _style({Theme::STYLE_ID_INVALID, style_tag})
        {
        }

        StyleUpdateFlags update_style() override
        {
            return resolve_style_selector(_style, id(), parent() ? parent()->id() : 0u, style_state());
        }

        void update_layout_min_size() override
        {
            if (_style.id == Theme::STYLE_ID_INVALID) update_style();
            const auto &style = get_theme()->get_style(_style.id);
            const auto margin = style.margin();
            const auto padding = style.padding();
            set_required_size({size().x + margin.x + margin.z + padding.x + padding.z,
                               size().y + margin.y + margin.w + padding.y + padding.w});
        }

        void update_layout(bool min_size_known) override
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

        void rebuild_clip_rects() override
        {
            if (parent()) set_clip_id(parent()->content_clip_id());
            else ensure_own_clip_rect({position().x, position().y, size().x, size().y});
            _draw.hit_id = AUIK_INVALID_DRAW_DATA_ID;
        }

        void draw(DrawCtx &ctx) override
        {
            auto *quads_stream = get_primary_quads_stream();
            QuadsInstanceData data{};
            data.rect = bounds();
            data.z_order = get_z_order();
            const bool visible = fill_quads_instance_by_style(get_theme()->get_style(_style.id), clip_id(), data);
            emit_quads_instance(ctx, quads_stream, _draw, data, get_rect(), visible, false);
        }

    private:
        StyleSelector _style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_SEPARATOR};
        DrawDataID _draw{};
    };

    inline WLine *make_w_line(u32 id, amal::axis axis = amal::axis::x,
                              WidgetFlags flags = WidgetFlagBits::visible, Widget *parent = nullptr,
                              u32 style_tag = AUIK_STYLE_TAG_SEPARATOR)
    {
        return acul::alloc<WLine>(id, axis, flags, parent, style_tag);
    }

    inline WRect *make_w_rect(u32 id, const amal::rect &bounds = {},
                              WidgetFlags flags = WidgetFlagBits::visible, Widget *parent = nullptr,
                              u32 style_tag = AUIK_STYLE_TAG_SEPARATOR)
    {
        return acul::alloc<WRect>(id, bounds, flags, parent, style_tag);
    }
} // namespace auik::v2
