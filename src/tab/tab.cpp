#include <acul/disposal_queue.hpp>
#include <awin/window.hpp>
#include <imgui/imgui_internal.h>
#include <uikit/button/button.hpp>
#include <uikit/button/combobox.hpp>
#include <uikit/tab/tab.hpp>

namespace uikit
{
    class TabMemCache : public acul::mem_cache
    {
    public:
        TabMemCache(u8 *active_index, acul::vector<TabItem> *items)
            : acul::mem_cache([this]() {
                  _free();
                  if (--itemsLeft == 0) offset = 0;
              }),
              active_index(active_index),
              _items(items)
        {
            ++itemsLeft;
        }

        virtual ~TabMemCache() = default;

    protected:
        static size_t offset;
        static size_t itemsLeft;
        u8 *active_index;
        acul::vector<TabItem> *_items;

        virtual void _free() = 0;
    };

    class AddMemCache : public TabMemCache
    {
    public:
        AddMemCache(TabItem tab, u8 *active_index, acul::vector<TabItem> *items)
            : TabMemCache(active_index, items), _tab(tab)
        {
        }

        virtual void _free() override
        {
            _items->push_back(_tab);
            ++offset;
            *active_index = _items->size() - 1;
        }

    private:
        TabItem _tab;
    };

    class RemoveMemCache : public TabMemCache
    {
    public:
        RemoveMemCache(const acul::vector<TabItem>::iterator &it, u8 *active_index, acul::vector<TabItem> *items)
            : TabMemCache(active_index, items), _it(it)
        {
        }

        virtual void _free() override
        {
            if (*active_index == _items->size() - 1 && *active_index != 0) --(*active_index);
            _items->erase(_it + offset);
            --offset;
        }

    private:
        acul::vector<TabItem>::iterator _it;
    };

    size_t TabMemCache::offset = 0;
    size_t TabMemCache::itemsLeft = 0;

    bool TabItem::render_item()
    {
        size = calculate_item_size();
        auto pos = ImGui::GetCursorPos();
        ImGuiWindow *window = GImGui->CurrentWindow;
        auto screen_pos = window->DC.CursorPos;
        Selectable::render();
        auto &style = ImGui::GetStyle();
        pos.x += size.x + style.ItemSpacing.x + style.ItemInnerSpacing.x;
        ImGui::SetCursorPos(pos);

        if (_tab_flags & TabItem::FlagBits::Closable || _tab_flags & TabItem::FlagBits::Unsaved)
        {
            bool want_delete = false;
            ImGuiID id = window->GetID(name.c_str());
            const ImGuiID close_button_id = ImGui::GetIDWithSeed("#CLOSE", NULL, id);
            ImVec2 next_pos{0, 0};
            if (_tab_flags & TabItem::FlagBits::Closable)
            {
                f32 closeSize = ImMax(2.0f, GImGui->FontSize * 0.5f + 1.0f) + style.ItemSpacing.x;
                next_pos = screen_pos + ImVec2{size.x - closeSize, style.ItemSpacing.y};
                if (close_button(close_button_id, next_pos)) want_delete = true;
            }
            if (_tab_flags & TabItem::FlagBits::Unsaved)
            {
                ImGuiContext &g = *GImGui;
                f32 bulletSize = ImMax(2.0f, g.FontSize * 0.5f + 1.0f);
                next_pos.x -= bulletSize + style.ItemSpacing.x;
                ImVec2 end = next_pos + ImVec2(bulletSize, size.y * 0.5f + bulletSize * 0.5f);
                const ImRect bullet_bb(next_pos, end);
                ImDrawList *draw_list = window->DrawList;
                ImGui::RenderBullet(draw_list, bullet_bb.GetCenter(), ImGui::GetColorU32(ImGuiCol_Text));
            }
            return want_delete;
        }
        return false;
    }

    ImVec2 TabItem::calculate_item_size()
    {
        auto &style = ImGui::GetStyle();
        auto label_size = ImGui::CalcTextSize(name.c_str());
        ImVec2 output_size = label_size + style.ItemSpacing * 2.0f;
        if (_tab_flags & FlagBits::Unsaved || _tab_flags & FlagBits::Closable)
        {
            ImGuiContext &g = *GImGui;
            f32 close_size = ImMax(2.0f, g.FontSize * 0.5f + 1.0f);
            f32 width = close_size + style.ItemSpacing.x;
            if (_tab_flags & FlagBits::Unsaved && _tab_flags & FlagBits::Closable)
                output_size.x += 2.0f * width;
            else
                output_size.x += width;
        }
        output_size.x += style.ItemSpacing.x;
        return output_size;
    }

    bool TabBar::render_tab(acul::vector<TabItem>::iterator &begin, int index)
    {
        if (_drag.it != begin)
        {
            if (begin->render_item())
            {
                ed->dispatch<TabCloseEvent>(this, *begin, false);
                return false;
            }
        }
        else
        {
            ImGui::Dummy(begin->size);
            ImVec2 next_pos = _drag.pos;
            const auto &style = ImGui::GetStyle();
            next_pos.x += begin->size.x + style.ItemSpacing.x + style.ItemInnerSpacing.x;
            ImGui::SetCursorPos(next_pos);
            if (_drag.offset < 0)
            {
                if (begin != items.begin())
                {
                    auto prev = std::prev(begin);

                    if (_drag.offset + _drag.pos_offset <= prev->size.x * -0.5f)
                    {
                        _drag.pos_offset = prev->size.x * 0.5f + style.ItemSpacing.x + style.ItemInnerSpacing.x;
                        std::rotate(prev, begin, std::next(begin));
                        ImGui::ResetMouseDragDelta();
                        _drag.it = prev;
                        if (index == active_index)
                            active_index = index - 1;
                        else if (index - 1 == active_index)
                            active_index = index;
                    }
                }
            }

            if (_drag.offset > 0)
            {
                auto next = std::next(begin);
                if (next != items.end() && _drag.offset + _drag.pos_offset >= next->size.x * 0.5f)
                {
                    _drag.pos_offset = next->size.x * -.5f - style.ItemSpacing.x - style.ItemInnerSpacing.x;
                    std::rotate(begin, next, std::next(next));
                    ImGui::ResetMouseDragDelta();
                    _drag.it = next;
                    if (index == active_index)
                        ++active_index;
                    else if (index + 1 == active_index)
                        active_index = index;
                }
            }
        }
        return false;
    }

    void TabBar::render_dragged()
    {
        ImGui::SetCursorPos({_drag.pos.x + _drag.offset + _drag.pos_offset, _drag.pos.y});
        _drag.it.value()->render_item();

        // Scroll tabbar if no enough space to render prev/next tabs
        ImVec2 window_size = ImGui::GetWindowSize();
        f32 current_scroll = ImGui::GetScrollX();
        f32 max_scroll = ImGui::GetScrollMaxX();
        f32 start_pos_of_tab = _drag.pos.x + _drag.offset;
        f32 end_pos_of_tab = start_pos_of_tab + _drag.it.value()->size.x;
        if (end_pos_of_tab > window_size.x + current_scroll)
        {
            f32 scroll_amount = end_pos_of_tab - window_size.x - current_scroll;
            f32 new_scroll = current_scroll + scroll_amount;
            new_scroll = (new_scroll > max_scroll) ? max_scroll : new_scroll;
            ImGui::SetScrollX(new_scroll * _style.scroll_offset_lr);
        }
        else if (start_pos_of_tab < current_scroll)
        {
            f32 newScroll = current_scroll - start_pos_of_tab;
            ImGui::SetScrollX(current_scroll - newScroll >= 0 ? newScroll * _style.scroll_offset_rl : 0);
        }
    }

    void TabBar::render_combobox()
    {
        size_t index = 0;
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        if (beginCombo("##v", nullptr, ImGuiComboFlags_NoPreview | ImGuiComboFlags_HeightLargest))
        {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 6));
            for (auto &item : items)
            {
                acul::string label = item.name;
                SelectableParams params{.selected = index == active_index};
                Selectable::render(label.c_str(), params);
                if (params.pressed)
                {
                    active_index = index;
                    awin::push_empty_event();
                }
                ++index;
            }
            ImGui::PopStyleVar();
            endCombo();
        }
        ImGui::PopStyleColor();
    }

    void TabBar::render()
    {
        std::function<void()> render_callback;
        auto &style = ImGui::GetStyle();
        ImVec2 size = _style.size;
        if (_style.size.x == 0) _style.size.x = ImGui::GetContentRegionAvail().x;
        if (_style.size.y == 0 && !items.empty()) size.y = items.begin()->size.y + style.WindowPadding.y * 2.0f;
        if (ImGui::BeginChild(name.c_str(), size, true))
        {
            if (ImGui::IsWindowHovered())
                _is_hovered = true;
            else if (_flags & FlagBits::Scrollable)
            {
                auto &io = ImGui::GetIO();
                if (_is_hovered && io.KeyMods & ImGuiMod_Shift) io.AddKeyEvent(ImGuiMod_Shift, false);
            }
            const auto &style = ImGui::GetStyle();
            f32 available_width = ImGui::GetContentRegionAvail().x;
            bool is_scrollable = _flags & FlagBits::Scrollable;
            bool is_reorderable = _flags & FlagBits::Reorderable;

            // Pre-calculate available width if not scrollable
            f32 arrow_width = 0.0f;
            if (!is_scrollable)
            {
                arrow_width = style::g_combo_box.arrowIcon->size().x + style.ItemSpacing.x * 2.0f +
                              style.FramePadding.x * 2.0f + style.ItemInnerSpacing.x * 2.0f;
                available_width -= arrow_width;
            }

            bool stop_render = false;
            int index{0};
            for (auto begin = items.begin(); begin != items.end(); ++index)
            {
                f32 itemWidth = begin->size.x + style.ItemSpacing.x;

                // Adjust available width and determine if rendering should stop
                if (!is_scrollable)
                {
                    available_width -= itemWidth;
                    if (available_width < 0) stop_render = true;
                }

                // Handle selection and rendering logic
                begin->selected = index == active_index;
                if (index == active_index) render_callback = begin->render_callback();

                // Handle reorderable logic
                bool was_drag_reset{false};
                if (is_reorderable)
                {
                    // Dragging logic
                    if (_drag.it.has_value())
                    {
                        if (_drag.it.value() == begin && ImGui::IsMouseDragging(0, 0.0f))
                        {
                            _drag.offset = ImGui::GetMouseDragDelta().x;
                            _drag.pos = ImGui::GetCursorPos();
                        }
                        else if (!ImGui::IsMouseDragging(0) && _drag.it.value() == begin)
                        {
                            _drag.offset = 0;
                            _drag.pos = {0, 0};
                            _drag.pos_offset = 0;
                            _drag.it.reset();
                            was_drag_reset = true;
                        }
                    }
                    else if (begin->hover && ImGui::IsMouseDragging(0))
                    {
                        _drag.it = begin;
                        _drag.pos = ImGui::GetCursorPos();
                    }
                }

                // Only render if scrollable or not stopped
                if (is_scrollable || !stop_render) render_tab(begin, index);

                // Update active index if pressed
                if (begin->pressed && !was_drag_reset)
                {
                    if (active_index != index)
                        ed->dispatch<TabSwitchEvent>(this, items.begin() + active_index, items.begin() + index);
                    active_index = index;
                    awin::push_empty_event();
                }
                ++begin;
            }

            if (_drag.it.has_value()) render_dragged();
            if (!(_flags & FlagBits::Scrollable) && stop_render) render_combobox();

            _avaliable_width = ImGui::GetContentRegionAvail().x - arrow_width;
            _height = ImGui::GetWindowHeight();
        }
        ImGui::EndChild();
        if (render_callback) render_callback();
    }

    bool TabBar::remove_tab(const TabItem &tab)
    {
        auto it = std::find_if(items.begin(), items.end(), [&](const TabItem &item) { return item.name == tab.name; });
        if (it != items.end())
        {
            _disposal_queue.push(acul::alloc<RemoveMemCache>(it, &active_index, &items));
            return true;
        }
        return false;
    }

    void TabBar::new_tab(const TabItem &tab)
    {
        auto it = std::find_if(items.begin(), items.end(), [&](const TabItem &item) { return item.id == tab.id; });
        if (it != items.end())
            active_index = it - items.begin();
        else
            _disposal_queue.push(acul::alloc<AddMemCache>(tab, &active_index, &items));
    }

    void TabBar::bind_events()
    {
        if (_flags & FlagBits::Scrollable)
            ed->bind_event(this, awin::event_id::Scroll, [this](const awin::ScrollEvent &event) {
                if (!_is_hovered) return;
                ImGuiIO &io = ImGui::GetIO();
                io.AddKeyEvent(ImGuiMod_Shift, true);
            });
        ed->bind_event(this, event_id::Changed, [this](const TabChangeEvent &e) {
            if (e.tabbar != this) return;
            if (e.flags & TabItem::FlagBits::Unsaved)
                items[active_index].flags() |= TabItem::FlagBits::Unsaved;
            else
                items[active_index].flags() &= ~TabItem::FlagBits::Unsaved;
            if (!e.display_name.empty()) items[active_index].name = e.display_name;
        });
    }
} // namespace uikit
