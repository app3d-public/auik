#pragma once

#include <imgui/imgui_internal.h>
#include "../menu/menu.hpp"
#include "types.hpp"

namespace auik
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
                auik::Icon *nav_icon = nullptr;
            } tabbar;
        } g_dock;
    } // namespace style

    namespace dock
    {
        class Space;

        class ChangeEvent : public acul::events::event
        {
        public:
            Space *space;
            const char *window_id;
            int section_id; // section index
            int node_id;    // node index

            ChangeEvent(u64 id, Space *space = nullptr, const char *window_id = "", int section_id = -1,
                        int node_id = -1)
                : event(id), space(space), window_id(window_id), section_id(section_id), node_id(node_id)
            {
            }
        };

        class APPLIB_API PopupMenu : public Widget
        {
        public:
            Space *space;

            PopupMenu();

            virtual void render();

            void try_open(const ImVec2 &padding);

            inline void mark_openned(const ImRect &bb, int section_id, int node_id)
            {
                _is_openned = true;
                _btn_bb = bb;
                _section_id = section_id;
                _node_id = node_id;
            }

        private:
            ImVec2 _size;
            ImVec2 _pos;
            MenuItem _common_items[3];
            bool _is_openned;
            ImRect _btn_bb;
            int _section_id, _node_id;
        };

        class APPLIB_API Space : public Widget
        {
        public:
            FrameState frame;
            acul::vector<Section> sections;
            PopupMenu popup_menu;

            explicit Space(acul::events::dispatcher *ed, acul::disposal_queue &disposal_queue)
                : Widget("dockspace"), _ed(ed), _disposal_queue(disposal_queue)
            {
                popup_menu.space = this;
            }

            ~Space()
            {
                for (auto &section : sections)
                    for (auto &node : section.nodes) node.destroy(_ed);
            }

            void render_content();

            virtual void render() override;

            Section *hover_section() const { return _hovered ? frame.hover_section : nullptr; }

            bool hovered() const { return _hovered; }

            void close_window(int section_id, int node_id, int tab_id)
            {
                close_window(section_id, node_id, tab_id, [](auik::Window *window) { acul::release(window); });
            }

            void close_window(int section_id, int node_id, int tab_id,
                              acul::unique_function<void(auik::Window *window)> callback);

            ImVec2 window_size() const { return _window_size; }

            void draw_node(int section_id, int node_id, f32 section_size, ImVec2 &pos, bool is_first);

        private:
            i8 _stretch_min = -1, _stretch_max = -1;
            ImVec2 _window_size;
            bool _hovered = false;
            acul::events::dispatcher *_ed;
            acul::disposal_queue &_disposal_queue;

            friend class PopupMenu;

            void draw_section(int index, ImVec2 size, ImVec2 &pos);
            f32 prerender_section_nodes(int index);

            void resize_box_vertical(int section_id, const ImRect &draw_bb, f32 stretch_size);
            void resize_box_vertical_bounds(Section &section, int update_id, const ImRect &draw_bb,
                                            const ImRect &hover_bb);
            void resize_box_horizontal(Section &section, int node_id, const ImVec2 &pos, f32 size);
            void draw_overlay_layer(const ImVec2 &init_pos, f32 stretch_size);
            void node_drag_overlay(int section_id, int node_id, const ImVec2 &pos, const ImVec2 &node_rect);
            ImVec2 draw_node(int section_id, int node_id, f32 section_size); // return drawn content size

            template <typename T>
            void update_hover_state(T &subject, bool hovered, const ImVec2 &mouse_pos)
            {
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                {
                    if (_hovered && hovered)
                    {
                        frame.flags |= FrameStateFlagBits::resizing;
                        subject.is_resizing = true;
                    }
                    frame.mouse_pos = mouse_pos;
                }
                else if (subject.is_resizing && (ImGui::IsMouseReleased(ImGuiMouseButton_Left) || mouse_pos.x < 0))
                {
                    frame.flags &= ~FrameStateFlagBits::resizing;
                    subject.is_resizing = false;
                }
            }

            void update_drag_state(bool hovered, int section_id, int node_id, bool is_content = true);

            void alloc_node_tabbar(Node &node);
            void draw_tabbar(int section_id, int node_id, ImVec2 &size);
        };

        APPLIB_API bool try_insert_node(Section &section, Node &node, size_t index, f32 height);

        inline void reset_frame(FrameState &frame)
        {
            auto &style = ImGui::GetStyle();
            frame.padding = style.WindowPadding;
            frame.item_spacing = style.ItemSpacing;
            frame.flags &= ~(FrameStateFlagBits::op_locked | FrameStateFlagBits::layout_update);
        }

        inline f32 get_avaliable_width(Window *window, Space *dock, f32 min_width = 5.0f)
        {
            if (window->dock_flags & WindowDockFlags_Docked && dock->frame.flags & FrameStateFlagBits::layout_update)
                return min_width;
            return ImMax(ImGui::GetContentRegionAvail().x, min_width);
        }
    } // namespace dock
} // namespace auik