#pragma once

#include <acul/vector.hpp>
#include <amal/geometric.hpp>
#include "../detail/vertex_draw.hpp"
#include "../model.hpp"
#include "../pipelines.hpp"
#include "image.hpp"
#include "widget.hpp"


#define AUIK_TAG_SLIDER                      0x96FAB223u
#define AUIK_TAG_SLIDER_GRAB                 0xCA7863FAu
#define AUIK_TAG_GRADIENT_SLIDER             0x9C0AE52Du
#define AUIK_TAG_GRADIENT_SLIDER_GRAB        0x4B49A40Eu
#define AUIK_TAG_GRADIENT_SLIDER_GRAB_BORDER 0xA91B53C2u
#define AUIK_TAG_TRANSPARENCY_SLIDER         0x45BB3829u
#define AUIK_TAG_RANGE_SLIDER                0x8A233C5Fu
#define AUIK_RANGE_SLIDER_START_FIELD        0u
#define AUIK_RANGE_SLIDER_END_FIELD          1u

namespace auik
{
    class Slider;
    class GradientSlider;
    class TransparencySlider;
    class RangeSlider;

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

        void build_quad_slider_track_visual(SliderTrackVisual &visual, const Style &style, const amal::rect &track_rect,
                                            const amal::vec2 &depth_range, u16 clip_id);
        void build_gradient_slider_track_visual(SliderTrackVisual &visual, GradientTrackVisual &gradient_visual,
                                                const Style &style, const amal::rect &track_rect,
                                                const amal::vec2 &depth_range, u16 clip_id, const amal::vec4 *colors,
                                                u32 color_count);
    } // namespace detail

    constexpr inline WidgetFlags get_default_slider_flags()
    { return get_default_widget_flags() | WidgetFlagBits::hittable; }

    class Slider final : public Widget
    {
    public:
        AUIK_EXPORT Slider(u32 id, f32 value, f32 min_value, f32 max_value, f32 range_start_value, amal::axis axis,
                           f32 size, WidgetFlags widget_flags = get_default_slider_flags(), Widget *parent = nullptr);
        AUIK_EXPORT Slider(u32 id, ModelBinding *binding, f32 min_value, f32 max_value, f32 range_start_value,
                           amal::axis axis, f32 size, WidgetFlags widget_flags = get_default_slider_flags(),
                           Widget *parent = nullptr);

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

        f32 value() const { return _value; }
        f32 min_value() const { return _min_value; }
        f32 max_value() const { return _max_value; }
        f32 range_start_value() const { return _range_start_value; }
        amal::axis axis() const { return _axis; }
        u32 track_style_tag() const { return _track_style.tag_id; }
        u32 fill_style_tag() const { return _fill_style.tag_id; }
        u32 grab_style_tag() const { return _grab_style.tag_id; }
        AUIK_EXPORT void set_value(f32 value);
        AUIK_EXPORT void set_model_binding(ModelBinding *binding);
        AUIK_EXPORT void set_range_start_value(f32 value);
        AUIK_EXPORT void set_step(f32 step);
        AUIK_EXPORT void set_axis(amal::axis axis);
        AUIK_EXPORT void set_style_tags(u32 track_tag_id, u32 fill_tag_id, u32 grab_tag_id);
        f32 step() const { return _step; }
        AUIK_EXPORT bool has_draw_record() const;
        virtual u32 signature() const override { return AUIK_TAG_SLIDER; }

    private:
        f32 _value = 0.0f;
        ModelBinding *_model_binding = nullptr;
        f32 _range_start_value = 0.0f;
        f32 _min_value = 0.0f;
        f32 _max_value = 1.0f;
        amal::axis _axis = amal::axis::x;
        StyleSelector _track_style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_SLIDER};
        StyleSelector _fill_style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_SLIDER};
        StyleSelector _grab_style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_SLIDER_GRAB};
        detail::SliderTrackVisual _track_visual;
        DrawDataID _grab_draw_id{};
        QuadsInstanceData _grab_visual{};
        amal::rect _track_rect{};
        amal::rect _grab_rect{};
        detail::RectData _grab_hit_rect{};
        amal::vec2 _track_depth_range{0.0f, 1.0f};
        amal::vec2 _grab_depth_range{0.0f, 1.0f};
        f32 _step = 0.0f;
        f32 _drag_grab_offset = 0.0f;

        void rebuild_track_visuals();
        void rebuild_grab_visual();
        void rebuild_cached_visuals();
        void rebuild_track_fill_visual();
        void update_value_from_mouse();
        amal::vec2 resolve_grab_size(const Style &grab_style) const;
        bool resolve_range_values(f32 &out_start, f32 &out_end) const;
    };

    class GradientSlider final : public Widget
    {
    public:
        AUIK_EXPORT ~GradientSlider() override;
        AUIK_EXPORT GradientSlider(u32 id, f32 value, f32 min_value, f32 max_value,
                                   const acul::vector<amal::vec4> &colors, amal::axis axis, f32 size,
                                   WidgetFlags widget_flags = get_default_slider_flags(), Widget *parent = nullptr);
        AUIK_EXPORT GradientSlider(u32 id, ModelBinding *binding, f32 min_value, f32 max_value,
                                   const acul::vector<amal::vec4> &colors, amal::axis axis, f32 size,
                                   WidgetFlags widget_flags = get_default_slider_flags(), Widget *parent = nullptr);

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

        f32 value() const { return _value; }
        f32 min_value() const { return _min_value; }
        f32 max_value() const { return _max_value; }
        const acul::vector<amal::vec4> &colors() const { return _colors; }
        amal::axis axis() const { return _axis; }
        u32 track_style_tag() const { return _track_style.tag_id; }
        u32 grab_style_tag() const { return _grab_style.tag_id; }
        AUIK_EXPORT void set_value(f32 value);
        AUIK_EXPORT void set_model_binding(ModelBinding *binding);
        AUIK_EXPORT void set_step(f32 step);
        AUIK_EXPORT void set_axis(amal::axis axis);
        AUIK_EXPORT void set_style_tags(u32 track_tag_id, u32 grab_tag_id);
        f32 step() const { return _step; }
        AUIK_EXPORT bool has_draw_record() const;
        virtual u32 signature() const override { return AUIK_TAG_GRADIENT_SLIDER; }

    private:
        f32 _value = 0.0f;
        ModelBinding *_model_binding = nullptr;
        f32 _min_value = 0.0f;
        f32 _max_value = 1.0f;
        amal::axis _axis = amal::axis::x;
        StyleSelector _track_style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_GRADIENT_SLIDER};
        StyleSelector _grab_style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_GRADIENT_SLIDER_GRAB};
        acul::vector<amal::vec4> _colors;
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
        f32 _drag_grab_offset = 0.0f;

        void rebuild_track_visuals();
        void rebuild_grab_visual();
        void rebuild_cached_visuals();
        void update_value_from_mouse();
        amal::vec2 resolve_grab_size(const Style &grab_style) const;
        amal::vec4 resolve_active_color(f32 factor) const;
    };

    class TransparencySlider final : public Widget
    {
    public:
        AUIK_EXPORT TransparencySlider(u32 id, f32 value, f32 min_value, f32 max_value, f32 size,
                                       const amal::vec4 &colors, amal::axis axis,
                                       WidgetFlags widget_flags = get_default_slider_flags(), Widget *parent = nullptr);
        AUIK_EXPORT TransparencySlider(u32 id, ModelBinding *binding, f32 min_value, f32 max_value, f32 size,
                                       const amal::vec4 &colors, amal::axis axis,
                                       WidgetFlags widget_flags = get_default_slider_flags(), Widget *parent = nullptr);
        AUIK_EXPORT ~TransparencySlider() override;

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

        f32 value() const { return _value; }
        f32 min_value() const { return _min_value; }
        f32 max_value() const { return _max_value; }
        u32 track_style_tag() const { return _track_style.tag_id; }
        u32 grab_style_tag() const { return _grab_style.tag_id; }
        AUIK_EXPORT void set_value(f32 value);
        AUIK_EXPORT void set_model_binding(ModelBinding *binding);
        AUIK_EXPORT void set_step(f32 step);
        AUIK_EXPORT void set_style_tags(u32 track_tag_id, u32 grab_tag_id);
        f32 step() const { return _step; }
        AUIK_EXPORT void set_color(const amal::vec4 &color);
        const amal::vec4 &color() const { return _color; }
        amal::axis axis() const { return _axis; }
        AUIK_EXPORT void set_axis(amal::axis axis);
        AUIK_EXPORT bool has_draw_record() const;
        virtual u32 signature() const override { return AUIK_TAG_TRANSPARENCY_SLIDER; }

    private:
        f32 _value = 0.0f;
        ModelBinding *_model_binding = nullptr;
        f32 _min_value = 0.0f;
        f32 _max_value = 1.0f;
        amal::axis _axis = amal::axis::x;
        CheckerImage *_checker = nullptr;
        amal::vec4 _color{1.0f, 1.0f, 1.0f, 1.0f};
        acul::vector<amal::vec4> _colors;
        StyleSelector _track_style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_GRADIENT_SLIDER};
        StyleSelector _grab_style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_GRADIENT_SLIDER_GRAB};
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
        f32 _drag_grab_offset = 0.0f;

        void rebuild_track_visuals();
        void rebuild_grab_visual();
        void rebuild_cached_visuals();
        void update_value_from_mouse();
        amal::rect resolve_track_rect(const amal::rect &bounds, const Style &track_style) const;
        amal::rect resolve_grab_rect(const Style &grab_style, f32 factor, const amal::vec2 &visual_size) const;
        amal::vec2 resolve_grab_size(const Style &grab_style) const;
        amal::vec4 resolve_active_color(f32 factor) const;
        void rebuild_gradient_colors();
    };

    class RangeSlider final : public Widget
    {
    public:
        AUIK_EXPORT RangeSlider(u32 id, f32 from_value, f32 to_value, f32 min_value, f32 max_value, amal::axis axis,
                                f32 size, WidgetFlags widget_flags = get_default_slider_flags(),
                                Widget *parent = nullptr);
        AUIK_EXPORT RangeSlider(u32 id, ModelBinding *binding, f32 min_value, f32 max_value, amal::axis axis, f32 size,
                                WidgetFlags widget_flags = get_default_slider_flags(), Widget *parent = nullptr);

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

        AUIK_EXPORT void set_step(f32 step);
        AUIK_EXPORT void set_values(f32 from_value, f32 to_value);
        AUIK_EXPORT void set_model_binding(ModelBinding *binding);
        f32 from_value() const { return _from_value; }
        f32 to_value() const { return _to_value; }
        f32 min_value() const { return _min_value; }
        f32 max_value() const { return _max_value; }
        f32 step() const { return _step; }
        amal::axis axis() const { return _axis; }
        u32 track_style_tag() const { return _track_style.tag_id; }
        u32 fill_style_tag() const { return _fill_style.tag_id; }
        u32 from_grab_style_tag() const { return _from_grab_style.tag_id; }
        u32 to_grab_style_tag() const { return _to_grab_style.tag_id; }
        AUIK_EXPORT void set_style_tags(u32 track_tag_id, u32 fill_tag_id, u32 from_grab_tag_id, u32 to_grab_tag_id);
        AUIK_EXPORT void set_axis(amal::axis axis);
        AUIK_EXPORT bool has_draw_record() const;
        virtual u32 signature() const override { return AUIK_TAG_RANGE_SLIDER; }

    private:
        enum class ActiveGrab : u8
        {
            none = 0,
            from = 1,
            to = 2
        };

        f32 _from_value = 0.0f;
        ModelBinding *_model_binding = nullptr;
        f32 _to_value = 0.0f;
        f32 _min_value = 0.0f;
        f32 _max_value = 1.0f;
        f32 _step = 0.0f;
        amal::axis _axis = amal::axis::x;
        StyleSelector _track_style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_SLIDER};
        StyleSelector _fill_style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_SLIDER};
        StyleSelector _from_grab_style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_SLIDER_GRAB};
        StyleSelector _to_grab_style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_SLIDER_GRAB};
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
        f32 _drag_grab_offset = 0.0f;

        void rebuild_track_visuals();
        void rebuild_grab_visuals();
        void rebuild_cached_visuals();
        void update_active_grab_from_mouse();
        void update_active_value_from_mouse();
        amal::vec2 resolve_grab_size(const Style &grab_style) const;
        f32 resolve_grab_center(f32 value, f32 outer_half_grab) const;
        f32 clamped_value(f32 value) const;
    };

    inline Slider *make_slider(u32 id, f32 value, f32 min_value = 0.0f, f32 max_value = 1.0f, f32 size = 0.0f,
                               amal::axis axis = amal::axis::x, Widget *parent = nullptr)
    {
        return acul::alloc<Slider>(id, value, min_value, max_value, min_value, axis, size, get_default_slider_flags(),
                                   parent);
    }

    inline GradientSlider *make_gradient_slider(u32 id, f32 value, f32 min_value, f32 max_value,
                                                const acul::vector<amal::vec4> &colors, f32 size = 0.0f,
                                                amal::axis axis = amal::axis::x, Widget *parent = nullptr)
    {
        return acul::alloc<GradientSlider>(id, value, min_value, max_value, colors, axis, size,
                                           get_default_slider_flags(), parent);
    }

    inline Slider *make_slider_with_range(u32 id, f32 value, f32 range_start_value, f32 min_value = 0.0f,
                                          f32 max_value = 1.0f, f32 size = 0.0f, amal::axis axis = amal::axis::x,
                                          Widget *parent = nullptr)
    {
        return acul::alloc<Slider>(id, value, min_value, max_value, range_start_value, axis, size,
                                   get_default_slider_flags(), parent);
    }

    inline GradientSlider *make_hsl_slider(u32 id, f32 value, amal::axis axis = amal::axis::x, Widget *parent = nullptr)
    {
        acul::vector<amal::vec4> hsl_colors{
            {1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 1.0f, 1.0f},
            {0.0f, 0.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f},
        };
        return acul::alloc<GradientSlider>(id, value, 0.0f, 360.0f, hsl_colors, axis, 0.0f, get_default_slider_flags(),
                                           parent);
    }

    inline TransparencySlider *make_transparency_slider(u32 id, f32 value, const amal::vec4 &color,
                                                        f32 min_value = 0.0f, f32 max_value = 1.0f, f32 size = 0.0f,
                                                        amal::axis axis = amal::axis::x, Widget *parent = nullptr)
    {
        return acul::alloc<TransparencySlider>(id, value, min_value, max_value, size, color, axis,
                                               get_default_slider_flags(), parent);
    }

    inline RangeSlider *make_range_slider(u32 id, f32 from_value, f32 to_value, f32 min_value = 0.0f,
                                          f32 max_value = 1.0f, f32 size = 0.0f, amal::axis axis = amal::axis::x,
                                          Widget *parent = nullptr)
    {
        return acul::alloc<RangeSlider>(id, from_value, to_value, min_value, max_value, axis, size,
                                        get_default_slider_flags(), parent);
    }

    inline RangeSlider *make_range_slider(u32 id, ModelBinding *binding, f32 min_value = 0.0f, f32 max_value = 1.0f,
                                          f32 size = 0.0f, amal::axis axis = amal::axis::x, Widget *parent = nullptr)
    {
        return acul::alloc<RangeSlider>(id, binding, min_value, max_value, axis, size, get_default_slider_flags(),
                                        parent);
    }

    namespace streams
    {
        extern AUIK_EXPORT const umbf::streams::Stream slider;
        extern AUIK_EXPORT const umbf::streams::Stream gradient_slider;
        extern AUIK_EXPORT const umbf::streams::Stream transparency_slider;
        extern AUIK_EXPORT const umbf::streams::Stream range_slider;
    } // namespace streams
} // namespace auik
