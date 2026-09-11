#pragma once

#include <amal/geometric.hpp>
#include <iterator>
#include <type_traits>
#include "detail/dock_tree.hpp"
#include "widget.hpp"

#define AUIK_TAG_DOCK_LAYOUT 0xD8A1014Au

namespace auik
{
    struct DockLayoutStreamAccess;
    using DockLayoutNodeID = u32;

    struct DockLayoutNodeSettings
    {
        u32 style_tag = 0u;
        amal::vec2 size = AUIK_SIZE_FIT;
        amal::vec2 min_size{0.0f, 0.0f};

        DockLayoutNodeSettings() = default;
        DockLayoutNodeSettings(const amal::vec2 &node_size, const amal::vec2 &node_min_size = {0.0f, 0.0f})
        {
            size = node_size;
            min_size = node_min_size;
        }
    };

    class DockLayout final : public Widget, public umbf::Block
    {
    public:
        AUIK_EXPORT explicit DockLayout(u32 id, const amal::vec2 &inline_size, WidgetFlags widget_flags);
        AUIK_EXPORT ~DockLayout() override;

        DockLayoutNodeID root_node() const { return _tree.root_node(); }
        AUIK_EXPORT DockLayoutNodeID create_split(DockLayoutNodeID parent, amal::axis axis,
                                                  DockLayoutNodeSettings settings = {});
        AUIK_EXPORT DockLayoutNodeID create_leaf(DockLayoutNodeID parent, DockLayoutNodeSettings settings = {});
        AUIK_EXPORT void set_split_axis(DockLayoutNodeID node, amal::axis axis);
        AUIK_EXPORT void set_node_settings(DockLayoutNodeID node, DockLayoutNodeSettings settings);
        AUIK_EXPORT void add_child(DockLayoutNodeID node, Widget *child,
                                   ChildLayoutFlags layout = default_child_layout_flags());
        // Detach operations keep widgets alive; the caller takes ownership.
        AUIK_EXPORT bool remove_child(DockLayoutNodeID node, Widget *child);
        AUIK_EXPORT void clear_node(DockLayoutNodeID node);
        AUIK_EXPORT void clear();

        AUIK_EXPORT StyleUpdateFlags update_style() override;
        AUIK_EXPORT void update_layout_min_size_force() override;
        AUIK_EXPORT void update_layout(bool min_size_known) override;
        AUIK_EXPORT void translate(const amal::vec2 &delta) override;
        AUIK_EXPORT void reset_clip_rect_records() override;
        AUIK_EXPORT void rebuild_clip_rects() override;
        AUIK_EXPORT void reset_draw_records() override;
        AUIK_EXPORT void invalidate_style() override;
        AUIK_EXPORT bool update_locale() override;
        AUIK_EXPORT void add_state_flags_inherit(WidgetStateFlags flags) override;
        AUIK_EXPORT void remove_state_flags_inherit(WidgetStateFlags flags) override;
        AUIK_EXPORT u32 get_depth_requirement() const override;
        AUIK_EXPORT void update_depth(const amal::vec2 &depth_range) override;
        AUIK_EXPORT void draw(DrawCtx &ctx) override;
        AUIK_EXPORT void on_attach() override;
        AUIK_EXPORT void on_detach() override;
        u16 content_clip_id() const override { return clip_id(); }
        amal::vec4 get_content_clip_rect() const override { return get_clip_rect(clip_id()); }
        u32 signature() const noexcept override { return AUIK_TAG_DOCK_LAYOUT; }
        umbf::Block *as_snapshot_block() noexcept override { return this; }

    private:
        struct Item
        {
            Widget *widget = nullptr;
            ChildLayoutFlags layout = default_child_layout_flags();
        };

        struct Node
        {
            DockLayoutNodeSettings settings{};
            amal::axis axis = amal::axis::x;
            DockLayoutNodeID parent = 0xFFFFFFFFu;
            acul::vector<DockLayoutNodeID> children;
            acul::vector<Item> items;
            amal::rect bounds{};
            amal::vec2 required_size{0.0f, 0.0f};
            amal::vec2 style_size{AUIK_SIZE_X_FIT, AUIK_SIZE_Y_FIT};
            amal::vec2 min_size{0.0f, 0.0f};
        };

        template <bool Const>
        class BasicItemIterator
        {
            using DockPtr = std::conditional_t<Const, const DockLayout *, DockLayout *>;

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

            reference operator*() const { return _dock->_tree.nodes()[_node_index].items[_item_index]; }
            pointer operator->() const { return &_dock->_tree.nodes()[_node_index].items[_item_index]; }

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
                while (_node_index < _dock->_tree.nodes().size() &&
                       _item_index >= _dock->_tree.nodes()[_node_index].items.size())
                {
                    ++_node_index;
                    _item_index = 0u;
                }
                if (_node_index >= _dock->_tree.nodes().size()) _item_index = 0u;
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
            using DockPtr = std::conditional_t<Const, const DockLayout *, DockLayout *>;
            using Iterator = BasicItemIterator<Const>;

        public:
            explicit BasicItemRange(DockPtr dock) : _dock(dock) {}

            Iterator begin() const { return Iterator{_dock, 0u, 0u}; }
            Iterator end() const { return Iterator{_dock, _dock ? _dock->_tree.nodes().size() : 0u, 0u}; }

        private:
            DockPtr _dock = nullptr;
        };

        using item_range = BasicItemRange<false>;
        using const_item_range = BasicItemRange<true>;

        item_range items() { return item_range{this}; }
        const_item_range items() const { return const_item_range{this}; }
        item_iterator item_begin() { return item_iterator{this, 0u, 0u}; }
        item_iterator item_end() { return item_iterator{this, _tree.nodes().size(), 0u}; }
        const_item_iterator item_begin() const { return const_item_iterator{this, 0u, 0u}; }
        const_item_iterator item_end() const { return const_item_iterator{this, _tree.nodes().size(), 0u}; }

        Node *get_node(DockLayoutNodeID id);
        const Node *get_node(DockLayoutNodeID id) const;
        DockLayoutNodeID create_node(DockLayoutNodeID parent, bool split, DockLayoutNodeSettings settings);
        void update_node_style_cache(Node &node);
        amal::vec2 measure_node(DockLayoutNodeID id);
        void layout_node(DockLayoutNodeID id, const amal::rect &bounds);
        void attach_item(Item &item);
        void detach_item(Item &item);
        void layout_item(const amal::rect &bounds, Item &item);
        detail::DockTree<Node> _tree;

        friend struct DockLayoutStreamAccess;
    };

    inline DockLayout *make_dock_layout(u32 id, const amal::vec2 &inline_size = AUIK_SIZE_INHERIT)
    {
        return acul::alloc<DockLayout>(
            id, inline_size, WidgetFlagBits::visible | WidgetFlagBits::attachable | WidgetFlagBits::cache_snapshot);
    }

    namespace streams
    {
        extern AUIK_EXPORT const umbf::registry::BlockStream dock_layout;
    }
} // namespace auik
