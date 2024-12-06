#pragma once

#include <imgui/imgui_internal.h>
#include "../menu/menu.hpp"
#include "types.hpp"

namespace uikit
{
    namespace dock
    {
        class StretchChangeEvent : public events::IEvent
        {
        public:
            ImVec2 pos;
            ImVec2 size;

            StretchChangeEvent(const std::string &name = "", ImVec2 pos = ImVec2(0, 0), ImVec2 size = ImVec2(0, 0))
                : IEvent(name), pos(pos), size(size)
            {
            }
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

        class Space;

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
            struct Style
            {
                struct Helper
                {
                    f32 width;
                    f32 cap_offset;
                    ImU32 color;
                    ImU32 hover_color;
                } helper;
                f32 stretch_min_size;
                f32 height_resize_limit;
                ImU32 node_overlay_color;
                struct Tabbar
                {
                    f32 item_spacing;
                    f32 min_size;
                    ImU32 background;
                    uikit::Icon *navIcon = nullptr;
                } tabbar;
            } style;
            astl::vector<Section> sections;
            PopupMenu popupMenu;

            explicit Space(events::Manager *e, DisposalQueue &disposalQueue)
                : Widget("dockspace"), _e(e), _disposalQueue(disposalQueue)
            {
                popupMenu.space = this;
            }

            ~Space()
            {
                for (auto &section : sections)
                    for (auto &node : section.nodes) node.destroy(_e);
            }

            virtual void render() override;

            int stretchID() const { return _stretch_id; }

            int hoverSection() const { return _hovered ? _frame.hover_section : -1; }

            bool hovered() const { return _hovered; }

            void closeWindow(int si, int ni, int tab_id)
            {
                closeWindow(si, ni, tab_id, [](uikit::Window *window) { astl::release(window); });
            }

            void closeWindow(int si, int ni, int tab_id, const std::function<void(uikit::Window *window)> &callback);

        private:
            FrameState _frame;
            int _stretch_id = INT_MAX;
            ImVec2 _window_size;
            bool _hovered = false;
            events::Manager *_e;
            DisposalQueue &_disposalQueue;

            friend class PopupMenu;

            void drawSection(int index, ImVec2 size, ImVec2 &pos);
            f32 prerenderSectionNodes(int index);

            void drawHelperV(int index, const ImVec2 &pos);
            void drawHelpersH(int si, ImRect *nodes_bb);
            ImVec2 drawNode(int si, int ni, f32 *offset = nullptr); // return drawn content size
            void drawNodeDragOverlay(int si, ImRect *nodes_bb, f32 *nodes_offsets);

            template <typename T>
            void updateHoverState(T &subject, bool hovered, const ImVec2 &mouse_pos)
            {
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                {
                    if (_hovered && hovered)
                    {
                        _frame.flags |= FrameStateFlagBits::resizing;
                        subject.isResizing = true;
                    }
                    _frame.mouse_pos = mouse_pos;
                }
                else if (subject.isResizing && (ImGui::IsMouseReleased(ImGuiMouseButton_Left) || mouse_pos.x < 0))
                {
                    _frame.flags &= ~FrameStateFlagBits::resizing;
                    subject.isResizing = false;
                }
            }

            void updateDragState(bool hovered, int si, int ni, bool is_content = true);

            void allocNodeTabbar(Node &node);
            void drawTabbar(int si, int ni, ImVec2 &pos, ImVec2 &size);
        };
    } // namespace dock
} // namespace uikit