#ifndef AUIK_WINDOW_IMGUI_H
#define AUIK_WINDOW_IMGUI_H

#include <acul/api.hpp>
#include <awin/types.hpp>
#include <awin/window.hpp>
#include <imgui/imgui.h>

#ifdef _WIN32
    #include <windows.h>
#endif

namespace auik
{
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
        acul::map<awin::io::Key, ImGuiKey> _key_map{{awin::io::Key::tab, ImGuiKey_Tab},
                                                    {awin::io::Key::left, ImGuiKey_LeftArrow},
                                                    {awin::io::Key::right, ImGuiKey_RightArrow},
                                                    {awin::io::Key::up, ImGuiKey_UpArrow},
                                                    {awin::io::Key::down, ImGuiKey_DownArrow},
                                                    {awin::io::Key::page_up, ImGuiKey_PageUp},
                                                    {awin::io::Key::down, ImGuiKey_PageDown},
                                                    {awin::io::Key::home, ImGuiKey_Home},
                                                    {awin::io::Key::end, ImGuiKey_End},
                                                    {awin::io::Key::insert, ImGuiKey_Insert},
                                                    {awin::io::Key::del, ImGuiKey_Delete},
                                                    {awin::io::Key::backspace, ImGuiKey_Backspace},
                                                    {awin::io::Key::space, ImGuiKey_Space},
                                                    {awin::io::Key::enter, ImGuiKey_Enter},
                                                    {awin::io::Key::escape, ImGuiKey_Escape},
                                                    {awin::io::Key::apostroph, ImGuiKey_Apostrophe},
                                                    {awin::io::Key::comma, ImGuiKey_Comma},
                                                    {awin::io::Key::minus, ImGuiKey_Minus},
                                                    {awin::io::Key::period, ImGuiKey_Period},
                                                    {awin::io::Key::slash, ImGuiKey_Slash},
                                                    {awin::io::Key::semicolon, ImGuiKey_Semicolon},
                                                    {awin::io::Key::equal, ImGuiKey_Equal},
                                                    {awin::io::Key::lbrace, ImGuiKey_LeftBracket},
                                                    {awin::io::Key::backslash, ImGuiKey_Backslash},
                                                    {awin::io::Key::rbrace, ImGuiKey_RightBracket},
                                                    {awin::io::Key::grave_accent, ImGuiKey_GraveAccent},
                                                    {awin::io::Key::caps_lock, ImGuiKey_CapsLock},
                                                    {awin::io::Key::scroll_lock, ImGuiKey_ScrollLock},
                                                    {awin::io::Key::num_lock, ImGuiKey_NumLock},
                                                    {awin::io::Key::print_screen, ImGuiKey_PrintScreen},
                                                    {awin::io::Key::pause, ImGuiKey_Pause},
                                                    {awin::io::Key::kp_0, ImGuiKey_Keypad0},
                                                    {awin::io::Key::kp_1, ImGuiKey_Keypad1},
                                                    {awin::io::Key::kp_2, ImGuiKey_Keypad2},
                                                    {awin::io::Key::kp_3, ImGuiKey_Keypad3},
                                                    {awin::io::Key::kp_4, ImGuiKey_Keypad4},
                                                    {awin::io::Key::kp_5, ImGuiKey_Keypad5},
                                                    {awin::io::Key::kp_6, ImGuiKey_Keypad6},
                                                    {awin::io::Key::kp_7, ImGuiKey_Keypad7},
                                                    {awin::io::Key::kp_8, ImGuiKey_Keypad8},
                                                    {awin::io::Key::kp_9, ImGuiKey_Keypad9},
                                                    {awin::io::Key::kp_decimal, ImGuiKey_KeypadDecimal},
                                                    {awin::io::Key::kp_divide, ImGuiKey_KeypadDivide},
                                                    {awin::io::Key::kp_multiply, ImGuiKey_KeypadMultiply},
                                                    {awin::io::Key::kp_subtract, ImGuiKey_KeypadSubtract},
                                                    {awin::io::Key::kp_add, ImGuiKey_KeypadAdd},
                                                    {awin::io::Key::kp_enter, ImGuiKey_KeypadEnter},
                                                    {awin::io::Key::kp_equal, ImGuiKey_KeypadEqual},
                                                    {awin::io::Key::lshift, ImGuiKey_LeftShift},
                                                    {awin::io::Key::lcontrol, ImGuiKey_LeftCtrl},
                                                    {awin::io::Key::lalt, ImGuiKey_LeftAlt},
                                                    {awin::io::Key::lsuper, ImGuiKey_LeftSuper},
                                                    {awin::io::Key::rshift, ImGuiKey_RightShift},
                                                    {awin::io::Key::rcontrol, ImGuiKey_RightCtrl},
                                                    {awin::io::Key::ralt, ImGuiKey_RightAlt},
                                                    {awin::io::Key::rsuper, ImGuiKey_RightSuper},
                                                    {awin::io::Key::menu, ImGuiKey_Menu},
                                                    {awin::io::Key::d0, ImGuiKey_0},
                                                    {awin::io::Key::d1, ImGuiKey_1},
                                                    {awin::io::Key::d2, ImGuiKey_2},
                                                    {awin::io::Key::d3, ImGuiKey_3},
                                                    {awin::io::Key::d4, ImGuiKey_4},
                                                    {awin::io::Key::d5, ImGuiKey_5},
                                                    {awin::io::Key::d6, ImGuiKey_6},
                                                    {awin::io::Key::d7, ImGuiKey_7},
                                                    {awin::io::Key::d8, ImGuiKey_8},
                                                    {awin::io::Key::d9, ImGuiKey_9},
                                                    {awin::io::Key::a, ImGuiKey_A},
                                                    {awin::io::Key::b, ImGuiKey_B},
                                                    {awin::io::Key::c, ImGuiKey_C},
                                                    {awin::io::Key::d, ImGuiKey_D},
                                                    {awin::io::Key::e, ImGuiKey_E},
                                                    {awin::io::Key::f, ImGuiKey_F},
                                                    {awin::io::Key::g, ImGuiKey_G},
                                                    {awin::io::Key::h, ImGuiKey_H},
                                                    {awin::io::Key::i, ImGuiKey_I},
                                                    {awin::io::Key::j, ImGuiKey_J},
                                                    {awin::io::Key::k, ImGuiKey_K},
                                                    {awin::io::Key::l, ImGuiKey_L},
                                                    {awin::io::Key::m, ImGuiKey_M},
                                                    {awin::io::Key::n, ImGuiKey_N},
                                                    {awin::io::Key::o, ImGuiKey_O},
                                                    {awin::io::Key::p, ImGuiKey_P},
                                                    {awin::io::Key::q, ImGuiKey_Q},
                                                    {awin::io::Key::r, ImGuiKey_R},
                                                    {awin::io::Key::s, ImGuiKey_S},
                                                    {awin::io::Key::t, ImGuiKey_T},
                                                    {awin::io::Key::u, ImGuiKey_U},
                                                    {awin::io::Key::v, ImGuiKey_V},
                                                    {awin::io::Key::w, ImGuiKey_W},
                                                    {awin::io::Key::x, ImGuiKey_X},
                                                    {awin::io::Key::y, ImGuiKey_Y},
                                                    {awin::io::Key::z, ImGuiKey_Z},
                                                    {awin::io::Key::f1, ImGuiKey_F1},
                                                    {awin::io::Key::f2, ImGuiKey_F2},
                                                    {awin::io::Key::f3, ImGuiKey_F3},
                                                    {awin::io::Key::f4, ImGuiKey_F4},
                                                    {awin::io::Key::f5, ImGuiKey_F5},
                                                    {awin::io::Key::f6, ImGuiKey_F6},
                                                    {awin::io::Key::f7, ImGuiKey_F7},
                                                    {awin::io::Key::f8, ImGuiKey_F8},
                                                    {awin::io::Key::f9, ImGuiKey_F9},
                                                    {awin::io::Key::f10, ImGuiKey_F10},
                                                    {awin::io::Key::f11, ImGuiKey_F11},
                                                    {awin::io::Key::f12, ImGuiKey_F12},
                                                    {awin::io::Key::f13, ImGuiKey_F13},
                                                    {awin::io::Key::f14, ImGuiKey_F14},
                                                    {awin::io::Key::f15, ImGuiKey_F15},
                                                    {awin::io::Key::f16, ImGuiKey_F16},
                                                    {awin::io::Key::f17, ImGuiKey_F17},
                                                    {awin::io::Key::f18, ImGuiKey_F18},
                                                    {awin::io::Key::f19, ImGuiKey_F19},
                                                    {awin::io::Key::f20, ImGuiKey_F20},
                                                    {awin::io::Key::f21, ImGuiKey_F21},
                                                    {awin::io::Key::f22, ImGuiKey_F22},
                                                    {awin::io::Key::f23, ImGuiKey_F23},
                                                    {awin::io::Key::f24, ImGuiKey_F24}};

        void update_mouse_data();
        void update_mouse_cursor();
    };
} // namespace auik

#endif