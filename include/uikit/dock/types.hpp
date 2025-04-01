#pragma once

#include "../tab/tab.hpp"
#include "../window/window.hpp"

namespace uikit
{
    namespace dock
    {
        struct SectionFlagBits
        {
            enum enum_type
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

            using flag_bitmask = std::true_type;
        };

        using SectionFlags = acul::flags<SectionFlagBits>;

        struct Node
        {
            acul::string id;
            f32 size;
            acul::vector<Window *> windows;
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

            Node(const acul::vector<Window *> &windows, bool isTransparent = false, f32 size = 0.0f)
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

            WindowDockFlags dockFlags() const { return windows.front()->dockFlags; }

            void reset()
            {
                id = "";
                if (tabNav)
                {
                    tabNav->tabbar.name = "";
                    tabNav->tabbar.size({0, 0});
                }
            }

            void destroy(acul::events::dispatcher *ed)
            {
                for (auto window : windows) acul::release(window);
                if (tabNav)
                {
                    ed->unbind_listeners(&tabNav->tabbar);
                    acul::release(tabNav);
                }
            }
        };

        struct Section
        {
            acul::string id;
            SectionFlags flags;
            f32 size = 0.0f;
            f32 min_size = 50.0f;
            f32 fixed_size = 0;
            f32 content_height = 0.0f;
            acul::vector<Node> nodes;
            bool isResizing = false;
            size_t fixed_sections = 0;

            Section(const acul::vector<Node> &nodes, SectionFlags flags, const acul::string &id = "",
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

            Section(const acul::vector<Node> &nodes, SectionFlags flags, f32 size)
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

        struct FrameStateFlagBits
        {
            enum enum_type
            {
                none = 0x0,
                resizing = 0x1,
                op_locked = 0x2,
                dropped = 0x4,
                layout_update = 0x8
            };

            using flag_bitmask = std::true_type;
        };

        using FrameStateFlags = acul::flags<FrameStateFlagBits>;

        struct FrameState
        {
            ImVec2 mouse_pos;
            ImVec2 padding;
            ImVec2 item_spacing;
            Section *hover_section = nullptr;
            FrameStateFlags flags = FrameStateFlagBits::none;
        };

        namespace event_id
        {
            enum : u64
            {
                none = 0x0,
                windowDocked = 0x1AFFF3AAFCE36195,
                newTab = 0x34493086A50E2106,
                undock = 0x2CFFB00717B9FC3A
            };
        };
    } // namespace dock
} // namespace uikit