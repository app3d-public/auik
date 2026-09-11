#pragma once

#include <acul/vector.hpp>
#include <utility>
#include <cstdint>
#include <cassert>

namespace auik::detail
{
    // Owns node storage and links only. Payload lifetime belongs to the container.
    // Detached nodes retain their IDs and remain accessible for serialization.
    template <class Node>
    class DockTree
    {
    public:
        using NodeID = std::uint32_t;
        static constexpr NodeID invalid_node = 0xFFFFFFFFu;

        DockTree() { reset(); }
        NodeID root_node() const { return 0u; }
        acul::vector<Node> &nodes() { return _nodes; }
        const acul::vector<Node> &nodes() const { return _nodes; }
        Node *get(NodeID id) { return id < _nodes.size() ? &_nodes[id] : nullptr; }
        const Node *get(NodeID id) const { return id < _nodes.size() ? &_nodes[id] : nullptr; }
        bool attached(NodeID id) const
        {
            return id < _nodes.size() && (id == root_node() || _nodes[id].parent != invalid_node);
        }

        NodeID append(NodeID parent, Node node, size_t position = static_cast<size_t>(-1))
        {
            assert(parent < _nodes.size() && "parent dock node is invalid");
            const NodeID id = static_cast<NodeID>(_nodes.size());
            node.parent = parent;
            _nodes.push_back(std::move(node));
            auto &children = _nodes[parent].children;
            if (position >= children.size()) children.push_back(id);
            else children.insert(children.begin() + position, id);
            return id;
        }

        void detach(NodeID id)
        {
            auto *node = get(id);
            if (!node || id == root_node()) return;
            if (auto *parent = get(node->parent))
                for (size_t i = 0; i < parent->children.size(); ++i)
                    if (parent->children[i] == id)
                    {
                        parent->children.erase(parent->children.begin() + i);
                        break;
                    }
            node->parent = invalid_node;
        }

        // Callers must dispose of owned payloads before resetting the tree.
        void reset()
        {
            _nodes.clear();
            Node root{};
            root.parent = invalid_node;
            _nodes.push_back(std::move(root));
        }

    private:
        acul::vector<Node> _nodes;
    };
} // namespace auik::detail
