#include <uikit/button/group.hpp>

namespace uikit
{
    namespace style
    {
        BtnGroup g_BtnGroup{0.0f, 0.0f, nullptr};
        BtnGroupH g_BtnGroupH{};
    } // namespace style

    void BtnGroup::renderItem(size_t index, ImVec2 &pos, ImVec2 &rectSize)
    {
        ImGui::PushID(index);
        ImGui::SetCursorScreenPos(pos);
        selected = _items[index].selected;
        Selectable::render();
        if (pressed)
        {
            if (_flags != FlagBits::none)
            {
                if (_flags & FlagBits::resetOnClick && index != _activeID)
                {
                    if (_activeID >= 0)
                    {
                        _items[_activeID].selected = false;
                        _items[_activeID].disabled = true;
                        _items[index].disabled = false;
                    }
                    _activeID = index;
                }
                if (_flags & FlagBits::toogle)
                    _items[index].selected = !_items[index].selected;
                else
                    _items[index].selected = true;
            }
            _items[index].callback();
        }

        ImVec2 selectablePos = ImGui::GetItemRectMin();
        rectSize = ImGui::GetItemRectSize();

        ImVec2 iconPos = selectablePos + (rectSize - _items[index].icon->size()) * 0.5f;
        ImGui::SetCursorScreenPos(iconPos);

        if (_items[index].disabled && _items[index].disabledIcon)
            _items[index].disabledIcon->render(iconPos);
        else
            _items[index].icon->render(iconPos);

        // Lock
        if (_flags & FlagBits::lock && _items[index].selected)
        {
            auto diff = selectablePos - iconPos;
            auto lockPos = iconPos;
            lockPos.x -= diff.x * 0.75f;
            lockPos.y -= diff.y * 0.5f;
            style::g_BtnGroup.lockIcon->render(lockPos);
        }
        ImGui::PopID();
    }

    void BtnGroup::renderAsGroup()
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
        float totalWidth = _items.size() * size.x;
        ImVec2 childSize = ImVec2(totalWidth, size.y);

        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, style::g_BtnGroupH.groupBg);
        ImGui::PushStyleColor(ImGuiCol_Header, style::g_BtnGroupH.groupItemBg);

        if (ImGui::BeginChild(name.c_str(), childSize, false,
                              ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
        {
            ImVec2 pos = ImGui::GetCursorScreenPos();
            for (size_t i = 0; i < _items.size(); ++i)
            {
                auto rounding_copy = rounding;
                if (i == 0 || i == _items.size() - 1)
                {
                    auto cornerFlags = i == 0 ? ImDrawFlags_RoundCornersLeft : ImDrawFlags_RoundCornersRight;
                    dFlags = cornerFlags & ImDrawFlags_RoundCornersMask_;
                }
                else
                    rounding = 0.0f;
                ImVec2 rectSize;
                renderItem(i, pos, rectSize);
                pos.x += rectSize.x;
                rounding = rounding_copy;
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }
} // namespace uikit