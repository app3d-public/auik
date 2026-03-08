#pragma once

#include <amal/geometric.hpp>
#include "../../theme.hpp"
#include "../widget.hpp"

#define AUIK_TAG_SCROLLBAR_TRACK 0x5E57D9C1
#define AUIK_TAG_SCROLLBAR_THUMB 0x0DA3B8EE
#define AUIK_ID_SCROLLBAR        0x2F8B5D22
#define AUIK_SCROLL_STEP         24

namespace auik::v2::detail
{
    struct APPLIB_API ScrollBehavior
    {
        explicit ScrollBehavior(amal::axis axis = amal::axis::y) : axis(axis) {}

        amal::axis axis = amal::axis::y;
        f32 normalized = 0.0f;
        f32 max_scroll_px = 0.0f;

        void set_axis(amal::axis axis_value) { axis = axis_value; }
        void set_scroll_normalized(f32 value) { normalized = amal::clamp(value, 0.0f, 1.0f); }
        void set_metrics(f32 content_size, f32 view_size);
        f32 max_scroll() const { return max_scroll_px; }
        f32 scroll_offset() const { return normalized * max_scroll_px; }
        void set_scroll_offset(f32 offset_px);
        bool scroll_by_pixels(f32 delta_px);
    };

    class APPLIB_API Scrollbar : public Widget
    {
    public:
        Scrollbar(Widget *parent = nullptr, amal::axis axis = amal::axis::y)
            : Widget(AUIK_ID_SCROLLBAR, WidgetFlagBits::visible | WidgetFlagBits::foreground, parent, {0.0f, 0.0f},
                     {0.0f, 0.0f}, AUIK_TAG_SCROLLBAR_TRACK),
              _track_style({0, AUIK_TAG_SCROLLBAR_TRACK}),
              _thumb_style({0, AUIK_TAG_SCROLLBAR_THUMB}),
              _thumb_rect(detail::make_rect_data(0, AUIK_TAG_SCROLLBAR_THUMB)),
              _behavior(axis)
        {
            assert(parent);
            u32 owner_id = parent->id();
            _rect.widget_id = owner_id;
            _thumb_rect.widget_id = owner_id;
            _thumb_rect.tag_id = AUIK_TAG_SCROLLBAR_THUMB;
            _thumb_rect.clip_rect_id = parent->clip_rect_id();
        }

        void set_visible(bool value)
        {
            Widget::set_visible(value);
        }
        bool is_visible() const { return Widget::is_visible(); }
        amal::vec4 get_track_margin() const;
        f32 get_min_track_thickness() const;
        void set_axis(amal::axis axis) { _behavior.set_axis(axis); }
        void set_scroll_normalized(f32 value) { _behavior.set_scroll_normalized(value); }
        void set_metrics(f32 content_size, f32 view_size) { _behavior.set_metrics(content_size, view_size); }
        f32 max_scroll() const { return _behavior.max_scroll(); }
        f32 scroll_offset() const { return _behavior.scroll_offset(); }
        void set_scroll_offset(f32 offset_px) { _behavior.set_scroll_offset(offset_px); }
        bool scroll_by_pixels(f32 delta_px) { return _behavior.scroll_by_pixels(delta_px); }
        bool scroll_to_track_click(const amal::vec2 &mouse_pos);
        bool scroll_thumb_by_drag_delta(const amal::vec2 &delta);
        bool is_point_on_thumb(const amal::vec2 &mouse_pos) const;
        bool set_thumb_hovered(bool value)
        {
            if (value)
            {
                if (is_thumb_active()) return false;
                return set_style_state(StyleState::hover);
            }
            if (style_state() == StyleState::hover) return set_style_state(StyleState::normal);
            return false;
        }
        bool is_thumb_hovered() const { return style_state() == StyleState::hover; }
        bool set_thumb_active(bool value)
        {
            if (value) return set_style_state(StyleState::active);
            if (style_state() == StyleState::active) return set_style_state(StyleState::normal);
            return false;
        }
        bool is_thumb_active() const { return style_state() == StyleState::active; }
        bool has_draw_record() const
        {
            return _track_draw_id.render_id != AUIK_INVALID_DRAW_DATA_ID &&
                   _thumb_draw_id.render_id != AUIK_INVALID_DRAW_DATA_ID;
        }
        const ScrollBehavior &behavior() const { return _behavior; }

        void configure(const amal::vec2 &track_pos, const amal::vec2 &track_size, f32 content_size, f32 view_size);

        void update_style() override;
        void rebuild_clip_rects() override;

        void draw(DrawCtx &ctx) override;

    private:
        DrawDataID _track_draw_id;
        DrawDataID _thumb_draw_id;
        StyleSelector _track_style;
        StyleSelector _thumb_style;
        detail::RectData _thumb_rect;
        ScrollBehavior _behavior;
        f32 _content_size = 0.0f;
        f32 _view_size = 0.0f;
    };

    inline void ensure_scrollbar(Scrollbar *&scrollbar, Widget *parent, amal::axis axis)
    {
        if (scrollbar) return;
        scrollbar = acul::alloc<Scrollbar>(parent, axis);
    }
} // namespace auik::v2::detail
