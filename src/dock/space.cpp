#include <astl/hash.hpp>
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
            if (_hovered || frame.flags & FrameStateFlagBits::resizing) return;
            if (ImGui::IsMouseDragging(ImGuiMouseButton_Left) && g.MovingWindow &&
                !(g.MovingWindow->ChildFlags & WindowDockFlags_NoDock))
                frame.flags |= FrameStateFlagBits::dropped;
            else if (frame.flags & FrameStateFlagBits::dropped && hovered)
            {
                frame.flags &= ~FrameStateFlagBits::dropped;
                if (g.NavWindow)
                    _e->dispatch<ChangeEvent>(is_content ? "ds:window_docked" : "ds:newtab", this, g.NavWindow->Name,
                                              si, ni);
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

        void Space::drawTabbar(int si, int ni, ImVec2 &size)
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
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {frame.item_spacing.x, style::g_Dock.tabbar.item_spacing});
            ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, {0, 0});
            auto &colors = ImGui::GetStyle().Colors;
            ImGui::PushStyleColor(ImGuiCol_Header, colors[ImGuiCol_ChildBg]);
            ImGui::PushStyleColor(ImGuiCol_ChildBg, style::g_Dock.tabbar.background);

            size.y = ImGui::GetFrameHeight() + style::g_Dock.tabbar.item_spacing;

            ImVec2 start_pos = ImGui::GetCursorScreenPos();
            f32 icon_rect_width = style::g_Dock.tabbar.navIcon->size().x + frame.padding.x * 2.0f;

            nav.tabbar.render();
            if (sections[si].size == 0.0f)
                nav.tabbar.size({ImGui::GetWindowWidth() - nav.tabbar.avaliableWidth(), size.y});
            size.x = ImMax(nav.tabbar.size().x + icon_rect_width, sections[si].size);

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
            // bg
            ImRect bg_bb;
            bg_bb.Min = {start_pos.x + nav.tabbar.size().x, start_pos.y};
            bg_bb.Max = start_pos + size;
            auto *draw_list = ImGui::GetCurrentWindow()->DrawList;
            draw_list->AddRectFilled(bg_bb.Min, bg_bb.Max, style::g_Dock.tabbar.background);

            // btn
            ImGui::PushStyleColor(ImGuiCol_Header, style::g_Dock.tabbar.background);
            nav.btn.size = {icon_rect_width, size.y};
            ImVec2 btn_pos = {bg_bb.Max.x - icon_rect_width, start_pos.y};
            ImGui::SetCursorScreenPos(btn_pos);
            nav.btn.render();
            if (nav.btn.pressed) popupMenu.markOpenned(bg_bb, si, ni);

            // icon
            ImVec2 nav_icon_size = style::g_Dock.tabbar.navIcon->size();
            ImVec2 icon_pos = btn_pos;
            icon_pos.x += (icon_rect_width - nav_icon_size.x) / 2.0f;
            icon_pos.y += (size.y - nav_icon_size.y) / 2.0f;
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

        ImVec2 Space::drawNode(int si, int ni, f32 section_size)
        {
            auto &section = sections[si];
            auto &node = section.nodes[ni];

            if (node.id.empty()) node.id = astl::format("%s:%d", section.id.c_str(), ni);
            ImVec2 node_size{section_size, node.size};
            auto init_pos = ImGui::GetCursorPos();
            if (node.dockFlags() & WindowDockFlags_Stretch)
            {
                f32 avaliable_ss_height = ImGui::GetWindowHeight() - section.fixed_size;
                bool justify =
                    ni == section.nodes.size() - 1 ||
                    (ni == section.nodes.size() - 2 && !(section.nodes[ni + 1].dockFlags() & WindowDockFlags_Stretch));
                f32 full_ss = justify ? avaliable_ss_height - init_pos.y : avaliable_ss_height * node.size;
                node_size.y = (int)(full_ss - node.extra_offset);
                if (ni != section.nodes.size() - 1) node_size.y -= style::g_Dock.helper.size.y;
            }
            ImVec2 tabbar_size{0, 0};
            size_t window_id = 0;
            f32 min_width = 0;
            if (node.dockFlags() & WindowDockFlags_TabMenu)
            {
                drawTabbar(si, ni, tabbar_size);
                window_id = node.tabNav->window_id;
                min_width = style::g_Dock.tabbar.min_size;
            }
            ImGui::SetCursorPos({init_pos.x, init_pos.y + tabbar_size.y});
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, frame.item_spacing);
            auto *window = node.windows[window_id];
            window->updateStyleStack();
            auto child_flags = section.flags & SectionFlagBits::discard_padding
                                   ? ImGuiChildFlags_None
                                   : ImGuiChildFlags_AlwaysUseWindowPadding;
            ImGui::BeginChild(node.id.c_str(), node_size, child_flags, window->imguiFlags);
            ImGui::BeginGroup();
            window->render();
            ImGui::EndGroup();
            ImVec2 node_rect;
            if (section.size == 0)
            {
                node_rect = ImGui::GetItemRectSize();
                node_rect.x += ImGui::GetStyle().WindowPadding.x * 2.0f;
                node_rect.x = ImMax(ImMax(node_rect.x, tabbar_size.x), min_width);
            }
            else
                node_rect = node_size;
            ImGui::PopStyleVar();
            if (!(section.flags & SectionFlagBits::scrollbar_hovered))
            {
                bool scrollbar_hovered = isScrollbarHovered();
                if (scrollbar_hovered) section.flags |= SectionFlagBits::scrollbar_hovered;
            }
            if (frame.flags & FrameStateFlagBits::resizing)
                frame.hover_section = nullptr;
            else if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows))
                frame.hover_section = &sections[si];
            ImGui::EndChild();
            node.extra_offset = tabbar_size.y;
            return node_rect;
        }

        void Space::resizeBoxHorizontal(Section &section, int node_index, const ImVec2 &pos, f32 size)
        {
            auto &node = section.nodes[node_index];
            auto *window = ImGui::GetCurrentWindow();
            auto *draw_list = window->DrawList;
            ImRect draw_bb, hover_bb;
            draw_bb.Min = {pos.x, pos.y - style::g_Dock.helper.size.y};
            draw_bb.Max = {pos.x + size, pos.y};
            hover_bb.Min = {pos.x, pos.y - style::g_Dock.helper.cap_offset};
            hover_bb.Max = {draw_bb.Max.x, draw_bb.Max.y + style::g_Dock.helper.cap_offset};
            ImVec2 mouse_pos = ImGui::GetMousePos();
            bool hovered = hover_bb.Contains(mouse_pos) && !(frame.flags & FrameStateFlagBits::op_locked);
            bool allowResize = node_index != 0 && node.dockFlags() & WindowDockFlags_Stretch;

            if (allowResize) updateHoverState(node, hovered, mouse_pos);
            if (hovered || node.isResizing)
            {
                if (allowResize && (_hovered || node.isResizing)) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
                draw_list->AddRectFilled(draw_bb.Min, draw_bb.Max, style::g_Dock.helper.hover_color);
                frame.flags |= FrameStateFlagBits::op_locked;
                if (node.isResizing)
                {
                    f32 delta_y = mouse_pos.y - frame.mouse_pos.y;
                    int target_node_id = -1;
                    for (int j = node_index - 1; j >= 0; --j)
                    {
                        if (section.nodes[j].dockFlags() & WindowDockFlags_Stretch)
                        {
                            target_node_id = j;
                            break;
                        }
                    }

                    if (target_node_id != -1)
                    {
                        auto &target_node = section.nodes[target_node_id];
                        f32 avaliable_ss_height = ImGui::GetWindowHeight() - section.fixed_size;
                        f32 delta_ratio = delta_y / avaliable_ss_height;

                        f32 max_delta_node = node.size - (style::g_Dock.height_resize_limit / avaliable_ss_height);
                        f32 max_delta_target =
                            (style::g_Dock.height_resize_limit / avaliable_ss_height) - target_node.size;

                        f32 clamped_delta_ratio = std::clamp(delta_ratio, max_delta_target, max_delta_node);
                        if (clamped_delta_ratio != 0.0f)
                        {
                            if (section.flags & SectionFlagBits::sync_stretch_hresize)
                            {
                                for (int i = _stretch_min; i <= _stretch_max; i++)
                                {
                                    sections[i].nodes[node_index].next_offset = -clamped_delta_ratio;
                                    sections[i].nodes[target_node_id].next_offset = clamped_delta_ratio;
                                }
                            }
                            else
                            {
                                node.next_offset = -clamped_delta_ratio;
                                target_node.next_offset = clamped_delta_ratio;
                            }
                        }
                    }

                    frame.mouse_pos = mouse_pos;
                }
            }
            else
                draw_list->AddRectFilled(draw_bb.Min, draw_bb.Max, style::g_Dock.helper.color);
        }

        f32 Space::prerenderSectionNodes(int index)
        {
            auto &section = sections[index];
            section.fixed_size = 0.0f;
            section.fixed_sections = 0;
            f32 height = ImGui::GetContentRegionAvail().y;
            f32 content_max = 0.0f;
            for (int i = 0; i < section.nodes.size(); ++i)
            {
                ImGui::SetCursorPos({0, 0});
                auto &node = section.nodes[i];
                ImVec2 node_rect = drawNode(index, i, 0);
                content_max = ImMax(content_max, node_rect.x);
                if (!(node.dockFlags() & WindowDockFlags_Stretch))
                {
                    node.size = node_rect.y + frame.padding.y * 2.0f;
                    section.fixed_size += node.size;
                    ++section.fixed_sections;
                }
            }
            return content_max;
        }

        void Space::nodeDragOverlay(int si, int ni, const ImVec2 &pos, const ImVec2 &node_rect)
        {
            auto &section = sections[si];
            auto &node = section.nodes[ni];
            ImVec2 mouse_pos = ImGui::GetMousePos();
            ImRect node_bb;
            node_bb.Min = pos;
            node_bb.Max = {pos.x + section.size, pos.y + node_rect.y + node.extra_offset};
            bool hovered = node_bb.Contains(mouse_pos) && g_LastCursor == 0;
            ImGuiContext &g = *GImGui;
            f32 diff = mouse_pos.y - pos.y;
            bool is_content = diff > node.extra_offset;
            updateDragState(hovered, si, ni, is_content);
            if (!hovered || !(frame.flags & FrameStateFlagBits::dropped) ||
                !(node.dockFlags() & WindowDockFlags_Stretch))
                return;
            frame.flags |= FrameStateFlagBits::op_locked;
            ImRect draw_bb = node_bb;
            if (is_content)
                draw_bb.Min.y += node.extra_offset - style::g_Dock.helper.size.y;
            else
                draw_bb.Max.y -= node_rect.y;
            auto *draw_list = ImGui::GetForegroundDrawList();
            draw_list->AddRectFilled(draw_bb.Min, draw_bb.Max, style::g_Dock.node_overlay_color);
        }

        void Space::drawNode(int section_id, int node_id, f32 section_size, ImVec2 &pos, bool isFirst)
        {
            auto &section = sections[section_id];
            if (!isFirst || !(section.flags & SectionFlagBits::hide_top_line))
                resizeBoxHorizontal(section, node_id, pos, section_size);
            ImGui::SetCursorScreenPos(pos);
            auto &style = ImGui::GetStyle();
            ImVec4 node_bg = style.Colors[ImGuiCol_ChildBg];
            auto &node = section.nodes[node_id];
            if (node.isTransparent) node_bg.w = 0.0f;
            ImGui::PushStyleColor(ImGuiCol_ChildBg, node_bg);
            auto node_rect = drawNode(section_id, node_id, section_size);
            ImGui::PopStyleColor();
            if (!(section.flags & (SectionFlagBits::stretch | SectionFlagBits::op_locked)))
                nodeDragOverlay(section_id, node_id, pos, node_rect);
            pos.y += node_rect.y + node.extra_offset + style::g_Dock.helper.size.y;
        }

        void Space::drawSection(int index, ImVec2 size, ImVec2 &pos)
        {
            auto &section = sections[index];
            section.flags &= ~SectionFlagBits::scrollbar_hovered;
            if (section.id.empty()) section.id = astl::format("dock:%llx", astl::IDGen()());

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, frame.padding);

            if (section.size == 0.0f)
            {
                ImGui::SetCursorScreenPos(pos);
                auto pre_size = ImMax(prerenderSectionNodes(index) + frame.padding.x * 2.0f, 50.0f);
                if (!(section.flags & SectionFlagBits::stretch))
                {
                    section.size = pre_size;
                    section.min_size = pre_size;
                }
            }
            else
            {
                f32 section_size = (section.flags & SectionFlagBits::stretch) ? size.x : section.size;
                if (!(section.flags & SectionFlagBits::hide_top_line)) pos.y += style::g_Dock.helper.size.y;
                for (int i = 0; i < section.nodes.size(); ++i) drawNode(index, i, section_size, pos, i == 0);
                // Update sizes for a next frame
                for (auto &node : section.nodes)
                {
                    node.size += node.next_offset;
                    node.next_offset = 0.0f;
                }
            }
            ImGui::PopStyleVar(2);
        }

        void Space::render()
        {
            ImGuiViewport *viewport = ImGui::GetMainViewport();
            ImVec2 window_size{viewport->Size.x,
                               viewport->Size.y - style::g_Dock.top_offset - style::g_Dock.bottom_offset};
            ImGui::SetNextWindowSize(window_size);
            ImGui::SetNextWindowPos({0, style::g_Dock.top_offset});

            ImGuiWindowFlags windowFlags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse |
                                           ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
                                           ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBringToFrontOnFocus;

            resetFrame(frame);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            auto bg = ImGui::ColorConvertFloat4ToU32(ImGui::GetStyle().Colors[ImGuiCol_WindowBg]);
            ImGui::PushStyleColor(ImGuiCol_ChildBg, bg);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, 0);
            ImGui::Begin("dockspace", nullptr, windowFlags);
            renderContent();
            ImGui::End();
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(2);
        }

        void Space::renderContent()
        {
            ImGuiContext &g = *GImGui;

            _hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
            auto window = g.CurrentWindow;
            auto pos = window->DC.CursorPos;
            auto init_pos = pos;
            // Left sections
            for (int i = 0; i < sections.size(); i++)
            {
                auto &section = sections[i];
                ImVec2 child_size = ImVec2(0, window->Size.y);
                if (section.flags & SectionFlagBits::stretch)
                {
                    _stretch_min = i;
                    break;
                }
                if (section.size == 0.0f)
                    frame.flags |= FrameStateFlagBits::layout_update;
                else
                    child_size.x = section.size;
                drawSection(i, child_size, pos);
                pos.x += child_size.x + style::g_Dock.helper.size.x;
                pos.y = init_pos.y;
            }
            if (pos.x > init_pos.x) pos.x -= style::g_Dock.helper.size.x;
            assert(_stretch_min != -1);

            // Right section
            auto left_pos = pos;
            f32 max_width = ImGui::GetWindowContentRegionMax().x;
            pos.x = max_width;
            f32 right_rendered_width = 0;
            int i = sections.size() - 1;
            for (; i > _stretch_min; --i)
            {
                auto &section = sections[i];
                if (section.flags & SectionFlagBits::stretch) break;
                ImVec2 child_size = ImVec2(0, window->Size.y);
                if (section.size != 0.0f)
                {
                    child_size.x = section.size;
                    right_rendered_width += child_size.x;
                    if (i != sections.size() - 1) right_rendered_width += style::g_Dock.helper.size.x;
                    pos.x = max_width - right_rendered_width;
                }
                else
                {
                    pos.x -= child_size.x;
                    frame.flags |= FrameStateFlagBits::layout_update;
                }
                drawSection(i, child_size, pos);
                pos.y = init_pos.y;
            }
            _stretch_max = i;

            // Center
            f32 avail = pos.x - left_pos.x - (style::g_Dock.helper.size.x * (_stretch_max - _stretch_min));
            for (i = _stretch_min; i < _stretch_max + 1; ++i)
            {
                f32 stretch_size = i == _stretch_max ? pos.x - left_pos.x : (int)(avail * sections[i].size);
                ImVec2 tmp_pos = left_pos;
                drawSection(i, ImVec2(stretch_size, 0), tmp_pos);
                left_pos.x = tmp_pos.x + stretch_size + style::g_Dock.helper.size.x;
            }
            _window_size = window->Size;
            drawOverlayLayer(init_pos, avail);

            if (frame.flags & FrameStateFlagBits::layout_update) window::pushEmptyEvent();
            if (!ImGui::IsMouseDragging(ImGuiMouseButton_Left)) frame.flags &= ~FrameStateFlagBits::dropped;
        }

        void Space::resizeBoxVertical(int section_index, const ImRect &draw_bb, f32 stretch_size)
        {
            ImRect hover_bb = draw_bb;
            hover_bb.Min.x -= style::g_Dock.helper.cap_offset;
            hover_bb.Max.x += style::g_Dock.helper.cap_offset;
            ImVec2 mouse_pos = ImGui::GetMousePos();
            bool hovered = hover_bb.Contains(mouse_pos) && !(frame.flags & FrameStateFlagBits::op_locked);
            auto &section = sections[section_index];
            if (hovered)
                section.flags |= SectionFlagBits::op_locked;
            else
                section.flags &= ~SectionFlagBits::op_locked;
            updateHoverState(section, hovered, mouse_pos);
            auto *draw_list = ImGui::GetCurrentWindow()->DrawList;
            if (hovered || section.isResizing)
            {
                section.flags |= SectionFlagBits::op_locked;
                updateDragState(hovered, section_index + 1, -1);
                if (_hovered || frame.flags & FrameStateFlagBits::resizing)
                    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                draw_list->AddRectFilled(draw_bb.Min, draw_bb.Max, style::g_Dock.helper.hover_color);
                frame.flags |= FrameStateFlagBits::op_locked;
                if (section.isResizing)
                {
                    f32 delta_x = mouse_pos.x - frame.mouse_pos.x;
                    if (section_index > _stretch_max) delta_x = -delta_x;
                    if (!(delta_x > 0 && stretch_size < style::g_Dock.stretch_min_size))
                    {
                        if (section_index >= _stretch_min && section_index <= _stretch_max)
                        {
                            f32 delta_ratio = delta_x / stretch_size;
                            f32 nsSize = section.size + delta_ratio;
                            f32 psSize = sections[section_index + 1].size - delta_ratio;
                            if (nsSize * stretch_size > style::g_Dock.stretch_section_size &&
                                psSize * stretch_size > style::g_Dock.stretch_section_size)
                            {
                                section.size = nsSize;
                                sections[section_index + 1].size = psSize;
                            }
                        }
                        else
                            section.size = ImClamp(section.size + delta_x, section.min_size, FLT_MAX);
                    }
                    frame.mouse_pos = mouse_pos;
                    window::pushEmptyEvent();
                }
            }
            else
            {
                section.flags &= ~SectionFlagBits::op_locked;
                if (section_index != _stretch_min - 1 && section_index != _stretch_max + 1)
                    draw_list->AddRectFilled(draw_bb.Min, draw_bb.Max, style::g_Dock.helper.color);
            }
        }

        void Space::resizeBoxVerticalBounds(Section &section, int si, const ImRect &draw_bb, const ImRect &hover_bb)
        {
            ImVec2 mouse_pos = ImGui::GetMousePos();
            bool hovered = hover_bb.Contains(mouse_pos);
            if (hovered)
                section.flags |= SectionFlagBits::op_locked;
            else
                section.flags &= ~SectionFlagBits::op_locked;
            updateDragState(hovered, si, -1);
            if (hovered && frame.flags & FrameStateFlagBits::dropped)
            {
                auto *draw_list = ImGui::GetForegroundDrawList();
                draw_list->AddRectFilled(draw_bb.Min, draw_bb.Max, style::g_Dock.helper.hover_color);
                frame.flags |= FrameStateFlagBits::op_locked;
            }
        }

        void Space::drawOverlayLayer(const ImVec2 &init_pos, f32 stretch_size)
        {
            popupMenu.tryOpen(frame.padding);

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, frame.padding);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, frame.item_spacing);
            popupMenu.render();
            ImGui::PopStyleVar(2);

            ImGui::PushStyleColor(ImGuiCol_ChildBg, 0);
            ImGui::SetCursorPos({0, 0});
            // For hidden resize boxes need to draw em on the top layer. So I need to add the overlay child layer.
            ImGui::BeginChild("dock:overlay", ImVec2(0, 0), 0,
                              ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoScrollbar |
                                  ImGuiWindowFlags_NoScrollWithMouse);

            ImVec2 left_pos = init_pos;
            int i = 0;
            // Left
            for (; i < _stretch_min; ++i)
            {
                left_pos.x += sections[i].size;
                ImRect bb;
                bb.Min = {left_pos.x, left_pos.y};
                bb.Max = {left_pos.x + style::g_Dock.helper.size.x, _window_size.y + init_pos.y};
                left_pos.x += style::g_Dock.helper.size.x;
                resizeBoxVertical(i, bb, stretch_size);
            }
            if (i != 0) left_pos.x -= style::g_Dock.helper.size.x;
            auto *window = ImGui::GetCurrentWindow();
            ImVec2 right_pos{window->Pos.x + window->Size.x, init_pos.y};
            // Right
            for (i = sections.size() - 1; i > _stretch_max; --i)
            {
                right_pos.x -= sections[i].size;
                ImRect bb;
                bb.Min = {right_pos.x - style::g_Dock.helper.size.x, right_pos.y};
                bb.Max = {right_pos.x, _window_size.y + init_pos.y};
                right_pos.x -= style::g_Dock.helper.size.x;
                resizeBoxVertical(i, bb, stretch_size);
            }
            // Center
            for (i = _stretch_min; i < _stretch_max; ++i)
            {
                left_pos.x += (int)(stretch_size * sections[i].size);
                ImRect bb;
                bb.Min = left_pos;
                left_pos.x += style::g_Dock.helper.size.x;
                bb.Max = {left_pos.x, _window_size.y + init_pos.y};
                resizeBoxVertical(i, bb, stretch_size);
            }

            // Front
            {
                ImVec2 draw_bb_max = {init_pos.x + style::g_Dock.helper.size.x, init_pos.y + _window_size.y};
                ImRect hover_bb;
                hover_bb.Min = init_pos;
                hover_bb.Max = {draw_bb_max.x + style::g_Dock.helper.cap_offset, draw_bb_max.y};
                resizeBoxVerticalBounds(sections.front(), 0, {init_pos, draw_bb_max}, hover_bb);
            }
            // End
            {
                ImRect draw_bb;
                draw_bb.Min.y = init_pos.y;
                draw_bb.Max.x = window->Pos.x + window->Size.x;
                draw_bb.Min.x = draw_bb.Max.x - style::g_Dock.helper.size.x;
                draw_bb.Max.y = init_pos.y + _window_size.y;
                ImRect hover_bb;
                hover_bb.Min = {draw_bb.Min.x - style::g_Dock.helper.cap_offset, draw_bb.Min.y};
                hover_bb.Max = draw_bb.Max;
                resizeBoxVerticalBounds(sections.back(), sections.size(), draw_bb, hover_bb);
            }

            ImGui::EndChild();
            ImGui::PopStyleColor();
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
                    auto removed_size = section.nodes[ni].size;
                    section.nodes.erase(section.nodes.begin() + ni);
                    if (!section.nodes.empty())
                    {
                        auto &adusted_node = ni == 0 ? section.nodes.front() : section.nodes[ni - 1];
                        adusted_node.size += removed_size;
                    }
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
                                space->_e->dispatch<ChangeEvent>("ds:undock", space, window_name.c_str(), _si, _ni);
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

        bool tryInsertNode(Section &section, Node &node, size_t index, f32 height)
        {
            f32 avaliable_size = height - section.fixed_size;

            // Check if enough space
            if (avaliable_size <= 0.0f) return false;
            f32 limit_size = uikit::style::g_Dock.height_resize_limit;
            size_t stretch_count = section.nodes.size() - section.fixed_sections;
            if ((stretch_count + 1) * limit_size > avaliable_size) return false;

            auto &old_node = section.nodes[index];
            old_node.size *= 0.5f;
            node.size = old_node.size;
            section.nodes.insert(section.nodes.begin() + index + 1, std::move(node));

            // Redistribute space
            f32 first_abs = old_node.size * avaliable_size;
            f32 second_abs = section.nodes[index + 1].size * avaliable_size;
            if (first_abs < limit_size || second_abs < limit_size)
            {
                size_t new_stretch_count = section.nodes.size() - section.fixed_sections;
                f32 total_min = new_stretch_count * limit_size;

                f32 leftover = avaliable_size - total_min;
                f32 add_each = leftover / (f32)new_stretch_count;

                for (auto &n : section.nodes)
                {
                    if (n.dockFlags() & uikit::WindowDockFlags_Stretch)
                    {
                        f32 node_px = limit_size + add_each;
                        n.size = node_px / avaliable_size;
                    }
                }
            }

            section.reset();
            return true;
        }
    } // namespace dock
} // namespace uikit