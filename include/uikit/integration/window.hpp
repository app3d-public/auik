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
        [[maybe_unused]] static LRESULT CALLBACK ImGuiWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

        struct ImGuiBackendData
        {
            awin::Window *Window;
            f64 Time;
            awin::Cursor MouseCursors[ImGuiMouseCursor_COUNT];
            ImVec2 LastValidMousePos;
#ifdef _WIN32
            WNDPROC WndProc;
#endif
        };
    } // namespace

    class APPLIB_API WindowImGuiBinder
    {
    public:
        explicit WindowImGuiBinder(awin::Window &window, acul::events::dispatcher *ed);

        ~WindowImGuiBinder();

        void bindEvents();

        void newFrame();

    private:
        acul::events::dispatcher *ed;
        ImGuiBackendData *_bd;
        acul::map<awin::io::Key, ImGuiKey> _keyMap{{awin::io::Key::kTab, ImGuiKey_Tab},
                                                   {awin::io::Key::kLeft, ImGuiKey_LeftArrow},
                                                   {awin::io::Key::kRight, ImGuiKey_RightArrow},
                                                   {awin::io::Key::kUp, ImGuiKey_UpArrow},
                                                   {awin::io::Key::kDown, ImGuiKey_DownArrow},
                                                   {awin::io::Key::kPageUp, ImGuiKey_PageUp},
                                                   {awin::io::Key::kDown, ImGuiKey_PageDown},
                                                   {awin::io::Key::kHome, ImGuiKey_Home},
                                                   {awin::io::Key::kEnd, ImGuiKey_End},
                                                   {awin::io::Key::kInsert, ImGuiKey_Insert},
                                                   {awin::io::Key::kDelete, ImGuiKey_Delete},
                                                   {awin::io::Key::kBackspace, ImGuiKey_Backspace},
                                                   {awin::io::Key::kSpace, ImGuiKey_Space},
                                                   {awin::io::Key::kEnter, ImGuiKey_Enter},
                                                   {awin::io::Key::kEscape, ImGuiKey_Escape},
                                                   {awin::io::Key::kApostroph, ImGuiKey_Apostrophe},
                                                   {awin::io::Key::kComma, ImGuiKey_Comma},
                                                   {awin::io::Key::kMinus, ImGuiKey_Minus},
                                                   {awin::io::Key::kPeriod, ImGuiKey_Period},
                                                   {awin::io::Key::kSlash, ImGuiKey_Slash},
                                                   {awin::io::Key::kSemicolon, ImGuiKey_Semicolon},
                                                   {awin::io::Key::kEqual, ImGuiKey_Equal},
                                                   {awin::io::Key::kLeftBrace, ImGuiKey_LeftBracket},
                                                   {awin::io::Key::kBackslash, ImGuiKey_Backslash},
                                                   {awin::io::Key::kRightBrace, ImGuiKey_RightBracket},
                                                   {awin::io::Key::kGraveAccent, ImGuiKey_GraveAccent},
                                                   {awin::io::Key::kCapsLock, ImGuiKey_CapsLock},
                                                   {awin::io::Key::kScrollLock, ImGuiKey_ScrollLock},
                                                   {awin::io::Key::kNumLock, ImGuiKey_NumLock},
                                                   {awin::io::Key::kPrintScreen, ImGuiKey_PrintScreen},
                                                   {awin::io::Key::kPause, ImGuiKey_Pause},
                                                   {awin::io::Key::kKP0, ImGuiKey_Keypad0},
                                                   {awin::io::Key::kKP1, ImGuiKey_Keypad1},
                                                   {awin::io::Key::kKP2, ImGuiKey_Keypad2},
                                                   {awin::io::Key::kKP3, ImGuiKey_Keypad3},
                                                   {awin::io::Key::kKP4, ImGuiKey_Keypad4},
                                                   {awin::io::Key::kKP5, ImGuiKey_Keypad5},
                                                   {awin::io::Key::kKP6, ImGuiKey_Keypad6},
                                                   {awin::io::Key::kKP7, ImGuiKey_Keypad7},
                                                   {awin::io::Key::kKP8, ImGuiKey_Keypad8},
                                                   {awin::io::Key::kKP9, ImGuiKey_Keypad9},
                                                   {awin::io::Key::kKPDecimal, ImGuiKey_KeypadDecimal},
                                                   {awin::io::Key::kKPDivide, ImGuiKey_KeypadDivide},
                                                   {awin::io::Key::kKPMultiply, ImGuiKey_KeypadMultiply},
                                                   {awin::io::Key::kKPSubtract, ImGuiKey_KeypadSubtract},
                                                   {awin::io::Key::kKPAdd, ImGuiKey_KeypadAdd},
                                                   {awin::io::Key::kKPEnter, ImGuiKey_KeypadEnter},
                                                   {awin::io::Key::kKPEqual, ImGuiKey_KeypadEqual},
                                                   {awin::io::Key::kLeftShift, ImGuiKey_LeftShift},
                                                   {awin::io::Key::kLeftControl, ImGuiKey_LeftCtrl},
                                                   {awin::io::Key::kLeftAlt, ImGuiKey_LeftAlt},
                                                   {awin::io::Key::kLeftSuper, ImGuiKey_LeftSuper},
                                                   {awin::io::Key::kRightShift, ImGuiKey_RightShift},
                                                   {awin::io::Key::kRightControl, ImGuiKey_RightCtrl},
                                                   {awin::io::Key::kRightAlt, ImGuiKey_RightAlt},
                                                   {awin::io::Key::kRightSuper, ImGuiKey_RightSuper},
                                                   {awin::io::Key::kMenu, ImGuiKey_Menu},
                                                   {awin::io::Key::k0, ImGuiKey_0},
                                                   {awin::io::Key::k1, ImGuiKey_1},
                                                   {awin::io::Key::k2, ImGuiKey_2},
                                                   {awin::io::Key::k3, ImGuiKey_3},
                                                   {awin::io::Key::k4, ImGuiKey_4},
                                                   {awin::io::Key::k5, ImGuiKey_5},
                                                   {awin::io::Key::k6, ImGuiKey_6},
                                                   {awin::io::Key::k7, ImGuiKey_7},
                                                   {awin::io::Key::k8, ImGuiKey_8},
                                                   {awin::io::Key::k9, ImGuiKey_9},
                                                   {awin::io::Key::kA, ImGuiKey_A},
                                                   {awin::io::Key::kB, ImGuiKey_B},
                                                   {awin::io::Key::kC, ImGuiKey_C},
                                                   {awin::io::Key::kD, ImGuiKey_D},
                                                   {awin::io::Key::kE, ImGuiKey_E},
                                                   {awin::io::Key::kF, ImGuiKey_F},
                                                   {awin::io::Key::kG, ImGuiKey_G},
                                                   {awin::io::Key::kH, ImGuiKey_H},
                                                   {awin::io::Key::kI, ImGuiKey_I},
                                                   {awin::io::Key::kJ, ImGuiKey_J},
                                                   {awin::io::Key::kK, ImGuiKey_K},
                                                   {awin::io::Key::kL, ImGuiKey_L},
                                                   {awin::io::Key::kM, ImGuiKey_M},
                                                   {awin::io::Key::kN, ImGuiKey_N},
                                                   {awin::io::Key::kO, ImGuiKey_O},
                                                   {awin::io::Key::kP, ImGuiKey_P},
                                                   {awin::io::Key::kQ, ImGuiKey_Q},
                                                   {awin::io::Key::kR, ImGuiKey_R},
                                                   {awin::io::Key::kS, ImGuiKey_S},
                                                   {awin::io::Key::kT, ImGuiKey_T},
                                                   {awin::io::Key::kU, ImGuiKey_U},
                                                   {awin::io::Key::kV, ImGuiKey_V},
                                                   {awin::io::Key::kW, ImGuiKey_W},
                                                   {awin::io::Key::kX, ImGuiKey_X},
                                                   {awin::io::Key::kY, ImGuiKey_Y},
                                                   {awin::io::Key::kZ, ImGuiKey_Z},
                                                   {awin::io::Key::kF1, ImGuiKey_F1},
                                                   {awin::io::Key::kF2, ImGuiKey_F2},
                                                   {awin::io::Key::kF3, ImGuiKey_F3},
                                                   {awin::io::Key::kF4, ImGuiKey_F4},
                                                   {awin::io::Key::kF5, ImGuiKey_F5},
                                                   {awin::io::Key::kF6, ImGuiKey_F6},
                                                   {awin::io::Key::kF7, ImGuiKey_F7},
                                                   {awin::io::Key::kF8, ImGuiKey_F8},
                                                   {awin::io::Key::kF9, ImGuiKey_F9},
                                                   {awin::io::Key::kF10, ImGuiKey_F10},
                                                   {awin::io::Key::kF11, ImGuiKey_F11},
                                                   {awin::io::Key::kF12, ImGuiKey_F12},
                                                   {awin::io::Key::kF13, ImGuiKey_F13},
                                                   {awin::io::Key::kF14, ImGuiKey_F14},
                                                   {awin::io::Key::kF15, ImGuiKey_F15},
                                                   {awin::io::Key::kF16, ImGuiKey_F16},
                                                   {awin::io::Key::kF17, ImGuiKey_F17},
                                                   {awin::io::Key::kF18, ImGuiKey_F18},
                                                   {awin::io::Key::kF19, ImGuiKey_F19},
                                                   {awin::io::Key::kF20, ImGuiKey_F20},
                                                   {awin::io::Key::kF21, ImGuiKey_F21},
                                                   {awin::io::Key::kF22, ImGuiKey_F22},
                                                   {awin::io::Key::kF23, ImGuiKey_F23},
                                                   {awin::io::Key::kF24, ImGuiKey_F24}};

        void updateMouseData();
        void updateMouseCursor();
    };
} // namespace uikit

#endif