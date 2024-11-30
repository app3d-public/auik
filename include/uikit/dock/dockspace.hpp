#pragma once

#include <astl/enum.hpp>
#include <astl/vector.hpp>
#include <core/api.hpp>
#include <core/event.hpp>
#include <imgui/imgui_internal.h>
#include <uikit/window/window.hpp>

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
            std::string id = "";
            float size = 0.0f;
            Window *window;
            bool isResizing = false;
        };

        struct Section
        {
            std::string id;
            SectionFlags flags;
            float size = 0.0f;
            float min_size = 50.0f;
            float fixed_size = FLT_MAX;
            astl::vector<Node> nodes;
            bool isResizing = false;
            size_t fixed_sections = 0;

            void reset()
            {
                size = 0;
                id = "";
                for (auto &node : nodes)
                {
                    node.id = "";
                    node.size = 0;
                }
            }
        };

        class APPLIB_API Space : public Widget
        {
        public:
            struct Style
            {
                struct HelperStyle
                {
                    float width;
                    float cap_offset;
                    ImU32 color;
                    ImU32 hover_color;
                } helper;
                float stretch_min_size;
                float height_resize_limit;
                ImU32 node_overlay_color;
            } style;
            astl::vector<Section> sections;

            explicit Space(events::Manager *e) : Widget("dockspace"), _e(e) {}

            ~Space()
            {
                for (auto &section : sections)
                    for (auto &node : section.nodes) astl::release(node.window);
            }

            void addSectionNodes(const astl::vector<Window *> &windows, SectionFlags flags = SectionFlagBits::none)
            {
                astl::vector<Node> nodes(windows.size());
                for (int i = 0; i < windows.size(); i++) nodes[i].window = windows[i];
                sections.emplace_back("", flags, 0, 50, 0, nodes);
            }

            virtual void render() override;

            int stretchID() const { return _stretch_id; }

            int hoverSection() const { return _hovered ? _frame.hover_section : -1; }

            bool hovered() const { return _hovered; }

        private:
            struct FrameState
            {
                ImVec2 mouse_pos;
                ImVec2 padding;
                ImVec2 item_spacing;
                int hover_section;
                bool dropStarted = false;
                bool isAnyLineHovered = false;
                bool isResizing = false;
            } _frame;
            int _stretch_id = INT_MAX;
            ImVec2 _window_size;
            bool _hovered = false;
            events::Manager *_e;

            void drawSection(int index, ImVec2 size, ImVec2 &pos);
            float prerenderSectionNodes(int index);

            void updateHelper(bool hovered, int si, int ni, const ImRect &bb);
            // Return true if helper was drawn
            inline bool updateHelper(bool hovered, int si, int ni, const ImRect &bb, const ImVec2 &mouse_pos,
                                     Section *section = nullptr)
            {
                if (!hovered) return false;
                updateHoverState(section, mouse_pos);
                if (_hovered || _frame.isResizing) return false;
                updateHelper(hovered, si, ni, bb);
                return true;
            }

            void drawHelperV(int index, const ImVec2 &pos);
            void drawHelpersH(int si, ImRect *nodes_bb);
            ImVec2 drawNode(int si, int ni); // return drawn content size
            void drawNodeDragOverlay(int si, ImRect *nodes_bb);

            template <typename T>
            void updateHoverState(T *subject, const ImVec2 &mouse_pos)
            {
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                {
                    if (subject)
                    {
                        subject->isResizing = _hovered;
                        if (_hovered) _frame.isResizing = true;
                    }
                    _frame.mouse_pos = mouse_pos;
                }
            }
        };

        class StretchChangeEvent : public events::IEvent
        {
        public:
            ImVec2 size;

            StretchChangeEvent(const std::string &name = "", ImVec2 size = ImVec2(0, 0)) : IEvent(name), size(size) {}
        };

        class ChangeEvent : public events::IEvent
        {
        public:
            const char *window_id;
            int si; // section index
            int ni; // node index

            ChangeEvent(const std::string &name = "", const char *window_id = "", int si = -1, int ni = -1)
                : IEvent(name), window_id(window_id), si(si), ni(ni)
            {
            }
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