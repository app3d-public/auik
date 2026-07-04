#include <auik/widgets/dock_layout.hpp>

namespace auik
{
    static inline amal::vec2 dock_layout_align_pos(const amal::rect &bounds, const amal::vec2 &size,
                                                   ChildLayoutFlags layout)
    {
        amal::vec2 pos = bounds.offset;
        if (layout & ChildLayoutFlagBits::aright)
            pos.x += amal::max(bounds.size.x - size.x, 0.0f);
        else if (layout & ChildLayoutFlagBits::hcenter)
            pos.x += amal::floor(amal::max(bounds.size.x - size.x, 0.0f) * 0.5f);

        if (layout & ChildLayoutFlagBits::bottom)
            pos.y += amal::max(bounds.size.y - size.y, 0.0f);
        else if (layout & ChildLayoutFlagBits::vcenter)
            pos.y += amal::floor(amal::max(bounds.size.y - size.y, 0.0f) * 0.5f);
        return pos;
    }

    DockLayout::DockLayout(u32 id, const amal::vec2 &inline_size, WidgetFlags widget_flags)
        : Widget(id, widget_flags, EventFlagBits::none, {{0.0f, 0.0f}, inline_size}, AUIK_TAG_DOCK_LAYOUT),
          _dock(this)
    {
    }

    DockLayout::~DockLayout() { clear(); }

    DockLayoutNodeID DockLayout::create_split(DockLayoutNodeID parent, amal::axis axis,
                                              DockLayoutNodeSettings settings)
    { return _dock.create_split(parent, axis, settings); }

    DockLayoutNodeID DockLayout::create_leaf(DockLayoutNodeID parent, DockLayoutNodeSettings settings)
    { return _dock.create_leaf(parent, settings); }

    void DockLayout::set_split_axis(DockLayoutNodeID node, amal::axis axis) { _dock.set_split_axis(node, axis); }

    void DockLayout::set_node_settings(DockLayoutNodeID node, DockLayoutNodeSettings settings)
    { _dock.set_node_settings(node, settings); }

    void DockLayout::add_child(DockLayoutNodeID node, Widget *child, ChildLayoutFlags layout)
    {
        assert(child && "child is null");
        Item item{};
        item.widget = child;
        item.layout = layout;
        _dock.add_item(node, item);
    }

    bool DockLayout::remove_child(DockLayoutNodeID node, Widget *child) { return _dock.remove_item(node, child); }

    void DockLayout::clear_node(DockLayoutNodeID node) { _dock.clear_items(node); }

    void DockLayout::clear() { _dock.clear(); }

    StyleUpdateFlags DockLayout::update_style()
    {
        StyleUpdateFlags flags = StyleUpdateFlagBits::none;
        for (Item &item : _dock.items())
            if (item.widget) flags |= item.widget->update_style();
        return flags;
    }

    void DockLayout::update_layout_min_size() { set_required_size(_dock.measure(root_node())); }

    void DockLayout::update_layout(bool min_size_known)
    {
        if (!min_size_known) update_layout_min_size();
        set_layout_size(resolve_layout_size_from_required());
        Widget::update_layout(true);
        set_clip_id(parent() ? parent()->content_clip_id() : clip_id());
        _dock.layout(root_node(), bounds());
    }

    void DockLayout::translate(const amal::vec2 &delta)
    {
        if (delta.x == 0.0f && delta.y == 0.0f) return;
        Widget::translate(delta);
        for (Item &item : _dock.items())
            if (item.widget) item.widget->translate(delta);
    }

    void DockLayout::reset_clip_rect_records()
    {
        Widget::reset_clip_rect_records();
        for (Item &item : _dock.items())
            if (item.widget) item.widget->reset_clip_rect_records();
    }

    void DockLayout::rebuild_clip_rects()
    {
        set_clip_id(parent() ? parent()->content_clip_id() : clip_id());
        for (Item &item : _dock.items())
            if (item.widget) item.widget->rebuild_clip_rects();
    }

    void DockLayout::reset_draw_records()
    {
        for (Item &item : _dock.items())
            if (item.widget) item.widget->reset_draw_records();
    }

    u32 DockLayout::get_depth_requirement() const
    {
        u32 requirement = 1u;
        for (const Item &item : _dock.items())
            if (item.widget) requirement += amal::max(item.widget->get_depth_requirement(), 1u);
        return requirement;
    }

    void DockLayout::update_depth(const amal::vec2 &depth_range)
    {
        Widget::update_depth(depth_range);
        DepthCursor cursor(this->depth_range(), get_depth_requirement());
        cursor.next(1u);
        for (Item &item : _dock.items())
            if (item.widget) item.widget->update_depth(cursor.next(amal::max(item.widget->get_depth_requirement(), 1u)));
    }

    void DockLayout::draw(DrawCtx &ctx)
    {
        for (Item &item : _dock.items())
            if (item.widget) item.widget->draw_local(ctx);
    }

    void DockLayout::on_attach()
    {
        Widget::on_attach();
        for (Item &item : _dock.items())
            if (item.widget && (item.widget->widget_flags & WidgetFlagBits::attachable)) item.widget->on_attach();
    }

    void DockLayout::on_detach()
    {
        for (Item &item : _dock.items())
            if (item.widget && (item.widget->widget_flags & WidgetFlagBits::attachable)) item.widget->on_detach();
        Widget::on_detach();
    }

    void DockLayout::Policy::attach_item(DockLayout *owner, Item &item)
    {
        if (!owner || !item.widget) return;
        item.widget->set_parent(owner);
        item.widget->set_focus_parent(owner);
        item.widget->update_style();
        if (detail::get_context().id_map.find(owner->id()) != detail::get_context().id_map.end() &&
            (item.widget->widget_flags & WidgetFlagBits::attachable))
            item.widget->on_attach();
    }

    void DockLayout::Policy::detach_item(DockLayout *owner, Item &item)
    {
        if (!owner || !item.widget) return;
        if (detail::get_context().id_map.find(item.widget->id()) != detail::get_context().id_map.end() &&
            (item.widget->widget_flags & WidgetFlagBits::attachable))
            item.widget->on_detach();
        item.widget->set_parent(nullptr);
        item.widget->set_focus_parent(nullptr);
    }

    void DockLayout::Policy::update_item_min_size(DockLayout *, Item &item)
    {
        if (item.widget) item.widget->update_layout_min_size();
    }

    void DockLayout::Policy::layout_item(DockLayout *, const amal::rect &bounds, Item &item)
    {
        if (!item.widget) return;
        const amal::vec2 required = item.widget->required_size();
        amal::vec2 layout_size = required;
        if (item.widget->fill_width()) layout_size.x = bounds.size.x;
        if (item.widget->fill_height()) layout_size.y = bounds.size.y;
        layout_size = amal::min(layout_size, bounds.size);
        item.widget->set_position(dock_layout_align_pos(bounds, layout_size, item.layout));
        item.widget->set_layout_size(layout_size);
        item.widget->update_layout(true);
        item.widget->rebuild_clip_rects();
    }
} // namespace auik
