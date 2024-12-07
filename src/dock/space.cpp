#include <astl/string.hpp>
#include <core/locales.hpp>
#include <core/task.hpp>
#include <uikit/dock/space.hpp>
#include <window/window.hpp>

namespace uikit
{
    namespace style
    {
        Dock g_Dock{};
    }

    namespace dock
    {
        void Space::updateDragState(bool hovered, int si, int ni, bool is_content)
        {
            ImGuiContext &g = *GImGui;
            if (_hovered || _frame.flags & FrameStateFlagBits::resizing) return;
            if (ImGui::IsMouseDragging(ImGuiMouseButton_Left) && g.MovingWindow &&
                !(g.MovingWindow->ChildFlags & WindowDockFlags_NoDock))
                _frame.flags |= FrameStateFlagBits::dropped;
            else if (_frame.flags & FrameStateFlagBits::dropped && hovered)
            {
                _frame.flags &= ~FrameStateFlagBits::dropped;
                if (g.NavWindow)
                    _e->dispatch<ChangeEvent>(is_content ? "ds:window_docked" : "ds:newtab", g.NavWindow->Name, si, ni);
            }
        }

        void Space::drawHelperV(int index, const ImVec2 &pos)
        {
            auto window = ImGui::GetCurrentWindow();
            ImVec2 child_size = window->ContentRegionRect.GetSize();
            ImVec2 mouse_pos = ImGui::GetMousePos();
            auto &helper_style = style::g_Dock.helper;
            bool hovered = !(_frame.flags & FrameStateFlagBits::op_locked);
            if (index == -1)
            {
                ImRect draw_bb;
                draw_bb.Min = ImVec2(pos.x, pos.y);
                draw_bb.Max = ImVec2(pos.x + helper_style.width, pos.y + child_size.y);
                ImRect hover_bb;
                hover_bb.Min = draw_bb.Min;
                hover_bb.Max = draw_bb.Max + ImVec2(helper_style.cap_offset, 0);
                hovered = hover_bb.Contains(mouse_pos) && hovered;
                updateDragState(hovered, 0, -1);
                if (hovered && _frame.flags & FrameStateFlagBits::dropped)
                {
                    auto *draw_list = ImGui::GetForegroundDrawList();
                    draw_list->AddRectFilled(draw_bb.Min, draw_bb.Max, style::g_Dock.helper.hover_color);
                    _frame.flags |= FrameStateFlagBits::op_locked;
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

                hovered = hover_bb.Contains(mouse_pos) && !sections[index].isScrollbarHovered && hovered;
                if (index == sections.size() - 1)
                {
                    updateDragState(hovered, index + 1, -1);
                    if (hovered && _frame.flags & FrameStateFlagBits::dropped)
                    {
                        auto *draw_list = ImGui::GetForegroundDrawList();
                        draw_list->AddRectFilled(draw_bb.Min, draw_bb.Max, style::g_Dock.helper.hover_color);
                        _frame.flags |= FrameStateFlagBits::op_locked;
                    }
                }
                else
                {
                    int active_index = index >= _stretch_id && index != sections.size() - 1 ? index + 1 : index;
                    Section &active_section = sections[active_index];
                    updateHoverState(active_section, hovered, mouse_pos);

                    auto *draw_list = window->DrawList;
                    if (hovered || active_section.isResizing)
                    {
                        updateDragState(hovered, index + 1, -1);
                        if (_hovered || _frame.flags & FrameStateFlagBits::resizing)
                            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

                        draw_list->AddRectFilled(draw_bb.Min, draw_bb.Max, helper_style.hover_color);
                        _frame.flags |= FrameStateFlagBits::op_locked;
                        if (active_section.isResizing)
                        {
                            f32 delta_x = mouse_pos.x - _frame.mouse_pos.x;
                            if (index >= _stretch_id) delta_x = -delta_x;
                            f32 stretch_size = _stretch_id == INT_MAX ? FLT_MAX : sections[_stretch_id].size;
                            if (!(delta_x > 0 && stretch_size < style::g_Dock.stretch_min_size))
                                active_section.size =
                                    ImClamp(active_section.size + delta_x, active_section.min_size, FLT_MAX);
                            _frame.mouse_pos = mouse_pos;
                            window::pushEmptyEvent();
                        }
                    }
                    else if (active_section.flags & SectionFlagBits::showHelper ||
                             (index < _stretch_id - 1) ||                           // If left more than 1
                             (index == _stretch_id + 1 || index > _stretch_id + 1)) // If right more than 1
                        draw_list->AddRectFilled(draw_bb.Min, draw_bb.Max, helper_style.color);
                }
            }
        }

        void Space::allocNodeTabbar(Node &node)
        {
            astl::vector<TabItem> items;
            items.reserve(node.windows.size());
            for (int i = 0; i < node.windows.size(); ++i)
            {
                u64 id = std::hash<std::string>()(node.windows[i]->name);
                items.emplace_back(id, node.windows[i]->name);
            }
            TabBar::Style style;
            style.size = {0, 0};
            TabBar tabbar(node.id + ":tab", _e, _disposalQueue, items, TabBar::FlagBits::reorderable, style);
            Selectable btn(astl::format("##%s:btn", node.id.c_str()), false, 2.0f, 0);
            node.tabNav = astl::alloc<Node::TabNavArea>(std::move(tabbar), std::move(btn));
            _e->bindEvent(&node.tabNav->tabbar, "tabbar:switched", [&node](uikit::TabChangeEvent &e) {
                if (e.tabbar != &node.tabNav->tabbar) return;
                auto it = std::find_if(node.windows.begin(), node.windows.end(),
                                       [&](Window *w) { return w->name == e.current->name; });
                if (it == node.windows.end()) return;
                node.tabNav->window_id = it - node.windows.begin();
            });
        }

        void Space::drawTabbar(int si, int ni, ImVec2 &pos, ImVec2 &size)
        {
            auto &node = sections[si].nodes[ni];
            if (!node.tabNav) allocNodeTabbar(node);
            auto &nav = *node.tabNav;
            if (nav.tabbar.name.empty())
            {
                nav.tabbar.name = node.id + ":tab";
                nav.btn.name = astl::format("##%s:btn", node.id.c_str());
            }
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {_frame.item_spacing.x, style::g_Dock.tabbar.item_spacing});
            ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, {0, 0});
            auto &colors = ImGui::GetStyle().Colors;
            ImGui::PushStyleColor(ImGuiCol_Header, colors[ImGuiCol_ChildBg]);
            ImGui::PushStyleColor(ImGuiCol_ChildBg, style::g_Dock.tabbar.background);
            pos.y += style::g_Dock.helper.width;
            ImGui::SetCursorPos(pos);

            size.y = ImGui::GetFrameHeight() + style::g_Dock.tabbar.item_spacing * 2.0f;

            ImVec2 start_pos = ImGui::GetCursorScreenPos();
            ImVec2 icon_pos = start_pos;
            f32 nav_size = style::g_Dock.tabbar.navIcon->width() + _frame.padding.x * 2.0f;

            icon_pos.x += ImGui::GetWindowWidth() - style::g_Dock.tabbar.navIcon->width() - _frame.padding.x;
            icon_pos.y += (size.y - style::g_Dock.tabbar.navIcon->height()) / 2.0f - style::g_Dock.helper.width;
            f32 icon_rect_width = style::g_Dock.tabbar.navIcon->width() + _frame.padding.x * 2.0f;

            nav.tabbar.size({ImGui::GetContentRegionAvail().x - icon_rect_width, 0});
            nav.tabbar.render();
            size.x = ImGui::GetWindowWidth() - nav.tabbar.avaliableWidth() + icon_rect_width;

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
            // bg
            f32 window_width = ImGui::GetWindowWidth();
            ImRect bg_bb;
            bg_bb.Min = {start_pos.x + window_width - nav_size, start_pos.y};
            bg_bb.Max = {start_pos.x + window_width, start_pos.y + nav.tabbar.height()};
            auto *draw_list = ImGui::GetCurrentWindow()->DrawList;
            draw_list->AddRectFilled(bg_bb.Min, bg_bb.Max, style::g_Dock.tabbar.background);

            // btn
            ImGui::PushStyleColor(ImGuiCol_Header, style::g_Dock.tabbar.background);
            nav.btn.size = {nav_size, nav.tabbar.height()};
            ImGui::SetCursorScreenPos(bg_bb.Min);
            nav.btn.render();
            if (nav.btn.pressed) popupMenu.markOpenned(bg_bb, si, ni);

            // icon
            style::g_Dock.tabbar.navIcon->render(icon_pos);
            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar(4);
        }

        bool isScrollbarHovered()
        {
            ImGuiWindow *window = ImGui::GetCurrentWindow();
            if (window->ScrollbarY)
            {
                ImVec2 min{window->InnerRect.Max.x, window->InnerRect.Min.y};
                ImVec2 max{window->InnerRect.Max.x + ImGui::GetStyle().ScrollbarSize, window->InnerRect.Max.y};
                ImRect bb(min, max);
                if (bb.Contains(ImGui::GetMousePos())) return true;
            }
            return false;
        }

        ImVec2 Space::drawNode(int si, int ni, f32 *offset)
        {
            auto &section = sections[si];
            auto &node = section.nodes[ni];
            if (node.id.empty())
            {
                node.id = astl::format("dock:s-%d-%d", si, ni);
                node.dockFlags = node.windows.front()->dockFlags;
            }
            ImVec2 node_size{section.size, node.size};
            ImVec2 tabbar_size{0, 0};
            auto pos = ImGui::GetCursorPos();
            auto init_pos = pos;
            size_t window_id = 0;
            f32 min_width = 0;
            if (node.dockFlags & WindowDockFlags_TabMenu)
            {
                drawTabbar(si, ni, pos, tabbar_size);
                window_id = node.tabNav->window_id;
                min_width = style::g_Dock.tabbar.min_size;
            }
            ImGui::SetCursorPos({init_pos.x, init_pos.y + tabbar_size.y});
            ImGui::BeginChild(node.id.c_str(), node_size, ImGuiChildFlags_AlwaysUseWindowPadding);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, _frame.item_spacing);
            ImGui::BeginGroup();
            node.windows[window_id]->render();
            ImGui::EndGroup();
            ImVec2 node_rect;
            if (section.size == 0)
            {
                node_rect = ImGui::GetItemRectSize();
                node_rect.x = ImMax(ImMax(node_rect.x, tabbar_size.x), min_width);
            }
            else
                node_rect = {section.size, node.size};
            ImGui::PopStyleVar();
            if (!section.isScrollbarHovered) section.isScrollbarHovered = isScrollbarHovered();

            ImGui::EndChild();
            if (offset) *offset = tabbar_size.y;
            return node_rect;
        }

        void Space::drawHelpersH(int si, ImRect *nodes_bb)
        {
            auto *window = ImGui::GetCurrentWindow();
            auto *draw_list = window->DrawList;
            auto &helper_style = style::g_Dock.helper;
            auto &section = sections[si];
            for (int i = 0; i < section.nodes.size(); ++i)
            {
                auto &draw_bb = nodes_bb[i];
                ImRect hover_line_bb;
                hover_line_bb.Min = {draw_bb.Min.x, draw_bb.Min.y - helper_style.cap_offset};
                hover_line_bb.Max = {draw_bb.Max.x, draw_bb.Max.y + helper_style.cap_offset};
                ImVec2 mouse_pos = ImGui::GetMousePos();
                bool hovered_line = hover_line_bb.Contains(mouse_pos);
                auto &node = section.nodes[i];
                bool allow_resize = i != 0 && node.dockFlags & WindowDockFlags_Stretch;
                if (allow_resize) updateHoverState(node, hovered_line, mouse_pos);
                if (hovered_line || node.isResizing)
                {
                    if (allow_resize && _hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
                    draw_list->AddRectFilled(draw_bb.Min, draw_bb.Max, helper_style.hover_color);
                    _frame.flags |= FrameStateFlagBits::op_locked;
                    if (node.isResizing)
                    {
                        f32 delta_y = mouse_pos.y - _frame.mouse_pos.y;
                        int target_node_id = -1;
                        for (int j = i - 1; j >= 0; --j)
                        {
                            if (section.nodes[j].dockFlags & WindowDockFlags_Stretch)
                            {
                                target_node_id = j;
                                break;
                            }
                        }

                        if (target_node_id != -1)
                        {
                            auto &target_node = section.nodes[target_node_id];
                            auto new_size = node.size - delta_y;
                            auto target_new_size = target_node.size + delta_y;

                            if (new_size >= style::g_Dock.height_resize_limit &&
                                target_new_size >= style::g_Dock.height_resize_limit)
                            {
                                node.size = new_size;
                                target_node.size = target_new_size;
                            }
                        }
                        _frame.mouse_pos = mouse_pos;
                    }
                }
                else
                    draw_list->AddRectFilled(nodes_bb[i].Min, nodes_bb[i].Max, style::g_Dock.helper.color);
            }
        }

        f32 Space::prerenderSectionNodes(int index)
        {
            auto &section = sections[index];
            section.fixed_size = 0.0f;
            section.fixed_sections = 0;
            f32 height = ImGui::GetContentRegionAvail().y;
            f32 content_max = 0.0f;
            f32 top_offsets[section.nodes.size()];
            for (int i = 0; i < section.nodes.size(); ++i)
            {
                ImGui::SetCursorPosY(0);
                auto &node = section.nodes[i];
                ImVec2 node_rect = drawNode(index, i, &top_offsets[i]);
                content_max = ImMax(content_max, node_rect.x);
                if (!(section.nodes[i].dockFlags & WindowDockFlags_Stretch))
                {
                    node.size = node_rect.y + _frame.padding.y * 2.0f;
                    section.fixed_size += node.size;
                    ++section.fixed_sections;
                }
            }
            // Init start stretch sizes
            f32 ss_size = (height - section.fixed_size) / (section.nodes.size() - section.fixed_sections);
            for (int i = 0; i < section.nodes.size(); ++i)
            {
                auto &node = section.nodes[i];
                if (node.dockFlags & WindowDockFlags_Stretch) node.size = ss_size - top_offsets[i];
            }
            return content_max;
        }

        void Space::drawNodeDragOverlay(int si, ImRect *nodes_bb, f32 *nodes_offsets)
        {
            auto &section = sections[si];
            for (int i = 0; i < section.nodes.size(); ++i)
            {
                auto &node = section.nodes[i];
                ImVec2 mouse_pos = ImGui::GetMousePos();
                bool hovered = nodes_bb[i].Contains(mouse_pos) && g_LastCursor == 0;
                ImGuiContext &g = *GImGui;
                f32 diff = mouse_pos.y - nodes_bb[i].Min.y;
                bool is_content = diff > nodes_offsets[i];
                updateDragState(hovered, si, i, is_content);
                if (!hovered || !(_frame.flags & FrameStateFlagBits::dropped)) continue;
                _frame.flags |= FrameStateFlagBits::op_locked;
                ImRect bb = nodes_bb[i];
                if (is_content)
                {
                    bb.Min.y += nodes_offsets[i];
                    bb.Max.y += nodes_offsets[i];
                }
                else
                    bb.Max.y = bb.Min.y + nodes_offsets[i];
                auto *draw_list = ImGui::GetForegroundDrawList();
                draw_list->AddRectFilled(bb.Min, bb.Max, style::g_Dock.node_overlay_color);
            }
        }

        void Space::drawSection(int index, ImVec2 size, ImVec2 &pos)
        {
            auto &section = sections[index];
            section.isScrollbarHovered = false;
            if (section.size != 0.0f)
                ImGui::SetCursorScreenPos(pos);
            else
                ImGui::SetCursorPosY(0);
            if (section.id.empty()) section.id = astl::format("dock:s-%d", index);
            ImGui::BeginChild(section.id.c_str(), size, 0, ImGuiWindowFlags_NoScrollbar);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            bool drawOverlay = false;
            ImRect node_lines_bb[section.nodes.size()];
            ImRect node_content_bb[section.nodes.size()];
            f32 node_offsets[section.nodes.size()];
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, _frame.padding);

            if (section.flags & SectionFlagBits::stretch)
            {
                section.size = size.x;
                auto node_pos = ImGui::GetCursorScreenPos();
                node_content_bb[0].Min = node_pos;
                node_content_bb[0].Max = ImVec2(node_pos.x + size.x, node_pos.y + style::g_Dock.helper.width);
                drawNode(index, 0);
                drawHelpersH(index, node_content_bb);
            }
            else if (section.size == 0.0f)
            {
                section.size = ImMax(prerenderSectionNodes(index) + _frame.padding.x * 2.0f, 50.0f);
                section.min_size = section.size;
            }
            else
            {
                drawOverlay = true;
                for (int i = 0; i < section.nodes.size(); ++i)
                {
                    auto &node = section.nodes[i];
                    auto node_pos = ImGui::GetCursorScreenPos();
                    node_content_bb[i].Min = node_pos;
                    node_content_bb[i].Max = ImVec2(node_pos.x + section.size, node_pos.y + node.size);
                    node_lines_bb[i].Min = node_pos;
                    node_lines_bb[i].Max = ImVec2(node_pos.x + section.size, node_pos.y + style::g_Dock.helper.width);
                    drawNode(index, i, &node_offsets[i]);
                }
                drawHelpersH(index, node_lines_bb);
            }
            ImGui::PopStyleVar();

            drawHelperV(index, pos);
            if (index == 0) drawHelperV(-1, pos);

            if (drawOverlay && !(_frame.flags & FrameStateFlagBits::op_locked))
                drawNodeDragOverlay(index, node_content_bb, node_offsets);

            if (_frame.flags & FrameStateFlagBits::resizing)
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

            // Reset frame
            _frame.padding = style.WindowPadding;
            _frame.item_spacing = style.ItemSpacing;
            _frame.flags &= ~FrameStateFlagBits::op_locked;

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            auto bg = ImGui::ColorConvertFloat4ToU32(style.Colors[ImGuiCol_WindowBg]);
            ImGui::PushStyleColor(ImGuiCol_ChildBg, bg);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, 0);
            ImGuiViewport *viewport = ImGui::GetMainViewport();
            ImVec2 window_size{viewport->Size.x, viewport->Size.y - style::g_Dock.topOffset - style::g_Dock.bottomOffset};
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
            f32 max_width = ImGui::GetWindowContentRegionMax().x;
            pos.x = max_width;
            f32 right_rendered_width = 0;

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
                if (newSize != oldSize && (newSize.x > 0.0f && newSize.y > 0.0f))
                    _e->dispatch<StretchChangeEvent>("ds:stretch_changed", ImVec2(left_pos.x, style::g_Dock.topOffset),
                                                     newSize);
            }
            _window_size = window_size;
            popupMenu.tryOpen(_frame.padding);

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, _frame.padding);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, _frame.item_spacing);
            popupMenu.render();
            ImGui::PopStyleVar(2);

            ImGui::End();
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(2);
            if (update_layout) window::pushEmptyEvent();
            if (!ImGui::IsMouseDragging(ImGuiMouseButton_Left)) _frame.flags &= ~FrameStateFlagBits::dropped;
        }

        void Space::closeWindow(int si, int ni, int tab_id, const std::function<void(uikit::Window *window)> &callback)
        {
            auto &node = sections[si].nodes[ni];
            std::string name = node.tabNav->tabbar.items[tab_id].name;
            auto it = std::find_if(node.windows.begin(), node.windows.end(), [&](auto &w) { return w->name == name; });
            if (it == node.windows.end()) return;
            node.tabNav->tabbar.removeTab(node.tabNav->tabbar.items[tab_id]);
            _disposalQueue.push(astl::alloc<task::MemCache>(task::addTask([callback, it, &node, si, ni, this]() {
                callback(*it);
                node.windows.erase(it);
                auto &section = sections[si];
                if (node.windows.empty())
                {
                    node.destroy(_e);
                    section.nodes.erase(section.nodes.begin() + ni);
                }
                else
                {
                    auto new_active_it = std::find_if(node.windows.begin(), node.windows.end(), [&](auto &w) {
                        return w->name == node.tabNav->tabbar.activeTab().name;
                    });
                    if (new_active_it == node.windows.end())
                        node.tabNav->window_id = 0;
                    else
                        node.tabNav->window_id = new_active_it - node.windows.begin();
                }
                if (section.nodes.empty())
                    sections.erase(sections.begin() + si);
                else
                    section.reset();
            })));
        }

        PopupMenu::PopupMenu()
            : Widget("dock:menu"),
              _commonItems{{_("dock:menu:undock"),
                            [this]() {
                                auto window_name = space->sections[_si].nodes[_ni].tabNav->tabbar.activeTab().name;
                                space->_e->dispatch<ChangeEvent>("ds:undock", window_name.c_str(), _si, _ni);
                            }},
                           {_("dock:menu:close"),
                            [this]() {
                                auto &node = space->sections[_si].nodes[_ni];
                                space->closeWindow(_si, _ni, node.tabNav->tabbar.activeIndex);
                            }},
                           {_("dock:menu:close:all"),
                            [this]() {
                                auto &window = space->sections[_si].nodes[_ni].windows;
                                for (int i = 0; i < window.size(); ++i) space->closeWindow(_si, _ni, 0);
                            }}},
              _isOpenned(false)
        {
        }

        void PopupMenu::tryOpen(const ImVec2 &padding)
        {
            if (!_isOpenned) return;
            bool isSizeCalculated = false;
            if (ImGui::IsPopupOpen(name.c_str()))
            {
                if (_size.x - padding.x * 2.0f != 0.0f)
                {
                    _isOpenned = false;
                    _pos = _btn_bb.Max;
                    if (_btn_bb.Max.x >= _size.x) _pos.x -= _size.x;
                }
                else
                    _pos = {-1000.0f, -1000.0f};
            }
            ImGui::SetNextWindowPos(_pos, ImGuiCond_Always);
            ImGui::OpenPopup(name.c_str());
        }

        void PopupMenu::render()
        {
            if (ImGui::BeginPopup(name.c_str()))
            {
                for (auto &item : _commonItems) item.render();
                _size = ImGui::GetWindowSize();
                ImGui::EndPopup();
            }
        }
    } // namespace dock
} // namespace uikit