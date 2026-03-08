#include <auik/v2/auik.hpp>
#include <auik/v2/backends/awin/awin.hpp>
#include <auik/v2/detail/context.hpp>

namespace auik::v2
{
    static void set_window_cursor(detail::CursorID::enum_type id, detail::WindowContext *window_ctx)
    {
        auto *backend = static_cast<detail::AwinBackend *>(window_ctx);
        auto &window = backend->window;
        window.set_cursor(backend->cursors + id);
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
        ed.bind_event(backend, awin::event_id::resize, [&window](const awin::PosEvent &event) {
            if (event.window != &window) return;
            detail::on_resize_event({event.position.x, event.position.y});
        });
        ed.bind_event(backend, awin::event_id::mouse_move, [&window](const awin::PosEvent &event) {
            if (event.window != &window) return;
            detail::on_mouse_move_event({event.position.x, event.position.y});
            if (!window.is_cursor_hidden()) detail::on_drag_event({0.0f, 0.0f});
        });
        ed.bind_event(backend, awin::event_id::mouse_move_delta, [&window](const awin::PosEvent &event) {
            if (event.window != &window) return;
            if (window.is_cursor_hidden())
                detail::on_drag_event({static_cast<f32>(event.position.x), static_cast<f32>(event.position.y)});
        });
        ed.bind_event(backend, awin::event_id::focus, [&window](const awin::FocusEvent &event) {
            if (event.window != &window) return;
            if (!event.focused) detail::reset_event_state();
        });
        ed.bind_event(backend, awin::event_id::char_input,
                      [](const awin::CharInputEvent &) { std::printf("[auik::v2::awin] char_input event\n"); });
        ed.bind_event(backend, awin::event_id::key_input,
                      [](const awin::KeyInputEvent &) { std::printf("[auik::v2::awin] key_input event\n"); });
        ed.bind_event(backend, awin::event_id::scroll,
                      [](const awin::ScrollEvent &e) { detail::on_scroll_event({e.h, e.v}); });
        ed.bind_event(backend, awin::event_id::dpi_changed,
                      [](const awin::DpiChangedEvent &) { std::printf("[auik::v2::awin] dpi_changed event\n"); });
        ed.bind_event(backend, awin::event_id::minimize,
                      [](const awin::StateEvent &) { std::printf("[auik::v2::awin] minimize event\n"); });
        ed.bind_event(backend, awin::event_id::maximize,
                      [](const awin::StateEvent &) { std::printf("[auik::v2::awin] maximize event\n"); });
        ed.bind_event(backend, awin::event_id::move,
                      [](const awin::PosEvent &) { std::printf("[auik::v2::awin] move event\n"); });
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
        auto dimensions = awin_ctx->window.dimensions();
        global_ctx.io.display_size.x = dimensions.x;
        global_ctx.io.display_size.y = dimensions.y;
    }

    APPLIB_API detail::WindowContext *create_awin_backend(awin::Window &window)
    {
        detail::AwinBackend *ctx = acul::alloc<detail::AwinBackend>(window);
        ctx->set_cursor = &set_window_cursor;
        ctx->update_time = &window_new_frame;
        ctx->new_frame = &window_new_frame;
        ctx->construct_backend = &construct_window_backend;
        ctx->destroy_backend = &destroy_window_backend;
        return ctx;
    }
} // namespace auik::v2
