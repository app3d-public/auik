#pragma once

#include <acul/memory/alloc.hpp>
#include "../theme.hpp"
#include "widget.hpp"

#define AUIK_TAG_RADIO_BUTTON           0x4D3217F1u
#define AUIK_TAG_RADIO_BUTTON_INDICATOR 0xFE07F470u

namespace auik::v2
{
    constexpr inline WidgetFlags get_default_radio_button_flags()
    {
        return get_default_widget_flags() | WidgetFlagBits::hittable | WidgetFlagBits::fixed_layout;
    }

    class APPLIB_API RadioButton final : public Widget
    {
    public:
        RadioButton(u32 id, bool *value, WidgetFlags widget_flags = get_default_radio_button_flags(),
                    Widget *parent = nullptr);

        StyleUpdateFlags update_style() override;
        void update_layout_min_size() override;
        void update_layout(bool min_size_known) override;
        void translate(const amal::vec2 &delta) override;
        void rebuild_clip_rects() override;
        void update_depth(const amal::vec2 &depth_range) override;
        void back_hit_depth() override;
        void restore_hit_depth() override;
        void draw(DrawCtx &ctx) override;
        void on_click(MouseKey key, KeyPressState state, u32 click_count) override;

        bool value() const { return _value ? *_value : false; }
        void set_value(bool value);

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
} // namespace auik::v2
