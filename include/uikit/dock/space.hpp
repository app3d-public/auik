#pragma once

#include <imgui/imgui_internal.h>
#include "../menu/menu.hpp"
#include "types.hpp"

namespace uikit
{
    namespace style
    {
        extern APPLIB_API struct Dock
        {
            struct Helper
            {
                ImVec2 size;
                f32 cap_offset;
                ImU32 color;
                ImU32 hover_color;
            } helper;
            f32 top_offset = 0;
            f32 bottom_offset = 0;
            f32 stretch_min_size;
            f32 stretch_section_size;
            f32 height_resize_limit;
            ImU32 node_overlay_color;
            struct Tabbar
            {
                f32 item_spacing;
                f32 min_size;
                ImU32 background;
                uikit::Icon *navIcon = nullptr;
            } tabbar;
        } g_Dock;
    } // namespace style

    namespace dock
    {
        class Space;

        class ChangeEvent : public acul::events::event
        {
        public:
            Space *space;
            const char *window_id;
            int si; // section index
            int ni; // node index

            ChangeEvent(u64 id, Space *space = nullptr, const char *window_id = "", int si = -1, int ni = -1)
                : event(id), space(space), window_id(window_id), si(si), ni(ni)
            {
            }
        };

        class APPLIB_API PopupMenu : public Widget
        {
        public:
            Space *space;

            PopupMenu();

            virtual void render();

            void tryOpen(const ImVec2 &padding);

            inline void markOpenned(const ImRect &bb, int si, int ni)
            {
                _isOpenned = true;
                _btn_bb = bb;
                _si = si;
                _ni = ni;
            }

        private:
            ImVec2 _size;
            ImVec2 _pos;
            MenuItem _commonItems[3];
            bool _isOpenned;
            ImRect _btn_bb;
            int _si, _ni;
        };

        class APPLIB_API Space : public Widget
        {
        public:
            FrameState frame;
            acul::vector<Section> sections;
            PopupMenu popupMenu;

            explicit Space(acul::events::dispatcher *ed, acul::disposal_queue &disposalQueue)
                : Widget("dockspace"), _ed(ed), _disposalQueue(disposalQueue)
            {
                popupMenu.space = this;
            }

            ~Space()
            {
                for (auto &section : sections)
                    for (auto &node : section.nodes) node.destroy(_ed);
            }

            void renderContent();

            virtual void render() override;

            Section *hoverSection() const { return _hovered ? frame.hover_section : nullptr; }

            bool hovered() const { return _hovered; }

            void closeWindow(int si, int ni, int tab_id)
            {
                closeWindow(si, ni, tab_id, [](uikit::Window *window) { acul::release(window); });
            }

            void closeWindow(int si, int ni, int tab_id, const std::function<void(uikit::Window *window)> &callback);

            ImVec2 windowSize() const { return _window_size; }

            void drawNode(int section_id, int node_id, f32 section_size, ImVec2 &pos, bool isFirst);

        private:
            i8 _stretch_min = -1, _stretch_max = -1;
            ImVec2 _window_size;
            bool _hovered = false;
            acul::events::dispatcher *_ed;
            acul::disposal_queue &_disposalQueue;

            friend class PopupMenu;

            void drawSection(int index, ImVec2 size, ImVec2 &pos);
            f32 prerenderSectionNodes(int index);

            void resizeBoxVertical(int section_index, const ImRect &draw_bb, f32 stretch_size);
            void resizeBoxVerticalBounds(Section &section, int si, const ImRect &draw_bb, const ImRect &hover_bb);
            void resizeBoxHorizontal(Section &section, int node_index, const ImVec2 &pos, f32 size);
            void drawOverlayLayer(const ImVec2 &init_pos, f32 stretch_size);
            void nodeDragOverlay(int si, int ni, const ImVec2 &pos, const ImVec2 &node_rect);
            ImVec2 drawNode(int si, int ni, f32 section_size); // return drawn content size

            template <typename T>
            void updateHoverState(T &subject, bool hovered, const ImVec2 &mouse_pos)
            {
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                {
                    if (_hovered && hovered)
                    {
                        frame.flags |= FrameStateFlagBits::resizing;
                        subject.isResizing = true;
                    }
                    frame.mouse_pos = mouse_pos;
                }
                else if (subject.isResizing && (ImGui::IsMouseReleased(ImGuiMouseButton_Left) || mouse_pos.x < 0))
                {
                    frame.flags &= ~FrameStateFlagBits::resizing;
                    subject.isResizing = false;
                }
            }

            void updateDragState(bool hovered, int si, int ni, bool is_content = true);

            void allocNodeTabbar(Node &node);
            void drawTabbar(int si, int ni, ImVec2 &size);
        };

        APPLIB_API bool tryInsertNode(Section &section, Node &node, size_t index, f32 height);

        inline void resetFrame(FrameState &frame)
        {
            auto &style = ImGui::GetStyle();
            frame.padding = style.WindowPadding;
            frame.item_spacing = style.ItemSpacing;
            frame.flags &= ~(FrameStateFlagBits::op_locked | FrameStateFlagBits::layout_update);
        }

        inline f32 getAvaliableWidth(Window *window, Space *dock, f32 min_width = 5.0f)
        {
            if (window->dockFlags & WindowDockFlags_Docked && dock->frame.flags & FrameStateFlagBits::layout_update)
                return min_width;
            return ImMax(ImGui::GetContentRegionAvail().x, min_width);
        }
    } // namespace dock
} // namespace uikit