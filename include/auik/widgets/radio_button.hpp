#pragma once

#include <acul/memory/alloc.hpp>
#include "../theme.hpp"
#include "widget.hpp"

#define AUIK_TAG_RADIO_BUTTON           0x4D3217F1u
#define AUIK_TAG_RADIO_BUTTON_INDICATOR 0xFE07F470u

namespace auik
{
    constexpr inline WidgetFlags get_default_radio_button_flags()
    {
        return get_default_widget_flags() | WidgetFlagBits::hittable | WidgetFlagBits::fixed_layout;
    }

    class RadioButton final : public Widget
    {
    public:
        AUIK_EXPORT RadioButton(u32 id, bool *value, WidgetFlags widget_flags = get_default_radio_button_flags(),
                    Widget *parent = nullptr);

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

        bool value() const { return _value ? *_value : false; }
        AUIK_EXPORT void set_value(bool value);

    private:
        bool *_value = nullptr;
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

    inline RadioButton *make_radio_button(u32 id, bool *value, Widget *parent = nullptr)
    {
        return acul::alloc<RadioButton>(id, value, get_default_radio_button_flags(), parent);
    }
} // namespace auik
