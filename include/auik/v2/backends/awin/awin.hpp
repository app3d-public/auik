#pragma once

#include <acul/api.hpp>
#include <acul/scalars.hpp>
#include <awin/awin.hpp>
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

    APPLIB_API detail::WindowContext *create_awin_backend(awin::Window &window);
    inline void adjust_window_hints_by_titlebar_settings(awin::WindowFlags &flags)
    {
        flags |= awin::WindowFlagBits::extended_nc_area;
        flags &= ~awin::WindowFlagBits::decorated;
    }

} // namespace auik::v2
