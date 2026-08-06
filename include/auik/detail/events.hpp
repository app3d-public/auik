#pragma once

#include <acul/enum.hpp>
#include <acul/pair.hpp>
#include <acul/scalars.hpp>
#include <amal/rect.hpp>
#include <auik/symbol_export.h>
#include <umbf/utils.hpp>
#include "../events.hpp"


#define AUIK_TAG_HITBOX 0xBF9B2277u

namespace auik::detail
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

    using CursorID = auik::CursorID;

    using PFN_get_window_handle = void *(*)(struct WindowContext *);
    using PFN_set_window_cursor = void (*)(CursorID::enum_type, struct WindowContext *);
    using PFN_get_clipboard_string = acul::string (*)(struct WindowContext *);
    using PFN_set_clipboard_string = void (*)(struct WindowContext *, const acul::string &);
    using PFN_destroy_window_backend = void (*)(struct WindowContext *);
    using PFN_update_window_time = void (*)(struct WindowContext *);
    using PFN_window_new_frame = void (*)(struct WindowContext *);
    using PFN_construct_window_backend = void (*)(struct WindowContext *);
    using PFN_get_window_icon_image = bool (*)(struct WindowContext *, umbf::Image2D &);

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

    AUIK_EXPORT void on_mouse_move(const amal::vec2 &delta);
    AUIK_EXPORT void on_scroll_event(const amal::vec2 &pos);
    AUIK_EXPORT void on_mouse_click_event(MouseKey key, KeyPressState state);
    AUIK_EXPORT void on_key_event(Key key, KeyPressState state, KeyMode mods);
    AUIK_EXPORT void on_char_event(u32 char_code);
    AUIK_EXPORT void flush_frame_changes();
    AUIK_EXPORT void reset_event_state();
    AUIK_EXPORT void on_hover_id_updated(const ElementID &prev_hover_id, const ElementID &hover_id);
    AUIK_EXPORT void deregister_widget_shortcuts(u32 widget_id);
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
        f32 hit_depth = 0.0f;
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
        rect.hit_depth = depth;
        rect.clip_id = clip_id;
        rect.flags = flags;
        return rect;
    }
} // namespace auik::detail
