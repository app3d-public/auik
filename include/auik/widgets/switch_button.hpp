#pragma once

#include <acul/memory/alloc.hpp>
#include "../model.hpp"
#include "../theme.hpp"
#include "widget.hpp"

#define AUIK_TAG_SWITCH_BUTTON      0x5265F0C2u
#define AUIK_TAG_SWITCH_BUTTON_ON   0x8656429Fu
#define AUIK_TAG_SWITCH_BUTTON_GRAB 0xA20B0D91u

namespace auik
{
    constexpr inline WidgetFlags get_default_switch_button_flags()
    {
        return get_default_widget_flags() | WidgetFlagBits::hittable;
    }

    class SwitchButton final : public Widget
    {
    public:
        AUIK_EXPORT SwitchButton(u32 id, bool value, WidgetFlags widget_flags = get_default_switch_button_flags());
        AUIK_EXPORT SwitchButton(u32 id, ModelBinding *binding,
                                 WidgetFlags widget_flags = get_default_switch_button_flags());

        AUIK_EXPORT StyleUpdateFlags update_style() override;
        AUIK_EXPORT void update_layout_min_size() override;
        AUIK_EXPORT void update_layout(bool min_size_known) override;
        AUIK_EXPORT void translate(const amal::vec2 &delta) override;
        AUIK_EXPORT void rebuild_clip_rects() override;
        AUIK_EXPORT void update_depth(const amal::vec2 &depth_range) override;
        AUIK_EXPORT void back_hit_depth() override;
        AUIK_EXPORT void restore_hit_depth() override;
        AUIK_EXPORT void draw(DrawCtx &ctx) override;
        AUIK_EXPORT void on_click(MouseKey key, KeyPressState state, u32 click_count) override;
        u32 signature() const override { return AUIK_TAG_SWITCH_BUTTON; }

        bool value() const { return _value; }
        AUIK_EXPORT void set_value(bool value);
        AUIK_EXPORT void set_model_binding(ModelBinding *binding);

    private:
        bool _value = false;
        ModelBinding *_model_binding = nullptr;
        DrawDataID _track_draw{};
        DrawDataID _grab_draw{};
        StyleSelector _track_style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_SWITCH_BUTTON};
        StyleSelector _grab_style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_SWITCH_BUTTON_GRAB};
        detail::RectData _grab_rect{};
        amal::rect _track_rect{};
        amal::vec2 _track_depth_range{0.0f, 1.0f};
        amal::vec2 _grab_depth_range{0.0f, 1.0f};

        amal::vec2 resolve_grab_size(const Style &grab_style) const;
        amal::vec2 resolve_track_size(const Style &track_style, const Style &grab_style) const;
        u32 track_tag() const;
        void sync_track_tag();
        void rebuild_grab_layout();
        bool has_draw_record() const;
    };

    inline SwitchButton *make_switch_button(u32 id, bool value)
    {
        return acul::alloc<SwitchButton>(id, value);
    }

    namespace streams
    {
        extern AUIK_EXPORT const umbf::streams::Stream switch_button;
    }
} // namespace auik
