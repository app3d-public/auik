#pragma once

#include <acul/api.hpp>
#include <acul/pair.hpp>
#include <acul/scalars.hpp>
#include <awin/awin.hpp>
#include "../../detail/events.hpp"

namespace auik::v2
{
    struct CustomTitlebarPadding
    {
        i32 left = 0;
        i32 top = 0;
        i32 right = 0;
        i32 bottom = 0;

        i32 width() const { return left + right; }
        i32 height() const { return top + bottom; }
    };

    namespace detail
    {
        struct AwinBackend final : WindowContext
        {
            awin::Window &window;
            awin::Cursor cursors[detail::CursorID::max];
            bool custom_titlebar_supported = false;
            CustomTitlebarPadding custom_titlebar_padding{};
            acul::point2D<i32> initial_display_size{};

            AwinBackend(awin::Window &window, acul::point2D<i32> initial_display_size = {})
                : window(window), initial_display_size(initial_display_size)
            {
            }
        };
    } // namespace detail

    APPLIB_API detail::WindowContext *create_awin_backend(awin::Window &window,
                                                          acul::point2D<i32> initial_display_size = {});
    APPLIB_API bool is_custom_titlebar_supported();
    APPLIB_API bool is_custom_titlebar_supported(const awin::Window &window);
    APPLIB_API CustomTitlebarPadding get_custom_titlebar_padding();
    APPLIB_API CustomTitlebarPadding get_custom_titlebar_padding(const awin::Window &window);

    inline void adjust_window_hints_by_titlebar_settings(awin::WindowFlags &flags)
    {
        flags |= awin::WindowFlagBits::extended_nc_area;
        flags &= ~awin::WindowFlagBits::decorated;
    }

} // namespace auik::v2
