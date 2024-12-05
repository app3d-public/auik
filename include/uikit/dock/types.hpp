#pragma once

#include "../tab/tab.hpp"
#include "../window/window.hpp"

namespace uikit
{
    namespace dock
    {
        enum class SectionFlagBits
        {
            none = 0x0,
            stretch = 0x1,
            transparent = 0x2,
            showHelper = 0x4
        };

        using SectionFlags = Flags<SectionFlagBits>;

        struct Node
        {
            std::string id;
            f32 size;
            astl::vector<Window *> windows;
            WindowDockFlags dockFlags;
            struct TabNavArea
            {
                TabBar tabbar;
                Selectable btn;
                size_t window_id = 0;
            } *tabNav;
            bool isResizing;

            Node(const astl::vector<Window *> &windows)
                : id(""), size(0.0f), windows(windows), tabNav(nullptr), isResizing(false)
            {
            }

            void destroy(events::Manager *ed)
            {
                for (auto window : windows) astl::release(window);
                if (tabNav)
                {
                    ed->unbindListeners(&tabNav->tabbar);
                    astl::release(tabNav);
                }
            }
        };

        struct Section
        {
            std::string id;
            SectionFlags flags;
            f32 size = 0.0f;
            f32 min_size = 50.0f;
            f32 fixed_size = FLT_MAX;
            astl::vector<Node> nodes;
            bool isResizing = false;
            bool isScrollbarHovered = false;
            size_t fixed_sections = 0;

            Section(const astl::vector<Node> &nodes, SectionFlags flags = SectionFlagBits::none,
                    const std::string &id = "", f32 min_size = 50.0f)
                : id(id),
                  flags(flags),
                  size(0.0f),
                  min_size(min_size),
                  fixed_size(FLT_MAX),
                  nodes(nodes),
                  isResizing(false),
                  fixed_sections(0)
            {
            }

            void reset()
            {
                size = 0;
                id = "";
                for (auto &node : nodes)
                {
                    node.id = "";
                    node.size = 0;
                    if (node.tabNav) node.tabNav->tabbar.name = "";
                }
            }
        };

        enum class FrameStateFlagBits
        {
            none = 0x0,
            resizing = 0x1,
            op_locked = 0x2,
            dropped = 0x4,
        };

        using FrameStateFlags = Flags<FrameStateFlagBits>;

        struct FrameState
        {
            ImVec2 mouse_pos;
            ImVec2 padding;
            ImVec2 item_spacing;
            int hover_section;
            FrameStateFlags flags = FrameStateFlagBits::none;
        };
    } // namespace dock
} // namespace uikit

template <>
struct FlagTraits<uikit::dock::SectionFlagBits>
{
    static constexpr bool isBitmask = true;
    static constexpr uikit::dock::SectionFlags allFlags =
        uikit::dock::SectionFlagBits::none | uikit::dock::SectionFlagBits::stretch |
        uikit::dock::SectionFlagBits::transparent | uikit::dock::SectionFlagBits::showHelper;
};

template <>
struct FlagTraits<uikit::dock::FrameStateFlagBits>
{
    static constexpr bool isBitmask = true;
    static constexpr uikit::dock::FrameStateFlags allFlags =
        uikit::dock::FrameStateFlagBits::none | uikit::dock::FrameStateFlagBits::resizing |
        uikit::dock::FrameStateFlagBits::op_locked | uikit::dock::FrameStateFlagBits::dropped;
};