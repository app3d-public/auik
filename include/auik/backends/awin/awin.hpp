#pragma once

#include <acul/pair.hpp>
#include <acul/event.hpp>
#include <acul/scalars.hpp>
#include <awin/awin.hpp>
#include "../../detail/events.hpp"

namespace auik
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
            acul::events::dispatcher &event_dispatcher;
            awin::Cursor cursors[detail::CursorID::max];
            CustomTitlebarPadding custom_titlebar_padding{};
            acul::point2D<i32> initial_display_size{};

            AwinBackend(awin::Window &window, acul::events::dispatcher &event_dispatcher,
                        acul::point2D<i32> initial_display_size = {})
                : window(window), event_dispatcher(event_dispatcher), initial_display_size(initial_display_size)
            {
            }
        };
    } // namespace detail

    AUIK_EXPORT detail::WindowContext *create_awin_backend(awin::Window &window,
                                                           acul::events::dispatcher &event_dispatcher,
                                                           acul::point2D<i32> initial_display_size = {});
    AUIK_EXPORT bool is_custom_titlebar_supported(const awin::Window &window);
    AUIK_EXPORT CustomTitlebarPadding get_custom_titlebar_padding();
    AUIK_EXPORT CustomTitlebarPadding get_custom_titlebar_padding(const awin::Window &window);

    inline void adjust_window_hints_by_titlebar_settings(awin::WindowFlags &flags)
    {
        flags |= awin::WindowFlagBits::extended_nc_area;
        flags &= ~awin::WindowFlagBits::decorated;
    }

} // namespace auik
