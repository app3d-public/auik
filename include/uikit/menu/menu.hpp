#ifndef UIKIT_WIDGETS_MENU_H
#define UIKIT_WIDGETS_MENU_H

#include <core/std/forward_list.hpp>
#include <functional>
#include <imgui/imgui_internal.h>
#include <memory>
#include "../icon/icon.hpp"
#include "../selectable/selectable.hpp"
#include "../widget.hpp"

namespace ui
{

    class APPLIB_API VMenu : public Widget
    {
        struct _Item
        {
            Selectable *menu;
            VMenu *submenu{nullptr};
            std::function<void()> callback{nullptr};
            std::function<void(Selectable *menu)> beforeRender{nullptr};
        };

        using _ItemGroup = ForwardList<_Item>;

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

        using ItemGroup = ForwardList<Item>;

        VMenu(std::initializer_list<ItemGroup> itemgroups, const Style &style,
              const std::shared_ptr<Icon> &arrowIcon = nullptr);

        virtual void render() override;

        void destroyItems();

        Style style() const { return _style; }

    private:
        ForwardList<_ItemGroup> _itemGroups;

        const Style &_style;
    };

    class APPLIB_API BeginMenu : public Selectable
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

    class APPLIB_API MenuItem : public Selectable
    {
    public:
        MenuItem(const std::string &label, const std::string &shortcut, const VMenu::Style &style)
            : Selectable(label, false, style.rounding,
                         ImGuiSelectableFlags_SelectOnRelease | ImGuiSelectableFlags_NoSetKeyOwner |
                             ImGuiSelectableFlags_SetNavIdOnHover),
              _shortcut(shortcut),
              _style(style)
        {
        }

        virtual void render() override;

    private:
        std::string _shortcut;
        const VMenu::Style &_style;
    };

    class APPLIB_API HMenu : public Selectable
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
                : padding(padding),
                  margin(margin + padding * 0.5f),
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

    class APPLIB_API MenuBar : public Widget
    {
    public:
        struct Style
        {
            HMenu::Style menubar;
            VMenu::Style submenu;
        };

        MenuBar(const ForwardList<HMenu> &items, Style *style) : _style(style), _items(items) {}

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
        ForwardList<HMenu> _items;
    };
} // namespace ui

#endif