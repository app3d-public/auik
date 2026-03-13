#pragma once

#include <acul/api.hpp>
#include <acul/enum.hpp>
#include <acul/pair.hpp>
#include <acul/scalars.hpp>
#include <amal/vector.hpp>

#define AUIK_TAG_HITBOX 0xBF9B2277u

namespace auik::v2
{
    enum class KeyPressState : i8
    {
        release,
        press,
        repeat
    };

    enum class MouseKey
    {
        unknown = -1,
        left = 0,
        right = 1,
        middle = 2
    };

    enum class HoverState : i8
    {
        leave = 0,
        enter = 1,
        active = 2
    };
} // namespace auik::v2

namespace auik::v2::detail
{
    struct RectData;
    struct RectBits
    {
        enum enum_type : u16
        {
            none = 0x0,
            hitbox = 0x1
        };
    };

    struct HitboxZoneBits
    {
        enum enum_type : u8
        {
            none = 0x0,
            left = 0x1,
            right = 0x2,
            top = 0x4,
            bottom = 0x8
        };
        using flag_bitmask = std::true_type;
    };
    using HitboxZone = acul::flags<HitboxZoneBits>;

    struct CursorID
    {
        enum enum_type
        {
            arrow,       // The regular arrow cursor.
            ibeam,       // The text input I-beam cursor.
            resize_ew,   // The horizontal resize/move arrow cursor.  This is usually a horizontal double-headed arrow.
            resize_ns,   // The vertical resize/move cursor. This is usually a vertical double-headed arrow.
            resize_nwse, // The top-left to bottom-right diagonal resize/move cursor.  This is usually a diagonal
                         // double-headed arrow.
            resize_nesw, // The top-right to bottom-left diagonal resize/move cursor.  This is usually a diagonal
                         // double-headed arrow.
            max          // The maximum cursor ID
        };
    };

    using PFN_set_window_cursor = void (*)(CursorID::enum_type, struct WindowContext *);
    using PFN_destroy_window_backend = void (*)(struct WindowContext *);
    using PFN_update_window_time = void (*)(struct WindowContext *);
    using PFN_window_new_frame = void (*)(struct WindowContext *);
    using PFN_construct_window_backend = void (*)(struct WindowContext *);

    struct ElementID
    {
        u32 widget_id = 0;
        u32 tag_id = 0;

        constexpr bool is_valid() const { return widget_id != 0; }
        constexpr explicit operator bool() const { return is_valid(); }
    };

    inline constexpr ElementID make_element_id(u32 widget_id = 0, u32 tag_id = 0) { return {widget_id, tag_id}; }

    struct WindowContext
    {
        f64 time = 0.0;
        PFN_set_window_cursor set_cursor = nullptr;
        PFN_construct_window_backend construct_backend = nullptr;
        PFN_destroy_window_backend destroy_backend = nullptr;
        PFN_update_window_time update_time = nullptr;
        PFN_window_new_frame new_frame = nullptr;
    };

    APPLIB_API void on_mouse_move(const amal::vec2 &delta);
    APPLIB_API void on_scroll_event(const amal::vec2 &pos);
    APPLIB_API void on_mouse_click_event(MouseKey key, KeyPressState state);
    APPLIB_API void reset_event_state();
    APPLIB_API void on_hover_id_updated(u32 prev_widget_id, u32 prev_tag_id, u32 widget_id, u32 tag_id);
    APPLIB_API HitboxZone get_hitbox_zone(const RectData &rect, const amal::vec2 &mouse_pos);
    APPLIB_API CursorID::enum_type get_cursor_for_hitbox_zone(HitboxZone zone);

    inline void set_window_cursor(CursorID::enum_type id, WindowContext *window_ctx)
    {
        assert(window_ctx && "auik window context is not initialized");
        window_ctx->set_cursor(id, window_ctx);
    }

    inline void construct_window_backend(WindowContext *window_ctx) { window_ctx->construct_backend(window_ctx); }

    inline void destroy_window_context(WindowContext *window_ctx)
    {
        assert(window_ctx && "auik window context is not initialized");
        window_ctx->destroy_backend(window_ctx);
    }

    inline void update_window_time(WindowContext *window_ctx)
    {
        assert(window_ctx && "auik window context is not initialized");
        if (window_ctx->update_time) window_ctx->update_time(window_ctx);
    }

    inline void new_window_frame(WindowContext *window_ctx) { window_ctx->new_frame(window_ctx); }

    struct RectData
    {
        u32 widget_id = 0;
        u32 tag_id = 0;
        amal::vec2 position{0.0f, 0.0f};
        amal::vec2 size{0.0f, 0.0f};
        f32 depth = 0.0f;
        u16 clip_id = 0xFFFFu;
        u16 flags = 0;
    };
    static_assert(sizeof(RectData) == 32, "RectData must match picker std430 layout");

    inline RectData make_rect_data(u32 widget_id, u32 tag_id, amal::vec2 position = {0.0f, 0.0f},
                                   amal::vec2 size = {0.0f, 0.0f}, u16 clip_id = 0xFFFFu, f32 depth = 0.0f,
                                   u16 flags = 0)
    {
        RectData rect{};
        rect.widget_id = widget_id;
        rect.tag_id = tag_id;
        rect.position = position;
        rect.size = size;
        rect.depth = depth;
        rect.clip_id = clip_id;
        rect.flags = flags;
        return rect;
    }
} // namespace auik::v2::detail
