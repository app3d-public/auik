#pragma once
#include "../widgets/widget.hpp"

namespace auik::detail
{
    void register_root_widget(Widget *widget, DepthZone zone);
    void update_registered_depth(Widget *widget);
    void unregister_root_widget(Widget *widget);
    AUIK_EXPORT void dismiss_owned_windows(Widget *owner);
} // namespace auik::detail
