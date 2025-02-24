#pragma once

#include <astl/enum.hpp>
#include <astl/vector.hpp>
#include <functional>
#include "../icon/icon.hpp"
#include "../selectable/selectable.hpp"

namespace uikit
{
    namespace style
    {
        extern APPLIB_API struct BtnGroup
        {
            f32 btnSize;
            f32 rounding;
            uikit::Icon *lockIcon;
        } g_BtnGroup;

        extern APPLIB_API struct BtnGroupH
        {
            ImVec4 groupBg;
            ImVec4 groupItemBg;
        } g_BtnGroupH;
    }; // namespace style

    class APPLIB_API BtnGroup final : public uikit::Selectable
    {
    public:
        struct Item
        {
            uikit::Icon *icon;
            u16 id; // Note: We don't use this field. It needed as id for calls from external context
            bool selected;
            std::function<void()> callback = nullptr;
            uikit::Icon *disabledIcon = nullptr;
            bool disabled = false;

            ~Item() {}
        };

        using value_type = Item;
        using iterator = astl::vector<Item>::iterator;
        using const_iterator = astl::vector<Item>::const_iterator;

        struct FlagBits
        {
            enum enum_type
            {
                none = 0x0,
                resetOnClick = 0x1,
                toogle = 0x2,
                lock = 0x4
            };

            using flag_bitmask = std::true_type;
        };

        using Flags = astl::flags<FlagBits>;

        BtnGroup(const std::string &name, const astl::vector<Item> &items, Flags flags, int activeID)
            : Selectable({"##image_group_" + name,
                          false,
                          style::g_BtnGroup.rounding,
                          0,
                          {style::g_BtnGroup.btnSize, style::g_BtnGroup.btnSize}}),
              _items(items),
              _flags(flags),
              _activeID(activeID)
        {
        }

        size_t items_count() const { return _items.size(); }

        void renderHorizontal()
        {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
            ImVec2 pos = ImGui::GetCursorScreenPos();
            for (size_t i = 0; i < _items.size(); ++i)
            {
                ImVec2 rectSize;
                renderItem(i, pos, rectSize);
                pos.x += rectSize.x;
            }
            ImGui::PopStyleVar();
        }

        void renderVertical()
        {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
            ImVec2 pos = ImGui::GetCursorScreenPos();
            for (size_t i = 0; i < _items.size(); ++i)
            {
                ImVec2 rectSize;
                renderItem(i, pos, rectSize);
                pos.y += rectSize.y;
            }
            ImGui::PopStyleVar();
        }

        void renderAsGroup();

        Item &operator[](size_t index) { return _items[index]; }
        const Item &operator[](size_t index) const { return _items[index]; }

        iterator begin() { return _items.begin(); }
        iterator end() { return _items.end(); }

        const_iterator begin() const { return _items.begin(); }
        const_iterator end() const { return _items.end(); }

        astl::vector<Item> &items() { return _items; }
        const astl::vector<Item> &items() const { return _items; }

    private:
        astl::vector<Item> _items;
        Flags _flags;
        int _activeID;

        void renderItem(size_t index, ImVec2 &pos, ImVec2 &rectSize);
    };
} // namespace uikit