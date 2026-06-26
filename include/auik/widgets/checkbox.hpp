#pragma once

#include <acul/memory/alloc.hpp>
#include "../theme.hpp"
#include "widget.hpp"

#define AUIK_TAG_CHECKBOX           0x9224482Cu
#define AUIK_TAG_CHECKBOX_CHECKMARK 0x1F3D44CEu

namespace auik
{
    constexpr inline WidgetFlags get_default_checkbox_flags()
    {
        return get_default_widget_flags() | WidgetFlagBits::hittable;
    }

    class Checkbox final : public Widget
    {
    public:
        AUIK_EXPORT Checkbox(u32 id, bool value, WidgetFlags widget_flags = get_default_checkbox_flags());

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
        u32 signature() const override { return AUIK_TAG_CHECKBOX; }

        bool value() const { return _value; }
        AUIK_EXPORT void set_value(bool value);

    private:
        bool _value = false;
        DrawDataID _box_bg{};
        DrawDataID _checkmark_draw{};
        StyleSelector _style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_CHECKBOX};
        detail::RectData _checkmark_rect{};
        amal::rect _box_rect{};
        TextureID _checkmark_texture{};
        amal::rect _checkmark_uv_rect{{0.0f, 0.0f}, {1.0f, 1.0f}};
        amal::vec2 _box_depth_range{0.0f, 1.0f};
        amal::vec2 _content_depth_range{0.0f, 1.0f};
        amal::vec2 _checkmark_size{0.0f, 0.0f};

        void ensure_checkmark_resource();
        void rebuild_checkmark_layout();
        amal::vec2 resolve_box_size(const Style &style) const;
        bool has_draw_record() const;
    };

    inline Checkbox *make_checkbox(u32 id, bool value)
    {
        return acul::alloc<Checkbox>(id, value);
    }

    namespace streams
    {
        extern AUIK_EXPORT const umbf::streams::Stream checkbox;
    }
} // namespace auik
