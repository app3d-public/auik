#ifndef UIKIT_WINDOW_IMGUI_H
#define UIKIT_WINDOW_IMGUI_H

#include <imgui/imgui.h>
#include <window/window.hpp>
#include <core/api.hpp>
#include "window/types.hpp"
#ifdef _WIN32
    #include <windows.h>
#endif

namespace uikit
{
    namespace
    {
        static LRESULT CALLBACK ImGuiWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

        struct ImGuiBackendData
        {
            window::Window *Window;
            f64 Time;
            window::Cursor MouseCursors[ImGuiMouseCursor_COUNT];
            ImVec2 LastValidMousePos;
#ifdef _WIN32
            WNDPROC WndProc;
#endif
        };
    } // namespace

    class APPLIB_API WindowImGuiBinder
    {
    public:
        explicit WindowImGuiBinder(window::Window &window);

        ~WindowImGuiBinder();

        void bindEvents();

        void newFrame();

    private:
        ImGuiBackendData *_bd;
        Map<window::io::Key, ImGuiKey> _keyMap{{window::io::Key::kTab, ImGuiKey_Tab},
                                       {window::io::Key::kLeft, ImGuiKey_LeftArrow},
                                       {window::io::Key::kRight, ImGuiKey_RightArrow},
                                       {window::io::Key::kUp, ImGuiKey_UpArrow},
                                       {window::io::Key::kDown, ImGuiKey_DownArrow},
                                       {window::io::Key::kPageUp, ImGuiKey_PageUp},
                                       {window::io::Key::kDown, ImGuiKey_PageDown},
                                       {window::io::Key::kHome, ImGuiKey_Home},
                                       {window::io::Key::kEnd, ImGuiKey_End},
                                       {window::io::Key::kInsert, ImGuiKey_Insert},
                                       {window::io::Key::kDelete, ImGuiKey_Delete},
                                       {window::io::Key::kBackspace, ImGuiKey_Backspace},
                                       {window::io::Key::kSpace, ImGuiKey_Space},
                                       {window::io::Key::kEnter, ImGuiKey_Enter},
                                       {window::io::Key::kEscape, ImGuiKey_Escape},
                                       {window::io::Key::kApostroph, ImGuiKey_Apostrophe},
                                       {window::io::Key::kComma, ImGuiKey_Comma},
                                       {window::io::Key::kMinus, ImGuiKey_Minus},
                                       {window::io::Key::kPeriod, ImGuiKey_Period},
                                       {window::io::Key::kSlash, ImGuiKey_Slash},
                                       {window::io::Key::kSemicolon, ImGuiKey_Semicolon},
                                       {window::io::Key::kEqual, ImGuiKey_Equal},
                                       {window::io::Key::kLeftBrace, ImGuiKey_LeftBracket},
                                       {window::io::Key::kBackslash, ImGuiKey_Backslash},
                                       {window::io::Key::kRightBrace, ImGuiKey_RightBracket},
                                       {window::io::Key::kGraveAccent, ImGuiKey_GraveAccent},
                                       {window::io::Key::kCapsLock, ImGuiKey_CapsLock},
                                       {window::io::Key::kScrollLock, ImGuiKey_ScrollLock},
                                       {window::io::Key::kNumLock, ImGuiKey_NumLock},
                                       {window::io::Key::kPrintScreen, ImGuiKey_PrintScreen},
                                       {window::io::Key::kPause, ImGuiKey_Pause},
                                       {window::io::Key::kKP0, ImGuiKey_Keypad0},
                                       {window::io::Key::kKP1, ImGuiKey_Keypad1},
                                       {window::io::Key::kKP2, ImGuiKey_Keypad2},
                                       {window::io::Key::kKP3, ImGuiKey_Keypad3},
                                       {window::io::Key::kKP4, ImGuiKey_Keypad4},
                                       {window::io::Key::kKP5, ImGuiKey_Keypad5},
                                       {window::io::Key::kKP6, ImGuiKey_Keypad6},
                                       {window::io::Key::kKP7, ImGuiKey_Keypad7},
                                       {window::io::Key::kKP8, ImGuiKey_Keypad8},
                                       {window::io::Key::kKP9, ImGuiKey_Keypad9},
                                       {window::io::Key::kKPDecimal, ImGuiKey_KeypadDecimal},
                                       {window::io::Key::kKPDivide, ImGuiKey_KeypadDivide},
                                       {window::io::Key::kKPMultiply, ImGuiKey_KeypadMultiply},
                                       {window::io::Key::kKPSubtract, ImGuiKey_KeypadSubtract},
                                       {window::io::Key::kKPAdd, ImGuiKey_KeypadAdd},
                                       {window::io::Key::kKPEnter, ImGuiKey_KeypadEnter},
                                       {window::io::Key::kKPEqual, ImGuiKey_KeypadEqual},
                                       {window::io::Key::kLeftShift, ImGuiKey_LeftShift},
                                       {window::io::Key::kLeftControl, ImGuiKey_LeftCtrl},
                                       {window::io::Key::kLeftAlt, ImGuiKey_LeftAlt},
                                       {window::io::Key::kLeftSuper, ImGuiKey_LeftSuper},
                                       {window::io::Key::kRightShift, ImGuiKey_RightShift},
                                       {window::io::Key::kRightControl, ImGuiKey_RightCtrl},
                                       {window::io::Key::kRightAlt, ImGuiKey_RightAlt},
                                       {window::io::Key::kRightSuper, ImGuiKey_RightSuper},
                                       {window::io::Key::kMenu, ImGuiKey_Menu},
                                       {window::io::Key::k0, ImGuiKey_0},
                                       {window::io::Key::k1, ImGuiKey_1},
                                       {window::io::Key::k2, ImGuiKey_2},
                                       {window::io::Key::k3, ImGuiKey_3},
                                       {window::io::Key::k4, ImGuiKey_4},
                                       {window::io::Key::k5, ImGuiKey_5},
                                       {window::io::Key::k6, ImGuiKey_6},
                                       {window::io::Key::k7, ImGuiKey_7},
                                       {window::io::Key::k8, ImGuiKey_8},
                                       {window::io::Key::k9, ImGuiKey_9},
                                       {window::io::Key::kA, ImGuiKey_A},
                                       {window::io::Key::kB, ImGuiKey_B},
                                       {window::io::Key::kC, ImGuiKey_C},
                                       {window::io::Key::kD, ImGuiKey_D},
                                       {window::io::Key::kE, ImGuiKey_E},
                                       {window::io::Key::kF, ImGuiKey_F},
                                       {window::io::Key::kG, ImGuiKey_G},
                                       {window::io::Key::kH, ImGuiKey_H},
                                       {window::io::Key::kI, ImGuiKey_I},
                                       {window::io::Key::kJ, ImGuiKey_J},
                                       {window::io::Key::kK, ImGuiKey_K},
                                       {window::io::Key::kL, ImGuiKey_L},
                                       {window::io::Key::kM, ImGuiKey_M},
                                       {window::io::Key::kN, ImGuiKey_N},
                                       {window::io::Key::kO, ImGuiKey_O},
                                       {window::io::Key::kP, ImGuiKey_P},
                                       {window::io::Key::kQ, ImGuiKey_Q},
                                       {window::io::Key::kR, ImGuiKey_R},
                                       {window::io::Key::kS, ImGuiKey_S},
                                       {window::io::Key::kT, ImGuiKey_T},
                                       {window::io::Key::kU, ImGuiKey_U},
                                       {window::io::Key::kV, ImGuiKey_V},
                                       {window::io::Key::kW, ImGuiKey_W},
                                       {window::io::Key::kX, ImGuiKey_X},
                                       {window::io::Key::kY, ImGuiKey_Y},
                                       {window::io::Key::kZ, ImGuiKey_Z},
                                       {window::io::Key::kF1, ImGuiKey_F1},
                                       {window::io::Key::kF2, ImGuiKey_F2},
                                       {window::io::Key::kF3, ImGuiKey_F3},
                                       {window::io::Key::kF4, ImGuiKey_F4},
                                       {window::io::Key::kF5, ImGuiKey_F5},
                                       {window::io::Key::kF6, ImGuiKey_F6},
                                       {window::io::Key::kF7, ImGuiKey_F7},
                                       {window::io::Key::kF8, ImGuiKey_F8},
                                       {window::io::Key::kF9, ImGuiKey_F9},
                                       {window::io::Key::kF10, ImGuiKey_F10},
                                       {window::io::Key::kF11, ImGuiKey_F11},
                                       {window::io::Key::kF12, ImGuiKey_F12},
                                       {window::io::Key::kF13, ImGuiKey_F13},
                                       {window::io::Key::kF14, ImGuiKey_F14},
                                       {window::io::Key::kF15, ImGuiKey_F15},
                                       {window::io::Key::kF16, ImGuiKey_F16},
                                       {window::io::Key::kF17, ImGuiKey_F17},
                                       {window::io::Key::kF18, ImGuiKey_F18},
                                       {window::io::Key::kF19, ImGuiKey_F19},
                                       {window::io::Key::kF20, ImGuiKey_F20},
                                       {window::io::Key::kF21, ImGuiKey_F21},
                                       {window::io::Key::kF22, ImGuiKey_F22},
                                       {window::io::Key::kF23, ImGuiKey_F23},
                                       {window::io::Key::kF24, ImGuiKey_F24}};

        void updateMouseData();
        void updateMouseCursor();
    };
} // namespace ui

#endif