#ifndef UIKIT_WINDOW_IMGUI_H
#define UIKIT_WINDOW_IMGUI_H

#include <imgui/imgui.h>
#include <map>
#include <window/window.hpp>
#include "window/types.hpp"
#ifdef _WIN32
    #include <windows.h>
#endif

namespace ui
{
    namespace
    {
        LRESULT CALLBACK ImGuiWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

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

    class WindowImGuiBinder : public events::ListenerRegistry
    {
    public:
        explicit WindowImGuiBinder(window::Window &window);

        ~WindowImGuiBinder();

        virtual void bindListeners() override;

        void newFrame();

    private:
        ImGuiBackendData *_bd;
        std::map<io::Key, ImGuiKey> _keyMap{{io::Key::kTab, ImGuiKey_Tab},
                                       {io::Key::kLeft, ImGuiKey_LeftArrow},
                                       {io::Key::kRight, ImGuiKey_RightArrow},
                                       {io::Key::kUp, ImGuiKey_UpArrow},
                                       {io::Key::kDown, ImGuiKey_DownArrow},
                                       {io::Key::kPageUp, ImGuiKey_PageUp},
                                       {io::Key::kDown, ImGuiKey_PageDown},
                                       {io::Key::kHome, ImGuiKey_Home},
                                       {io::Key::kEnd, ImGuiKey_End},
                                       {io::Key::kInsert, ImGuiKey_Insert},
                                       {io::Key::kDelete, ImGuiKey_Delete},
                                       {io::Key::kBackspace, ImGuiKey_Backspace},
                                       {io::Key::kSpace, ImGuiKey_Space},
                                       {io::Key::kEnter, ImGuiKey_Enter},
                                       {io::Key::kEscape, ImGuiKey_Escape},
                                       {io::Key::kApostroph, ImGuiKey_Apostrophe},
                                       {io::Key::kComma, ImGuiKey_Comma},
                                       {io::Key::kMinus, ImGuiKey_Minus},
                                       {io::Key::kPeriod, ImGuiKey_Period},
                                       {io::Key::kSlash, ImGuiKey_Slash},
                                       {io::Key::kSemicolon, ImGuiKey_Semicolon},
                                       {io::Key::kEqual, ImGuiKey_Equal},
                                       {io::Key::kLeftBrace, ImGuiKey_LeftBracket},
                                       {io::Key::kBackslash, ImGuiKey_Backslash},
                                       {io::Key::kRightBrace, ImGuiKey_RightBracket},
                                       {io::Key::kGraveAccent, ImGuiKey_GraveAccent},
                                       {io::Key::kCapsLock, ImGuiKey_CapsLock},
                                       {io::Key::kScrollLock, ImGuiKey_ScrollLock},
                                       {io::Key::kNumLock, ImGuiKey_NumLock},
                                       {io::Key::kPrintScreen, ImGuiKey_PrintScreen},
                                       {io::Key::kPause, ImGuiKey_Pause},
                                       {io::Key::kKP0, ImGuiKey_Keypad0},
                                       {io::Key::kKP1, ImGuiKey_Keypad1},
                                       {io::Key::kKP2, ImGuiKey_Keypad2},
                                       {io::Key::kKP3, ImGuiKey_Keypad3},
                                       {io::Key::kKP4, ImGuiKey_Keypad4},
                                       {io::Key::kKP5, ImGuiKey_Keypad5},
                                       {io::Key::kKP6, ImGuiKey_Keypad6},
                                       {io::Key::kKP7, ImGuiKey_Keypad7},
                                       {io::Key::kKP8, ImGuiKey_Keypad8},
                                       {io::Key::kKP9, ImGuiKey_Keypad9},
                                       {io::Key::kKPDecimal, ImGuiKey_KeypadDecimal},
                                       {io::Key::kKPDivide, ImGuiKey_KeypadDivide},
                                       {io::Key::kKPMultiply, ImGuiKey_KeypadMultiply},
                                       {io::Key::kKPSubtract, ImGuiKey_KeypadSubtract},
                                       {io::Key::kKPAdd, ImGuiKey_KeypadAdd},
                                       {io::Key::kKPEnter, ImGuiKey_KeypadEnter},
                                       {io::Key::kKPEqual, ImGuiKey_KeypadEqual},
                                       {io::Key::kLeftShift, ImGuiKey_LeftShift},
                                       {io::Key::kLeftControl, ImGuiKey_LeftCtrl},
                                       {io::Key::kLeftAlt, ImGuiKey_LeftAlt},
                                       {io::Key::kLeftSuper, ImGuiKey_LeftSuper},
                                       {io::Key::kRightShift, ImGuiKey_RightShift},
                                       {io::Key::kRightControl, ImGuiKey_RightCtrl},
                                       {io::Key::kRightAlt, ImGuiKey_RightAlt},
                                       {io::Key::kRightSuper, ImGuiKey_RightSuper},
                                       {io::Key::kMenu, ImGuiKey_Menu},
                                       {io::Key::k0, ImGuiKey_0},
                                       {io::Key::k1, ImGuiKey_1},
                                       {io::Key::k2, ImGuiKey_2},
                                       {io::Key::k3, ImGuiKey_3},
                                       {io::Key::k4, ImGuiKey_4},
                                       {io::Key::k5, ImGuiKey_5},
                                       {io::Key::k6, ImGuiKey_6},
                                       {io::Key::k7, ImGuiKey_7},
                                       {io::Key::k8, ImGuiKey_8},
                                       {io::Key::k9, ImGuiKey_9},
                                       {io::Key::kA, ImGuiKey_A},
                                       {io::Key::kB, ImGuiKey_B},
                                       {io::Key::kC, ImGuiKey_C},
                                       {io::Key::kD, ImGuiKey_D},
                                       {io::Key::kE, ImGuiKey_E},
                                       {io::Key::kF, ImGuiKey_F},
                                       {io::Key::kG, ImGuiKey_G},
                                       {io::Key::kH, ImGuiKey_H},
                                       {io::Key::kI, ImGuiKey_I},
                                       {io::Key::kJ, ImGuiKey_J},
                                       {io::Key::kK, ImGuiKey_K},
                                       {io::Key::kL, ImGuiKey_L},
                                       {io::Key::kM, ImGuiKey_M},
                                       {io::Key::kN, ImGuiKey_N},
                                       {io::Key::kO, ImGuiKey_O},
                                       {io::Key::kP, ImGuiKey_P},
                                       {io::Key::kQ, ImGuiKey_Q},
                                       {io::Key::kR, ImGuiKey_R},
                                       {io::Key::kS, ImGuiKey_S},
                                       {io::Key::kT, ImGuiKey_T},
                                       {io::Key::kU, ImGuiKey_U},
                                       {io::Key::kV, ImGuiKey_V},
                                       {io::Key::kW, ImGuiKey_W},
                                       {io::Key::kX, ImGuiKey_X},
                                       {io::Key::kY, ImGuiKey_Y},
                                       {io::Key::kZ, ImGuiKey_Z},
                                       {io::Key::kF1, ImGuiKey_F1},
                                       {io::Key::kF2, ImGuiKey_F2},
                                       {io::Key::kF3, ImGuiKey_F3},
                                       {io::Key::kF4, ImGuiKey_F4},
                                       {io::Key::kF5, ImGuiKey_F5},
                                       {io::Key::kF6, ImGuiKey_F6},
                                       {io::Key::kF7, ImGuiKey_F7},
                                       {io::Key::kF8, ImGuiKey_F8},
                                       {io::Key::kF9, ImGuiKey_F9},
                                       {io::Key::kF10, ImGuiKey_F10},
                                       {io::Key::kF11, ImGuiKey_F11},
                                       {io::Key::kF12, ImGuiKey_F12},
                                       {io::Key::kF13, ImGuiKey_F13},
                                       {io::Key::kF14, ImGuiKey_F14},
                                       {io::Key::kF15, ImGuiKey_F15},
                                       {io::Key::kF16, ImGuiKey_F16},
                                       {io::Key::kF17, ImGuiKey_F17},
                                       {io::Key::kF18, ImGuiKey_F18},
                                       {io::Key::kF19, ImGuiKey_F19},
                                       {io::Key::kF20, ImGuiKey_F20},
                                       {io::Key::kF21, ImGuiKey_F21},
                                       {io::Key::kF22, ImGuiKey_F22},
                                       {io::Key::kF23, ImGuiKey_F23},
                                       {io::Key::kF24, ImGuiKey_F24}};

        void updateMouseData();
        void updateMouseCursor();
    };
} // namespace ui

#endif