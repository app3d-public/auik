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
            detail::on_resize_event(event.position);
        });
        ed.bind_event(backend, awin::event_id::focus,
                      [](const awin::FocusEvent &) { std::printf("[auik::v2::awin] focus event\n"); });
        ed.bind_event(backend, awin::event_id::char_input,
                      [](const awin::CharInputEvent &) { std::printf("[auik::v2::awin] char_input event\n"); });
        ed.bind_event(backend, awin::event_id::key_input,
                      [](const awin::KeyInputEvent &) { std::printf("[auik::v2::awin] key_input event\n"); });
        ed.bind_event(backend, awin::event_id::scroll,
                      [](const awin::ScrollEvent &) { std::printf("[auik::v2::awin] scroll event\n"); });
        ed.bind_event(backend, awin::event_id::dpi_changed,
                      [](const awin::DpiChangedEvent &) { std::printf("[auik::v2::awin] dpi_changed event\n"); });
        ed.bind_event(backend, awin::event_id::minimize,
                      [](const awin::StateEvent &) { std::printf("[auik::v2::awin] minimize event\n"); });
        ed.bind_event(backend, awin::event_id::maximize,
                      [](const awin::StateEvent &) { std::printf("[auik::v2::awin] maximize event\n"); });
        ed.bind_event(backend, awin::event_id::move,
                      [](const awin::PosEvent &) { std::printf("[auik::v2::awin] move event\n"); });
    }

    detail::WindowContext *create_awin_backend(awin::Window &window, acul::events::dispatcher &ed)
    {
        detail::AwinBackend *ctx = acul::alloc<detail::AwinBackend>(window);
        auto *cursors = ctx->cursors;
        cursors[detail::CursorID::arrow] = awin::Cursor::create(awin::Cursor::Type::arrow);
        cursors[detail::CursorID::ibeam] = awin::Cursor::create(awin::Cursor::Type::ibeam);
        cursors[detail::CursorID::resize_ew] = awin::Cursor::create(awin::Cursor::Type::resize_ew);
        cursors[detail::CursorID::resize_ns] = awin::Cursor::create(awin::Cursor::Type::resize_ns);
        cursors[detail::CursorID::resize_nwse] = awin::Cursor::create(awin::Cursor::Type::resize_nwse);
        cursors[detail::CursorID::resize_nesw] = awin::Cursor::create(awin::Cursor::Type::resize_nesw);
        ctx->set_cursor = &set_window_cursor;
        ctx->new_frame = &window_new_frame;
        ctx->destroy_backend = &destroy_window_backend;
        bind_window_events(window, ed, ctx);
        return ctx;
    }
} // namespace auik::v2
