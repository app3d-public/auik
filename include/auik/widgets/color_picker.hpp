#pragma once

#include <acul/vector.hpp>
#include "../model.hpp"
#include "../pipelines.hpp"
#include "widget.hpp"

#define AUIK_TAG_CIRCLE_COLOR_PICKER        0xD3C6A92Fu
#define AUIK_TAG_CIRCLE_COLOR_PICKER_GRAB   0x5A9E10C4u
#define AUIK_TAG_GRADIENT_COLOR_PICKER      0x254F849Eu
#define AUIK_TAG_GRADIENT_COLOR_PICKER_GRAB 0x74BAB002u
#define AUIK_TAG_SQUARE_COLOR_PICKER        0x18D73B9Au
#define AUIK_TAG_SQUARE_COLOR_PICKER_GRAB   0x9B41E062u

namespace auik
{
    class CircleColorPicker final : public Widget
    {
    public:
        AUIK_EXPORT CircleColorPicker(u32 id, const amal::vec4 &value, f32 diameter, WidgetFlags widget_flags);
        AUIK_EXPORT StyleUpdateFlags update_style() override;
        AUIK_EXPORT void update_layout_min_size() override;
        AUIK_EXPORT void update_layout(bool min_size_known) override;
        AUIK_EXPORT void translate(const amal::vec2 &delta) override;
        AUIK_EXPORT void rebuild_clip_rects() override;
        AUIK_EXPORT void reset_draw_records() override;
        AUIK_EXPORT void update_depth(const amal::vec2 &depth_range) override;
        AUIK_EXPORT void back_hit_depth() override;
        AUIK_EXPORT void restore_hit_depth() override;
        AUIK_EXPORT void draw(DrawCtx &ctx) override;
        AUIK_EXPORT void on_click(MouseKey key, KeyPressState state, u32 click_count) override;
        AUIK_EXPORT void on_drag(const amal::vec2 &delta, KeyPressState state) override;

        const amal::vec4 &color() const { return _value; }
        f32 hue_deg() const { return _hue_deg; }
        f32 radius_norm() const { return _radius_norm; }
        AUIK_EXPORT void set_color(const amal::vec4 &color);
        AUIK_EXPORT void set_model_binding(ModelBinding *binding);
        AUIK_EXPORT void set_hue_radius(f32 hue_deg, f32 radius_t);
        AUIK_EXPORT bool has_draw_record() const;
        virtual u32 signature() const override { return AUIK_TAG_CIRCLE_COLOR_PICKER; }

    private:
        f32 _preferred_side = 0.0f;
        amal::vec4 _value = {1.0f, 1.0f, 1.0f, 1.0f};
        ModelBinding *_model_binding = nullptr;
        f32 _hue_deg = 0.0f;
        f32 _radius_norm = 0.0f;
        struct LayoutCache
        {
            amal::vec2 center{0.0f, 0.0f};
            f32 wheel_radius_outer = 0.0f;
            f32 wheel_radius = 0.0f;
        } _layout{};

        StyleSelector _grab_style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_GRADIENT_SLIDER_GRAB};
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
        void sync_batch();
        static amal::vec2 resolve_grab_size(const Style &grab_style);
    };

    inline CircleColorPicker *make_circle_color_picker(u32 id, const amal::vec4 &value = {1.0f, 1.0f, 1.0f, 1.0f})
    {
        auto *theme = get_theme();
        assert(theme && "theme is null");
        f32 diameter = theme->get_var<f32>(AUIK_STYLE_VAR_COLOR_PICKER_SIZE);
        constexpr WidgetFlags widget_flags = WidgetFlagBits::visible | WidgetFlagBits::attachable |
                                             WidgetFlagBits::configurable | WidgetFlagBits::hittable;
        return acul::alloc<CircleColorPicker>(id, value, diameter, widget_flags);
    }

    inline CircleColorPicker *make_circle_color_picker(u32 id, f32 diameter,
                                                       const amal::vec4 &value = {1.0f, 1.0f, 1.0f, 1.0f})
    {
        constexpr WidgetFlags widget_flags = WidgetFlagBits::visible | WidgetFlagBits::attachable |
                                             WidgetFlagBits::configurable | WidgetFlagBits::hittable;
        return acul::alloc<CircleColorPicker>(id, value, diameter, widget_flags);
    }

    class GradientColorPicker final : public Widget
    {
    public:
        AUIK_EXPORT GradientColorPicker(u32 id, const amal::vec4 &value, const amal::vec2 &size,
                                        WidgetFlags widget_flags);

        AUIK_EXPORT StyleUpdateFlags update_style() override;
        AUIK_EXPORT void update_layout_min_size() override;
        AUIK_EXPORT void update_layout(bool min_size_known) override;
        AUIK_EXPORT void translate(const amal::vec2 &delta) override;
        AUIK_EXPORT void rebuild_clip_rects() override;
        AUIK_EXPORT void reset_draw_records() override;
        AUIK_EXPORT void update_depth(const amal::vec2 &depth_range) override;
        AUIK_EXPORT void back_hit_depth() override;
        AUIK_EXPORT void restore_hit_depth() override;
        AUIK_EXPORT void draw(DrawCtx &ctx) override;
        AUIK_EXPORT void on_click(MouseKey key, KeyPressState state, u32 click_count) override;
        AUIK_EXPORT void on_drag(const amal::vec2 &delta, KeyPressState state) override;

        const amal::vec4 &color() const { return _value; }
        f32 hue_deg() const { return _hue_deg; }
        f32 saturation() const { return _saturation; }
        f32 value_norm() const { return _value_norm; }
        AUIK_EXPORT void set_color(const amal::vec4 &color);
        AUIK_EXPORT void set_model_binding(ModelBinding *binding);
        AUIK_EXPORT void set_hsv(f32 hue_deg, f32 saturation, f32 value_t);
        AUIK_EXPORT bool has_draw_record() const;
        virtual u32 signature() const override { return AUIK_TAG_GRADIENT_COLOR_PICKER; }

    private:
        amal::vec2 _preferred_size{0.0f, 0.0f};
        amal::vec4 _value = {1.0f, 0.0f, 0.0f, 1.0f};
        ModelBinding *_model_binding = nullptr;
        f32 _hue_deg = 0.0f;
        f32 _saturation = 1.0f;
        f32 _value_norm = 1.0f;
        amal::rect _gradient_rect{};

        StyleSelector _grab_style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_GRADIENT_SLIDER_GRAB};
        DrawDataID _gradient_draw_id{};
        DrawDataID _grab_draw_id{};
        DrawDataID _grab_back_draw_id{};
        VertexStreamBatchData _gradient_batch{};
        acul::vector<VertexStreamVertex> _gradient_vertices;
        acul::vector<VertexStreamIndex> _gradient_indices;
        QuadsInstanceData _grab_visual{};
        QuadsInstanceData _grab_back_visual{};
        detail::RectData _grab_hit_rect{};
        amal::vec2 _gradient_depth_range{0.0f, 1.0f};
        amal::vec2 _overlay_depth_range{0.0f, 1.0f};
        amal::vec2 _grab_depth_range{0.0f, 1.0f};
        bool _cache_valid = false;

        void rebuild_gradient_visual();
        void rebuild_grab_visual();
        void rebuild_cached_visuals();
        void translate_cached_visuals(const amal::vec2 &delta);
        void update_value_from_mouse();
        void sync_batch();
        static amal::vec2 resolve_grab_size(const Style &grab_style);
    };

    inline GradientColorPicker *make_gradient_color_picker(u32 id,
                                                           const amal::vec4 &value = {1.0f, 0.0f, 0.0f, 1.0f},
                                                           const amal::vec2 &size = AUIK_SIZE_FIT)
    {
        constexpr WidgetFlags widget_flags = WidgetFlagBits::visible | WidgetFlagBits::attachable |
                                             WidgetFlagBits::configurable | WidgetFlagBits::hittable;
        return acul::alloc<GradientColorPicker>(id, value, size, widget_flags);
    }

    class SquareColorPicker final : public Widget
    {
    public:
        AUIK_EXPORT SquareColorPicker(u32 id, const amal::vec4 &value, f32 size, WidgetFlags widget_flags);
        AUIK_EXPORT ~SquareColorPicker() override;
        AUIK_EXPORT StyleUpdateFlags update_style() override;
        AUIK_EXPORT void update_layout_min_size() override;
        AUIK_EXPORT void update_layout(bool min_size_known) override;
        AUIK_EXPORT void translate(const amal::vec2 &delta) override;
        AUIK_EXPORT void rebuild_clip_rects() override;
        AUIK_EXPORT void reset_draw_records() override;
        AUIK_EXPORT void update_depth(const amal::vec2 &depth_range) override;
        AUIK_EXPORT void back_hit_depth() override;
        AUIK_EXPORT void restore_hit_depth() override;
        AUIK_EXPORT void draw(DrawCtx &ctx) override;
        AUIK_EXPORT void on_click(MouseKey key, KeyPressState state, u32 click_count) override;
        AUIK_EXPORT void on_drag(const amal::vec2 &delta, KeyPressState state) override;

        const amal::vec4 &color() const;
        f32 hue_deg() const { return _hue_deg; }
        f32 saturation() const;
        f32 value_norm() const;
        AUIK_EXPORT void set_color(const amal::vec4 &color);
        AUIK_EXPORT void set_model_binding(ModelBinding *binding);
        AUIK_EXPORT void set_hsv(f32 hue_deg, f32 saturation, f32 value_t);
        AUIK_EXPORT bool has_draw_record() const;
        virtual u32 signature() const override { return AUIK_TAG_SQUARE_COLOR_PICKER; }

    private:
        enum class ActiveZone : u8
        {
            none = 0,
            ring,
            square
        };

        f32 _preferred_side = 0.0f;
        GradientColorPicker *_gradient = nullptr;
        ModelBinding *_model_binding = nullptr;
        f32 _hue_deg = 0.0f;
        struct LayoutCache
        {
            amal::vec2 center{0.0f, 0.0f};
            f32 ring_outer_radius = 0.0f;
            f32 ring_inner_radius = 0.0f;
            amal::rect sv_rect{};
        } _layout{};

        StyleSelector _grab_style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_GRADIENT_SLIDER_GRAB};
        DrawDataID _ring_draw_id{};
        DrawDataID _ring_grab_draw_id{};
        DrawDataID _ring_grab_back_draw_id{};
        VertexStreamBatchData _ring_batch{};
        acul::vector<VertexStreamVertex> _ring_vertices;
        acul::vector<VertexStreamIndex> _ring_indices;
        QuadsInstanceData _ring_grab_visual{};
        QuadsInstanceData _ring_grab_back_visual{};
        detail::RectData _ring_grab_hit_rect{};
        amal::vec2 _ring_depth_range{0.0f, 1.0f};
        amal::vec2 _grab_depth_range{0.0f, 1.0f};
        ActiveZone _active_zone = ActiveZone::none;
        bool _cache_valid = false;

        void rebuild_layout_geometry();
        void rebuild_ring_visual();
        void rebuild_grab_visual();
        void rebuild_cached_visuals();
        void translate_cached_visuals(const amal::vec2 &delta);
        void update_value_from_mouse();
        void sync_batches();
        ActiveZone pick_active_zone_from_mouse() const;
        static amal::vec2 resolve_grab_size(const Style &grab_style);
    };

    inline SquareColorPicker *make_square_color_picker(u32 id, const amal::vec4 &value = {1.0f, 1.0f, 1.0f, 1.0f})
    {
        constexpr WidgetFlags widget_flags = WidgetFlagBits::visible | WidgetFlagBits::attachable |
                                             WidgetFlagBits::configurable | WidgetFlagBits::hittable;
        return acul::alloc<SquareColorPicker>(id, value, 0.0f, widget_flags);
    }

    inline SquareColorPicker *make_square_color_picker(u32 id, f32 size,
                                                       const amal::vec4 &value = {1.0f, 1.0f, 1.0f, 1.0f})
    {
        constexpr WidgetFlags widget_flags = WidgetFlagBits::visible | WidgetFlagBits::attachable |
                                             WidgetFlagBits::configurable | WidgetFlagBits::hittable;
        return acul::alloc<SquareColorPicker>(id, value, size, widget_flags);
    }

    namespace streams
    {
        extern AUIK_EXPORT const umbf::streams::Stream circle_color_picker;
        extern AUIK_EXPORT const umbf::streams::Stream gradient_color_picker;
        extern AUIK_EXPORT const umbf::streams::Stream square_color_picker;
    } // namespace streams
} // namespace auik
