#pragma once

#include <acul/memory/alloc.hpp>
#include <acul/vector.hpp>
#include "slider.hpp"

#define AUIK_VAR_COLOR_PICKER_SIZE        0x81C54C6Eu
#define AUIK_TAG_CIRCLE_COLOR_PICKER      0xD3C6A92Fu
#define AUIK_TAG_CIRCLE_COLOR_PICKER_GRAB 0x5A9E10C4u
#define AUIK_TAG_SQUARE_COLOR_PICKER      0x18D73B9Au
#define AUIK_TAG_SQUARE_COLOR_PICKER_GRAB 0x9B41E062u

namespace auik::v2
{
    class APPLIB_API CircleColorPicker final : public Widget
    {
    public:
        CircleColorPicker(u32 id, amal::vec4 *value, f32 diameter = 0.0f,
                          WidgetFlags widget_flags = get_default_widget_flags() | WidgetFlagBits::hittable |
                                                     WidgetFlagBits::fixed,
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
        f32 _preferred_side = 0.0f;
        amal::vec4 *_value = nullptr;
        amal::vec4 _resolved_color = {1.0f, 1.0f, 1.0f, 1.0f};
        f32 _hue_deg = 0.0f;
        f32 _radius_norm = 0.0f;
        struct LayoutCache
        {
            amal::vec2 center{0.0f, 0.0f};
            f32 wheel_radius_outer = 0.0f;
            f32 wheel_radius = 0.0f;
        } _layout{};

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
        bool _cache_valid = false;

        void rebuild_wheel_visual();
        void rebuild_grab_visual();
        void rebuild_cached_visuals();
        void translate_cached_visuals(const amal::vec2 &delta);
        void rebuild_layout_cache();
        void update_value_from_mouse();
        void set_hue_radius(f32 hue_deg, f32 radius_t);
        void sync_batch();
        static amal::vec2 resolve_grab_size(const Style &grab_style);
    };

    inline CircleColorPicker *make_circle_color_picker(u32 id, amal::vec4 *value, Widget *parent = nullptr)
    {
        auto *theme = get_theme();
        assert(theme && "theme is null");
        f32 diameter = theme->get_var<f32>(AUIK_VAR_COLOR_PICKER_SIZE);
        return acul::alloc<CircleColorPicker>(id, value, diameter,
                                              get_default_widget_flags() | WidgetFlagBits::hittable |
                                                  WidgetFlagBits::fixed,
                                              parent);
    }

    inline CircleColorPicker *make_circle_color_picker(u32 id, amal::vec4 *value, f32 diameter,
                                                       Widget *parent = nullptr)
    {
        return acul::alloc<CircleColorPicker>(id, value, diameter,
                                              get_default_widget_flags() | WidgetFlagBits::hittable |
                                                  WidgetFlagBits::fixed,
                                              parent);
    }

    class APPLIB_API SquareColorPicker final : public Widget
    {
    public:
        SquareColorPicker(u32 id, amal::vec4 *value, f32 size = 0.0f,
                          WidgetFlags widget_flags = get_default_widget_flags() | WidgetFlagBits::hittable |
                                                     WidgetFlagBits::fixed,
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
        enum class ActiveZone : u8
        {
            none = 0,
            ring,
            square
        };

        f32 _preferred_side = 0.0f;
        amal::vec4 *_value = nullptr;
        amal::vec4 _resolved_color = {1.0f, 0.0f, 0.0f, 1.0f};
        f32 _hue_deg = 0.0f;
        f32 _saturation = 1.0f;
        f32 _value_norm = 1.0f;
        struct LayoutCache
        {
            amal::vec2 center{0.0f, 0.0f};
            f32 ring_outer_radius = 0.0f;
            f32 ring_inner_radius = 0.0f;
            amal::rect sv_rect{};
        } _layout{};

        StyleSelector _grab_style{Theme::STYLE_ID_INVALID, AUIK_TAG_GRADIENT_SLIDER_GRAB};
        DrawDataID _ring_draw_id{};
        DrawDataID _sv_draw_id{};
        DrawDataID _ring_grab_draw_id{};
        DrawDataID _ring_grab_back_draw_id{};
        DrawDataID _sv_grab_draw_id{};
        DrawDataID _sv_grab_back_draw_id{};
        VertexStreamBatchData _ring_batch{};
        VertexStreamBatchData _sv_batch{};
        acul::vector<VertexStreamVertex> _ring_vertices;
        acul::vector<VertexStreamIndex> _ring_indices;
        acul::vector<VertexStreamVertex> _sv_vertices;
        acul::vector<VertexStreamIndex> _sv_indices;
        QuadsInstanceData _ring_grab_visual{};
        QuadsInstanceData _ring_grab_back_visual{};
        QuadsInstanceData _sv_grab_visual{};
        QuadsInstanceData _sv_grab_back_visual{};
        detail::RectData _ring_grab_hit_rect{};
        detail::RectData _sv_grab_hit_rect{};
        amal::vec2 _ring_depth_range{0.0f, 1.0f};
        amal::vec2 _square_depth_range{0.0f, 1.0f};
        amal::vec2 _square_overlay_depth_range{0.0f, 1.0f};
        amal::vec2 _grab_depth_range{0.0f, 1.0f};
        ActiveZone _active_zone = ActiveZone::none;
        bool _cache_valid = false;

        void rebuild_layout_geometry();
        void rebuild_ring_visual();
        void rebuild_square_visual();
        void rebuild_grab_visuals();
        void rebuild_cached_visuals();
        void translate_cached_visuals(const amal::vec2 &delta);
        void update_value_from_mouse();
        void set_hsv(f32 hue_deg, f32 saturation, f32 value_t);
        void sync_batches();
        ActiveZone pick_active_zone_from_mouse() const;
        static amal::vec2 resolve_grab_size(const Style &grab_style);
    };

    inline SquareColorPicker *make_square_color_picker(u32 id, amal::vec4 *value, Widget *parent = nullptr)
    {
        return acul::alloc<SquareColorPicker>(id, value, 0.0f,
                                              get_default_widget_flags() | WidgetFlagBits::hittable |
                                                  WidgetFlagBits::fixed,
                                              parent);
    }

    inline SquareColorPicker *make_square_color_picker(u32 id, amal::vec4 *value, f32 size, Widget *parent = nullptr)
    {
        return acul::alloc<SquareColorPicker>(id, value, size,
                                              get_default_widget_flags() | WidgetFlagBits::hittable |
                                                  WidgetFlagBits::fixed,
                                              parent);
    }
} // namespace auik::v2
