#pragma once

#include "widget.hpp"
#include "window.hpp"

#define AUIK_ID_POPUP_TIP 0x1E1CB209u

namespace auik
{
    // Creates an empty, hidden popup. The caller owns its contents; ownership of
    // the window is transferred to add_popup_tip_to_widget.
    AUIK_EXPORT Window *make_popup_tip_window(u32 id = AUIK_ID_POPUP_TIP);
    AUIK_EXPORT bool add_popup_tip_to_widget(Widget *widget, Window *window);
    AUIK_EXPORT void remove_popup_tip_from_widget(Widget *widget);
} // namespace auik
