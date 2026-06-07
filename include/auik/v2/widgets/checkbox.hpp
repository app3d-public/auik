#pragma once

#include <acul/memory/alloc.hpp>
#include "../theme.hpp"
#include "widget.hpp"

#define AUIK_TAG_CHECKBOX           0x9224482Cu
#define AUIK_TAG_CHECKBOX_CHECKMARK 0x1F3D44CEu

namespace auik::v2
{
    constexpr inline WidgetFlags get_default_checkbox_flags()
    {
        return get_default_widget_flags() | WidgetFlagBits::hittable | WidgetFlagBits::fixed_layout;
    }

    class APPLIB_API Checkbox final : public Widget
    {
    public:
        Checkbox(u32 id, bool *value, WidgetFlags widget_flags = get_default_checkbox_flags(),
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

    inline Checkbox *make_checkbox(u32 id, bool *value, Widget *parent = nullptr)
    {
        return acul::alloc<Checkbox>(id, value, get_default_checkbox_flags(), parent);
    }

} // namespace auik::v2
