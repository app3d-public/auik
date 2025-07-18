#pragma once

#include <acul/enum.hpp>
#include "../icon/icon.hpp"
#include "../selectable/selectable.hpp"

namespace auik
{
    namespace style
    {
        extern APPLIB_API struct BtnGroup
        {
            f32 btn_size;
            f32 rounding;
            auik::Icon *lock_icon;
        } g_btn_group;

        extern APPLIB_API struct BtnGroupH
        {
            ImVec4 group_bg;
            ImVec4 group_item_bg;
        } g_btn_group_h;
    }; // namespace style

    class APPLIB_API BtnGroup final : public auik::Selectable
    {
    public:
        struct Item
        {
            auik::Icon *icon;
            u16 id; // Note: We don't use this field. It needed as id for calls from external context
            bool selected;
            std::function<void()> callback = nullptr;
            auik::Icon *disabled_icon = nullptr;
            bool disabled = false;
        };

        using value_type = Item;
        using iterator = acul::vector<Item>::iterator;
        using const_iterator = acul::vector<Item>::const_iterator;

        struct FlagBits
        {
            enum enum_type
            {
                none = 0x0,
                reset_on_click = 0x1,
                toogle = 0x2,
                lock = 0x4
            };

            using flag_bitmask = std::true_type;
        };

        using Flags = acul::flags<FlagBits>;

        BtnGroup(const acul::string &name, const acul::vector<Item> &items, Flags flags, int active_id)
            : Selectable({"##image_group_" + name,
                          false,
                          style::g_btn_group.rounding,
                          0,
                          {style::g_btn_group.btn_size, style::g_btn_group.btn_size}}),
              _items(items),
              _flags(flags),
              _active_id(active_id)
        {
        }

        size_t items_count() const { return _items.size(); }

        void render_horizontal()
        {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
            ImVec2 pos = ImGui::GetCursorScreenPos();
            for (size_t i = 0; i < _items.size(); ++i)
            {
                ImVec2 rect_size;
                render_item(i, pos, rect_size);
                pos.x += rect_size.x;
            }
            ImGui::PopStyleVar();
        }

        void render_vertical()
        {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
            ImVec2 pos = ImGui::GetCursorScreenPos();
            for (size_t i = 0; i < _items.size(); ++i)
            {
                ImVec2 rect_size;
                render_item(i, pos, rect_size);
                pos.y += rect_size.y;
            }
            ImGui::PopStyleVar();
        }

        void render_as_group();

        Item &operator[](size_t index) { return _items[index]; }
        const Item &operator[](size_t index) const { return _items[index]; }

        iterator begin() { return _items.begin(); }
        iterator end() { return _items.end(); }

        const_iterator begin() const { return _items.begin(); }
        const_iterator end() const { return _items.end(); }

        acul::vector<Item> &items() { return _items; }
        const acul::vector<Item> &items() const { return _items; }

    private:
        acul::vector<Item> _items;
        Flags _flags;
        int _active_id;

        void render_item(int index, ImVec2 &pos, ImVec2 &rect_size);
    };
} // namespace auik