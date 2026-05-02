#pragma once

#include <auik/v2/auik.hpp>
#include <auik/v2/post_effects.hpp>
#include "../widget.hpp"

namespace auik::v2::detail
{
    class APPLIB_API PopupTrigger final
    {
    public:
        PopupTrigger(u32 style_tag, u32 hit_tag, u32 closed_icon, u32 open_icon, bool animated = true);
        ~PopupTrigger();

        void set_owner(Widget *owner) { _owner = owner; }
        void set_open(bool value) { _open = value; }
        bool is_open() const { return _open; }
        void set_style_state(StyleState value) { _style_state = value; }
        void set_icons(u32 closed_icon, u32 open_icon);

        StyleUpdateFlags update_style(u32 self_id, u32 parent_id, StyleState state);
        void update_layout_min_size(amal::vec2 requested_size, bool fixed);
        void update_layout(const amal::rect &bounds, u16 clip_id);
        void translate(const amal::vec2 &delta);
        void rebuild_clip_rects(u16 clip_id);
        void update_depth(const amal::vec2 &depth_range);
        void draw(DrawCtx &ctx, bool emit_hit_rect);

        void start_icon_animation(bool opening);
        const amal::vec2 &required_size() const { return _required_size; }
        const amal::vec2 &icon_size() const { return _icon_size; }
        const amal::rect &bounds() const { return _bounds; }
        f32 icon_slot_left() const { return _icon_slot.offset.x; }
        bool has_draw_record() const;

    private:
        void ensure_icon_resources();
        void clear_animated_icon_draw();
        void schedule_icon_tick();
        void tick_icon_animation();

        Widget *_owner = nullptr;
        u32 _hit_tag = 0u;
        u32 _closed_icon = 0u;
        u32 _open_icon = 0u;
        bool _animated = true;
        bool _open = false;

        StyleState _style_state = StyleState::normal;
        StyleSelector _style{Theme::STYLE_ID_INVALID, 0u};
        DrawDataID _bg_draw{};
        DrawDataID _icon_draw{};
        DrawDataID _animated_icon_draw{};
        TextureID _icon_texture{};
        amal::rect _icon_uv_rect{{0.0f, 0.0f}, {1.0f, 1.0f}};
        amal::vec2 _icon_size{0.0f, 0.0f};
        amal::rect _bounds{};
        amal::rect _icon_slot{};
        amal::rect _icon_rect{};
        RectData _hit_rect{};
        amal::vec2 _required_size{0.0f, 0.0f};
        amal::vec2 _bg_depth_range{0.0f, 1.0f};
        amal::vec2 _content_depth_range{0.0f, 1.0f};
        u32 _rotate_post_id = AUIK_INVALID_POST_EFFECT_DATA_ID;
    };
} // namespace auik::v2::detail
