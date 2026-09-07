#include <auik/widgets/dock_layout.hpp>
#include "../core/session_stream_utils.hpp"

namespace auik
{
    static inline amal::vec2 dock_layout_align_pos(const amal::rect &bounds, const amal::vec2 &size,
                                                   ChildLayoutFlags layout)
    {
        amal::vec2 pos = bounds.offset;
        if (layout & ChildLayoutFlagBits::aright) pos.x += amal::max(bounds.size.x - size.x, 0.0f);
        else if (layout & ChildLayoutFlagBits::hcenter)
            pos.x += amal::floor(amal::max(bounds.size.x - size.x, 0.0f) * 0.5f);

        if (layout & ChildLayoutFlagBits::bottom) pos.y += amal::max(bounds.size.y - size.y, 0.0f);
        else if (layout & ChildLayoutFlagBits::vcenter)
            pos.y += amal::floor(amal::max(bounds.size.y - size.y, 0.0f) * 0.5f);
        return pos;
    }

    DockLayout::DockLayout(u32 id, const amal::vec2 &inline_size, WidgetFlags widget_flags)
        : Widget(id, widget_flags, EventFlagBits::none, {{0.0f, 0.0f}, inline_size}, AUIK_TAG_DOCK_LAYOUT), _dock(this)
    {
    }

    DockLayout::~DockLayout() { clear(); }

    DockLayoutNodeID DockLayout::create_split(DockLayoutNodeID parent, amal::axis axis, DockLayoutNodeSettings settings)
    {
        return _dock.create_split(parent, axis, settings);
    }

    DockLayoutNodeID DockLayout::create_leaf(DockLayoutNodeID parent, DockLayoutNodeSettings settings)
    {
        return _dock.create_leaf(parent, settings);
    }

    void DockLayout::set_split_axis(DockLayoutNodeID node, amal::axis axis) { _dock.set_split_axis(node, axis); }

    void DockLayout::set_node_settings(DockLayoutNodeID node, DockLayoutNodeSettings settings)
    {
        _dock.set_node_settings(node, settings);
    }

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
            if (item.widget && item.widget->is_visible()) flags |= item.widget->update_style_invalidated();
        return flags;
    }

    void DockLayout::update_layout_min_size_force() { set_required_size(_dock.measure(root_node())); }

    void DockLayout::update_layout(bool min_size_known)
    {
        if (layout_measure_required(min_size_known)) update_layout_min_size_force();
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
            if (item.widget && item.widget->is_visible()) item.widget->rebuild_clip_rects();
    }

    void DockLayout::reset_draw_records()
    {
        for (Item &item : _dock.items())
            if (item.widget) item.widget->reset_draw_records();
    }

    void DockLayout::invalidate_style()
    {
        Widget::invalidate_style();
        for (Item &item : _dock.items())
            if (item.widget) item.widget->invalidate_style();
    }

    bool DockLayout::update_locale()
    {
        bool changed = Widget::update_locale();
        for (Item &item : _dock.items())
            if (item.widget) changed |= item.widget->update_locale();
        return changed;
    }

    void DockLayout::add_state_flags_inherit(WidgetStateFlags flags)
    {
        Widget::add_state_flags_inherit(flags);
        if ((flags & WidgetStateFlagBits::visible) && !is_visible()) flags &= ~WidgetStateFlagBits::visible;
        for (auto &item : _dock.items())
            if (item.widget) item.widget->add_state_flags_inherit(flags);
    }

    void DockLayout::remove_state_flags_inherit(WidgetStateFlags flags)
    {
        Widget::remove_state_flags_inherit(flags);
        for (auto &item : _dock.items())
            if (item.widget) item.widget->remove_state_flags_inherit(flags);
    }

    u32 DockLayout::get_depth_requirement() const
    {
        u32 requirement = 1u;
        for (const Item &item : _dock.items())
            if (item.widget && item.widget->is_visible())
                requirement += amal::max(item.widget->get_depth_requirement(), 1u);
        return requirement;
    }

    void DockLayout::update_depth(const amal::vec2 &depth_range)
    {
        Widget::update_depth(depth_range);
        DepthCursor cursor(this->depth_range(), get_depth_requirement());
        cursor.next(1u);
        for (Item &item : _dock.items())
            if (item.widget && item.widget->is_visible())
                item.widget->update_depth(cursor.next(amal::max(item.widget->get_depth_requirement(), 1u)));
    }

    void DockLayout::draw(DrawCtx &ctx)
    {
        if (!is_visible() && !(ctx.reason & DrawReasonBits::invalidate)) return;
        for (Item &item : _dock.items())
            if (item.widget && (item.widget->is_visible() || (ctx.reason & DrawReasonBits::invalidate)))
                item.widget->draw_local(ctx);
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
        item.widget->update_style_invalidated();
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

    struct DockLayoutStreamAccess
    {
        static void write_settings(acul::bin_stream &stream, const detail::DockBaseNodeSettings &settings)
        {
            stream.write(settings.style_tag).write(settings.size).write(settings.min_size);
        }

        static detail::DockBaseNodeSettings read_settings(acul::bin_stream &stream)
        {
            detail::DockBaseNodeSettings settings{};
            stream.read(settings.style_tag).read(settings.size).read(settings.min_size);
            return settings;
        }

        static void write(acul::bin_stream &stream, umbf::Block *block)
        {
            const auto *layout = static_cast<DockLayout *>(block);
            detail::write_widget_common_data(stream, *layout);
            const auto &nodes = layout->_dock.nodes();
            stream.write(static_cast<u32>(nodes.size()));
            for (const auto &node : nodes)
            {
                write_settings(stream, node.settings);
                stream.write(static_cast<u8>(node.axis))
                    .write(node.parent)
                    .write(static_cast<u32>(node.children.size()));
                if (!node.children.empty()) stream.write(node.children.data(), node.children.size());

                acul::vector<DockLayout::Item> items;
                acul::vector<Widget *> blocks;
                for (const auto &item : node.items)
                {
                    if (!item.widget || !(item.widget->widget_flags & WidgetFlagBits::cache_snapshot)) continue;
                    items.push_back(item);
                    blocks.push_back(item.widget);
                }
                stream.write(static_cast<u32>(items.size()));
                for (const auto &item : items) stream.write(static_cast<u32>(item.layout));
                stream.write(blocks);
            }
        }

        static umbf::Block *read(acul::bin_stream &stream)
        {
            const auto common = detail::read_widget_common_data(stream);
            auto *layout = acul::alloc<DockLayout>(common.id, common.inline_size, WidgetFlags(common.widget_flags));
            detail::apply_widget_common_data(layout, common);
            layout->_dock.nodes().clear();

            u32 node_count = 0u;
            stream.read(node_count);
            if (node_count == 0u) node_count = 1u;
            layout->_dock.nodes().resize(node_count);
            for (u32 node_i = 0u; node_i < node_count; ++node_i)
            {
                auto &node = layout->_dock.nodes()[node_i];
                const auto settings = read_settings(stream);
                u8 axis = static_cast<u8>(amal::axis::x);
                stream.read(axis).read(node.parent);
                node.axis = static_cast<amal::axis>(axis);
                node.settings = settings;
                layout->_dock.set_node_settings(node_i, settings);
                u32 child_count = 0u;
                stream.read(child_count);
                node.children.resize(child_count);
                if (!node.children.empty()) stream.read(node.children.data(), node.children.size());

                u32 item_count = 0u;
                stream.read(item_count);
                acul::vector<ChildLayoutFlags> item_layouts;
                item_layouts.reserve(item_count);
                for (u32 item_i = 0u; item_i < item_count; ++item_i)
                {
                    u32 item_layout = 0u;
                    stream.read(item_layout);
                    item_layouts.push_back(ChildLayoutFlags(item_layout));
                }
                acul::vector<Widget *> blocks;
                stream.read(blocks);
                if (blocks.size() != item_layouts.size())
                {
                    for (auto *item : blocks)
                        if (item) acul::release(item);
                    acul::release(layout);
                    throw acul::runtime_error("Invalid dock layout item count");
                }
                for (u32 item_i = 0u; item_i < item_count; ++item_i)
                {
                    auto *widget = dynamic_cast<Widget *>(blocks[item_i]);
                    if (!widget)
                    {
                        if (blocks[item_i]) acul::release(blocks[item_i]);
                        continue;
                    }
                    layout->_dock.add_item(node_i, DockLayout::Item{widget, item_layouts[item_i]});
                }
            }
            return layout;
        }
    };

    namespace streams
    {
        AUIK_EXPORT const umbf::registry::BlockStream dock_layout{DockLayoutStreamAccess::read,
                                                                  DockLayoutStreamAccess::write};
    }
} // namespace auik
