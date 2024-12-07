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
            float btnSize;
            float rounding;
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
            bool selected;
            std::function<void()> callback;
            uikit::Icon *disabledIcon = nullptr;
        };

        enum class FlagBits
        {
            none = 0x0,
            resetOnClick = 0x1,
            toogle = 0x2,
            lock = 0x4
        };

        using Flags = Flags<FlagBits>;

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

        // void renderHorizontal(); // todo: for most recent commands

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

    private:
        astl::vector<Item> _items;
        Flags _flags;
        int _activeID;

        void renderItem(size_t index, ImVec2 &pos, ImVec2 &rectSize);
    };
} // namespace uikit

template <>
struct FlagTraits<uikit::BtnGroup::FlagBits>
{
    static constexpr bool isBitmask = true;
    static constexpr uikit::BtnGroup::Flags allFlags =
        uikit::BtnGroup::FlagBits::none | uikit::BtnGroup::FlagBits::resetOnClick | uikit::BtnGroup::FlagBits::toogle |
        uikit::BtnGroup::FlagBits::lock;
};