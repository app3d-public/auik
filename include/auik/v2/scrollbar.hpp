#pragma once

#include <amal/geometric.hpp>
#include "theme.hpp"
#include "widget.hpp"

#define AUIK_TAG_SCROLLBAR_TRACK 0x5E57D9C1
#define AUIK_TAG_SCROLLBAR_THUMB 0x0DA3B8EE
#define AUIK_ID_SCROLLBAR        0x2F8B5D22
#define AUIK_SCROLL_STEP         24

namespace auik::v2
{
    class APPLIB_API Scrollbar : public Widget
    {
    public:
        Scrollbar(Widget *parent = nullptr, amal::axis axis = amal::axis::y)
            : Widget(AUIK_ID_SCROLLBAR, WidgetFlagBits::visible | WidgetFlagBits::foreground, parent, {0.0f, 0.0f},
                     {0.0f, 0.0f}, AUIK_TAG_SCROLLBAR_TRACK),
              _track_style({0, AUIK_TAG_SCROLLBAR_TRACK}),
              _thumb_style({0, AUIK_TAG_SCROLLBAR_THUMB}),
              _thumb_rect(detail::make_rect_data(0, AUIK_TAG_SCROLLBAR_THUMB)),
              _axis(axis)
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
            if (value)
                widget_flags |= WidgetFlagBits::visible;
            else
                widget_flags &= ~WidgetFlagBits::visible;
        }
        bool is_visible() const { return (widget_flags & WidgetFlagBits::visible); }
        amal::vec4 get_track_margin() const;
        f32 get_min_track_thickness() const;
        void set_axis(amal::axis axis) { _axis = axis; }
        void set_scroll_normalized(f32 value) { _scroll = amal::clamp(value, 0.0f, 1.0f); }
        void set_metrics(f32 content_size, f32 view_size);
        f32 max_scroll() const { return _max_scroll; }
        f32 scroll_offset() const { return _scroll * _max_scroll; }
        void set_scroll_offset(f32 offset_px);
        bool scroll_by_pixels(f32 delta_px);

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
        amal::axis _axis = amal::axis::y;
        f32 _scroll = 0.0f;
        f32 _max_scroll = 0.0f;
    };

    static inline void ensure_scrollbar(Scrollbar *&scrollbar, Widget *parent, amal::axis axis)
    {
        if (scrollbar) return;
        scrollbar = acul::alloc<Scrollbar>(parent, axis);
    }
} // namespace auik::v2
