#pragma once

#include <amal/geometric.hpp>
#include <iterator>
#include <type_traits>
#include "../widget.hpp"

namespace auik::detail
{
    static inline f32 dock_axis_size(const amal::vec2 &value, amal::axis axis)
    { return axis == amal::axis::x ? value.x : value.y; }

    static inline f32 dock_cross_size(const amal::vec2 &value, amal::axis axis)
    { return axis == amal::axis::x ? value.y : value.x; }

    static inline amal::vec2 make_dock_axis_size(amal::axis axis, f32 main, f32 cross)
    { return axis == amal::axis::x ? amal::vec2{main, cross} : amal::vec2{cross, main}; }

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

    struct DockBaseNodeSettings
    {
        u32 style_tag = 0u;
        amal::vec2 size = AUIK_SIZE_FIT;
        amal::vec2 min_size{0.0f, 0.0f};
    };

    template <class Item, class Policy>
    class DockBase
    {
    public:
        using Owner = typename Policy::Owner;
        using NodeID = u32;
        static constexpr NodeID invalid_node = 0xFFFFFFFFu;

        struct Node
        {
            DockBaseNodeSettings settings{};
            amal::axis axis = amal::axis::x;
            NodeID parent = invalid_node;
            acul::vector<NodeID> children;
            acul::vector<Item> items;
            amal::rect bounds{};
            amal::vec2 required_size{0.0f, 0.0f};
            amal::vec2 style_size{AUIK_SIZE_X_FIT, AUIK_SIZE_Y_FIT};
            amal::vec2 min_size{0.0f, 0.0f};
        };

        explicit DockBase(Owner *owner) : _owner(owner)
        {
            Node root{};
            root.parent = invalid_node;
            _nodes.push_back(root);
        }

        NodeID root_node() const { return 0u; }
        const acul::vector<Node> &nodes() const { return _nodes; }
        acul::vector<Node> &nodes() { return _nodes; }

        template <bool Const>
        class BasicItemIterator
        {
            using DockPtr = std::conditional_t<Const, const DockBase *, DockBase *>;

        public:
            using iterator_category = std::forward_iterator_tag;
            using value_type = Item;
            using difference_type = std::ptrdiff_t;
            using pointer = std::conditional_t<Const, const Item *, Item *>;
            using reference = std::conditional_t<Const, const Item &, Item &>;

            BasicItemIterator() = default;
            BasicItemIterator(DockPtr dock, size_t node_index, size_t item_index)
                : _dock(dock), _node_index(node_index), _item_index(item_index)
            {
                skip_empty_nodes();
            }

            reference operator*() const { return _dock->_nodes[_node_index].items[_item_index]; }
            pointer operator->() const { return &_dock->_nodes[_node_index].items[_item_index]; }

            BasicItemIterator &operator++()
            {
                ++_item_index;
                skip_empty_nodes();
                return *this;
            }

            BasicItemIterator operator++(int)
            {
                BasicItemIterator copy = *this;
                ++(*this);
                return copy;
            }

            bool operator==(const BasicItemIterator &other) const
            {
                return _dock == other._dock && _node_index == other._node_index && _item_index == other._item_index;
            }

            bool operator!=(const BasicItemIterator &other) const { return !(*this == other); }

        private:
            void skip_empty_nodes()
            {
                if (!_dock) return;
                while (_node_index < _dock->_nodes.size() && _item_index >= _dock->_nodes[_node_index].items.size())
                {
                    ++_node_index;
                    _item_index = 0u;
                }
                if (_node_index >= _dock->_nodes.size()) _item_index = 0u;
            }

            DockPtr _dock = nullptr;
            size_t _node_index = 0u;
            size_t _item_index = 0u;
        };

        using item_iterator = BasicItemIterator<false>;
        using const_item_iterator = BasicItemIterator<true>;

        template <bool Const>
        class BasicItemRange
        {
            using DockPtr = std::conditional_t<Const, const DockBase *, DockBase *>;
            using Iterator = BasicItemIterator<Const>;

        public:
            explicit BasicItemRange(DockPtr dock) : _dock(dock) {}

            Iterator begin() const { return Iterator{_dock, 0u, 0u}; }
            Iterator end() const { return Iterator{_dock, _dock ? _dock->_nodes.size() : 0u, 0u}; }

        private:
            DockPtr _dock = nullptr;
        };

        using item_range = BasicItemRange<false>;
        using const_item_range = BasicItemRange<true>;

        item_range items() { return item_range{this}; }
        const_item_range items() const { return const_item_range{this}; }
        item_iterator item_begin() { return item_iterator{this, 0u, 0u}; }
        item_iterator item_end() { return item_iterator{this, _nodes.size(), 0u}; }
        const_item_iterator item_begin() const { return const_item_iterator{this, 0u, 0u}; }
        const_item_iterator item_end() const { return const_item_iterator{this, _nodes.size(), 0u}; }

        Node *get_node(NodeID id)
        { return id < _nodes.size() && (_nodes[id].parent != invalid_node || id == 0u) ? &_nodes[id] : nullptr; }

        const Node *get_node(NodeID id) const
        { return id < _nodes.size() && (_nodes[id].parent != invalid_node || id == 0u) ? &_nodes[id] : nullptr; }

        NodeID create_split(NodeID parent, amal::axis axis, DockBaseNodeSettings settings = {})
        {
            const NodeID id = create_node(parent, true, settings);
            if (auto *node = get_node(id)) node->axis = axis;
            return id;
        }

        NodeID create_leaf(NodeID parent, DockBaseNodeSettings settings = {})
        { return create_node(parent, false, settings); }

        void set_split_axis(NodeID id, amal::axis axis)
        {
            if (auto *node = get_node(id)) node->axis = axis;
        }

        void set_node_settings(NodeID id, DockBaseNodeSettings settings)
        {
            if (auto *node = get_node(id))
            {
                node->settings = settings;
                update_node_style_cache(*node);
            }
        }

        void add_item(NodeID id, Item item)
        {
            auto *node = get_node(id);
            if (!node) return;
            Policy::attach_item(_owner, item);
            node->items.push_back(item);
        }

        bool remove_item(NodeID id, typename Policy::ItemWidget *widget)
        {
            auto *node = get_node(id);
            if (!node || !widget) return false;
            for (size_t i = 0; i < node->items.size(); ++i)
            {
                if (Policy::item_widget(node->items[i]) != widget) continue;
                Policy::detach_item(_owner, node->items[i]);
                node->items.erase(node->items.begin() + i);
                return true;
            }
            return false;
        }

        void clear_items(NodeID id)
        {
            auto *node = get_node(id);
            if (!node) return;
            for (auto &item : node->items) Policy::detach_item(_owner, item);
            node->items.clear();
        }

        void clear()
        {
            for (auto &node : _nodes)
            {
                for (auto &item : node.items) Policy::detach_item(_owner, item);
                node.items.clear();
                node.children.clear();
            }
            _nodes.clear();
            Node root{};
            root.parent = invalid_node;
            _nodes.push_back(root);
        }

        amal::vec2 measure(NodeID id)
        {
            auto *node = get_node(id);
            if (!node) return {};
            update_node_style_cache(*node);
            amal::vec2 required = node->min_size;
            if (!node->children.empty())
            {
                f32 main = 0.0f;
                f32 cross = 0.0f;
                for (NodeID child_id : node->children)
                {
                    const amal::vec2 child_required = measure(child_id);
                    main += dock_axis_size(child_required, node->axis);
                    cross = amal::max(cross, dock_cross_size(child_required, node->axis));
                }
                required = amal::max(required, make_dock_axis_size(node->axis, main, cross));
            }
            else
            {
                for (auto &item : node->items)
                {
                    Policy::update_item_min_size(_owner, item);
                    required = amal::max(required, Policy::item_required_size(item));
                }
            }

            if (is_size_concrete(node->style_size.x)) required.x = amal::max(required.x, node->style_size.x);
            if (is_size_concrete(node->style_size.y)) required.y = amal::max(required.y, node->style_size.y);
            node->required_size = required;
            return required;
        }

        void layout(NodeID id, const amal::rect &bounds)
        {
            auto *node = get_node(id);
            if (!node) return;
            node->bounds = bounds;
            if (node->children.empty())
            {
                for (auto &item : node->items) Policy::layout_item(_owner, node->bounds, item);
                return;
            }

            const f32 main_available = amal::max(dock_axis_size(bounds.size, node->axis), 0.0f);
            const f32 cross_available = amal::max(dock_cross_size(bounds.size, node->axis), 0.0f);
            f32 fixed_total = 0.0f;
            size_t fill_count = 0u;

            for (NodeID child_id : node->children)
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

            for (size_t i = 0; i < node->children.size(); ++i)
            {
                auto *child = get_node(node->children[i]);
                if (!child) continue;
                const f32 style_main = dock_axis_size(child->style_size, node->axis);
                const f32 min_main = dock_axis_size(child->min_size, node->axis);
                f32 child_main = 0.0f;
                if (is_size_fill(style_main))
                    child_main = fill_size;
                else if (is_size_concrete(style_main)) child_main = style_main;
                else child_main = dock_axis_size(child->required_size, node->axis);
                child_main = amal::max(child_main, min_main);

                amal::rect child_bounds = bounds;
                set_dock_axis_offset(child_bounds.offset, node->axis, cursor);
                set_dock_axis_size(child_bounds.size, node->axis, child_main);
                if (node->axis == amal::axis::x) child_bounds.size.y = cross_available;
                else child_bounds.size.x = cross_available;
                layout(node->children[i], child_bounds);
                cursor += child_main;
            }
        }

    private:
        NodeID create_node(NodeID parent, bool split, DockBaseNodeSettings settings)
        {
            assert(parent < _nodes.size() && "parent dock node is invalid");
            NodeID id = static_cast<NodeID>(_nodes.size());
            Node node{};
            node.parent = parent;
            node.settings = settings;
            update_node_style_cache(node);
            _nodes.push_back(std::move(node));
            auto &parent_node = _nodes[parent];
            parent_node.children.push_back(id);
            if (split) _nodes[id].axis = parent_node.axis;
            return id;
        }

        void update_node_style_cache(Node &node)
        {
            node.style_size = node.settings.size;
            node.min_size = node.settings.min_size;
            if (node.settings.style_tag == 0u) return;
            const StyleID style_id =
                get_theme()->get_resolved_style(node.settings.style_tag, 0u, 0u, StyleState::normal);
            const Style &style = get_theme()->get_style(style_id);
            node.style_size = style.size();
            node.min_size = {amal::max(node.settings.min_size.x, style.min_width()),
                             amal::max(node.settings.min_size.y, style.min_height())};
        }

        Owner *_owner = nullptr;
        acul::vector<Node> _nodes;
    };
} // namespace auik::detail
