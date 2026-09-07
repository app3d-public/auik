#pragma once

#include <acul/scalars.hpp>
#include <amal/geometric.hpp>
#include <auik/symbol_export.h>

namespace auik
{
    class Widget;

    struct ScrollData
    {
        using PFN_scroll_to = void (*)(ScrollData &, amal::axis);

        Widget *widget = nullptr;
        amal::vec2 content_offset{0.0f};
        amal::vec2 content_size{0.0f};
        amal::vec2 view_size{0.0f};
        amal::vec2 max_offset{0.0f};
        PFN_scroll_to on_scroll_to = nullptr;

        f32 offset(amal::axis axis) const { return axis == amal::axis::x ? content_offset.x : content_offset.y; }
        f32 max_scroll(amal::axis axis) const { return axis == amal::axis::x ? max_offset.x : max_offset.y; }
        f32 content(amal::axis axis) const { return axis == amal::axis::x ? content_size.x : content_size.y; }
        f32 view(amal::axis axis) const { return axis == amal::axis::x ? view_size.x : view_size.y; }
        f32 normalized(amal::axis axis) const
        {
            const f32 maximum = max_scroll(axis);
            return maximum > 0.0f ? offset(axis) / maximum : 0.0f;
        }
        AUIK_EXPORT void set_metrics(f32 content, f32 view, amal::axis axis);
        AUIK_EXPORT bool scroll_to(f32 value, amal::axis axis);
        bool scroll_by(f32 delta, amal::axis axis) { return scroll_to(offset(axis) + delta, axis); }
    };
} // namespace auik
