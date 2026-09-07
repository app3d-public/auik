#include <auik/widgets/scroll.hpp>

namespace auik
{
    void ScrollData::set_metrics(f32 content, f32 view, amal::axis axis)
    {
        f32 &stored_content = axis == amal::axis::x ? content_size.x : content_size.y;
        f32 &stored_view = axis == amal::axis::x ? view_size.x : view_size.y;
        f32 &maximum = axis == amal::axis::x ? max_offset.x : max_offset.y;
        f32 &current = axis == amal::axis::x ? content_offset.x : content_offset.y;

        stored_content = content > 0.0f ? content : 0.0f;
        stored_view = view > 0.0f ? view : 0.0f;
        maximum = stored_content > stored_view ? stored_content - stored_view : 0.0f;
        current = current < 0.0f ? 0.0f : (current > maximum ? maximum : current);
    }

    bool ScrollData::scroll_to(f32 value, amal::axis axis)
    {
        f32 &current = axis == amal::axis::x ? content_offset.x : content_offset.y;
        const f32 maximum = max_scroll(axis);
        const f32 next = value < 0.0f ? 0.0f : (value > maximum ? maximum : value);
        if (current == next) return false;
        current = next;

        if (on_scroll_to) on_scroll_to(*this, axis);
        return true;
    }
} // namespace auik
