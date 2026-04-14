#pragma once

#include <acul/memory/alloc.hpp>
#include <acul/vector.hpp>
#include "slider.hpp"

#define AUIK_TAG_CIRCLE_COLOR_PICKER      0xD3C6A92Fu
#define AUIK_TAG_CIRCLE_COLOR_PICKER_GRAB 0x5A9E10C4u

namespace auik::v2
{
    class APPLIB_API CircleColorPicker final : public Widget
    {
    public:
        CircleColorPicker(u32 id, amal::vec4 *value, f32 diameter = 0.0f,
                          WidgetFlags widget_flags = get_default_widget_flags() | WidgetFlagBits::hittable,
                          Widget *parent = nullptr);

        StyleUpdateFlags update_style() override;
        void update_layout_min_size() override;
        void update_layout(bool min_size_known) override;
        void translate(const amal::vec2 &delta) override;
        void rebuild_clip_rects() override;
        void update_depth(const amal::vec2 &depth_range) override;
        void draw(DrawCtx &ctx) override;
        void on_click(MouseKey key, KeyPressState state, u32 click_count) override;
        void on_drag(const amal::vec2 &delta, KeyPressState state) override;

        amal::vec4 color() const { return _value ? *_value : _resolved_color; }
        void set_color(const amal::vec4 &color);
        bool has_draw_record() const;

    private:
        amal::vec4 *_value = nullptr;
        amal::vec4 _resolved_color = {1.0f, 1.0f, 1.0f, 1.0f};
        f32 _hue_deg = 0.0f;
        f32 _radius_t = 0.0f;

        StyleSelector _grab_style{Theme::STYLE_ID_INVALID, AUIK_TAG_GRADIENT_SLIDER_GRAB};
        DrawDataID _wheel_draw_id{};
        DrawDataID _grab_draw_id{};
        DrawDataID _grab_back_draw_id{};
        VertexStreamBatchData _wheel_batch{};
        acul::vector<VertexStreamVertex> _wheel_vertices;
        acul::vector<VertexStreamIndex> _wheel_indices;
        QuadsInstanceData _grab_visual{};
        QuadsInstanceData _grab_back_visual{};
        detail::RectData _grab_hit_rect{};
        amal::vec2 _track_depth_range{0.0f, 1.0f};
        amal::vec2 _grab_depth_range{0.0f, 1.0f};
        bool _drag_started = false;

        void rebuild_wheel_visual();
        void rebuild_grab_visual();
        void rebuild_cached_visuals();
        void update_value_from_mouse();
        void set_hue_radius(f32 hue_deg, f32 radius_t);
        void sync_batch();
        static amal::vec2 resolve_grab_size(const Style &grab_style);
    };

    inline CircleColorPicker *make_circle_color_picker(u32 id, amal::vec4 *value, Widget *parent = nullptr)
    {
        return acul::alloc<CircleColorPicker>(id, value, 0.0f, get_default_widget_flags() | WidgetFlagBits::hittable,
                                              parent);
    }

    inline CircleColorPicker *make_fixed_circle_color_picker(u32 id, amal::vec4 *value, f32 diameter,
                                                              Widget *parent = nullptr)
    {
        return acul::alloc<CircleColorPicker>(id, value, diameter,
                                              get_default_widget_flags() | WidgetFlagBits::hittable |
                                                  WidgetFlagBits::fixed,
                                              parent);
    }
} // namespace auik::v2
