#pragma once

#include <amal/geometric.hpp>
#include "../../theme.hpp"
#include "../widget.hpp"

#define AUIK_TAG_SCROLLBAR_TRACK          0x5E57D9C1
#define AUIK_TAG_SCROLLBAR_THUMB          0x0DA3B8EE
#define AUIK_ID_SCROLLBAR_X               0x2F8B5D23u
#define AUIK_ID_SCROLLBAR_Y               0x2F8B5D24u
#define AUIK_TAG_SCROLLBAR_TRACK_X        0x05548502u
#define AUIK_TAG_SCROLLBAR_THUMB_X        0x7EA30FD2u
#define AUIK_TAG_SCROLLBAR_TRACK_Y        0x2D8A351Cu
#define AUIK_TAG_SCROLLBAR_THUMB_Y        0x06058F64u
#define AUIK_TAG_SCROLLBAR_TRACK_INTERNAL 0xFF44005Bu
#define AUIK_TAG_SCROLLBAR_THUMB_INTERNAL 0x1791BD20u
#define AUIK_SCROLL_STEP                  24

namespace auik::detail
{
    struct ScrollBehavior
    {
        explicit ScrollBehavior(amal::axis axis = amal::axis::y) : axis(axis) {}

        amal::axis axis = amal::axis::y;
        f32 normalized = 0.0f;
        f32 max_scroll_px = 0.0f;

        void set_axis(amal::axis axis_value) { axis = axis_value; }
        void set_scroll_normalized(f32 value) { normalized = amal::clamp(value, 0.0f, 1.0f); }
        AUIK_EXPORT void set_metrics(f32 content_size, f32 view_size);
        f32 max_scroll() const { return max_scroll_px; }
        f32 scroll_offset() const { return normalized * max_scroll_px; }
        AUIK_EXPORT void set_scroll_offset(f32 offset_px);
        AUIK_EXPORT bool scroll_by_pixels(f32 delta_px);
    };

    class Scrollbar : public Widget
    {
    public:
        Scrollbar(u32 id, u32 track_tag_id, u32 thumb_tag_id, Widget *parent = nullptr, amal::axis axis = amal::axis::y)
            : Widget(id, WidgetFlagBits::visible | WidgetFlagBits::hittable, EventFlagBits::none, {}, track_tag_id),
              _track_style({Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_SCROLLBAR_TRACK}),
              _thumb_style({Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_SCROLLBAR_THUMB}),
              _thumb_rect(detail::make_rect_data(0, thumb_tag_id)),
              _behavior(axis)
        {
            assert(parent);
            set_parent(parent);
            u32 owner_id = parent->id();
            _rect.id.widget_id = owner_id;
            _thumb_rect.id.widget_id = owner_id;
            _rect.id.tag_id = track_tag_id;
            _thumb_rect.id.tag_id = thumb_tag_id;
            _thumb_rect.clip_id = parent->clip_id();
        }

        bool is_visible() const { return Widget::is_visible(); }
        AUIK_EXPORT amal::vec4 get_track_margin() const;
        AUIK_EXPORT f32 get_min_track_thickness() const;
        void set_axis(amal::axis axis) { _behavior.set_axis(axis); }
        AUIK_EXPORT void set_scroll_normalized(f32 value);
        void set_metrics(f32 content_size, f32 view_size) { _behavior.set_metrics(content_size, view_size); }
        f32 max_scroll() const { return _behavior.max_scroll(); }
        f32 scroll_offset() const { return _behavior.scroll_offset(); }
        AUIK_EXPORT void set_scroll_offset(f32 offset_px);
        AUIK_EXPORT bool scroll_by_pixels(f32 delta_px);
        AUIK_EXPORT bool scroll_to_track_click(const amal::vec2 &mouse_pos);
        AUIK_EXPORT void begin_thumb_drag(const amal::vec2 &mouse_pos);
        AUIK_EXPORT bool scroll_thumb_to_mouse_pos(const amal::vec2 &mouse_pos);
        AUIK_EXPORT bool scroll_thumb_by_drag_delta(const amal::vec2 &delta);
        AUIK_EXPORT bool is_point_on_thumb(const amal::vec2 &mouse_pos) const;
        const ScrollBehavior &behavior() const { return _behavior; }
        AUIK_EXPORT void configure(const amal::vec2 &track_pos, const amal::vec2 &track_size, f32 content_size,
                                   f32 view_size);
        AUIK_EXPORT StyleUpdateFlags update_style() override;
        AUIK_EXPORT void translate(const amal::vec2 &delta) override;
        AUIK_EXPORT void rebuild_clip_rects() override;
        AUIK_EXPORT void back_hit_depth() override;
        AUIK_EXPORT void restore_hit_depth() override;

        AUIK_EXPORT void draw(DrawCtx &ctx) override;
        AUIK_EXPORT bool has_draw_record() const;

        void set_track_style_tag(u32 tag_id) { _track_style.tag_id = tag_id; }
        void set_thumb_style_tag(u32 tag_id) { _thumb_style.tag_id = tag_id; }

    private:
        void update_thumb_rect();

        DrawDataID _track_draw_id;
        DrawDataID _thumb_draw_id;
        StyleSelector _track_style;
        StyleSelector _thumb_style;
        detail::RectData _thumb_rect;
        ScrollBehavior _behavior;
        f32 _content_size = 0.0f;
        f32 _view_size = 0.0f;
        f32 _thumb_drag_grab_offset = 0.0f;
    };

    inline bool is_scrollbar_track_tag(u32 tag_id)
    { return tag_id == AUIK_TAG_SCROLLBAR_TRACK_X || tag_id == AUIK_TAG_SCROLLBAR_TRACK_Y; }

    inline bool is_scrollbar_thumb_tag(u32 tag_id)
    { return tag_id == AUIK_TAG_SCROLLBAR_THUMB_X || tag_id == AUIK_TAG_SCROLLBAR_THUMB_Y; }

    inline bool is_scrollbar_tag(u32 tag_id)
    { return is_scrollbar_track_tag(tag_id) || is_scrollbar_thumb_tag(tag_id); }

    inline Scrollbar *make_x_scrollbar(Widget *parent)
    {
        return acul::alloc<Scrollbar>(AUIK_ID_SCROLLBAR_X, AUIK_TAG_SCROLLBAR_TRACK_X, AUIK_TAG_SCROLLBAR_THUMB_X,
                                      parent, amal::axis::x);
    }

    inline Scrollbar *make_y_scrollbar(Widget *parent)
    {
        return acul::alloc<Scrollbar>(AUIK_ID_SCROLLBAR_Y, AUIK_TAG_SCROLLBAR_TRACK_Y, AUIK_TAG_SCROLLBAR_THUMB_Y,
                                      parent, amal::axis::y);
    }

    inline Scrollbar *make_internal_y_scrollbar(Widget *parent)
    {
        Scrollbar *scrollbar = make_y_scrollbar(parent);
        scrollbar->set_track_style_tag(AUIK_STYLE_TAG_SCROLLBAR_TRACK_INTERNAL);
        scrollbar->set_thumb_style_tag(AUIK_STYLE_TAG_SCROLLBAR_THUMB_INTERNAL);
        return scrollbar;
    }

    inline Scrollbar *make_internal_x_scrollbar(Widget *parent)
    {
        Scrollbar *scrollbar = make_x_scrollbar(parent);
        scrollbar->set_track_style_tag(AUIK_STYLE_TAG_SCROLLBAR_TRACK_INTERNAL);
        scrollbar->set_thumb_style_tag(AUIK_STYLE_TAG_SCROLLBAR_THUMB_INTERNAL);
        return scrollbar;
    }

    inline void ensure_x_scrollbar(Scrollbar *&scrollbar, Widget *parent)
    {
        if (scrollbar) return;
        scrollbar = make_x_scrollbar(parent);
    }

    inline void ensure_y_scrollbar(Scrollbar *&scrollbar, Widget *parent)
    {
        if (scrollbar) return;
        scrollbar = make_y_scrollbar(parent);
    }

    inline void ensure_internal_y_scrollbar(Scrollbar *&scrollbar, Widget *parent)
    {
        if (scrollbar) return;
        scrollbar = make_internal_y_scrollbar(parent);
    }

    inline void ensure_internal_x_scrollbar(Scrollbar *&scrollbar, Widget *parent)
    {
        if (scrollbar) return;
        scrollbar = make_internal_x_scrollbar(parent);
    }

    inline bool is_scrollbar_x_drag(ElementID drag_id, u32 owner_id)
    {
        return drag_id.widget_id == owner_id &&
               (drag_id.tag_id == AUIK_TAG_SCROLLBAR_TRACK_X || drag_id.tag_id == AUIK_TAG_SCROLLBAR_THUMB_X);
    }

    inline bool is_scrollbar_y_drag(ElementID drag_id, u32 owner_id)
    {
        return drag_id.widget_id == owner_id &&
               (drag_id.tag_id == AUIK_TAG_SCROLLBAR_TRACK_Y || drag_id.tag_id == AUIK_TAG_SCROLLBAR_THUMB_Y);
    }

    inline bool is_scrollbar_thumb_drag(ElementID drag_id)
    { return drag_id.tag_id == AUIK_TAG_SCROLLBAR_THUMB_X || drag_id.tag_id == AUIK_TAG_SCROLLBAR_THUMB_Y; }

} // namespace auik::detail
