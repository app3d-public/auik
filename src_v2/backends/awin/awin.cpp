#include <auik/v2/auik.hpp>
#include <auik/v2/backends/awin/awin.hpp>
#include <auik/v2/detail/context.hpp>
#include <awin/native_access.hpp>

namespace auik::v2
{
#ifdef _WIN32
    static HICON resolve_window_icon(HWND hwnd)
    {
        HICON icon = reinterpret_cast<HICON>(SendMessageW(hwnd, WM_GETICON, ICON_SMALL2, 0));
        if (!icon) icon = reinterpret_cast<HICON>(SendMessageW(hwnd, WM_GETICON, ICON_SMALL, 0));
        if (!icon) icon = reinterpret_cast<HICON>(SendMessageW(hwnd, WM_GETICON, ICON_BIG, 0));
        if (!icon) icon = reinterpret_cast<HICON>(GetClassLongPtrW(hwnd, GCLP_HICONSM));
        if (!icon) icon = reinterpret_cast<HICON>(GetClassLongPtrW(hwnd, GCLP_HICON));
        return icon;
    }

    static bool get_window_icon_image(detail::WindowContext *window_ctx, umbf::Image2D &out)
    {
        auto *backend = static_cast<detail::AwinBackend *>(window_ctx);
        const HWND hwnd = awin::native_access::get_hwnd(backend->window);
        if (!hwnd) return false;

        const HICON icon = resolve_window_icon(hwnd);
        if (!icon) return false;

        ICONINFO icon_info{};
        if (!GetIconInfo(icon, &icon_info)) return false;

        BITMAP color_bitmap{};
        if (!GetObjectW(icon_info.hbmColor ? icon_info.hbmColor : icon_info.hbmMask, sizeof(color_bitmap),
                        &color_bitmap))
        {
            if (icon_info.hbmColor) DeleteObject(icon_info.hbmColor);
            if (icon_info.hbmMask) DeleteObject(icon_info.hbmMask);
            return false;
        }

        const i32 width = color_bitmap.bmWidth;
        const i32 height = icon_info.hbmColor ? color_bitmap.bmHeight : color_bitmap.bmHeight / 2;
        if (width <= 0 || height <= 0)
        {
            if (icon_info.hbmColor) DeleteObject(icon_info.hbmColor);
            if (icon_info.hbmMask) DeleteObject(icon_info.hbmMask);
            return false;
        }

        BITMAPINFO bmi{};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = width;
        bmi.bmiHeader.biHeight = -height;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        const size_t pixel_count = static_cast<size_t>(width) * static_cast<size_t>(height);
        auto *bgra_pixels = acul::alloc_n<u32>(pixel_count);
        HDC dc = GetDC(nullptr);
        const int copied = GetDIBits(dc, icon_info.hbmColor ? icon_info.hbmColor : icon_info.hbmMask, 0,
                                     static_cast<UINT>(height), bgra_pixels, &bmi, DIB_RGB_COLORS);
        ReleaseDC(nullptr, dc);
        if (copied == 0)
        {
            acul::release(bgra_pixels);
            if (icon_info.hbmColor) DeleteObject(icon_info.hbmColor);
            if (icon_info.hbmMask) DeleteObject(icon_info.hbmMask);
            return false;
        }

        out.width = static_cast<u32>(width);
        out.height = static_cast<u32>(height);
        out.format = {umbf::ImageFormat::Type::uint, 1};
        out.channels = {"r", "g", "b", "a"};
        out.pixels = acul::alloc_n<std::byte>(pixel_count * 4u);
        auto *dst = static_cast<u8 *>(out.pixels);
        for (size_t i = 0; i < pixel_count; ++i)
        {
            const u32 pixel = bgra_pixels[i];
            const u8 b = static_cast<u8>(pixel & 0xFFu);
            const u8 g = static_cast<u8>((pixel >> 8) & 0xFFu);
            const u8 r = static_cast<u8>((pixel >> 16) & 0xFFu);
            const u8 a = static_cast<u8>((pixel >> 24) & 0xFFu);
            dst[i * 4u + 0u] = r;
            dst[i * 4u + 1u] = g;
            dst[i * 4u + 2u] = b;
            dst[i * 4u + 3u] = a;
        }

        acul::release(bgra_pixels);
        if (icon_info.hbmColor) DeleteObject(icon_info.hbmColor);
        if (icon_info.hbmMask) DeleteObject(icon_info.hbmMask);
        return out.pixels != nullptr;
    }
#endif

    static inline HostWindowState resolve_host_window_state(const awin::Window &window)
    {
        if (window.fullscreen()) return HostWindowState::fullscreen;
        if (window.minimized()) return HostWindowState::minimized;
        if (window.maximized()) return HostWindowState::maximized;
        return HostWindowState::normal;
    }

    static void *get_window_handle(detail::WindowContext *window_ctx)
    {
        auto *backend = static_cast<detail::AwinBackend *>(window_ctx);
#ifdef _WIN32
        return awin::native_access::get_hwnd(backend->window);
#else
        int backend = native_access::get_backend_type();
        if (backend == AWIN_BACKEND_X11) return awin::native_access::get_x11_window_handle(backend->window);
        else if (backend == AWIN_BACKEND_WAYLAND)
            return awin::native_access::get_wayland_window_handle(backend->window);
        else return nullptr;
#endif
    }

    static void set_window_cursor(detail::CursorID::enum_type id, detail::WindowContext *window_ctx)
    {
        auto *backend = static_cast<detail::AwinBackend *>(window_ctx);
        auto &window = backend->window;
        window.set_cursor(backend->cursors + id);
    }

    static acul::string get_clipboard_string(detail::WindowContext *window_ctx)
    {
        auto *backend = static_cast<detail::AwinBackend *>(window_ctx);
        return awin::get_clipboard_string(backend->window);
    }

    static void set_clipboard_string(detail::WindowContext *window_ctx, const acul::string &text)
    {
        auto *backend = static_cast<detail::AwinBackend *>(window_ctx);
        awin::set_clipboard_string(backend->window, text);
    }

    static void destroy_window_backend(detail::WindowContext *window_ctx)
    {
        auto *backend = static_cast<detail::AwinBackend *>(window_ctx);
        for (int i = 0; i < detail::CursorID::max; i++) backend->cursors[i].reset();
        auto *ed = detail::get_context().ed;
        ed->unbind_listeners(backend);
        acul::release(backend);
    }

    static void window_new_frame(detail::WindowContext *window_ctx) { window_ctx->time = awin::get_time(); }

    static void bind_window_events(awin::Window &window, acul::events::dispatcher &ed, void *backend)
    {
        ed.bind_event(
            backend, awin::event_id::resize,
            [&window](const awin::PosEvent &event) {
                if (event.window != &window) return;
                auto &ctx = detail::get_context();
                const HostWindowState next_state = resolve_host_window_state(window);
                if (ctx.window_ctx->host_state != next_state) ctx.window_ctx->host_state = next_state;
                if (event.position.x <= 0 || event.position.y <= 0) return;
                ctx.io.display_size = {event.position.x, event.position.y};
                auto *pf = ctx.pending_filter;
                if (pf && !pf->allow()) pf->set(PendingMaskBits::resize);
                else ctx.dirty_flags |= DirtyFlagBits::layout;
            },
            4);
        ed.bind_event(backend, awin::event_id::mouse_move, [&window](const awin::PosEvent &event) {
            if (event.window != &window) return;
            auto &ctx = detail::get_context();
            ctx.io.mouse_pos = {event.position.x, event.position.y};
            if (ctx.raw_mouse_mode) return;
            auto *pending_filter = ctx.pending_filter;
            if (pending_filter && !pending_filter->allow()) pending_filter->set(PendingMaskBits::mouse_move);
            detail::on_mouse_move({0.0f, 0.0f});
        });
        ed.bind_event(backend, awin::event_id::mouse_move_delta, [&window](const awin::PosEvent &event) {
            if (event.window != &window) return;
            auto &ctx = detail::get_context();
            if (ctx.raw_mouse_mode) detail::on_mouse_move({event.position.x, event.position.y});
        });
        ed.bind_event(backend, awin::event_id::focus, [&window](const awin::FocusEvent &event) {
            if (event.window != &window) return;
            auto &ctx = detail::get_context();
            detail::update_window_time(ctx.window_ctx);
            if (!event.focused)
            {
                pause_delayed_tasks(ctx.window_ctx->time);
                detail::reset_event_state();
                return;
            }
            resume_delayed_tasks(ctx.window_ctx->time);
            detail::mark_host_refresh_request();
        });
        ed.bind_event(backend, awin::event_id::char_input, [&window](const awin::CharInputEvent &e) {
            if (e.window != &window) return;
            detail::on_char_event(e.char_code);
        });
        ed.bind_event(backend, awin::event_id::key_input, [&window](const awin::KeyInputEvent &e) {
            if (e.window != &window) return;
            detail::on_key_event(static_cast<Key>(e.key), static_cast<KeyPressState>(e.action),
                                 static_cast<KeyModeBits::enum_type>(static_cast<i8>(e.mods)));
        });
        ed.bind_event(backend, awin::event_id::scroll, [&window](const awin::ScrollEvent &e) {
            if (e.window != &window) return;
            detail::on_scroll_event({e.h, e.v});
        });
        ed.bind_event(backend, awin::event_id::minimize, [&window](const awin::StateEvent &event) {
            if (event.window != &window) return;
            auto &ctx = detail::get_context();
            const HostWindowState next_state = resolve_host_window_state(window);
            if (ctx.window_ctx->host_state == next_state) return;
            ctx.window_ctx->host_state = next_state;
        });
        ed.bind_event(backend, awin::event_id::maximize, [&window](const awin::StateEvent &event) {
            if (event.window != &window) return;
            auto &ctx = detail::get_context();
            const HostWindowState next_state = resolve_host_window_state(window);
            const HostWindowState prev_state = ctx.window_ctx->host_state;
            if (prev_state == next_state) return;
            ctx.window_ctx->host_state = next_state;
            detail::mark_host_refresh_request();
            ctx.dirty_flags |= DirtyFlagBits::layout;
        });
        ed.bind_event(backend, awin::event_id::mouse_click, [&window](const awin::MouseClickEvent &event) {
            if (event.window != &window) return;
            detail::on_mouse_click_event(static_cast<MouseKey>(event.button), static_cast<KeyPressState>(event.action));
        });
    }

    static void construct_window_backend(detail::WindowContext *window_ctx)
    {
        auto *awin_ctx = static_cast<detail::AwinBackend *>(window_ctx);
        auto *cursors = awin_ctx->cursors;
        cursors[detail::CursorID::arrow] = awin::Cursor::create(awin::Cursor::Type::arrow);
        cursors[detail::CursorID::ibeam] = awin::Cursor::create(awin::Cursor::Type::ibeam);
        cursors[detail::CursorID::resize_ew] = awin::Cursor::create(awin::Cursor::Type::resize_ew);
        cursors[detail::CursorID::resize_ns] = awin::Cursor::create(awin::Cursor::Type::resize_ns);
        cursors[detail::CursorID::resize_nwse] = awin::Cursor::create(awin::Cursor::Type::resize_nwse);
        cursors[detail::CursorID::resize_nesw] = awin::Cursor::create(awin::Cursor::Type::resize_nesw);
        auto &global_ctx = detail::get_context();
        bind_window_events(awin_ctx->window, *global_ctx.ed, awin_ctx);
        window_ctx->host_state = resolve_host_window_state(awin_ctx->window);
        auto dimensions = awin_ctx->window.dimensions();
        global_ctx.io.display_size.x = dimensions.x;
        global_ctx.io.display_size.y = dimensions.y;
    }

    APPLIB_API detail::WindowContext *create_awin_backend(awin::Window &window)
    {
        detail::AwinBackend *ctx = acul::alloc<detail::AwinBackend>(window);
        ctx->get_window_handle = &get_window_handle;
        ctx->set_cursor = &set_window_cursor;
        ctx->get_clipboard_string = &get_clipboard_string;
        ctx->set_clipboard_string = &set_clipboard_string;
        ctx->update_time = &window_new_frame;
        ctx->new_frame = &window_new_frame;
        ctx->construct_backend = &construct_window_backend;
        ctx->destroy_backend = &destroy_window_backend;
#ifdef _WIN32
        ctx->get_window_icon_image = &get_window_icon_image;
#endif
        return ctx;
    }
} // namespace auik::v2
