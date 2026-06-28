#pragma once

#include <acul/memory/alloc.hpp>
#include <amal/geometric.hpp>
#include "../animation.hpp"
#include "../model.hpp"
#include "../pipelines.hpp"
#include "widget.hpp"

#define AUIK_TAG_PROGRESS_BAR 0x243BD97Au

namespace auik
{
    class ProgressBar final : public Widget
    {
    public:
        AUIK_EXPORT ProgressBar(u32 id, f32 value = 0.0f, f32 min_value = 0.0f, f32 max_value = 1.0f,
                                f32 size = 0.0f, amal::axis axis = amal::axis::x,
                                WidgetFlags widget_flags = get_default_widget_flags(), Widget *parent = nullptr);
        AUIK_EXPORT ProgressBar(u32 id, ModelBinding *binding, f32 min_value = 0.0f, f32 max_value = 1.0f,
                                f32 size = 0.0f, amal::axis axis = amal::axis::x,
                                WidgetFlags widget_flags = get_default_widget_flags(), Widget *parent = nullptr);
        AUIK_EXPORT ~ProgressBar() override;

        AUIK_EXPORT StyleUpdateFlags update_style() override;
        AUIK_EXPORT void update_layout_min_size() override;
        AUIK_EXPORT void update_layout(bool min_size_known) override;
        AUIK_EXPORT void translate(const amal::vec2 &delta) override;
        AUIK_EXPORT void rebuild_clip_rects() override;
        AUIK_EXPORT void reset_draw_records() override;
        AUIK_EXPORT void update_depth(const amal::vec2 &depth_range) override;
        AUIK_EXPORT void draw(DrawCtx &ctx) override;
        AUIK_EXPORT void on_change(ChangeEvent &event) override;

        f32 value() const { return _value; }
        f32 min_value() const { return _min_value; }
        f32 max_value() const { return _max_value; }
        amal::axis axis() const { return _axis; }
        u32 track_style_tag() const { return _track_style.tag_id; }
        u32 active_style_tag() const { return _active_style.tag_id; }
        AUIK_EXPORT void set_value(f32 value);
        AUIK_EXPORT void set_model_binding(ModelBinding *binding);
        AUIK_EXPORT void set_range(f32 min_value, f32 max_value);
        AUIK_EXPORT void set_axis(amal::axis axis);
        AUIK_EXPORT void set_style_tags(u32 track_tag_id, u32 active_tag_id);
        AUIK_EXPORT bool has_draw_record() const;
        u32 signature() const override { return AUIK_TAG_PROGRESS_BAR; }

    private:
        f32 _value = 0.0f;
        ModelBinding *_model_binding = nullptr;
        f32 _min_value = 0.0f;
        f32 _max_value = 1.0f;
        f32 _display_value = 0.0f;
        f32 _change_from_value = 0.0f;
        amal::axis _axis = amal::axis::x;
        StyleSelector _track_style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_PROGRESS_BAR};
        StyleSelector _active_style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_PROGRESS_BAR_ACTIVE};
        DrawDataID _track_draw_id{};
        DrawDataID _active_draw_id{};
        QuadsInstanceData _track_visual{};
        QuadsInstanceData _active_visual{};
        amal::rect _track_rect{};
        amal::vec2 _track_depth_range{0.0f, 1.0f};
        amal::vec2 _active_depth_range{0.0f, 1.0f};
        AnimationState _animation;

        f32 clamped_value(f32 value) const;
        f32 value_factor(f32 value) const;
        amal::rect resolve_active_rect(f32 factor) const;
        void rebuild_track_visual();
        void rebuild_active_visual(f32 factor);
        void rebuild_cached_visuals();
    };

    inline ProgressBar *make_progress_bar(u32 id, f32 value = 0.0f, f32 min_value = 0.0f, f32 max_value = 1.0f,
                                          f32 size = 0.0f, amal::axis axis = amal::axis::x,
                                          Widget *parent = nullptr)
    {
        return acul::alloc<ProgressBar>(id, value, min_value, max_value, size, axis, get_default_widget_flags(),
                                        parent);
    }

    namespace streams
    {
        extern AUIK_EXPORT const umbf::streams::Stream progress_bar;
    } // namespace streams
} // namespace auik
