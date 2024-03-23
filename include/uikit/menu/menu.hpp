#ifndef UIKIT_WIDGETS_MENU_H
#define UIKIT_WIDGETS_MENU_H

#include <forward_list>
#include <functional>
#include <imgui/imgui_internal.h>
#include <memory>
#include "../icon/icon.hpp"
#include "../selectable/selectable.hpp"
#include "../widget.hpp"
#include "../icon/icon.hpp"

namespace ui
{

    class VMenu : public Widget
    {
        struct _Item
        {
            Selectable *menu;
            VMenu *submenu{nullptr};
            std::function<void()> callback{nullptr};
            std::function<void(Selectable *menu)> beforeRender{nullptr};
        };

        using _ItemGroup = std::forward_list<_Item>;

    public:
        struct Style
        {
            float rounding;
            ImVec2 padding;
            ImVec4 backgroundColor;
            ImVec4 separatorColor;
        };

        struct Item
        {
            std::string label;
            std::function<void()> callback;
            std::function<void(Selectable *menu)> beforeRender{nullptr};
            std::string shortcut;
            VMenu *submenu{nullptr};
        };

        using ItemGroup = std::forward_list<Item>;

        VMenu(std::initializer_list<ItemGroup> itemgroups, const Style &style,
              const std::shared_ptr<Icon> &arrowIcon = nullptr);

        virtual void render() override;

        void destroyItems();

        Style style() const { return _style; }

    private:
        std::forward_list<_ItemGroup> _itemGroups;

        const Style &_style;
    };

    class BeginMenu : public Selectable
    {
    public:
        BeginMenu(const std::string &label, const std::shared_ptr<Icon> &arrowIcon, const VMenu::Style &style)
            : Selectable(label, false, style.rounding,
                         ImGuiSelectableFlags_NoHoldingActiveID | ImGuiSelectableFlags_NoSetKeyOwner |
                             ImGuiSelectableFlags_SelectOnClick | ImGuiSelectableFlags_DontClosePopups),
              _style(style),
              _arrowIcon(arrowIcon)
        {
        }

        virtual void render() override;

    private:
        const VMenu::Style &_style;
        std::shared_ptr<Icon> _arrowIcon;
    };

    class MenuItem : public Selectable
    {
    public:
        MenuItem(const std::string &label, const std::string &shortcut, const VMenu::Style &style)
            : Selectable(label, false, style.rounding,
                         ImGuiSelectableFlags_SelectOnRelease | ImGuiSelectableFlags_NoSetKeyOwner |
                             ImGuiSelectableFlags_SetNavIdOnHover),
              _style(style),
              _shortcut(shortcut)
        {
        }

        virtual void render() override;

    private:
        std::string _shortcut;
        const VMenu::Style &_style;
    };

    class HMenu : public Selectable
    {
    public:
        struct Style
        {
            ImVec2 padding;
            ImVec2 margin;
            float rounding;
            ImVec4 backgroundColor;
            ImVec4 hoverColor;

            explicit Style(ImVec2 margin = ImGui::GetStyle().FramePadding,
                           ImVec2 padding = ImGui::GetStyle().ItemSpacing, float rounding = 0.0f,
                           ImVec4 backgroundColor = ImVec4(0.0f, 0.0f, 0.0f, 0.0f),
                           ImVec4 hoverColor = ImVec4(0.0f, 0.0f, 0.0f, 0.0f))
                : margin(margin + padding * 0.5f),
                  padding(padding),
                  rounding(rounding),
                  backgroundColor(backgroundColor),
                  hoverColor(hoverColor)
            {
            }
        };

        HMenu(const std::string &label, const VMenu &submenu, const Style &style)
            : Selectable(label, false, style.rounding,
                         ImGuiSelectableFlags_NoHoldingActiveID | ImGuiSelectableFlags_NoSetKeyOwner |
                             ImGuiSelectableFlags_SelectOnClick | ImGuiSelectableFlags_DontClosePopups),
              _submenu(submenu)
        {
        }

        void destroy() { _submenu.destroyItems(); }

        VMenu &submenu() { return _submenu; }

        virtual void render() override;

    private:
        VMenu _submenu;

        void beginMenu();
    };

    class MenuBar : public Widget
    {
    public:
        struct Style
        {
            HMenu::Style menubar;
            VMenu::Style submenu;
        };

        MenuBar(std::forward_list<HMenu> items, Style *style) : _style(style), _items(items) {}

        virtual ~MenuBar();

        MenuBar(MenuBar &&other) noexcept : _style(other._style), _items(std::move(other._items))
        {
            other._items.clear();
            other._style = nullptr;
        }

        Style *style() { return _style; }

        virtual void render() override;

    protected:
        Style *_style;
        std::forward_list<HMenu> _items;
    };
} // namespace ui

#endif