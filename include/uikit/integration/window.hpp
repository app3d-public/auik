#ifndef UIKIT_WINDOW_IMGUI_H
#define UIKIT_WINDOW_IMGUI_H

#include <acul/api.hpp>
#include <awin/types.hpp>
#include <awin/window.hpp>
#include <imgui/imgui.h>

#ifdef _WIN32
    #include <windows.h>
#endif

namespace uikit
{
    namespace
    {
        struct ImGuiBackendData
        {
            awin::Window *window;
            f64 time;
            awin::Cursor mouse_cursors[ImGuiMouseCursor_COUNT];
            ImVec2 last_valid_mouse_pos;
#ifdef _WIN32
            WNDPROC prev_wnd_proc;
#endif
        };
    } // namespace

    class APPLIB_API WindowImGuiBinder
    {
    public:
        explicit WindowImGuiBinder(awin::Window &window, acul::events::dispatcher *ed);

        ~WindowImGuiBinder();

        void bind_events();

        void new_frame();

    private:
        acul::events::dispatcher *ed;
        ImGuiBackendData *_bd;
        acul::map<awin::io::Key, ImGuiKey> _key_map{{awin::io::Key::Tab, ImGuiKey_Tab},
                                                    {awin::io::Key::Left, ImGuiKey_LeftArrow},
                                                    {awin::io::Key::Right, ImGuiKey_RightArrow},
                                                    {awin::io::Key::Up, ImGuiKey_UpArrow},
                                                    {awin::io::Key::Down, ImGuiKey_DownArrow},
                                                    {awin::io::Key::PageUp, ImGuiKey_PageUp},
                                                    {awin::io::Key::Down, ImGuiKey_PageDown},
                                                    {awin::io::Key::Home, ImGuiKey_Home},
                                                    {awin::io::Key::End, ImGuiKey_End},
                                                    {awin::io::Key::Insert, ImGuiKey_Insert},
                                                    {awin::io::Key::Delete, ImGuiKey_Delete},
                                                    {awin::io::Key::Backspace, ImGuiKey_Backspace},
                                                    {awin::io::Key::Space, ImGuiKey_Space},
                                                    {awin::io::Key::Enter, ImGuiKey_Enter},
                                                    {awin::io::Key::Escape, ImGuiKey_Escape},
                                                    {awin::io::Key::Apostroph, ImGuiKey_Apostrophe},
                                                    {awin::io::Key::Comma, ImGuiKey_Comma},
                                                    {awin::io::Key::Minus, ImGuiKey_Minus},
                                                    {awin::io::Key::Period, ImGuiKey_Period},
                                                    {awin::io::Key::Slash, ImGuiKey_Slash},
                                                    {awin::io::Key::Semicolon, ImGuiKey_Semicolon},
                                                    {awin::io::Key::Equal, ImGuiKey_Equal},
                                                    {awin::io::Key::LeftBrace, ImGuiKey_LeftBracket},
                                                    {awin::io::Key::Backslash, ImGuiKey_Backslash},
                                                    {awin::io::Key::RightBrace, ImGuiKey_RightBracket},
                                                    {awin::io::Key::GraveAccent, ImGuiKey_GraveAccent},
                                                    {awin::io::Key::CapsLock, ImGuiKey_CapsLock},
                                                    {awin::io::Key::ScrollLock, ImGuiKey_ScrollLock},
                                                    {awin::io::Key::NumLock, ImGuiKey_NumLock},
                                                    {awin::io::Key::PrintScreen, ImGuiKey_PrintScreen},
                                                    {awin::io::Key::Pause, ImGuiKey_Pause},
                                                    {awin::io::Key::KP0, ImGuiKey_Keypad0},
                                                    {awin::io::Key::KP1, ImGuiKey_Keypad1},
                                                    {awin::io::Key::KP2, ImGuiKey_Keypad2},
                                                    {awin::io::Key::KP3, ImGuiKey_Keypad3},
                                                    {awin::io::Key::KP4, ImGuiKey_Keypad4},
                                                    {awin::io::Key::KP5, ImGuiKey_Keypad5},
                                                    {awin::io::Key::KP6, ImGuiKey_Keypad6},
                                                    {awin::io::Key::KP7, ImGuiKey_Keypad7},
                                                    {awin::io::Key::KP8, ImGuiKey_Keypad8},
                                                    {awin::io::Key::KP9, ImGuiKey_Keypad9},
                                                    {awin::io::Key::KPDecimal, ImGuiKey_KeypadDecimal},
                                                    {awin::io::Key::KPDivide, ImGuiKey_KeypadDivide},
                                                    {awin::io::Key::KPMultiply, ImGuiKey_KeypadMultiply},
                                                    {awin::io::Key::KPSubtract, ImGuiKey_KeypadSubtract},
                                                    {awin::io::Key::KPAdd, ImGuiKey_KeypadAdd},
                                                    {awin::io::Key::KPEnter, ImGuiKey_KeypadEnter},
                                                    {awin::io::Key::KPEqual, ImGuiKey_KeypadEqual},
                                                    {awin::io::Key::LeftShift, ImGuiKey_LeftShift},
                                                    {awin::io::Key::LeftControl, ImGuiKey_LeftCtrl},
                                                    {awin::io::Key::LeftAlt, ImGuiKey_LeftAlt},
                                                    {awin::io::Key::LeftSuper, ImGuiKey_LeftSuper},
                                                    {awin::io::Key::RightShift, ImGuiKey_RightShift},
                                                    {awin::io::Key::RightControl, ImGuiKey_RightCtrl},
                                                    {awin::io::Key::RightAlt, ImGuiKey_RightAlt},
                                                    {awin::io::Key::RightSuper, ImGuiKey_RightSuper},
                                                    {awin::io::Key::Menu, ImGuiKey_Menu},
                                                    {awin::io::Key::D0, ImGuiKey_0},
                                                    {awin::io::Key::D1, ImGuiKey_1},
                                                    {awin::io::Key::D2, ImGuiKey_2},
                                                    {awin::io::Key::D3, ImGuiKey_3},
                                                    {awin::io::Key::D4, ImGuiKey_4},
                                                    {awin::io::Key::D5, ImGuiKey_5},
                                                    {awin::io::Key::D6, ImGuiKey_6},
                                                    {awin::io::Key::D7, ImGuiKey_7},
                                                    {awin::io::Key::D8, ImGuiKey_8},
                                                    {awin::io::Key::D9, ImGuiKey_9},
                                                    {awin::io::Key::A, ImGuiKey_A},
                                                    {awin::io::Key::B, ImGuiKey_B},
                                                    {awin::io::Key::C, ImGuiKey_C},
                                                    {awin::io::Key::D, ImGuiKey_D},
                                                    {awin::io::Key::E, ImGuiKey_E},
                                                    {awin::io::Key::F, ImGuiKey_F},
                                                    {awin::io::Key::G, ImGuiKey_G},
                                                    {awin::io::Key::H, ImGuiKey_H},
                                                    {awin::io::Key::I, ImGuiKey_I},
                                                    {awin::io::Key::J, ImGuiKey_J},
                                                    {awin::io::Key::K, ImGuiKey_K},
                                                    {awin::io::Key::L, ImGuiKey_L},
                                                    {awin::io::Key::M, ImGuiKey_M},
                                                    {awin::io::Key::N, ImGuiKey_N},
                                                    {awin::io::Key::O, ImGuiKey_O},
                                                    {awin::io::Key::P, ImGuiKey_P},
                                                    {awin::io::Key::Q, ImGuiKey_Q},
                                                    {awin::io::Key::R, ImGuiKey_R},
                                                    {awin::io::Key::S, ImGuiKey_S},
                                                    {awin::io::Key::T, ImGuiKey_T},
                                                    {awin::io::Key::U, ImGuiKey_U},
                                                    {awin::io::Key::V, ImGuiKey_V},
                                                    {awin::io::Key::W, ImGuiKey_W},
                                                    {awin::io::Key::X, ImGuiKey_X},
                                                    {awin::io::Key::Y, ImGuiKey_Y},
                                                    {awin::io::Key::Z, ImGuiKey_Z},
                                                    {awin::io::Key::F1, ImGuiKey_F1},
                                                    {awin::io::Key::F2, ImGuiKey_F2},
                                                    {awin::io::Key::F3, ImGuiKey_F3},
                                                    {awin::io::Key::F4, ImGuiKey_F4},
                                                    {awin::io::Key::F5, ImGuiKey_F5},
                                                    {awin::io::Key::F6, ImGuiKey_F6},
                                                    {awin::io::Key::F7, ImGuiKey_F7},
                                                    {awin::io::Key::F8, ImGuiKey_F8},
                                                    {awin::io::Key::F9, ImGuiKey_F9},
                                                    {awin::io::Key::F10, ImGuiKey_F10},
                                                    {awin::io::Key::F11, ImGuiKey_F11},
                                                    {awin::io::Key::F12, ImGuiKey_F12},
                                                    {awin::io::Key::F13, ImGuiKey_F13},
                                                    {awin::io::Key::F14, ImGuiKey_F14},
                                                    {awin::io::Key::F15, ImGuiKey_F15},
                                                    {awin::io::Key::F16, ImGuiKey_F16},
                                                    {awin::io::Key::F17, ImGuiKey_F17},
                                                    {awin::io::Key::F18, ImGuiKey_F18},
                                                    {awin::io::Key::F19, ImGuiKey_F19},
                                                    {awin::io::Key::F20, ImGuiKey_F20},
                                                    {awin::io::Key::F21, ImGuiKey_F21},
                                                    {awin::io::Key::F22, ImGuiKey_F22},
                                                    {awin::io::Key::F23, ImGuiKey_F23},
                                                    {awin::io::Key::F24, ImGuiKey_F24}};

        void update_mouse_data();
        void update_mouse_cursor();
    };
} // namespace uikit

#endif