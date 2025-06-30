#include <acul/log.hpp>
#include <implot/implot.h>
#include <uikit/integration/window.hpp>
#include <uikit/widget.hpp>

namespace uikit
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

    static void imgui_set_clipboard_text(void *user_data, const char *text)
    {
        awin::set_clipboard_string(*(awin::Window *)user_data, text);
    }

    static const char *imgui_get_clipboard_string(void *user_data)
    {
        awin::get_clipboard_string(*(awin::Window *)user_data);
        return awin::platform::env.clipboard_data.c_str();
    }

#ifdef _WIN32
    [[maybe_unused]] static LRESULT CALLBACK imgui_wnd_proc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

    WindowImGuiBinder::WindowImGuiBinder(awin::Window &window, acul::events::dispatcher *ed) : ed(ed)
    {
        ImGui::CreateContext();
        ImPlot::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        IM_ASSERT(io.BackendPlatformUserData == nullptr && "Already initialized a platform backend!");

        // Setup backend capabilities flags
        _bd = IM_NEW(ImGuiBackendData)();
        io.BackendPlatformUserData = (void *)_bd;
        io.BackendPlatformName = "imgui_impl_awin";
        io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
        io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;

        _bd->window = &window;
        _bd->time = 0.0;

        io.SetClipboardTextFn = imgui_set_clipboard_text;
        io.GetClipboardTextFn = imgui_get_clipboard_string;
        io.ClipboardUserData = _bd->window;

        _bd->mouse_cursors[ImGuiMouseCursor_Arrow] = awin::Cursor::create(awin::Cursor::Type::Arrow);
        _bd->mouse_cursors[ImGuiMouseCursor_TextInput] = awin::Cursor::create(awin::Cursor::Type::Ibeam);
        _bd->mouse_cursors[ImGuiMouseCursor_ResizeNS] = awin::Cursor::create(awin::Cursor::Type::ResizeNS);
        _bd->mouse_cursors[ImGuiMouseCursor_ResizeEW] = awin::Cursor::create(awin::Cursor::Type::ResizeEW);
        _bd->mouse_cursors[ImGuiMouseCursor_Hand] = awin::Cursor::create(awin::Cursor::Type::Hand);
        _bd->mouse_cursors[ImGuiMouseCursor_ResizeAll] = awin::Cursor::create(awin::Cursor::Type::ResizeAll);
        _bd->mouse_cursors[ImGuiMouseCursor_ResizeNESW] = awin::Cursor::create(awin::Cursor::Type::ResizeNESW);
        _bd->mouse_cursors[ImGuiMouseCursor_ResizeNWSE] = awin::Cursor::create(awin::Cursor::Type::ResizeNWSE);
        _bd->mouse_cursors[ImGuiMouseCursor_NotAllowed] = awin::Cursor::create(awin::Cursor::Type::NotAllowed);

        // Set platform dependent data in viewport
        ImGuiViewport *main_viewport = ImGui::GetMainViewport();
#ifdef _WIN32
        main_viewport->PlatformHandleRaw = awin::platform::native_access::get_hwnd(*_bd->window);
#elif defined(__APPLE__)
        main_viewport->PlatformHandleRaw = (void *)awin::platform::native_access::get_uinmpl(bd->window);
#else
        IM_UNUSED(main_viewport);
#endif

        // Windows: register a WndProc hook so we can intercept some messages.
#ifdef _WIN32
        _bd->prev_wnd_proc = (WNDPROC)::GetWindowLongPtrW((HWND)main_viewport->PlatformHandleRaw, GWLP_WNDPROC);
        IM_ASSERT(_bd->prev_wnd_proc != nullptr);
        ::SetWindowLongPtrW((HWND)main_viewport->PlatformHandleRaw, GWLP_WNDPROC, (LONG_PTR)imgui_wnd_proc);
#endif
    }

    void WindowImGuiBinder::new_frame()
    {
        ImGuiIO &io = ImGui::GetIO();
        IM_ASSERT(_bd != nullptr);

        // Setup display size (every frame to accommodate for window resizing)
        acul::point2D<i32> dimensions = _bd->window->dimensions();
        io.DisplaySize = ImVec2((f32)dimensions.x, (f32)dimensions.y);
        if (dimensions.x > 0 && dimensions.y > 0) io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);

        // Setup time step
        f64 current_time = awin::get_time();
        if (current_time <= _bd->time) current_time = _bd->time + 0.00001;
        io.DeltaTime = _bd->time > 0.0 ? (f64)(current_time - _bd->time) : (f64)(1.0 / 60.0);
        _bd->time = current_time;

        update_mouse_data();
        update_mouse_cursor();
    }

    void WindowImGuiBinder::update_mouse_data()
    {
        ImGuiIO &io = ImGui::GetIO();
        if (_bd->window->focused() && io.WantSetMousePos)
            _bd->window->cursor_position({static_cast<i32>(io.MousePos.x), static_cast<i32>(io.MousePos.y)});
    }

    void WindowImGuiBinder::update_mouse_cursor()
    {
        ImGuiIO &io = ImGui::GetIO();
        if ((io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange)) return;

        ImGuiMouseCursor imgui_cursor = ImGui::GetMouseCursor();
        g_last_cursor = imgui_cursor;
        {
            awin::Window *window = _bd->window;
            if (imgui_cursor == ImGuiMouseCursor_None || io.MouseDrawCursor)
                window->hide_cursor();
            else if (!window->is_cursor_hidden())
            {
                window->set_cursor(_bd->mouse_cursors[imgui_cursor].valid()
                                       ? &_bd->mouse_cursors[imgui_cursor]
                                       : &_bd->mouse_cursors[ImGuiMouseCursor_Arrow]);
                window->show_cursor();
            }
        }
    }

    void update_key_mods(ImGuiIO &io, const awin::io::KeyMode &mods)
    {
        io.AddKeyEvent(ImGuiMod_Ctrl, mods & awin::io::KeyModeBits::Control);
        io.AddKeyEvent(ImGuiMod_Shift, mods & awin::io::KeyModeBits::Shift);
        io.AddKeyEvent(ImGuiMod_Alt, mods & awin::io::KeyModeBits::Alt);
        io.AddKeyEvent(ImGuiMod_Super, mods & awin::io::KeyModeBits::Super);
    }

    void WindowImGuiBinder::bind_events()
    {
        LOG_INFO("Binding ImGui listeners");
        ed->bind_event(this, awin::event_id::Focus, [](const awin::FocusEvent &event) {
            ImGuiIO &io = ImGui::GetIO();
            io.AddFocusEvent(event.focused);
        });
        ed->bind_event(this, awin::event_id::MouseEnter, [this](const awin::MouseEnterEvent &event) {
            ImGuiIO &io = ImGui::GetIO();
            if (event.entered)
                io.AddMousePosEvent(_bd->last_valid_mouse_pos.x, _bd->last_valid_mouse_pos.y);
            else
            {
                _bd->last_valid_mouse_pos = io.MousePos;
                io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
            }
        });
        ed->bind_event(this, awin::event_id::MouseMoveAbs, [this](const awin::PosEvent &event) {
            ImGuiIO &io = ImGui::GetIO();
            io.AddMousePosEvent(event.position.x, event.position.y);
            _bd->last_valid_mouse_pos = ImVec2(event.position.x, event.position.y);
        });
        ed->bind_event(this, awin::event_id::MouseClick, [](const awin::MouseClickEvent &event) {
            ImGuiIO &io = ImGui::GetIO();
            auto button = event.button;
            if (button != awin::io::MouseKey::Unknown)
                io.AddMouseButtonEvent(+button, event.action == awin::io::KeyPressState::Press);
        });
        ed->bind_event(this, awin::event_id::Scroll, [](const awin::ScrollEvent &event) {
            ImGuiIO &io = ImGui::GetIO();
            io.AddMouseWheelEvent(event.h, event.v);
        });
        ed->bind_event(this, awin::event_id::KeyInput, [this](const awin::KeyInputEvent &event) {
            ImGuiIO &io = ImGui::GetIO();
            update_key_mods(io, event.mods);
            auto it = _key_map.find(event.key);
            ImGuiKey imgui_key = it != _key_map.end() ? it->second : ImGuiKey_None;
            io.AddKeyEvent(imgui_key, event.action != awin::io::KeyPressState::Release);
        });
        ed->bind_event(this, awin::event_id::CharInput, [](const awin::CharInputEvent &event) {
            ImGuiIO &io = ImGui::GetIO();
            io.AddInputCharacter(event.charCode);
        });
    }

#ifdef _WIN32
    static ImGuiMouseSource GetMouseSourceFromMessageExtraInfo()
    {
        LPARAM extra_info = ::GetMessageExtraInfo();
        if ((extra_info & 0xFFFFFF80) == 0xFF515700) return ImGuiMouseSource_Pen;
        if ((extra_info & 0xFFFFFF80) == 0xFF515780) return ImGuiMouseSource_TouchScreen;
        return ImGuiMouseSource_Mouse;
    }

    LRESULT CALLBACK imgui_wnd_proc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
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
        return ::CallWindowProcW(bd->prev_wnd_proc, hWnd, msg, wParam, lParam);
    }
#endif

    WindowImGuiBinder::~WindowImGuiBinder()
    {
        LOG_INFO("Destroying ImGui context");
        IM_ASSERT(_bd != nullptr && "No platform backend to shutdown, or already shutdown?");
        ImGuiIO &io = ImGui::GetIO();

        for (ImGuiMouseCursor cursor_n = 0; cursor_n < ImGuiMouseCursor_COUNT; cursor_n++)
            _bd->mouse_cursors[cursor_n].reset();

#ifdef _WIN32
        ImGuiViewport *main_viewport = ImGui::GetMainViewport();
        ::SetWindowLongPtrW((HWND)main_viewport->PlatformHandleRaw, GWLP_WNDPROC, (LONG_PTR)_bd->prev_wnd_proc);
        _bd->prev_wnd_proc = nullptr;
#endif

        io.BackendPlatformName = nullptr;
        io.BackendPlatformUserData = nullptr;
        io.BackendFlags &=
            ~(ImGuiBackendFlags_HasMouseCursors | ImGuiBackendFlags_HasSetMousePos | ImGuiBackendFlags_HasGamepad);
        IM_DELETE(_bd);
        ImPlot::DestroyContext();
        ImGui::DestroyContext();
    }
} // namespace uikit