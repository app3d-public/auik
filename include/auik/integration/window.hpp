#ifndef AUIK_WINDOW_IMGUI_H
#define AUIK_WINDOW_IMGUI_H

#include <acul/api.hpp>
#include <acul/lut_table.hpp>
#include <awin/types.hpp>
#include <awin/window.hpp>
#include <imgui/imgui.h>


namespace auik
{
    namespace internal
    {
        struct KeyTraits
        {
            using value_type = int;
            using enum_type = int;

            static constexpr enum_type unknown = ImGuiKey_None;

            static consteval void fill_lut_table(std::array<enum_type, 256> &a)
            {
                a[awin::io::Key::tab] = ImGuiKey_Tab;
                a[awin::io::Key::left] = ImGuiKey_LeftArrow;
                a[awin::io::Key::right] = ImGuiKey_RightArrow;
                a[awin::io::Key::up] = ImGuiKey_UpArrow;
                a[awin::io::Key::down] = ImGuiKey_DownArrow;
                a[awin::io::Key::page_up] = ImGuiKey_PageUp;
                a[awin::io::Key::down] = ImGuiKey_PageDown;
                a[awin::io::Key::home] = ImGuiKey_Home;
                a[awin::io::Key::end] = ImGuiKey_End;
                a[awin::io::Key::insert] = ImGuiKey_Insert;
                a[awin::io::Key::del] = ImGuiKey_Delete;
                a[awin::io::Key::backspace] = ImGuiKey_Backspace;
                a[awin::io::Key::space] = ImGuiKey_Space;
                a[awin::io::Key::enter] = ImGuiKey_Enter;
                a[awin::io::Key::escape] = ImGuiKey_Escape;
                a[awin::io::Key::apostroph] = ImGuiKey_Apostrophe;
                a[awin::io::Key::comma] = ImGuiKey_Comma;
                a[awin::io::Key::minus] = ImGuiKey_Minus;
                a[awin::io::Key::period] = ImGuiKey_Period;
                a[awin::io::Key::slash] = ImGuiKey_Slash;
                a[awin::io::Key::semicolon] = ImGuiKey_Semicolon;
                a[awin::io::Key::equal] = ImGuiKey_Equal;
                a[awin::io::Key::lbrace] = ImGuiKey_LeftBracket;
                a[awin::io::Key::backslash] = ImGuiKey_Backslash;
                a[awin::io::Key::rbrace] = ImGuiKey_RightBracket;
                a[awin::io::Key::grave_accent] = ImGuiKey_GraveAccent;
                a[awin::io::Key::caps_lock] = ImGuiKey_CapsLock;
                a[awin::io::Key::scroll_lock] = ImGuiKey_ScrollLock;
                a[awin::io::Key::num_lock] = ImGuiKey_NumLock;
                a[awin::io::Key::print_screen] = ImGuiKey_PrintScreen;
                a[awin::io::Key::pause] = ImGuiKey_Pause;
                a[awin::io::Key::kp_0] = ImGuiKey_Keypad0;
                a[awin::io::Key::kp_1] = ImGuiKey_Keypad1;
                a[awin::io::Key::kp_2] = ImGuiKey_Keypad2;
                a[awin::io::Key::kp_3] = ImGuiKey_Keypad3;
                a[awin::io::Key::kp_4] = ImGuiKey_Keypad4;
                a[awin::io::Key::kp_5] = ImGuiKey_Keypad5;
                a[awin::io::Key::kp_6] = ImGuiKey_Keypad6;
                a[awin::io::Key::kp_7] = ImGuiKey_Keypad7;
                a[awin::io::Key::kp_8] = ImGuiKey_Keypad8;
                a[awin::io::Key::kp_9] = ImGuiKey_Keypad9;
                a[awin::io::Key::kp_decimal] = ImGuiKey_KeypadDecimal;
                a[awin::io::Key::kp_divide] = ImGuiKey_KeypadDivide;
                a[awin::io::Key::kp_multiply] = ImGuiKey_KeypadMultiply;
                a[awin::io::Key::kp_subtract] = ImGuiKey_KeypadSubtract;
                a[awin::io::Key::kp_add] = ImGuiKey_KeypadAdd;
                a[awin::io::Key::kp_enter] = ImGuiKey_KeypadEnter;
                a[awin::io::Key::kp_equal] = ImGuiKey_KeypadEqual;
                a[awin::io::Key::lshift] = ImGuiKey_LeftShift;
                a[awin::io::Key::lcontrol] = ImGuiKey_LeftCtrl;
                a[awin::io::Key::lalt] = ImGuiKey_LeftAlt;
                a[awin::io::Key::lsuper] = ImGuiKey_LeftSuper;
                a[awin::io::Key::rshift] = ImGuiKey_RightShift;
                a[awin::io::Key::rcontrol] = ImGuiKey_RightCtrl;
                a[awin::io::Key::ralt] = ImGuiKey_RightAlt;
                a[awin::io::Key::rsuper] = ImGuiKey_RightSuper;
                a[awin::io::Key::menu] = ImGuiKey_Menu;
                a[awin::io::Key::d0] = ImGuiKey_0;
                a[awin::io::Key::d1] = ImGuiKey_1;
                a[awin::io::Key::d2] = ImGuiKey_2;
                a[awin::io::Key::d3] = ImGuiKey_3;
                a[awin::io::Key::d4] = ImGuiKey_4;
                a[awin::io::Key::d5] = ImGuiKey_5;
                a[awin::io::Key::d6] = ImGuiKey_6;
                a[awin::io::Key::d7] = ImGuiKey_7;
                a[awin::io::Key::d8] = ImGuiKey_8;
                a[awin::io::Key::d9] = ImGuiKey_9;
                a[awin::io::Key::a] = ImGuiKey_A;
                a[awin::io::Key::b] = ImGuiKey_B;
                a[awin::io::Key::c] = ImGuiKey_C;
                a[awin::io::Key::d] = ImGuiKey_D;
                a[awin::io::Key::e] = ImGuiKey_E;
                a[awin::io::Key::f] = ImGuiKey_F;
                a[awin::io::Key::g] = ImGuiKey_G;
                a[awin::io::Key::h] = ImGuiKey_H;
                a[awin::io::Key::i] = ImGuiKey_I;
                a[awin::io::Key::j] = ImGuiKey_J;
                a[awin::io::Key::k] = ImGuiKey_K;
                a[awin::io::Key::l] = ImGuiKey_L;
                a[awin::io::Key::m] = ImGuiKey_M;
                a[awin::io::Key::n] = ImGuiKey_N;
                a[awin::io::Key::o] = ImGuiKey_O;
                a[awin::io::Key::p] = ImGuiKey_P;
                a[awin::io::Key::q] = ImGuiKey_Q;
                a[awin::io::Key::r] = ImGuiKey_R;
                a[awin::io::Key::s] = ImGuiKey_S;
                a[awin::io::Key::t] = ImGuiKey_T;
                a[awin::io::Key::u] = ImGuiKey_U;
                a[awin::io::Key::v] = ImGuiKey_V;
                a[awin::io::Key::w] = ImGuiKey_W;
                a[awin::io::Key::x] = ImGuiKey_X;
                a[awin::io::Key::y] = ImGuiKey_Y;
                a[awin::io::Key::z] = ImGuiKey_Z;
                a[awin::io::Key::f1] = ImGuiKey_F1;
                a[awin::io::Key::f2] = ImGuiKey_F2;
                a[awin::io::Key::f3] = ImGuiKey_F3;
                a[awin::io::Key::f4] = ImGuiKey_F4;
                a[awin::io::Key::f5] = ImGuiKey_F5;
                a[awin::io::Key::f6] = ImGuiKey_F6;
                a[awin::io::Key::f7] = ImGuiKey_F7;
                a[awin::io::Key::f8] = ImGuiKey_F8;
                a[awin::io::Key::f9] = ImGuiKey_F9;
                a[awin::io::Key::f10] = ImGuiKey_F10;
                a[awin::io::Key::f11] = ImGuiKey_F11;
                a[awin::io::Key::f12] = ImGuiKey_F12;
                a[awin::io::Key::f13] = ImGuiKey_F13;
                a[awin::io::Key::f14] = ImGuiKey_F14;
                a[awin::io::Key::f15] = ImGuiKey_F15;
                a[awin::io::Key::f16] = ImGuiKey_F16;
                a[awin::io::Key::f17] = ImGuiKey_F17;
                a[awin::io::Key::f18] = ImGuiKey_F18;
                a[awin::io::Key::f19] = ImGuiKey_F19;
                a[awin::io::Key::f20] = ImGuiKey_F20;
                a[awin::io::Key::f21] = ImGuiKey_F21;
                a[awin::io::Key::f22] = ImGuiKey_F22;
                a[awin::io::Key::f23] = ImGuiKey_F23;
                a[awin::io::Key::f24] = ImGuiKey_F24;
            }
        };
    } // namespace internal

    class APPLIB_API WindowImGuiBinder
    {
    public:
        explicit WindowImGuiBinder(awin::Window &window, acul::events::dispatcher *ed);

        ~WindowImGuiBinder();

        void bind_events();

        void new_frame();

    private:
        acul::events::dispatcher *ed;
        struct ImGuiBackendData *_bd;
        acul::lut_table<256, internal::KeyTraits> _keymap;

        void update_mouse_data();
        void update_mouse_cursor();
    };
} // namespace auik

#endif