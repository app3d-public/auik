#pragma once

#include <amal/trigonometric.hpp>
#include <auik/auik.hpp>
#include <auik/post_effects.hpp>
#include "../widget.hpp"

namespace auik::detail
{
    class WRotateTransient;

    class PopupTrigger final
    {
    public:
        AUIK_EXPORT PopupTrigger(u32 style_tag, u32 hit_tag, u32 closed_icon, u32 open_icon, bool animated = true,
                                 f32 open_angle = amal::pi<f32>());
        AUIK_EXPORT ~PopupTrigger();

        void set_update_target(Widget *target) { _update_target = target; }
        void set_hit_id(ElementID id) { _hit_rect.id = id; }
        void set_open(bool value) { _open = value; }
        void set_element_id(u32 value) { _hit_rect.id.element_id = value; }
        bool is_open() const { return _open; }
        void set_style_state(StyleState value) { _style_state = value; }
        AUIK_EXPORT void set_style_tag(u32 style_tag);
        AUIK_EXPORT void set_icons(u32 closed_icon, u32 open_icon);

        AUIK_EXPORT StyleUpdateFlags update_style(u32 self_id, u32 parent_id, StyleState state);
        AUIK_EXPORT void update_layout_min_size_force(amal::vec2 style_size, bool fixed);
        AUIK_EXPORT void update_layout(const amal::rect &bounds, u16 clip_id);
        AUIK_EXPORT void translate(const amal::vec2 &delta);
        AUIK_EXPORT void rebuild_clip_rects(u16 clip_id);
        AUIK_EXPORT void reset_draw_records();
        u32 get_depth_requirement() const { return 2u; }
        AUIK_EXPORT void update_depth(const amal::vec2 &depth_range);
        AUIK_EXPORT void back_hit_depth();
        AUIK_EXPORT void restore_hit_depth();
        AUIK_EXPORT void draw(DrawCtx &ctx, bool is_hit_allowed);

        AUIK_EXPORT void start_icon_animation(bool opening);
        const amal::vec2 &required_size() const { return _required_size; }
        const amal::vec2 &icon_size() const { return _icon_size; }
        const amal::rect &bounds() const { return _bounds; }
        f32 icon_slot_left() const { return _icon_slot.offset.x; }
        AUIK_EXPORT bool has_draw_record() const;

    private:
        bool ensure_icon_resources();
        void update_icon_rect_from_slot();

        Widget *_update_target = nullptr;
        u32 _closed_icon = 0u;
        u32 _open_icon = 0u;
        f32 _open_angle = amal::pi<f32>();
        bool _animated = true;
        bool _open = false;

        StyleState _style_state = StyleState::normal;
        StyleSelector _style{Theme::STYLE_ID_INVALID, 0u};
        DrawDataID _bg_draw{};
        DrawDataID _icon_draw{};
        TextureID _icon_texture{};
        amal::rect _icon_uv_rect{{0.0f, 0.0f}, {1.0f, 1.0f}};
        amal::vec2 _icon_size{0.0f, 0.0f};
        amal::rect _outer_bounds{};
        amal::rect _bounds{};
        amal::rect _icon_slot{};
        amal::rect _icon_rect{};
        RectData _hit_rect{};
        amal::vec2 _required_size{0.0f, 0.0f};
        amal::vec2 _bg_depth_range{0.0f, 1.0f};
        amal::vec2 _content_depth_range{0.0f, 1.0f};
        WRotateTransient *_rotate_transient = nullptr;
    };
} // namespace auik::detail
