#include <auik/button/group.hpp>

namespace auik
{
    namespace style
    {
        BtnGroup g_btn_group{0.0f, 0.0f, nullptr};
        BtnGroupH g_btn_group_h{};
    } // namespace style

    void BtnGroup::render_item(int index, ImVec2 &pos, ImVec2 &rect_size)
    {
        ImGui::PushID(index);
        ImGui::SetCursorScreenPos(pos);
        selected = _items[index].selected;
        Selectable::render();
        if (pressed)
        {
            if (_flags != FlagBits::None)
            {
                if (_flags & FlagBits::ResetOnClick && index != _active_id)
                {
                    if (_active_id >= 0)
                    {
                        _items[_active_id].selected = false;
                        _items[_active_id].disabled = true;
                        _items[index].disabled = false;
                    }
                    _active_id = index;
                }
                if (_flags & FlagBits::Toogle)
                    _items[index].selected = !_items[index].selected;
                else
                    _items[index].selected = true;
            }
            _items[index].callback();
        }

        ImVec2 selectable_pos = ImGui::GetItemRectMin();
        rect_size = ImGui::GetItemRectSize();

        ImVec2 icon_pos = selectable_pos + (rect_size - _items[index].icon->size()) * 0.5f;
        ImGui::SetCursorScreenPos(icon_pos);
        if (_items[index].disabled && _items[index].disabled_icon)
            _items[index].disabled_icon->render(icon_pos);
        else
            _items[index].icon->render(icon_pos);

        // Lock
        if (_flags & FlagBits::Lock && _items[index].selected)
        {
            auto diff = selectable_pos - icon_pos;
            auto lock_pos = icon_pos;
            lock_pos.x -= diff.x * 0.75f;
            lock_pos.y -= diff.y * 0.5f;
            style::g_btn_group.lock_icon->render(lock_pos);
        }
        ImGui::PopID();
    }

    void BtnGroup::render_as_group()
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
        f32 total_width = _items.size() * size.x;
        ImVec2 child_size = ImVec2(total_width, size.y);

        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, style::g_btn_group_h.group_bg);
        ImGui::PushStyleColor(ImGuiCol_Header, style::g_btn_group_h.group_item_bg);

        if (ImGui::BeginChild(name.c_str(), child_size, false,
                              ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
        {
            ImVec2 pos = ImGui::GetCursorScreenPos();
            for (size_t i = 0; i < _items.size(); ++i)
            {
                auto rounding_copy = rounding;
                if (i == 0 || i == _items.size() - 1)
                {
                    auto corner_flags = i == 0 ? ImDrawFlags_RoundCornersLeft : ImDrawFlags_RoundCornersRight;
                    draw_flags = corner_flags & ImDrawFlags_RoundCornersMask_;
                }
                else
                    rounding = 0.0f;
                ImVec2 rect_size;
                render_item(i, pos, rect_size);
                pos.x += rect_size.x;
                rounding = rounding_copy;
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }
} // namespace auik