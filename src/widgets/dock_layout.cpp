#include <auik/widgets/dock_layout.hpp>
#include "../core/session_stream_utils.hpp"

namespace auik
{
    static inline f32 dock_axis_size(const amal::vec2 &value, amal::axis axis)
    {
        return axis == amal::axis::x ? value.x : value.y;
    }

    static inline f32 dock_cross_size(const amal::vec2 &value, amal::axis axis)
    {
        return axis == amal::axis::x ? value.y : value.x;
    }

    static inline amal::vec2 make_dock_axis_size(amal::axis axis, f32 main, f32 cross)
    {
        return axis == amal::axis::x ? amal::vec2{main, cross} : amal::vec2{cross, main};
    }

    static inline void set_dock_axis_size(amal::vec2 &value, amal::axis axis, f32 size)
    {
        if (axis == amal::axis::x) value.x = size;
        else value.y = size;
    }

    static inline void set_dock_axis_offset(amal::vec2 &value, amal::axis axis, f32 offset)
    {
        if (axis == amal::axis::x) value.x = offset;
        else value.y = offset;
    }

    amal::vec2 DockLayout::measure_node(DockLayoutNodeID id)
    {
        auto *node = get_node(id);
        if (!node) return {};
        update_node_style_cache(*node);
        amal::vec2 required = node->min_size;
        if (!node->children.empty())
        {
            f32 main = 0.0f;
            f32 cross = 0.0f;
            for (DockLayoutNodeID child_id : node->children)
            {
                const amal::vec2 child_required = measure_node(child_id);
                main += dock_axis_size(child_required, node->axis);
                cross = amal::max(cross, dock_cross_size(child_required, node->axis));
            }
            required = amal::max(required, make_dock_axis_size(node->axis, main, cross));
        }
        else
        {
            for (auto &item : node->items)
            {
                if (item.widget) item.widget->update_layout_min_size();
                required = amal::max(required, (item.widget ? item.widget->required_size() : amal::vec2{0.0f, 0.0f}));
            }
        }

        if (is_size_concrete(node->style_size.x)) required.x = amal::max(required.x, node->style_size.x);
        if (is_size_concrete(node->style_size.y)) required.y = amal::max(required.y, node->style_size.y);
        node->required_size = required;
        return required;
    }

    void DockLayout::layout_node(DockLayoutNodeID id, const amal::rect &bounds)
    {
        auto *node = get_node(id);
        if (!node) return;
        node->bounds = bounds;
        if (node->children.empty())
        {
            for (auto &item : node->items) layout_item(node->bounds, item);
            return;
        }

        const f32 main_available = amal::max(dock_axis_size(bounds.size, node->axis), 0.0f);
        const f32 cross_available = amal::max(dock_cross_size(bounds.size, node->axis), 0.0f);
        f32 fixed_total = 0.0f;
        size_t fill_count = 0u;

        for (DockLayoutNodeID child_id : node->children)
        {
            auto *child = get_node(child_id);
            if (!child) continue;
            update_node_style_cache(*child);
            const f32 style_size = dock_axis_size(child->style_size, node->axis);
            const f32 min_size = dock_axis_size(child->min_size, node->axis);
            if (is_size_fill(style_size))
            {
                ++fill_count;
                continue;
            }
            if (is_size_concrete(style_size)) fixed_total += amal::max(style_size, min_size);
            else fixed_total += amal::max(dock_axis_size(child->required_size, node->axis), min_size);
        }

        const f32 fill_size =
            fill_count > 0u ? amal::max(main_available - fixed_total, 0.0f) / static_cast<f32>(fill_count) : 0.0f;
        f32 cursor = dock_axis_size(bounds.offset, node->axis);
        const f32 end = cursor + main_available;

        for (size_t i = 0; i < node->children.size(); ++i)
        {
            auto *child = get_node(node->children[i]);
            if (!child) continue;
            const f32 style_main = dock_axis_size(child->style_size, node->axis);
            const f32 min_main = dock_axis_size(child->min_size, node->axis);
            f32 child_main = 0.0f;
            if (is_size_fill(style_main)) child_main = fill_size;
            else if (is_size_concrete(style_main)) child_main = style_main;
            else child_main = dock_axis_size(child->required_size, node->axis);
            child_main = amal::max(child_main, min_main);
            child_main = amal::min(child_main, amal::max(end - cursor, 0.0f));

            amal::rect child_bounds = bounds;
            set_dock_axis_offset(child_bounds.offset, node->axis, cursor);
            set_dock_axis_size(child_bounds.size, node->axis, child_main);
            if (node->axis == amal::axis::x) child_bounds.size.y = cross_available;
            else child_bounds.size.x = cross_available;
            layout_node(node->children[i], child_bounds);
            cursor += child_main;
        }
    }

    DockLayoutNodeID DockLayout::create_node(DockLayoutNodeID parent, bool split, DockLayoutNodeSettings settings)
    {
        assert(parent < _tree.nodes().size() && "parent dock node is invalid");
        DockLayoutNodeID id = static_cast<DockLayoutNodeID>(_tree.nodes().size());
        Node node{};
        node.parent = parent;
        node.settings = settings;
        update_node_style_cache(node);
        _tree.append(parent, std::move(node));
        auto &parent_node = _tree.nodes()[parent];
        if (split) _tree.nodes()[id].axis = parent_node.axis;
        return id;
    }

    void DockLayout::update_node_style_cache(Node &node)
    {
        node.style_size = node.settings.size;
        node.min_size = node.settings.min_size;
        if (node.settings.style_tag == 0u) return;
        const StyleID style_id = get_theme()->get_resolved_style(node.settings.style_tag, 0u, 0u, StyleState::normal);
        const Style &style = get_theme()->get_style(style_id);
        node.style_size = style.size();
        node.min_size = {amal::max(node.settings.min_size.x, style.min_width()),
                         amal::max(node.settings.min_size.y, style.min_height())};
    }

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
        : Widget(id, widget_flags, EventFlagBits::none, {{0.0f, 0.0f}, inline_size}, AUIK_TAG_DOCK_LAYOUT)
    {
    }

    DockLayout::~DockLayout()
    {
        acul::vector<Widget *> children;
        for (const auto &node : _tree.nodes())
            for (const auto &item : node.items)
                if (item.widget) children.push_back(item.widget);
        clear();
        for (auto *child : children) acul::release(child);
    }

    DockLayoutNodeID DockLayout::create_split(DockLayoutNodeID parent, amal::axis axis, DockLayoutNodeSettings settings)
    {
        const auto id = create_node(parent, true, settings);
        get_node(id)->axis = axis;
        return id;
    }

    DockLayoutNodeID DockLayout::create_leaf(DockLayoutNodeID parent, DockLayoutNodeSettings settings)
    {
        return create_node(parent, false, settings);
    }

    void DockLayout::set_split_axis(DockLayoutNodeID node, amal::axis axis)
    {
        if (auto *n = get_node(node)) n->axis = axis;
    }

    void DockLayout::set_node_settings(DockLayoutNodeID node, DockLayoutNodeSettings settings)
    {
        if (auto *n = get_node(node))
        {
            n->settings = settings;
            update_node_style_cache(*n);
        }
    }

    void DockLayout::add_child(DockLayoutNodeID node, Widget *child, ChildLayoutFlags layout)
    {
        assert(child && "child is null");
        Item item{};
        item.widget = child;
        item.layout = layout;
        if (auto *n = get_node(node))
        {
            attach_item(item);
            n->items.push_back(item);
        }
    }

    DockLayout::Node *DockLayout::get_node(DockLayoutNodeID id) { return _tree.attached(id) ? _tree.get(id) : nullptr; }

    const DockLayout::Node *DockLayout::get_node(DockLayoutNodeID id) const
    {
        return _tree.attached(id) ? _tree.get(id) : nullptr;
    }

    bool DockLayout::remove_child(DockLayoutNodeID id, Widget *child)
    {
        auto *node = get_node(id);
        if (!node || !child) return false;
        for (size_t i = 0; i < node->items.size(); ++i)
            if (node->items[i].widget == child)
            {
                detach_item(node->items[i]);
                node->items.erase(node->items.begin() + i);
                return true;
            }
        return false;
    }

    void DockLayout::clear_node(DockLayoutNodeID id)
    {
        if (auto *node = get_node(id))
        {
            for (auto &item : node->items) detach_item(item);
            node->items.clear();
        }
    }

    void DockLayout::clear()
    {
        for (auto &node : _tree.nodes())
            for (auto &item : node.items) detach_item(item);
        _tree.reset();
    }

    StyleUpdateFlags DockLayout::update_style()
    {
        StyleUpdateFlags flags = StyleUpdateFlagBits::none;
        for (Item &item : items())
            if (item.widget && item.widget->is_visible()) flags |= item.widget->update_style_invalidated();
        return flags;
    }

    void DockLayout::update_layout_min_size_force() { set_required_size(measure_node(root_node())); }

    void DockLayout::update_layout(bool min_size_known)
    {
        if (layout_measure_required(min_size_known)) update_layout_min_size_force();
        set_layout_size(resolve_layout_size_from_required());
        Widget::update_layout(true);
        set_clip_id(parent() ? parent()->content_clip_id() : clip_id());
        layout_node(root_node(), bounds());
    }

    void DockLayout::translate(const amal::vec2 &delta)
    {
        if (delta.x == 0.0f && delta.y == 0.0f) return;
        Widget::translate(delta);
        for (Item &item : items())
            if (item.widget) item.widget->translate(delta);
    }

    void DockLayout::reset_clip_rect_records()
    {
        Widget::reset_clip_rect_records();
        for (Item &item : items())
            if (item.widget) item.widget->reset_clip_rect_records();
    }

    void DockLayout::rebuild_clip_rects()
    {
        set_clip_id(parent() ? parent()->content_clip_id() : clip_id());
        for (Item &item : items())
            if (item.widget && item.widget->is_visible()) item.widget->rebuild_clip_rects();
    }

    void DockLayout::reset_draw_records()
    {
        for (Item &item : items())
            if (item.widget) item.widget->reset_draw_records();
    }

    void DockLayout::invalidate_style()
    {
        Widget::invalidate_style();
        for (Item &item : items())
            if (item.widget) item.widget->invalidate_style();
    }

    bool DockLayout::update_locale()
    {
        bool changed = Widget::update_locale();
        for (Item &item : items())
            if (item.widget) changed |= item.widget->update_locale();
        return changed;
    }

    void DockLayout::add_state_flags_inherit(WidgetStateFlags flags)
    {
        Widget::add_state_flags_inherit(flags);
        if ((flags & WidgetStateFlagBits::visible) && !is_visible()) flags &= ~WidgetStateFlagBits::visible;
        for (auto &item : items())
            if (item.widget) item.widget->add_state_flags_inherit(flags);
    }

    void DockLayout::remove_state_flags_inherit(WidgetStateFlags flags)
    {
        Widget::remove_state_flags_inherit(flags);
        for (auto &item : items())
            if (item.widget) item.widget->remove_state_flags_inherit(flags);
    }

    u32 DockLayout::get_depth_requirement() const
    {
        u32 requirement = 1u;
        for (const Item &item : items())
            if (item.widget && item.widget->is_visible())
                requirement += amal::max(item.widget->get_depth_requirement(), 1u);
        return requirement;
    }

    void DockLayout::update_depth(const amal::vec2 &depth_range)
    {
        Widget::update_depth(depth_range);
        DepthCursor cursor(this->depth_range(), get_depth_requirement());
        cursor.next(1u);
        for (Item &item : items())
            if (item.widget && item.widget->is_visible())
                item.widget->update_depth(cursor.next(amal::max(item.widget->get_depth_requirement(), 1u)));
    }

    void DockLayout::draw(DrawCtx &ctx)
    {
        if (!is_visible() && !(ctx.reason & DrawReasonBits::invalidate)) return;
        for (Item &item : items())
            if (item.widget && (item.widget->is_visible() || (ctx.reason & DrawReasonBits::invalidate)))
                item.widget->draw_local(ctx);
    }

    void DockLayout::on_attach()
    {
        Widget::on_attach();
        for (Item &item : items())
            if (item.widget && (item.widget->widget_flags & WidgetFlagBits::attachable)) item.widget->on_attach();
    }

    void DockLayout::on_detach()
    {
        for (Item &item : items())
            if (item.widget && (item.widget->widget_flags & WidgetFlagBits::attachable)) item.widget->on_detach();
        Widget::on_detach();
    }

    void DockLayout::attach_item(Item &item)
    {
        if (!item.widget) return;
        item.widget->set_parent(this);
        item.widget->set_focus_parent(this);
        item.widget->update_style_invalidated();
        if (detail::get_context().id_map.find(id()) != detail::get_context().id_map.end() &&
            (item.widget->widget_flags & WidgetFlagBits::attachable))
            item.widget->on_attach();
    }

    void DockLayout::detach_item(Item &item)
    {
        if (!item.widget) return;
        if (detail::get_context().id_map.find(item.widget->id()) != detail::get_context().id_map.end() &&
            (item.widget->widget_flags & WidgetFlagBits::attachable))
            item.widget->on_detach();
        item.widget->set_parent(nullptr);
        item.widget->set_focus_parent(nullptr);
    }

    void DockLayout::layout_item(const amal::rect &bounds, Item &item)
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
        static void write_settings(acul::bin_stream &stream, const DockLayoutNodeSettings &settings)
        {
            stream.write(settings.style_tag).write(settings.size).write(settings.min_size);
        }

        static DockLayoutNodeSettings read_settings(acul::bin_stream &stream)
        {
            DockLayoutNodeSettings settings{};
            stream.read(settings.style_tag).read(settings.size).read(settings.min_size);
            return settings;
        }

        static void write(acul::bin_stream &stream, umbf::Block *block)
        {
            const auto *layout = static_cast<DockLayout *>(block);
            detail::write_widget_common_data(stream, *layout);
            const auto &nodes = layout->_tree.nodes();
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
            layout->_tree.nodes().clear();

            u32 node_count = 0u;
            stream.read(node_count);
            if (node_count == 0u) node_count = 1u;
            layout->_tree.nodes().resize(node_count);
            for (u32 node_i = 0u; node_i < node_count; ++node_i)
            {
                auto &node = layout->_tree.nodes()[node_i];
                const auto settings = read_settings(stream);
                u8 axis = static_cast<u8>(amal::axis::x);
                stream.read(axis).read(node.parent);
                node.axis = static_cast<amal::axis>(axis);
                node.settings = settings;
                layout->set_node_settings(node_i, settings);
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
                    layout->add_child(node_i, widget, item_layouts[item_i]);
                }
            }
            return layout;
        }
    };

    namespace streams
    {
        extern AUIK_EXPORT const umbf::registry::BlockStream dock_layout{DockLayoutStreamAccess::read,
                                                                         DockLayoutStreamAccess::write};
    }
} // namespace auik
