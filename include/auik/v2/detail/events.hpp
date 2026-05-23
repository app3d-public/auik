#pragma once

#include <acul/enum.hpp>
#include <acul/pair.hpp>
#include <acul/scalars.hpp>
#include <amal/rect.hpp>
#include <umbf/utils.hpp>
#include "../events.hpp"

#define AUIK_TAG_HITBOX 0xBF9B2277u

namespace auik::v2::detail
{
    struct immediate_event_traits_tag
    {
    };
    struct deferred_event_traits_tag
    {
    };

    struct ImmediateEventTraits
    {
        using category = immediate_event_traits_tag;
    };

    struct DeferredEventTraits
    {
        using category = deferred_event_traits_tag;
    };

    using HoverEventTraits = DeferredEventTraits;
    using DragEventTraits = ImmediateEventTraits;
    using ScrollEventTraits = ImmediateEventTraits;
    using ClickEventTraits = DeferredEventTraits;
    using FocusEventTraits = DeferredEventTraits;
    using KeyEventTraits = DeferredEventTraits;
    using CharEventTraits = DeferredEventTraits;

    struct FrameChangesBits
    {
        enum enum_type : u8
        {
            none = 0x0,
            drag_delta = 0x1,
            scroll_delta = 0x2,
            key_input = 0x4,
            char_input = 0x8
        };
        using flag_bitmask = std::true_type;
    };
    using FrameChanges = acul::flags<FrameChangesBits>;

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

    using PFN_get_window_handle = void *(*)(struct WindowContext *);
    using PFN_set_window_cursor = void (*)(CursorID::enum_type, struct WindowContext *);
    using PFN_get_clipboard_string = acul::string (*)(struct WindowContext *);
    using PFN_set_clipboard_string = void (*)(struct WindowContext *, const acul::string &);
    using PFN_destroy_window_backend = void (*)(struct WindowContext *);
    using PFN_update_window_time = void (*)(struct WindowContext *);
    using PFN_window_new_frame = void (*)(struct WindowContext *);
    using PFN_construct_window_backend = void (*)(struct WindowContext *);
    using PFN_get_window_icon_image = bool (*)(struct WindowContext *, umbf::Image2D &);

    struct ElementID
    {
        u32 widget_id = 0;
        u32 tag_id = 0;
        u32 element_id = 0;

        constexpr bool is_valid() const { return widget_id != 0; }
        constexpr explicit operator bool() const { return is_valid(); }
        constexpr bool operator==(const ElementID &other) const
        {
            return widget_id == other.widget_id && tag_id == other.tag_id && element_id == other.element_id;
        }
        constexpr bool operator!=(const ElementID &other) const { return !(*this == other); }
    };

    inline constexpr ElementID make_element_id(u32 widget_id = 0, u32 tag_id = 0, u32 element_id = 0)
    {
        return {widget_id, tag_id, element_id};
    }

    struct WindowContext
    {
        f64 time = 0.0;
        HostWindowState host_state = HostWindowState::normal;
        PFN_get_window_handle get_window_handle = nullptr;
        PFN_set_window_cursor set_cursor = nullptr;
        PFN_get_clipboard_string get_clipboard_string = nullptr;
        PFN_set_clipboard_string set_clipboard_string = nullptr;
        PFN_construct_window_backend construct_backend = nullptr;
        PFN_destroy_window_backend destroy_backend = nullptr;
        PFN_update_window_time update_time = nullptr;
        PFN_window_new_frame new_frame = nullptr;
        PFN_get_window_icon_image get_window_icon_image = nullptr;
    };

    APPLIB_API void on_mouse_move(const amal::vec2 &delta);
    APPLIB_API void on_scroll_event(const amal::vec2 &pos);
    APPLIB_API void on_mouse_click_event(MouseKey key, KeyPressState state);
    APPLIB_API void on_key_event(Key key, KeyPressState state, KeyMode mods);
    APPLIB_API void on_char_event(u32 char_code);
    void deregister_widget_shortcuts(u32 widget_id);
    APPLIB_API void flush_frame_changes();
    APPLIB_API void reset_event_state();
    APPLIB_API void on_hover_id_updated(const ElementID &prev_hover_id, const ElementID &hover_id);
    HitboxZone get_hitbox_zone(const RectData &rect, const amal::vec2 &mouse_pos);
    CursorID::enum_type get_cursor_for_hitbox_zone(HitboxZone zone);

    inline void *get_window_handle(WindowContext *window_ctx)
    {
        assert(window_ctx && "auik window context is not initialized");
        return window_ctx->get_window_handle(window_ctx);
    }

    inline void set_window_cursor(CursorID::enum_type id, WindowContext *window_ctx)
    {
        assert(window_ctx && "auik window context is not initialized");
        window_ctx->set_cursor(id, window_ctx);
    }

    inline acul::string get_clipboard_string(WindowContext *window_ctx)
    {
        assert(window_ctx && "auik window context is not initialized");
        return window_ctx->get_clipboard_string ? window_ctx->get_clipboard_string(window_ctx) : acul::string();
    }

    inline void set_clipboard_string(WindowContext *window_ctx, const acul::string &text)
    {
        assert(window_ctx && "auik window context is not initialized");
        if (window_ctx->set_clipboard_string) window_ctx->set_clipboard_string(window_ctx, text);
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

    inline bool get_window_icon_image(WindowContext *window_ctx, umbf::Image2D &image)
    {
        assert(window_ctx && "auik window context is not initialized");
        if (!window_ctx->get_window_icon_image) return false;
        return window_ctx->get_window_icon_image(window_ctx, image);
    }

    struct RectData
    {
        ElementID id{};
        u32 reserved = 0;
        amal::rect bounds;
        f32 depth = 0.0f;
        u16 clip_id = 0xFFFFu;
        u16 flags = 0;
    };
    static_assert(sizeof(RectData) == 40, "RectData must match picker std430 layout");

    inline RectData make_rect_data(u32 widget_id, u32 tag_id, amal::rect bounds = {}, u16 clip_id = 0xFFFFu,
                                   f32 depth = 0.0f, u16 flags = 0, u32 element_id = 0)
    {
        RectData rect{};
        rect.id = make_element_id(widget_id, tag_id, element_id);
        rect.bounds = bounds;
        rect.depth = depth;
        rect.clip_id = clip_id;
        rect.flags = flags;
        return rect;
    }
} // namespace auik::v2::detail
