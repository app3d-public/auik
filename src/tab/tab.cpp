#include <core/disposal_queue.hpp>
#include <imgui/imgui_internal.h>
#include <uikit/button/button.hpp>
#include <uikit/button/combobox.hpp>
#include <uikit/tab/tab.hpp>
#include <window/window.hpp>
#include "imgui.h"

namespace uikit
{
    class TabMemCache : public MemCache
    {
    public:
        TabMemCache(u8 *activeIndex, astl::vector<TabItem> *items) : activeIndex(activeIndex), _items(items)
        {
            ++itemsLeft;
        }

        virtual ~TabMemCache() = default;

        virtual void free() override
        {
            _free();
            if (--itemsLeft == 0) offset = 0;
        }

    protected:
        static size_t offset;
        static size_t itemsLeft;
        u8 *activeIndex;
        astl::vector<TabItem> *_items;

        virtual void _free() = 0;
    };

    class AddMemCache : public TabMemCache
    {
    public:
        AddMemCache(TabItem tab, u8 *activeIndex, astl::vector<TabItem> *items)
            : TabMemCache(activeIndex, items), _tab(tab)
        {
        }

        virtual void _free() override
        {
            _items->push_back(_tab);
            ++offset;
            *activeIndex = _items->size() - 1;
        }

    private:
        TabItem _tab;
    };

    class RemoveMemCache : public TabMemCache
    {
    public:
        RemoveMemCache(const astl::vector<TabItem>::iterator &it, u8 *activeIndex, astl::vector<TabItem> *items)
            : TabMemCache(activeIndex, items), _it(it)
        {
        }

        virtual void _free() override
        {
            _items->erase(_it + offset);
            --offset;
        }

    private:
        astl::vector<TabItem>::iterator _it;
    };

    size_t TabMemCache::offset = 0;
    size_t TabMemCache::itemsLeft = 0;

    bool TabItem::renderItem()
    {
        _size = calculateItemSize();
        auto pos = ImGui::GetCursorPos();
        ImGuiWindow *window = GImGui->CurrentWindow;
        auto screenPos = window->DC.CursorPos;
        Selectable::render();
        auto &style = ImGui::GetStyle();
        pos.x += _size.x + style.ItemSpacing.x + style.ItemInnerSpacing.x;
        ImGui::SetCursorPos(pos);

        if (_tabFlags & TabItem::FlagBits::closable || _tabFlags & TabItem::FlagBits::unsaved)
        {
            bool wantDelete = false;
            ImGuiID id = window->GetID(name.c_str());
            const ImGuiID close_button_id = ImGui::GetIDWithSeed("#CLOSE", NULL, id);
            ImVec2 nextPos{0, 0};
            if (_tabFlags & TabItem::FlagBits::closable)
            {
                f32 closeSize = ImMax(2.0f, GImGui->FontSize * 0.5f + 1.0f) + style.ItemSpacing.x;
                nextPos = screenPos + ImVec2{_size.x - closeSize, style.ItemSpacing.y};
                if (closeButton(close_button_id, nextPos)) wantDelete = true;
            }
            if (_tabFlags & TabItem::FlagBits::unsaved)
            {
                ImGuiContext &g = *GImGui;
                f32 bulletSize = ImMax(2.0f, g.FontSize * 0.5f + 1.0f);
                nextPos.x -= bulletSize + style.ItemSpacing.x;
                ImVec2 end = nextPos + ImVec2(bulletSize, _size.y * 0.5f + bulletSize * 0.5f);
                const ImRect bullet_bb(nextPos, end);
                ImDrawList *draw_list = window->DrawList;
                ImGui::RenderBullet(draw_list, bullet_bb.GetCenter(), ImGui::GetColorU32(ImGuiCol_Text));
            }
            return wantDelete;
        }
        return false;
    }

    ImVec2 TabItem::calculateItemSize()
    {
        auto &style = ImGui::GetStyle();
        auto labelSize = ImGui::CalcTextSize(name.c_str());
        ImVec2 outputSize = labelSize + style.ItemSpacing * 2.0f;
        if (_tabFlags & FlagBits::unsaved || _tabFlags & FlagBits::closable)
        {
            ImGuiContext &g = *GImGui;
            f32 closeSize = ImMax(2.0f, g.FontSize * 0.5f + 1.0f);
            f32 width = closeSize + style.ItemSpacing.x;
            if (_tabFlags & FlagBits::unsaved && _tabFlags & FlagBits::closable)
                outputSize.x += 2.0f * width;
            else
                outputSize.x += width;
        }
        outputSize.x += style.ItemSpacing.x;
        return outputSize;
    }

    bool TabBar::renderTab(astl::vector<TabItem>::iterator &begin, int index)
    {
        if (_drag.it != begin)
        {
            if (begin->renderItem())
            {
                if (_isMainTabbar) e->dispatch<TabRemoveEvent>("tabbar:close", *begin, false);
                return false;
            }
        }
        else
        {
            ImGui::Dummy(begin->size());
            ImVec2 nextPos = _drag.pos;
            const auto &style = ImGui::GetStyle();
            nextPos.x += begin->size().x + style.ItemSpacing.x + style.ItemInnerSpacing.x;
            ImGui::SetCursorPos(nextPos);
            if (_drag.offset < 0)
            {
                if (begin != items.begin())
                {
                    auto prev = std::prev(begin);

                    if (_drag.offset + _drag.posOffset <= prev->size().x * -0.5f)
                    {
                        _drag.posOffset = prev->size().x * 0.5f + style.ItemSpacing.x + style.ItemInnerSpacing.x;
                        std::rotate(prev, begin, std::next(begin));
                        ImGui::ResetMouseDragDelta();
                        _drag.it = prev;
                        if (index == activeIndex)
                            activeIndex = index - 1;
                        else if (index - 1 == activeIndex)
                            activeIndex = index;
                    }
                }
            }

            if (_drag.offset > 0)
            {
                auto next = std::next(begin);
                if (next != items.end() && _drag.offset + _drag.posOffset >= next->size().x * 0.5f)
                {
                    _drag.posOffset = next->size().x * -.5f - style.ItemSpacing.x - style.ItemInnerSpacing.x;
                    std::rotate(begin, next, std::next(next));
                    ImGui::ResetMouseDragDelta();
                    _drag.it = next;
                    if (index == activeIndex)
                        ++activeIndex;
                    else if (index + 1 == activeIndex)
                        activeIndex = index;
                }
            }
        }
        return false;
    }

    void TabBar::renderDragged()
    {
        ImGui::SetCursorPos({_drag.pos.x + _drag.offset + _drag.posOffset, _drag.pos.y});
        _drag.it.value()->renderItem();

        // Scroll tabbar if no enough space to render prev/next tabs
        ImVec2 windowSize = ImGui::GetWindowSize();
        f32 currentScroll = ImGui::GetScrollX();
        f32 maxScroll = ImGui::GetScrollMaxX();
        f32 startPosOfTab = _drag.pos.x + _drag.offset;
        f32 endPosOfTab = startPosOfTab + _drag.it.value()->size().x;
        if (endPosOfTab > windowSize.x + currentScroll)
        {
            f32 scrollAmount = endPosOfTab - windowSize.x - currentScroll;
            f32 newScroll = currentScroll + scrollAmount;
            newScroll = (newScroll > maxScroll) ? maxScroll : newScroll;
            ImGui::SetScrollX(newScroll * _style.scrollOffsetLR);
        }
        else if (startPosOfTab < currentScroll)
        {
            f32 newScroll = currentScroll - startPosOfTab;
            ImGui::SetScrollX(currentScroll - newScroll >= 0 ? newScroll * _style.scrollOffsetRL : 0);
        }
    }

    void TabBar::renderCombobox()
    {
        size_t index = 0;
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        if (beginCombo("##v", nullptr, ImGuiComboFlags_NoPreview | ImGuiComboFlags_HeightLargest))
        {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 6));
            for (auto &item : items)
            {
                bool pressed;
                std::string label = item.name;
                Selectable::Params params{
                    .label = label.c_str(), .selected = index == activeIndex, .pressed = &pressed};
                Selectable::render(params);
                if (pressed)
                {
                    activeIndex = index;
                    window::pushEmptyEvent();
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
        std::function<void()> onRender;
        auto &style = ImGui::GetStyle();
        ImVec2 size = _style.size;
        if (_style.size.x == 0) _style.size.x = ImGui::GetContentRegionAvail().x;
        if (_style.size.y == 0 && !items.empty()) size.y = items.begin()->size().y + style.WindowPadding.y * 2.0f;
        if (ImGui::BeginChild(name.c_str(), size, true))
        {
            if (_flags & FlagBits::scrollable)
            {
                if (ImGui::IsWindowHovered())
                    _isHovered = true;
                else
                {
                    auto &io = ImGui::GetIO();
                    if (_isHovered && io.KeyMods & ImGuiMod_Shift) io.AddKeyEvent(ImGuiMod_Shift, false);
                }
            }
            const auto &style = ImGui::GetStyle();
            f32 availableWidth = ImGui::GetContentRegionAvail().x;
            bool isScrollable = _flags & FlagBits::scrollable;
            bool isReorderable = _flags & FlagBits::reorderable;

            // Pre-calculate available width if not scrollable
            if (!isScrollable)
            {
                f32 arrowWidth = style::g_StyleComboBox->arrowIcon->width() + style.ItemSpacing.x * 2.0f +
                                 style.FramePadding.x * 2.0f + style.ItemInnerSpacing.x * 2.0f;
                availableWidth -= arrowWidth;
            }

            bool stopRender = false;
            int index{0};
            for (auto begin = items.begin(); begin != items.end(); ++index)
            {
                f32 itemWidth = begin->size().x + style.ItemSpacing.x;

                // Adjust available width and determine if rendering should stop
                if (!isScrollable)
                {
                    if (availableWidth >= itemWidth)
                        availableWidth -= itemWidth;
                    else
                        stopRender = true;
                }

                // Handle selection and rendering logic
                begin->selected(index == activeIndex);
                if (index == activeIndex) onRender = begin->onRender();

                // Handle reorderable logic
                bool wasDragReset{false};
                if (isReorderable)
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
                            _drag.posOffset = 0;
                            _drag.it.reset();
                            wasDragReset = true;
                        }
                    }
                    else if (begin->hover() && ImGui::IsMouseDragging(0))
                    {
                        _drag.it = begin;
                        _drag.pos = ImGui::GetCursorPos();
                    }
                }

                // Only render if scrollable or not stopped
                if (isScrollable || !stopRender) renderTab(begin, index);

                // Update active index if pressed
                if (begin->pressed() && !wasDragReset)
                {
                    if (activeIndex != index)
                        e->dispatch<TabChangeEvent>("tabbar:switched", items.begin() + activeIndex,
                                                    items.begin() + index);
                    activeIndex = index;
                    window::pushEmptyEvent();
                }
                ++begin;
            }

            if (_drag.it.has_value()) renderDragged();
            if (!(_flags & FlagBits::scrollable) && stopRender) renderCombobox();

            _avaliableWidth = ImGui::GetContentRegionAvail().x;
        }
        ImGui::EndChild();
        if (onRender) onRender();
    }

    bool TabBar::removeTab(const TabItem &tab)
    {
        auto it = std::find_if(items.begin(), items.end(), [&](const TabItem &item) { return item.name == tab.name; });
        if (it != items.end())
        {
            if (activeIndex == items.size() - 1 && activeIndex != 0) --activeIndex;
            _disposalQueue.push(new RemoveMemCache(it, &activeIndex, &items));
            return true;
        }
        return false;
    }

    void TabBar::newTab(const TabItem &tab) { _disposalQueue.push(new AddMemCache(tab, &activeIndex, &items)); }

    void TabBar::bindEvents()
    {
        e->bindEvent(this, "window:scroll", [this](const window::ScrollEvent &event) {
            if (!_isHovered || !(_flags & FlagBits::scrollable)) return;
            ImGuiIO &io = ImGui::GetIO();
            io.AddKeyEvent(ImGuiMod_Shift, true);
        });
        if (_isMainTabbar)
        {
            e->bindEvent(this, "tabbar:changed", [this](const TabInfoEvent &e) {
                if (e.flags & TabItem::FlagBits::unsaved)
                    items[activeIndex].flags() |= TabItem::FlagBits::unsaved;
                else
                    items[activeIndex].flags() &= ~TabItem::FlagBits::unsaved;
                if (!e.fullname.empty())
                {
                    items[activeIndex].name = e.cn;
                    items[activeIndex].id = e.fullname;
                }
            });
        }
    }
} // namespace uikit
