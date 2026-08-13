#pragma once

#include <acul/memory/alloc.hpp>
#include "../model.hpp"
#include "../theme.hpp"
#include "widget.hpp"

#define AUIK_TAG_RADIO_BUTTON           0x4D3217F1u
#define AUIK_TAG_RADIO_BUTTON_INDICATOR 0xFE07F470u

namespace auik
{
    class RadioButton final : public Widget
    {
    public:
        AUIK_EXPORT RadioButton(u32 id, bool value, WidgetFlags widget_flags);
        AUIK_EXPORT RadioButton(u32 id, ModelBinding *binding, WidgetFlags widget_flags);

        AUIK_EXPORT StyleUpdateFlags update_style() override;
        AUIK_EXPORT void update_layout_min_size_force() override;
        AUIK_EXPORT void update_layout(bool min_size_known) override;
        AUIK_EXPORT void translate(const amal::vec2 &delta) override;
        AUIK_EXPORT void rebuild_clip_rects() override;
        AUIK_EXPORT void update_depth(const amal::vec2 &depth_range) override;
        AUIK_EXPORT void back_hit_depth() override;
        AUIK_EXPORT void restore_hit_depth() override;
        AUIK_EXPORT void draw(DrawCtx &ctx) override;
        AUIK_EXPORT void on_click(MouseKey key, KeyPressState state, u32 click_count) override;
        u32 signature() const override { return AUIK_TAG_RADIO_BUTTON; }

        bool value() const { return _value; }
        AUIK_EXPORT void set_value(bool value);
        AUIK_EXPORT void set_model_binding(ModelBinding *binding);

    private:
        bool _value = false;
        ModelBinding *_model_binding = nullptr;
        DrawDataID _background_draw{};
        DrawDataID _indicator_draw{};
        StyleSelector _background_style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_RADIO_BUTTON};
        StyleSelector _indicator_style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_RADIO_BUTTON_INDICATOR};
        detail::RectData _indicator_rect{};
        amal::rect _background_rect{};
        amal::vec2 _background_depth_range{0.0f, 1.0f};
        amal::vec2 _indicator_depth_range{0.0f, 1.0f};

        amal::vec2 resolve_indicator_size(const Style &indicator_style) const;
        amal::vec2 resolve_background_size(const Style &background_style) const;
        void rebuild_indicator_layout();
        bool has_draw_record() const;
    };

    inline RadioButton *make_radio_button(u32 id, bool value)
    {
        constexpr WidgetFlags widget_flags = WidgetFlagBits::visible | WidgetFlagBits::attachable |
                                             WidgetFlagBits::configurable | WidgetFlagBits::hittable;
        return acul::alloc<RadioButton>(id, value, widget_flags);
    }

    namespace streams
    {
        extern AUIK_EXPORT const umbf::streams::Stream radio_button;
    }
} // namespace auik
