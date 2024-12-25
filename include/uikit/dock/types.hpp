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
            hide_top_line = 0x1,
            discard_padding = 0x2,
            stretch = 0x4,
            sync_stretch_hresize = 0x8,
            // [Internal flags]
            scrollbar_hovered = 0x10,
            op_locked = 0x20
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
            bool isTransparent;
            f32 next_offset;
            f32 extra_offset;

            Node(const astl::vector<Window *> &windows, bool isTransparent = false, f32 size = 0.0f)
                : id(""),
                  size(size),
                  windows(windows),
                  tabNav(nullptr),
                  isResizing(false),
                  isTransparent(isTransparent),
                  next_offset(0.0f),
                  extra_offset(0.0f)
            {
            }

            void reset()
            {
                id = "";
                if (tabNav)
                {
                    tabNav->tabbar.name = "";
                    tabNav->tabbar.size({0, 0});
                }
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
            f32 fixed_size = 0;
            f32 content_height = 0.0f;
            astl::vector<Node> nodes;
            bool isResizing = false;
            size_t fixed_sections = 0;

            Section(const astl::vector<Node> &nodes, SectionFlags flags, const std::string &id = "",
                    f32 min_size = 50.0f)
                : id(id),
                  flags(flags),
                  size(0.0f),
                  min_size(min_size),
                  fixed_size(0),
                  nodes(nodes),
                  isResizing(false),
                  fixed_sections(0)
            {
            }

               Section(const astl::vector<Node> &nodes, SectionFlags flags, f32 size)
                : id(""),
                  flags(flags),
                  size(size),
                  min_size(50.0f),
                  fixed_size(0),
                  nodes(nodes),
                  isResizing(false),
                  fixed_sections(0)
            {
                
            }

            void reset()
            {
                size = 0;
                id = "";
                for (auto &node : nodes) node.reset();
            }
        };

        enum class FrameStateFlagBits
        {
            none = 0x0,
            resizing = 0x1,
            op_locked = 0x2,
            dropped = 0x4,
            layout_update = 0x8
        };

        using FrameStateFlags = Flags<FrameStateFlagBits>;

        struct FrameState
        {
            ImVec2 mouse_pos;
            ImVec2 padding;
            ImVec2 item_spacing;
            Section *hover_section = nullptr;
            FrameStateFlags flags = FrameStateFlagBits::none;
        };
    } // namespace dock
} // namespace uikit

template <>
struct FlagTraits<uikit::dock::SectionFlagBits>
{
    static constexpr bool isBitmask = true;
    static constexpr uikit::dock::SectionFlags allFlags =
        uikit::dock::SectionFlagBits::none | uikit::dock::SectionFlagBits::hide_top_line |
        uikit::dock::SectionFlagBits::discard_padding | uikit::dock::SectionFlagBits::stretch |
        uikit::dock::SectionFlagBits::sync_stretch_hresize | uikit::dock::SectionFlagBits::scrollbar_hovered |
        uikit::dock::SectionFlagBits::op_locked;
};

template <>
struct FlagTraits<uikit::dock::FrameStateFlagBits>
{
    static constexpr bool isBitmask = true;
    static constexpr uikit::dock::FrameStateFlags allFlags =
        uikit::dock::FrameStateFlagBits::none | uikit::dock::FrameStateFlagBits::resizing |
        uikit::dock::FrameStateFlagBits::op_locked | uikit::dock::FrameStateFlagBits::dropped |
        uikit::dock::FrameStateFlagBits::layout_update;
};