#pragma once

#include <acul/enum.hpp>
#include <acul/hash/hashmap.hpp>
#include <acul/hash/utils.hpp>
#include <acul/scalars.hpp>
#include <acul/string/string.hpp>
#include <acul/vector.hpp>
#include <amal/vector.hpp>

#ifndef AUIK_TAG_GLOBAL
    #define AUIK_TAG_GLOBAL 0x00000000u
#endif

namespace auik
{
    struct CursorID
    {
        enum enum_type
        {
            arrow,
            ibeam,
            resize_ew,
            resize_ns,
            resize_nwse,
            resize_nesw,
            max
        };
    };

    struct EventBase
    {
        void prevent_default() { _prevent_default = true; }
        bool is_prevented_default() const { return _prevent_default; }

    private:
        bool _prevent_default = false;
    };

    struct KeyEnum
    {
        enum enum_type : i16
        {
            unknown,
            space,
            apostroph,
            comma,
            minus,
            period,
            slash,
            d0,
            d1,
            d2,
            d3,
            d4,
            d5,
            d6,
            d7,
            d8,
            d9,
            semicolon,
            equal,
            a,
            b,
            c,
            d,
            e,
            f,
            g,
            h,
            i,
            j,
            k,
            l,
            m,
            n,
            o,
            p,
            q,
            r,
            s,
            t,
            u,
            v,
            w,
            x,
            y,
            z,
            lbrace,
            backslash,
            rbrace,
            grave_accent,
            escape,
            enter,
            tab,
            backspace,
            insert,
            del,
            right,
            left,
            down,
            up,
            page_up,
            page_down,
            home,
            end,
            print_screen,
            pause,
            f1,
            f2,
            f3,
            f4,
            f5,
            f6,
            f7,
            f8,
            f9,
            f10,
            f11,
            f12,
            f13,
            f14,
            f15,
            f16,
            f17,
            f18,
            f19,
            f20,
            f21,
            f22,
            f23,
            f24,
            kp_0,
            kp_1,
            kp_2,
            kp_3,
            kp_4,
            kp_5,
            kp_6,
            kp_7,
            kp_8,
            kp_9,
            kp_decimal,
            kp_divide,
            kp_multiply,
            kp_subtract,
            kp_add,
            kp_enter,
            kp_equal,
            caps_lock,
            scroll_lock,
            num_lock,
            lshift,
            lcontrol,
            lalt,
            lsuper,
            rshift,
            rcontrol,
            ralt,
            rsuper,
            menu,
            last = menu
        };
    };

    using Key = KeyEnum::enum_type;

    struct MouseKeyFlagsBits
    {
        enum enum_type : i8
        {
            unknown = -1,
            left = 0x01,
            right = 0x02,
            middle = 0x04
        };
        using flag_bitmask = std::true_type;
    };

    using MouseKey = MouseKeyFlagsBits::enum_type;
    using MouseKeyFlags = acul::flags<MouseKeyFlagsBits>;

    inline constexpr i16 operator+(MouseKey k) { return static_cast<i16>(k); }
    inline constexpr i16 operator+(MouseKey lhs, i16 rhs) { return static_cast<i16>(lhs) + rhs; }

    enum class HostWindowState : u8
    {
        normal = 0,
        minimized = 1,
        maximized = 2,
        fullscreen = 3
    };

    enum class KeyPressState : i8
    {
        release,
        press,
        repeat
    };

    struct KeyModeBits
    {
        enum enum_type : i8
        {
            shift = 0x0001,
            control = 0x0002,
            alt = 0x0004,
            super = 0x0008,
            caps_lock = 0x0010,
            num_lock = 0x0020
        };
        using flag_bitmask = std::true_type;
    };

    using KeyMode = acul::flags<KeyModeBits>;

    struct EventFlagBits
    {
        enum enum_type : u16
        {
            none = 0x0,
            hover = 0x1,
            click = 0x2,
            drag = 0x4,
            scroll = 0x8,
            focus = 0x10,
            key_input = 0x20,
            char_input = 0x40,
            shortcut = 0x80,
            change = 0x100,
            drop = 0x200
        };
        using flag_bitmask = std::true_type;
    };
    using EventFlags = acul::flags<EventFlagBits>;

    struct TextFlagBits
    {
        enum enum_type : u16
        {
            none = 0x0,
            chars_decimal = 0x1,
            chars_hexadecimal = 0x2,
            chars_uppercase = 0x4,
            chars_no_blank = 0x8,
            chars_scientific = 0x10,
            chars_ascii = 0x20,
            allow_tab_input = 0x40,
            password = 0x80
        };
        using flag_bitmask = std::true_type;
    };
    using TextFlags = acul::flags<TextFlagBits>;

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

    enum class HoverState : i8
    {
        leave = 0,
        enter = 1
    };

    struct HoverEvent : EventBase
    {
        HoverState state = HoverState::leave;
        ElementID target{};
    };

    struct ClickEvent : EventBase
    {
        MouseKey key = MouseKey::unknown;
        KeyPressState state = KeyPressState::release;
        u32 click_count = 0u;
        ElementID target{};
        ElementID drag_id{};
        KeyMode mods{};
    };

    struct FocusEvent : EventBase
    {
        bool focused = false;
    };

    struct DragEvent : EventBase
    {
        amal::vec2 delta{0.0f, 0.0f};
        KeyPressState state = KeyPressState::release;
        ElementID origin{};
        KeyMode mods{};
    };

    struct DropEvent : EventBase
    {
        ElementID drag_id{};
        ElementID drop_id{};
    };

    struct ScrollEvent : EventBase
    {
        amal::vec2 delta{0.0f, 0.0f};
    };

    struct KeyEvent : EventBase
    {
        Key key = Key::unknown;
        KeyPressState state = KeyPressState::release;
        KeyMode mods = KeyModeBits::enum_type(0);
    };

    struct CharEvent : EventBase
    {
        u32 char_code = 0u;
        u32 count = 0u;
    };

    struct ChangeEvent : EventBase
    {
        u32 target = 0u;
        u32 current_target = 0u;
    };

    struct Shortcut
    {
        u32 id = AUIK_TAG_GLOBAL;
        KeyMode mods;
        acul::vector<Key> keys;
        MouseKeyFlags mouse;

        bool empty() const { return (bool)mods == 0 && keys.empty() && !mouse; }

        bool operator==(const Shortcut &other) const
        {
            return id == other.id && mods == other.mods && keys == other.keys && mouse == other.mouse;
        }
    };

    namespace detail
    {
        template <class Keys>
        inline u64 make_shortcut_hash(const Keys &keys, MouseKeyFlags mouse_keys, KeyMode mods,
                                      u32 widget_id = AUIK_TAG_GLOBAL)
        {
            size_t result = 0u;
            acul::hash_combine(result, widget_id);
            acul::hash_combine(result, static_cast<i16>(mods));

            for (const auto key : keys) acul::hash_combine(result, +key);

            acul::hash_combine(result, static_cast<i16>(mouse_keys));

            return static_cast<u64>(result);
        }
    } // namespace detail
} // namespace auik
