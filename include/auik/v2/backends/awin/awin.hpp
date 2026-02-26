#pragma once

#include <acul/api.hpp>
#include <acul/scalars.hpp>
#include <awin/window.hpp>
#include "../../detail/events.hpp"

namespace auik::v2
{
    namespace detail
    {
        struct AwinBackend final : WindowContext
        {
            awin::Window &window;
            awin::Cursor cursors[detail::CursorID::max];

            AwinBackend(awin::Window &window) : window(window) {}
        };
    } // namespace detail

    APPLIB_API detail::WindowContext *create_awin_backend(awin::Window &window, acul::events::dispatcher &ed);
} // namespace auik::v2
