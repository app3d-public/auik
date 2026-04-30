#pragma once

#include <acul/memory/alloc.hpp>
#include "../theme.hpp"
#include "widget.hpp"

#define AUIK_TAG_SWITCH_BUTTON      0x5265F0C2u
#define AUIK_TAG_SWITCH_BUTTON_ON   0x8656429Fu
#define AUIK_TAG_SWITCH_BUTTON_GRAB 0xA20B0D91u

namespace auik::v2
{
    constexpr inline WidgetFlags get_default_switch_button_flags()
    {
        return get_default_widget_flags() | WidgetFlagBits::hittable | WidgetFlagBits::fixed;
    }

    class APPLIB_API SwitchButton final : public Widget
    {
    public:
        SwitchButton(u32 id, bool *value, WidgetFlags widget_flags = get_default_switch_button_flags(),
                     Widget *parent = nullptr);

        StyleUpdateFlags update_style() override;
        void update_layout_min_size() override;
        void update_layout(bool min_size_known) override;
        void translate(const amal::vec2 &delta) override;
        void rebuild_clip_rects() override;
        void update_depth(const amal::vec2 &depth_range) override;
        void draw(DrawCtx &ctx) override;
        void on_click(MouseKey key, KeyPressState state, u32 click_count) override;

        bool value() const { return _value ? *_value : false; }
        void set_value(bool value);

    private:
        bool *_value = nullptr;
        DrawDataID _track_draw{};
        DrawDataID _grab_draw{};
        StyleSelector _track_style{Theme::STYLE_ID_INVALID, AUIK_TAG_SWITCH_BUTTON};
        StyleSelector _grab_style{Theme::STYLE_ID_INVALID, AUIK_TAG_SWITCH_BUTTON_GRAB};
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

    inline SwitchButton *make_switch_button(u32 id, bool *value, Widget *parent = nullptr)
    {
        return acul::alloc<SwitchButton>(id, value, get_default_switch_button_flags(), parent);
    }
} // namespace auik::v2
