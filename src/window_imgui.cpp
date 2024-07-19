#include <core/log.hpp>
#include <uikit/window_imgui.hpp>

#ifdef _WIN32
    #include <window/platform_win32.hpp>
#endif

namespace uikit
{
    static void ImGuiSetClipboardText(void *user_data, const char *text)
    {
        window::setClipboardString(*(window::Window *)user_data, text);
    }

    static const char *ImGuiGetClipboardText(void *user_data)
    {
        window::getClipboardString(*(window::Window *)user_data);
        return window::platform::env.clipboardData.c_str();
    }

    WindowImGuiBinder::WindowImGuiBinder(window::Window &window)
    {
        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        IM_ASSERT(io.BackendPlatformUserData == nullptr && "Already initialized a platform backend!");

        // Setup backend capabilities flags
        _bd = IM_NEW(ImGuiBackendData)();
        io.BackendPlatformUserData = (void *)_bd;
        io.BackendPlatformName = "imgui_impl_windowA3D";
        io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
        io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;

        _bd->Window = &window;
        _bd->Time = 0.0;

        io.SetClipboardTextFn = ImGuiSetClipboardText;
        io.GetClipboardTextFn = ImGuiGetClipboardText;
        io.ClipboardUserData = _bd->Window;

        _bd->MouseCursors[ImGuiMouseCursor_Arrow] = window::Cursor::create(window::Cursor::Type::arrow);
        _bd->MouseCursors[ImGuiMouseCursor_TextInput] = window::Cursor::create(window::Cursor::Type::ibeam);
        _bd->MouseCursors[ImGuiMouseCursor_ResizeNS] = window::Cursor::create(window::Cursor::Type::resizeNS);
        _bd->MouseCursors[ImGuiMouseCursor_ResizeEW] = window::Cursor::create(window::Cursor::Type::resizeEW);
        _bd->MouseCursors[ImGuiMouseCursor_Hand] = window::Cursor::create(window::Cursor::Type::hand);
        _bd->MouseCursors[ImGuiMouseCursor_ResizeAll] = window::Cursor::create(window::Cursor::Type::resizeAll);
        _bd->MouseCursors[ImGuiMouseCursor_ResizeNESW] = window::Cursor::create(window::Cursor::Type::resizeNESW);
        _bd->MouseCursors[ImGuiMouseCursor_ResizeNWSE] = window::Cursor::create(window::Cursor::Type::resizeNWSE);
        _bd->MouseCursors[ImGuiMouseCursor_NotAllowed] = window::Cursor::create(window::Cursor::Type::notAllowed);

        // Set platform dependent data in viewport
        ImGuiViewport *main_viewport = ImGui::GetMainViewport();
#ifdef _WIN32
        main_viewport->PlatformHandleRaw = _bd->Window->accessBridge().hwnd();
#elif defined(__APPLE__)
        main_viewport->PlatformHandleRaw = (void *)glfwGetCocoaWindow(bd->Window);
#else
        IM_UNUSED(main_viewport);
#endif

        // Windows: register a WndProc hook so we can intercept some messages.
#ifdef _WIN32
        _bd->WndProc = (WNDPROC)::GetWindowLongPtrW((HWND)main_viewport->PlatformHandleRaw, GWLP_WNDPROC);
        IM_ASSERT(_bd->WndProc != nullptr);
        ::SetWindowLongPtrW((HWND)main_viewport->PlatformHandleRaw, GWLP_WNDPROC, (LONG_PTR)ImGuiWndProc);
#endif
    }

    void WindowImGuiBinder::newFrame()
    {
        ImGuiIO &io = ImGui::GetIO();
        IM_ASSERT(_bd != nullptr);

        // Setup display size (every frame to accommodate for window resizing)
        Point2D dimensions = _bd->Window->dimensions();
        io.DisplaySize = ImVec2((f32)dimensions.x, (f32)dimensions.y);
        if (dimensions.x > 0 && dimensions.y > 0) io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);

        // Setup time step
        f64 current_time = window::getTime();
        if (current_time <= _bd->Time) current_time = _bd->Time + 0.00001;
        io.DeltaTime = _bd->Time > 0.0 ? (f64)(current_time - _bd->Time) : (f64)(1.0 / 60.0);
        _bd->Time = current_time;

        updateMouseData();
        updateMouseCursor();
    }

    void WindowImGuiBinder::updateMouseData()
    {
        ImGuiIO &io = ImGui::GetIO();
        if (_bd->Window->focused())
        {
            if (io.WantSetMousePos)
                _bd->Window->cursorPosition({static_cast<i32>(io.MousePos.x), static_cast<i32>(io.MousePos.y)});
        }
    }

    void WindowImGuiBinder::updateMouseCursor()
    {
        ImGuiIO &io = ImGui::GetIO();
        if ((io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange)) return;

        ImGuiMouseCursor imgui_cursor = ImGui::GetMouseCursor();
        {
            window::Window *window = _bd->Window;
            if (imgui_cursor == ImGuiMouseCursor_None || io.MouseDrawCursor)
                window->hideCursor();
            else if (!window->isCursorHidden())
            {
                window->setCursor(_bd->MouseCursors[imgui_cursor].valid() ? &_bd->MouseCursors[imgui_cursor]
                                                                          : &_bd->MouseCursors[ImGuiMouseCursor_Arrow]);
                window->showCursor();
            }
        }
    }

    void updateKeyMods(ImGuiIO &io, const window::io::KeyMode &mods)
    {
        io.AddKeyEvent(ImGuiMod_Ctrl, mods & window::io::KeyModeBits::control);
        io.AddKeyEvent(ImGuiMod_Shift, mods & window::io::KeyModeBits::shift);
        io.AddKeyEvent(ImGuiMod_Alt, mods & window::io::KeyModeBits::alt);
        io.AddKeyEvent(ImGuiMod_Super, mods & window::io::KeyModeBits::super);
    }

    void WindowImGuiBinder::bindEvents()
    {
        logInfo("Binding ImGui listeners");
        events::bindEvent<window::FocusEvent>(this, "window:focus", [](const window::FocusEvent &event) {
            ImGuiIO &io = ImGui::GetIO();
            io.AddFocusEvent(event.focused);
        });
        events::bindEvent<window::CursorEnterEvent>(
            this, "window:cursor:enter", [this](const window::CursorEnterEvent &event) {
                ImGuiIO &io = ImGui::GetIO();
                if (event.entered)
                    io.AddMousePosEvent(_bd->LastValidMousePos.x, _bd->LastValidMousePos.y);
                else
                {
                    _bd->LastValidMousePos = io.MousePos;
                    io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
                }
            });
        events::bindEvent<window::PosEvent>(this, "window:cursor:move:abs", [this](const window::PosEvent &event) {
            ImGuiIO &io = ImGui::GetIO();
            io.AddMousePosEvent(event.position.x, event.position.y);
            _bd->LastValidMousePos = ImVec2(event.position.x, event.position.y);
        });
        events::bindEvent<window::MouseClickEvent>(
            this, "window:input:mouse", [](const window::MouseClickEvent &event) {
                ImGuiIO &io = ImGui::GetIO();
                updateKeyMods(io, event.mods);
                auto button = event.button;
                if (button != window::io::MouseKey::unknown)
                    io.AddMouseButtonEvent(+button, event.action == window::io::KeyPressState::press);
            });
        events::bindEvent<window::ScrollEvent>(this, "window:scroll", [](const window::ScrollEvent &event) {
            ImGuiIO &io = ImGui::GetIO();
            io.AddMouseWheelEvent(event.h, event.v);
        });
        events::bindEvent<window::KeyInputEvent>(this, "window:input:key", [this](const window::KeyInputEvent &event) {
            ImGuiIO &io = ImGui::GetIO();
            updateKeyMods(io, event.mods);
            auto it = _keyMap.find(event.key);
            ImGuiKey imgui_key = it != _keyMap.end() ? it->second : ImGuiKey_None;
            io.AddKeyEvent(imgui_key, event.action != window::io::KeyPressState::release);
        });
        events::bindEvent<window::CharInputEvent>(this, "window:input:char", [](const window::CharInputEvent &event) {
            ImGuiIO &io = ImGui::GetIO();
            io.AddInputCharacter(event.charCode);
        });
    }

    namespace
    {
        static ImGuiBackendData *getBackendData()
        {
            return ImGui::GetCurrentContext() ? (ImGuiBackendData *)ImGui::GetIO().BackendPlatformUserData : nullptr;
        }

        static ImGuiMouseSource GetMouseSourceFromMessageExtraInfo()
        {
            LPARAM extra_info = ::GetMessageExtraInfo();
            if ((extra_info & 0xFFFFFF80) == 0xFF515700) return ImGuiMouseSource_Pen;
            if ((extra_info & 0xFFFFFF80) == 0xFF515780) return ImGuiMouseSource_TouchScreen;
            return ImGuiMouseSource_Mouse;
        }

        LRESULT CALLBACK ImGuiWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
        {
            ImGuiBackendData *bd = getBackendData();
            switch (msg)
            {
                case WM_MOUSEMOVE:
                case WM_NCMOUSEMOVE:
                case WM_LBUTTONDOWN:
                case WM_LBUTTONDBLCLK:
                case WM_LBUTTONUP:
                case WM_RBUTTONDOWN:
                case WM_RBUTTONDBLCLK:
                case WM_RBUTTONUP:
                case WM_MBUTTONDOWN:
                case WM_MBUTTONDBLCLK:
                case WM_MBUTTONUP:
                case WM_XBUTTONDOWN:
                case WM_XBUTTONDBLCLK:
                case WM_XBUTTONUP:
                    ImGui::GetIO().AddMouseSourceEvent(GetMouseSourceFromMessageExtraInfo());
                    break;
            }
            return ::CallWindowProcW(bd->WndProc, hWnd, msg, wParam, lParam);
        }
    } // namespace

    WindowImGuiBinder::~WindowImGuiBinder()
    {
        logInfo("Destroying ImGui context");
        IM_ASSERT(_bd != nullptr && "No platform backend to shutdown, or already shutdown?");
        ImGuiIO &io = ImGui::GetIO();

#ifdef _WIN32
        ImGuiViewport *main_viewport = ImGui::GetMainViewport();
        ::SetWindowLongPtrW((HWND)main_viewport->PlatformHandleRaw, GWLP_WNDPROC, (LONG_PTR)_bd->WndProc);
        _bd->WndProc = nullptr;
#endif

        io.BackendPlatformName = nullptr;
        io.BackendPlatformUserData = nullptr;
        io.BackendFlags &=
            ~(ImGuiBackendFlags_HasMouseCursors | ImGuiBackendFlags_HasSetMousePos | ImGuiBackendFlags_HasGamepad);
        IM_DELETE(_bd);
        ImGui::DestroyContext();
    }
} // namespace uikit