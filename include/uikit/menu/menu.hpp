#ifndef UIKIT_WIDGETS_MENU_H
#define UIKIT_WIDGETS_MENU_H

#include <acul/enum.hpp>
#include <imgui/imgui_internal.h>
#include "../icon/icon.hpp"
#include "../selectable/selectable.hpp"

namespace uikit
{
    namespace style
    {
        extern APPLIB_API struct VMenu
        {
            f32 rounding;
            ImVec2 padding;
            ImVec2 margin;
            ImVec4 disabled_hover_color;
            ImVec4 hover_color;
            Icon *arrowRight = nullptr;
        } g_vmenu;

        extern APPLIB_API struct HMenu
        {
            ImVec4 background_color;
            ImVec4 hover_color;
            ImVec2 padding;
            f32 rounding;
            Icon *checkmark = nullptr;
        } g_hmenu;
    } // namespace style

    class APPLIB_API HMenu final : public Selectable
    {
    public:
        HMenu(const acul::string &name)
            : Selectable(name, false, style::g_hmenu.rounding,
                         ImGuiSelectableFlags_NoHoldingActiveID | ImGuiSelectableFlags_NoSetKeyOwner |
                             ImGuiSelectableFlags_SelectOnClick | ImGuiSelectableFlags_DontClosePopups)
        {
        }

        virtual void render() override;
    };

    class APPLIB_API VMenu final : public Selectable
    {
    public:
        VMenu(const acul::string &name)
            : Selectable(name, false, style::g_vmenu.rounding, ImGuiSelectableFlags_SpanAvailWidth)
        {
        }

        virtual void render() override;
    };

    class APPLIB_API MenuItem final : public Selectable
    {
    public:
        std::function<void()> callback;
        acul::string shortcut;

        MenuItem(const acul::string &name, const std::function<void()> &callback = nullptr,
                 const acul::string &shortcut = {})
            : Selectable(name, false, style::g_vmenu.rounding), callback(callback), shortcut(shortcut)
        {
        }

        virtual void render() override;
    };

    struct MenuNode
    {
        struct FlagBits
        {
            enum enum_type
            {
                Data = 0x0,
                Group = 0x1,
                Category = 0x2
            };

            using flag_bitmask = std::true_type;
        };
        using Flags = acul::flags<FlagBits>;

        Flags flags;
        acul::unique_ptr<Selectable> widget;
        acul::vector<MenuNode> nodes;
    };

    class MenuBar : public Widget
    {
    public:
        acul::vector<MenuNode> nodes;

        MenuBar(const acul::string &name) : Widget(name) {}
        virtual ~MenuBar() = default;

        virtual void render() override
        {
            auto &style = style::g_hmenu;
            ImGui::PushStyleColor(ImGuiCol_MenuBarBg, style.background_color);
            ImGui::PushStyleColor(ImGuiCol_Header, style.hover_color);
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, style.hover_color);
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, style.background_color);
            render_menu_nodes(nodes);
            ImGui::PopStyleColor(4);
        }

        static APPLIB_API void render_menu_nodes(const acul::vector<MenuNode> &nodes);

    protected:
        MenuBar(MenuBar &&other, const acul::string &id) noexcept : Widget(id), nodes(std::move(other.nodes)) {}
    };
} // namespace uikit

#endif