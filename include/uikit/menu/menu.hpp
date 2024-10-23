#ifndef UIKIT_WIDGETS_MENU_H
#define UIKIT_WIDGETS_MENU_H

#include <core/std/list.hpp>
#include <core/std/vector.hpp>
#include <functional>
#include <imgui/imgui_internal.h>
#include "../selectable/selectable.hpp"
#include "../style.hpp" // IWYU pragma: keep
#include "../widget.hpp"

namespace uikit
{
    namespace style
    {
        extern APPLIB_API struct VMenu
        {
            f32 rounding;
            ImVec2 padding;
            ImVec4 backgroundColor;
            ImVec4 separatorColor;
            Icon *arrowRight = nullptr;
        } *g_StyleVMenu;

        inline void registerStyle(VMenu *style) { g_StyleVMenu = style; }
    } // namespace style

    class APPLIB_API VMenu final : public Widget
    {
        struct _Item
        {
            Selectable *menu;
            VMenu *submenu{nullptr};
            std::function<void()> callback{nullptr};
            std::function<void(Selectable *menu)> beforeRender{nullptr};
        };

        using _ItemGroup = astl::list<_Item>;

    public:
        struct Item
        {
            std::string label;
            std::function<void()> callback;
            std::function<void(Selectable *menu)> beforeRender{nullptr};
            std::string shortcut;
            VMenu *submenu{nullptr};
        };
        style::VMenu *style;

        using ItemGroup = astl::list<Item>;

        VMenu(const astl::vector<ItemGroup> &itemgroups, style::VMenu *style) : Widget("vmenu"), style(style)
        {
            init(itemgroups);
        }

        VMenu(style::VMenu *style = nullptr) : Widget("vmenu"), style(style) {}

        virtual void render() override;

        void destroyItems();

        template <typename Container>
        APPLIB_API void init(const Container &itemgroups);

        bool empty() const { return _itemGroups.empty(); }

        void clear()
        {
            for (auto &g : _itemGroups)
            {
                for (auto &item : g)
                {
                    astl::release(item.menu);
                    astl::release(item.submenu);
                }
            }
            _itemGroups.clear();
        }

    private:
        astl::list<_ItemGroup> _itemGroups;
    };

    class APPLIB_API BeginMenu final : public Selectable
    {
    public:
        BeginMenu(const std::string &label, const style::VMenu &style)
            : Selectable(label, false, style.rounding,
                         ImGuiSelectableFlags_NoHoldingActiveID | ImGuiSelectableFlags_NoSetKeyOwner |
                             ImGuiSelectableFlags_SelectOnClick | ImGuiSelectableFlags_DontClosePopups),
              _style(style)
        {
        }

        virtual void render() override;

    private:
        const style::VMenu &_style;
    };

    class APPLIB_API MenuItem final : public Selectable
    {
    public:
        MenuItem(const std::string &label, const std::string &shortcut, const style::VMenu &style)
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
        const style::VMenu &_style;
    };

    class APPLIB_API HMenu final : public Selectable
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
        VMenu submenu;

        HMenu(const std::string &label, const Style &style, const VMenu &submenu = {})
            : Selectable(label, false, style.rounding,
                         ImGuiSelectableFlags_NoHoldingActiveID | ImGuiSelectableFlags_NoSetKeyOwner |
                             ImGuiSelectableFlags_SelectOnClick | ImGuiSelectableFlags_DontClosePopups),
              submenu(submenu)
        {
        }

        void destroy() { submenu.destroyItems(); }

        virtual void render() override;

    private:
        void beginMenu();
    };

    class APPLIB_API MenuBar : public Widget
    {
    public:
        struct Style
        {
            HMenu::Style menubar;
            style::VMenu submenu;
        };

        MenuBar(const astl::list<HMenu> &items, Style *style) : Widget("menubar"), _style(style), _items(items) {}

        virtual ~MenuBar();

        MenuBar(MenuBar &&other) noexcept : Widget("menubar"), _style(other._style), _items(std::move(other._items))
        {
            other._items.clear();
            other._style = nullptr;
        }

        Style *style() { return _style; }

        virtual void render() override;

    protected:
        Style *_style;
        astl::list<HMenu> _items;

        MenuBar(MenuBar &&other, const std::string &id) noexcept
            : Widget(id), _style(other._style), _items(std::move(other._items))
        {
            other._items.clear();
            other._style = nullptr;
        }
    };
} // namespace uikit

#endif