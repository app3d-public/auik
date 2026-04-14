#pragma once

#include <acul/memory/alloc.hpp>
#include <acul/vector.hpp>
#include "../detail/vertex_draw.hpp"
#include "../pipelines.hpp"
#include "widget.hpp"

#define AUIK_TAG_SLIDER                      0x96FAB223u
#define AUIK_TAG_SLIDER_GRAB                 0xCA7863FAu
#define AUIK_TAG_GRADIENT_SLIDER             0x9C0AE52Du
#define AUIK_TAG_GRADIENT_SLIDER_GRAB        0x4B49A40Eu
#define AUIK_TAG_GRADIENT_SLIDER_GRAB_BORDER 0xA91B53C2u
#define AUIK_TAG_RANGE_SLIDER                0x8A233C5Fu
#define AUIK_TAG_RANGE_SLIDER_GRAB           0x3214A7D9u

namespace auik::v2
{
    class Slider;
    class GradientSlider;
    class RangeSlider;
    enum class GradientTrackKind : u8
    {
        custom = 0,
        hsl = 1
    };

    namespace detail
    {
        struct SliderTrackVisual
        {
            struct LayerBits
            {
                enum enum_type : u8
                {
                    none = 0x00,
                    background = 0x01,
                    fill = 0x02,
                    border = 0x04
                };
            };

            DrawDataID background_draw_id{};
            DrawDataID fill_draw_id{};
            DrawDataID border_draw_id{};
            QuadsInstanceData background{};
            QuadsInstanceData fill{};
            QuadsInstanceData border{};
            u8 layer_mask = LayerBits::none;

            void clear()
            {
                background_draw_id = {};
                fill_draw_id = {};
                border_draw_id = {};
                clear_payload();
            }

            void clear_payload()
            {
                background = {};
                fill = {};
                border = {};
                layer_mask = LayerBits::none;
            }

            bool has_layer(u8 layer) const { return (layer_mask & layer) != 0; }
            void set_layer(u8 layer) { layer_mask |= layer; }
            void clear_layer(u8 layer) { layer_mask &= static_cast<u8>(~layer); }
        };

        struct GradientTrackVisual
        {
            DrawDataID draw_id{};
            GradientRectVertexData data{};
            bool valid = false;

            void clear()
            {
                draw_id = {};
                clear_payload();
            }

            void clear_payload()
            {
                data.clear();
                valid = false;
            }
        };

        void build_quad_slider_track_visual(SliderTrackVisual &visual, const Style &style,
                                            const amal::rect &track_rect, const amal::vec2 &depth_range,
                                            u16 clip_id);
        void build_gradient_slider_track_visual(SliderTrackVisual &visual, GradientTrackVisual &gradient_visual,
                                                const Style &style,
                                                const amal::rect &track_rect, const amal::vec2 &depth_range,
                                                u16 clip_id, const amal::vec4 *colors, u32 color_count);
    } // namespace detail

    constexpr inline WidgetFlags get_default_slider_flags()
    {
        return get_default_widget_flags() | WidgetFlagBits::hittable;
    }

    constexpr inline WidgetFlags get_default_fixed_slider_flags()
    {
        return get_default_slider_flags() | WidgetFlagBits::fixed;
    }

    class APPLIB_API Slider final : public Widget
    {
    public:
        Slider(u32 id, f32 *value, f32 min_value, f32 max_value, f32 width, f32 *range_start_value = nullptr,
               WidgetFlags widget_flags = get_default_slider_flags(), Widget *parent = nullptr);

        StyleUpdateFlags update_style() override;
        void update_layout_min_size() override;
        void update_layout(bool min_size_known) override;
        void translate(const amal::vec2 &delta) override;
        void rebuild_clip_rects() override;
        void update_depth(const amal::vec2 &depth_range) override;
        void draw(DrawCtx &ctx) override;
        void on_click(MouseKey key, KeyPressState state, u32 click_count) override;
        void on_drag(const amal::vec2 &delta, KeyPressState state) override;

        f32 value() const { return _value ? *_value : _min_value; }
        void set_value(f32 value);
        void set_range_start_value_ptr(f32 *value_ptr);
        void set_step(f32 step);
        f32 step() const { return _step; }
        bool has_draw_record() const;

    private:
        f32 *_value = nullptr;
        f32 *_range_start_value = nullptr;
        f32 _min_value = 0.0f;
        f32 _max_value = 1.0f;
        StyleSelector _track_style{Theme::STYLE_ID_INVALID, AUIK_TAG_SLIDER};
        StyleSelector _fill_style{Theme::STYLE_ID_INVALID, AUIK_TAG_SLIDER};
        StyleSelector _grab_style{Theme::STYLE_ID_INVALID, AUIK_TAG_SLIDER_GRAB};
        detail::SliderTrackVisual _track_visual;
        DrawDataID _grab_draw_id{};
        QuadsInstanceData _grab_visual{};
        amal::rect _track_rect{};
        amal::rect _grab_rect{};
        detail::RectData _grab_hit_rect{};
        amal::vec2 _track_depth_range{0.0f, 1.0f};
        amal::vec2 _grab_depth_range{0.0f, 1.0f};
        f32 _step = 0.0f;
        bool _drag_started = false;

        void rebuild_track_visuals();
        void rebuild_grab_visual();
        void rebuild_cached_visuals();
        void rebuild_track_fill_visual();
        void update_value_from_mouse();
        amal::vec2 resolve_grab_size(const Style &grab_style) const;
        bool resolve_range_values(f32 &out_start, f32 &out_end) const;
    };

    class APPLIB_API GradientSlider final : public Widget
    {
    public:
        ~GradientSlider() override;
        GradientSlider(u32 id, f32 *value, f32 min_value, f32 max_value, f32 width, const amal::vec4 *colors,
                       u32 color_count, WidgetFlags widget_flags = get_default_slider_flags(), Widget *parent = nullptr,
                       GradientTrackKind gradient_kind = GradientTrackKind::custom);

        StyleUpdateFlags update_style() override;
        void update_layout_min_size() override;
        void update_layout(bool min_size_known) override;
        void translate(const amal::vec2 &delta) override;
        void rebuild_clip_rects() override;
        void update_depth(const amal::vec2 &depth_range) override;
        void draw(DrawCtx &ctx) override;
        void on_click(MouseKey key, KeyPressState state, u32 click_count) override;
        void on_drag(const amal::vec2 &delta, KeyPressState state) override;

        f32 value() const { return _value ? *_value : _min_value; }
        void set_value(f32 value);
        void set_step(f32 step);
        f32 step() const { return _step; }
        bool has_draw_record() const;

    private:
        f32 *_value = nullptr;
        f32 _min_value = 0.0f;
        f32 _max_value = 1.0f;
        StyleSelector _track_style{Theme::STYLE_ID_INVALID, AUIK_TAG_GRADIENT_SLIDER};
        StyleSelector _grab_style{Theme::STYLE_ID_INVALID, AUIK_TAG_GRADIENT_SLIDER_GRAB};
        acul::vector<amal::vec4> _colors;
        acul::vector<amal::vec4> _hsl_cache;
        detail::SliderTrackVisual _track_visual;
        detail::GradientTrackVisual _gradient_visual;
        DrawDataID _grab_draw_id{};
        DrawDataID _grab_back_draw_id{};
        QuadsInstanceData _grab_visual{};
        QuadsInstanceData _grab_back_visual{};
        amal::rect _track_rect{};
        amal::rect _grab_rect{};
        detail::RectData _grab_hit_rect{};
        amal::vec2 _track_depth_range{0.0f, 1.0f};
        amal::vec2 _grab_depth_range{0.0f, 1.0f};
        f32 _step = 0.0f;
        bool _drag_started = false;
        GradientTrackKind _gradient_kind = GradientTrackKind::custom;

        void rebuild_track_visuals();
        void rebuild_grab_visual();
        void rebuild_cached_visuals();
        void update_value_from_mouse();
        amal::vec2 resolve_grab_size(const Style &grab_style) const;
        amal::vec4 resolve_active_color(f32 factor) const;
    };

    class APPLIB_API RangeSlider final : public Widget
    {
    public:
        RangeSlider(u32 id, f32 *from_value, f32 *to_value, f32 min_value, f32 max_value, f32 width,
                    WidgetFlags widget_flags = get_default_slider_flags(), Widget *parent = nullptr);

        StyleUpdateFlags update_style() override;
        void update_layout_min_size() override;
        void update_layout(bool min_size_known) override;
        void translate(const amal::vec2 &delta) override;
        void rebuild_clip_rects() override;
        void update_depth(const amal::vec2 &depth_range) override;
        void draw(DrawCtx &ctx) override;
        void on_click(MouseKey key, KeyPressState state, u32 click_count) override;
        void on_drag(const amal::vec2 &delta, KeyPressState state) override;

        void set_step(f32 step);
        void set_values(f32 from_value, f32 to_value);
        bool has_draw_record() const;

    private:
        enum class ActiveGrab : u8
        {
            none = 0,
            from = 1,
            to = 2
        };

        f32 *_from_value = nullptr;
        f32 *_to_value = nullptr;
        f32 _min_value = 0.0f;
        f32 _max_value = 1.0f;
        f32 _step = 0.0f;
        StyleSelector _track_style{Theme::STYLE_ID_INVALID, AUIK_TAG_SLIDER};
        StyleSelector _fill_style{Theme::STYLE_ID_INVALID, AUIK_TAG_SLIDER};
        StyleSelector _grab_style{Theme::STYLE_ID_INVALID, AUIK_TAG_RANGE_SLIDER_GRAB};
        detail::SliderTrackVisual _track_visual;
        DrawDataID _from_draw_id{};
        DrawDataID _to_draw_id{};
        QuadsInstanceData _from_visual{};
        QuadsInstanceData _to_visual{};
        amal::rect _track_rect{};
        amal::rect _from_rect{};
        amal::rect _to_rect{};
        detail::RectData _from_hit_rect{};
        detail::RectData _to_hit_rect{};
        amal::vec2 _track_depth_range{0.0f, 1.0f};
        amal::vec2 _grab_depth_range{0.0f, 1.0f};
        ActiveGrab _active_grab = ActiveGrab::none;

        void rebuild_track_visuals();
        void rebuild_grab_visuals();
        void rebuild_cached_visuals();
        void update_active_grab_from_mouse();
        void update_active_value_from_mouse();
        amal::vec2 resolve_grab_size(const Style &grab_style) const;
        f32 resolve_grab_center_x(f32 value, f32 half_grab_w) const;
        f32 clamped_value(f32 value) const;
    };

    inline Slider *make_slider(u32 id, f32 *value, f32 min_value = 0.0f, f32 max_value = 1.0f, Widget *parent = nullptr)
    {
        return acul::alloc<Slider>(id, value, min_value, max_value, 0.0f, nullptr, get_default_slider_flags(), parent);
    }

    inline GradientSlider *make_gradient_slider(u32 id, f32 *value, f32 min_value, f32 max_value,
                                                const amal::vec4 *colors, u32 color_count, Widget *parent = nullptr)
    {
        return acul::alloc<GradientSlider>(id, value, min_value, max_value, 0.0f, colors, color_count,
                                           get_default_slider_flags(), parent);
    }

    inline Slider *make_fixed_slider(u32 id, f32 *value, f32 width, f32 min_value = 0.0f, f32 max_value = 1.0f,
                                     Widget *parent = nullptr)
    {
        return acul::alloc<Slider>(id, value, min_value, max_value, width, nullptr, get_default_fixed_slider_flags(),
                                   parent);
    }

    inline GradientSlider *make_fixed_gradient_slider(u32 id, f32 *value, f32 width, f32 min_value, f32 max_value,
                                                      const amal::vec4 *colors, u32 color_count,
                                                      Widget *parent = nullptr)
    {
        return acul::alloc<GradientSlider>(id, value, min_value, max_value, width, colors, color_count,
                                           get_default_fixed_slider_flags(), parent);
    }

    inline Slider *make_slider_with_range(u32 id, f32 *value, f32 *range_start_value, f32 min_value = 0.0f,
                                          f32 max_value = 1.0f, Widget *parent = nullptr)
    {
        return acul::alloc<Slider>(id, value, min_value, max_value, 0.0f, range_start_value,
                                   get_default_slider_flags(), parent);
    }

    inline Slider *make_fixed_slider_with_range(u32 id, f32 *value, f32 *range_start_value, f32 width,
                                                f32 min_value = 0.0f, f32 max_value = 1.0f, Widget *parent = nullptr)
    {
        return acul::alloc<Slider>(id, value, min_value, max_value, width, range_start_value,
                                   get_default_fixed_slider_flags(), parent);
    }

    inline GradientSlider *make_hsl_slider(u32 id, f32 *value, Widget *parent = nullptr)
    {
        static constexpr amal::vec4 hsl_colors[] = {
            {1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 1.0f, 1.0f},
            {0.0f, 0.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f},
        };
        return acul::alloc<GradientSlider>(id, value, 0.0f, 360.0f, 0.0f, hsl_colors, 7u, get_default_slider_flags(),
                                           parent, GradientTrackKind::hsl);
    }

    inline GradientSlider *make_fixed_hsl_slider(u32 id, f32 *value, f32 width, Widget *parent = nullptr)
    {
        static constexpr amal::vec4 hsl_colors[] = {
            {1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 1.0f, 1.0f},
            {0.0f, 0.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f},
        };
        return acul::alloc<GradientSlider>(id, value, 0.0f, 360.0f, width, hsl_colors, 7u,
                                           get_default_fixed_slider_flags(), parent, GradientTrackKind::hsl);
    }

    inline RangeSlider *make_range_slider(u32 id, f32 *from_value, f32 *to_value, f32 min_value = 0.0f,
                                          f32 max_value = 1.0f, Widget *parent = nullptr)
    {
        return acul::alloc<RangeSlider>(id, from_value, to_value, min_value, max_value, 0.0f,
                                        get_default_slider_flags(), parent);
    }

    inline RangeSlider *make_fixed_range_slider(u32 id, f32 *from_value, f32 *to_value, f32 width, f32 min_value = 0.0f,
                                                f32 max_value = 1.0f, Widget *parent = nullptr)
    {
        return acul::alloc<RangeSlider>(id, from_value, to_value, min_value, max_value, width,
                                        get_default_fixed_slider_flags(), parent);
    }
} // namespace auik::v2
