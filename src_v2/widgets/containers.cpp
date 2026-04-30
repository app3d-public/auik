#include <auik/v2/widgets/containers.hpp>

namespace auik::v2
{
    void Block::update_layout_min_size()
    {
        amal::vec2 required{0.0f, 0.0f};
        bool has_child = false;
        const f32 gap = child_spacing();
        for (auto *child : children)
        {
            if (!child) continue;
            child->update_layout_min_size();
            const amal::vec2 child_required = child->required_size();
            if (has_child) required.x += gap;
            required.x += child_required.x;
            required.y = amal::max(required.y, child_required.y);
            has_child = true;
        }
        set_required_size(required);
    }

    void Block::update_layout(bool min_size_known)
    {
        if (!min_size_known) update_layout_min_size();

        amal::vec2 block_size = size();
        block_size.x = amal::max(block_size.x, required_size().x);
        block_size.y = amal::max(block_size.y, required_size().y);
        set_size(block_size);
        Widget::update_layout(true);
        set_clip_id(content_clip_id());

        const f32 gap = child_spacing();
        f32 cursor_x = position().x;
        for (auto *child : children)
        {
            if (!child) continue;
            const amal::vec2 child_required = child->required_size();
            const f32 child_y = position().y + amal::max((block_size.y - child_required.y) * 0.5f, 0.0f);
            child->set_position({cursor_x, child_y});
            child->update_layout(true);
            cursor_x += child->required_size().x + gap;
        }
    }
} // namespace auik::v2
