#include <astl/string.hpp>
#include <uikit/dock/dockspace.hpp>
#include <window/window.hpp>

namespace uikit
{
    namespace dock
    {
        void Space::updateHelper(bool hovered, int si, int ni, const ImRect &bb)
        {
            ImGuiContext &g = *GImGui;
            if (ImGui::IsMouseDragging(ImGuiMouseButton_Left))
                _frame.dropStarted = true;
            else if (_frame.dropStarted)
            {
                if (g.NavWindow) _e->dispatch<ChangeEvent>("ds:window_docked", g.NavWindow->Name, si, ni);
                _frame.dropStarted = false;
            }
        }

        void Space::drawHelperV(int index, const ImVec2 &pos)
        {
            auto window = ImGui::GetCurrentWindow();
            auto *draw_list = window->DrawList;
            ImVec2 child_size = window->ContentRegionRect.GetSize();
            ImVec2 mouse_pos = ImGui::GetMousePos();
            auto &helper_style = style.helper;
            if (index == -1)
            {
                ImRect draw_bb;
                draw_bb.Min = ImVec2(pos.x, pos.y);
                draw_bb.Max = ImVec2(pos.x + helper_style.width, pos.y + child_size.y);
                ImRect hover_bb;
                hover_bb.Min = draw_bb.Min;
                hover_bb.Max = draw_bb.Max + ImVec2(helper_style.cap_offset, 0);
                if (updateHelper(hover_bb.Contains(mouse_pos), 0, -1, draw_bb, mouse_pos))
                {
                    draw_list->AddRectFilled(draw_bb.Min, draw_bb.Max, style.helper.hover_color);
                    _frame.isAnyLineHovered = true;
                }
            }
            else
            {
                ImRect draw_bb;
                draw_bb.Min = {pos.x + child_size.x - helper_style.width, pos.y};
                draw_bb.Max = {pos.x + child_size.x, pos.y + child_size.y};
                ImRect hover_bb;
                hover_bb.Min = {draw_bb.Min.x - helper_style.cap_offset, draw_bb.Min.y};
                hover_bb.Max = {draw_bb.Max.x + helper_style.cap_offset, draw_bb.Max.y};

                bool hovered = hover_bb.Contains(mouse_pos);
                bool held = ImGui::IsMouseDown(ImGuiMouseButton_Left);
                if (index == sections.size() - 1)
                {
                    if (updateHelper(hovered, index + 1, -1, draw_bb, mouse_pos))
                    {
                        draw_list->AddRectFilled(draw_bb.Min, draw_bb.Max, style.helper.hover_color);
                        _frame.isAnyLineHovered = true;
                    }
                }
                else
                {
                    int active_index = index >= _stretch_id && index != sections.size() - 1 ? index + 1 : index;
                    Section &active_section = sections[active_index];
                    bool isDrawn = updateHelper(hovered, active_index, -1, draw_bb, mouse_pos, &active_section);
                    if (!held)
                    {
                        if (active_section.isResizing) _frame.isResizing = false;
                        active_section.isResizing = false;
                    }

                    if (hovered || active_section.isResizing)
                    {
                        if (_hovered || _frame.isResizing) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                        draw_list->AddRectFilled(draw_bb.Min, draw_bb.Max, helper_style.hover_color);
                        _frame.isAnyLineHovered = true;
                        if (active_section.isResizing)
                        {
                            float delta_x = mouse_pos.x - _frame.mouse_pos.x;
                            if (index >= _stretch_id) delta_x = -delta_x;
                            float stretch_size = _stretch_id == INT_MAX ? FLT_MAX : sections[_stretch_id].size;
                            if (!(delta_x > 0 && stretch_size < style.stretch_min_size))
                                active_section.size =
                                    ImClamp(active_section.size + delta_x, active_section.min_size, FLT_MAX);
                            _frame.mouse_pos = mouse_pos;
                        }
                    }
                    else if (active_section.flags & SectionFlagBits::showHelper ||
                             (index < _stretch_id - 1) ||                           // If left more than 1
                             (index == _stretch_id + 1 || index > _stretch_id + 1)) // If right more than 1
                        draw_list->AddRectFilled(draw_bb.Min, draw_bb.Max, helper_style.color);
                }
            }
        }

        ImVec2 Space::drawNode(int si, int ni)
        {
            auto &section = sections[si];
            auto &node = section.nodes[ni];
            if (node.id.empty()) node.id = astl::format("dock:s-%d-%d", si, ni);
            ImVec2 node_size{section.size, node.size};
            ImGui::BeginChild(node.id.c_str(), node_size, ImGuiChildFlags_AlwaysUseWindowPadding);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, _frame.item_spacing);
            ImGui::BeginGroup();
            node.window->render();
            ImGui::EndGroup();
            ImVec2 node_rect = ImGui::GetItemRectSize();
            ImGui::PopStyleVar();
            ImGui::EndChild();
            return node_rect;
        }

        void Space::drawHelpersH(int si, ImRect *nodes_bb)
        {
            auto *window = ImGui::GetCurrentWindow();
            auto *draw_list = window->DrawList;
            auto &helper_style = style.helper;
            auto &section = sections[si];
            for (int i = 0; i < section.nodes.size() - 1; ++i)
            {
                auto &draw_bb = nodes_bb[i];
                ImRect hover_line_bb;
                hover_line_bb.Min = {draw_bb.Min.x, draw_bb.Min.y - helper_style.cap_offset};
                hover_line_bb.Max = {draw_bb.Max.x, draw_bb.Max.y + helper_style.cap_offset};
                ImVec2 mouse_pos = ImGui::GetMousePos();
                bool hovered_line = hover_line_bb.Contains(mouse_pos);
                bool held = ImGui::IsMouseDown(ImGuiMouseButton_Left);
                auto &node = section.nodes[i];
                if (hovered_line)
                {
                    updateHoverState(&node, mouse_pos);
                    if (!_hovered && !_frame.isResizing) updateHelper(hovered_line, si, i, draw_bb);
                }

                if (!held)
                {
                    if (node.isResizing) _frame.isResizing = false;
                    node.isResizing = false;
                }
                if (hovered_line || node.isResizing)
                {
                    if (_hovered || _frame.isResizing) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
                    draw_list->AddRectFilled(draw_bb.Min, draw_bb.Max, helper_style.hover_color);
                    _frame.isAnyLineHovered = true;
                    if (node.isResizing)
                    {
                        float delta_y = mouse_pos.y - _frame.mouse_pos.y;
                        int next_node_id = -1;
                        for (int j = i + 1; j < section.nodes.size(); ++j)
                        {
                            if (section.nodes[j].window->isDockStretched)
                            {
                                next_node_id = j;
                                break;
                            }
                        }
                        if (next_node_id != -1 && node.window->isDockStretched)
                        {
                            auto &next_node = section.nodes[next_node_id];
                            auto new_size = node.size + delta_y;
                            auto next_size = next_node.size - delta_y;
                            if (new_size >= style.height_resize_limit && next_size >= style.height_resize_limit)
                            {
                                node.size = new_size;
                                next_node.size = next_size;
                            }
                        }
                        _frame.mouse_pos = mouse_pos;
                    }
                }
                else
                    draw_list->AddRectFilled(nodes_bb[i].Min, nodes_bb[i].Max, style.helper.color);
            }
        }

        float Space::prerenderSectionNodes(int index)
        {
            auto &section = sections[index];
            section.fixed_size = 0.0f;
            section.fixed_sections = 0;
            float height = ImGui::GetContentRegionAvail().y;
            float content_max = 0.0f;
            for (int i = 0; i < section.nodes.size(); ++i)
            {
                ImGui::SetCursorPosY(0);
                auto &node = section.nodes[i];
                ImVec2 node_rect = drawNode(index, i);
                content_max = ImMax(content_max, node_rect.x);
                if (!section.nodes[i].window->isDockStretched)
                {
                    node.size = node_rect.y + _frame.padding.y * 2.0f;
                    section.fixed_size += node.size;
                    ++section.fixed_sections;
                }
            }
            // Init start stretch sizes
            float ss_size = (height - section.fixed_size) / (section.nodes.size() - section.fixed_sections);
            for (auto &node : section.nodes)
                if (node.window->isDockStretched) node.size = ss_size;
            return content_max;
        }

        void Space::drawNodeDragOverlay(int si, ImRect *nodes_bb)
        {
            auto &section = sections[si];
            for (int i = 0; i < section.nodes.size(); ++i)
            {
                auto &node = section.nodes[i];
                ImVec2 mouse_pos = ImGui::GetMousePos();
                bool hovered = nodes_bb[i].Contains(mouse_pos);
                if (!_hovered && !_frame.isResizing && hovered)
                {
                    updateHelper(hovered, si, i, nodes_bb[i]);
                    auto *draw_list = ImGui::GetForegroundDrawList();
                    if (_frame.dropStarted)
                        draw_list->AddRectFilled(nodes_bb[i].Min, nodes_bb[i].Max, style.node_overlay_color);
                }
            }
        }

        void Space::drawSection(int index, ImVec2 size, ImVec2 &pos)
        {
            auto &section = sections[index];
            if (section.size != 0.0f)
                ImGui::SetCursorScreenPos(pos);
            else
                ImGui::SetCursorPosY(0);
            if (section.id.empty()) section.id = astl::format("dock:s-%d", index);
            ImGui::BeginChild(section.id.c_str(), size, 0, ImGuiWindowFlags_NoScrollbar);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            bool regular_behavior = false;
            ImRect node_lines_bb[section.nodes.size()];
            ImRect node_content_bb[section.nodes.size()];
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, _frame.padding);

            if (section.flags & SectionFlagBits::stretch)
            {
                section.size = size.x;
                drawNode(index, 0);
            }
            else if (section.size == 0.0f)
            {
                section.size = ImMax(prerenderSectionNodes(index) + _frame.padding.x * 2.0f, 50.0f);
                section.min_size = section.size;
            }
            else
            {
                regular_behavior = true;
                for (int i = 0; i < section.nodes.size(); ++i)
                {
                    auto &node = section.nodes[i];
                    auto node_pos = ImGui::GetCursorScreenPos();
                    node_content_bb[i].Min = node_pos;
                    node_content_bb[i].Max = ImVec2(node_pos.x + section.size, node_pos.y + node.size);
                    ImVec2 node_rect = drawNode(index, i);
                    node_pos = ImGui::GetCursorScreenPos();
                    node_lines_bb[i].Min = node_pos;
                    node_lines_bb[i].Max = ImVec2(node_pos.x + section.size, node_pos.y + style.helper.width);
                }
                drawHelpersH(index, node_lines_bb);
            }
            ImGui::PopStyleVar();

            drawHelperV(index, pos);
            if (index == 0) drawHelperV(-1, pos);

            if (regular_behavior && !_frame.isAnyLineHovered) // For right behavior need to render H helpers after V
                drawNodeDragOverlay(index, node_content_bb);

            if (_frame.isResizing)
                _frame.hover_section = -1;
            else if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows))
                _frame.hover_section = index;
            ImGui::PopStyleVar();
            ImGui::EndChild();
        }

        void Space::render()
        {
            ImGuiContext &g = *GImGui;
            auto &style = g.Style;
            _frame.padding = style.WindowPadding;
            _frame.item_spacing = style.ItemSpacing;
            _frame.isAnyLineHovered = false;
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            auto bg = ImGui::ColorConvertFloat4ToU32(style.Colors[ImGuiCol_WindowBg]);
            ImGui::PushStyleColor(ImGuiCol_ChildBg, bg);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, 0);
            ImGuiViewport *viewport = ImGui::GetMainViewport();
            float topOffset = g.NextWindowData.PosVal.y;
            float bottomOffset = 0;
            ImVec2 window_size{viewport->Size.x, viewport->Size.y - topOffset - bottomOffset};
            ImGui::SetNextWindowSize(window_size);

            ImGuiWindowFlags windowFlags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse |
                                           ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
                                           ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBringToFrontOnFocus;
            ImGui::Begin("dockspace", nullptr, windowFlags);
            _hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
            auto window = g.CurrentWindow;
            auto pos = window->DC.CursorPos;
            auto init_pos = pos;
            bool update_layout = false;
            // Left sections
            for (int i = 0; i < sections.size(); i++)
            {
                auto &section = sections[i];
                ImVec2 child_size = ImVec2(0, window_size.y);
                if (section.flags & SectionFlagBits::stretch)
                {
                    _stretch_id = i;
                    break;
                }
                if (section.size == 0.0f)
                    update_layout = true;
                else
                    child_size.x = section.size;
                drawSection(i, child_size, pos);
                pos.x += child_size.x;
            }
            if (_stretch_id == INT_MAX) _stretch_id = sections.size();

            // Right section
            auto left_pos = pos;
            float max_width = ImGui::GetWindowContentRegionMax().x;
            pos.x = max_width;
            float right_rendered_width = 0;

            for (int i = sections.size() - 1; i > _stretch_id; --i)
            {
                auto &section = sections[i];
                ImVec2 child_size = ImVec2(0, window_size.y);
                if (section.size != 0.0f)
                {
                    child_size.x = section.size;
                    right_rendered_width += child_size.x;
                    pos.x = max_width - right_rendered_width;
                }
                else
                {
                    pos.x -= child_size.x;
                    update_layout = true;
                }
                drawSection(i, child_size, pos);
            }

            // Center
            ImU32 stretch_color = 0;
            auto stretch_size = sections[_stretch_id].size;
            if (!(sections[_stretch_id].flags & SectionFlagBits::transparent)) stretch_color = bg;
            ImGui::PushStyleColor(ImGuiCol_ChildBg, stretch_color);
            drawSection(_stretch_id, ImVec2(pos.x - left_pos.x, 0), left_pos);
            ImGui::PopStyleColor();

            if (stretch_size != 0.0f && !update_layout)
            {
                ImVec2 oldSize{stretch_size, _window_size.y};
                ImVec2 newSize{sections[_stretch_id].size, window_size.y};
                if (newSize != oldSize) _e->dispatch<StretchChangeEvent>("ds:stretch_changed", newSize);
            }
            _window_size = window_size;
            ImGui::End();
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(2);
            if (update_layout) window::pushEmptyEvent();
        }
    } // namespace dock
} // namespace uikit