#ifndef UIKIT_WIDGETS_MENU_H
#define UIKIT_WIDGETS_MENU_H

#include <astl/enum.hpp>
#include <astl/list.hpp>
#include <astl/vector.hpp>
#include <functional>
#include <imgui/imgui_internal.h>
#include "../selectable/selectable.hpp"
#include "../style.hpp" // IWYU pragma: keep

namespace uikit
{
    namespace style
    {
        extern APPLIB_API struct VMenu
        {
            f32 rounding;
            ImVec2 padding;
            ImVec2 margin;
            ImVec4 disabledHoverColor;
            ImVec4 hoverColor;
            Icon *arrowRight = nullptr;
        } g_VMenu;

        extern APPLIB_API struct HMenu
        {
            ImVec4 backgroundColor;
            ImVec4 hoverColor;
            ImVec2 padding;
            f32 rounding;
            Icon* checkmark = nullptr;
        } g_HMenu;
    } // namespace style

    class APPLIB_API HMenu final : public Selectable
    {
    public:
        HMenu(const std::string &name)
            : Selectable(name, false, style::g_HMenu.rounding,
                         ImGuiSelectableFlags_NoHoldingActiveID | ImGuiSelectableFlags_NoSetKeyOwner |
                             ImGuiSelectableFlags_SelectOnClick | ImGuiSelectableFlags_DontClosePopups)
        {
        }

        virtual void render() override;
    };

    class APPLIB_API VMenu final : public Selectable
    {
    public:
        VMenu(const std::string &name)
            : Selectable(name, false, style::g_VMenu.rounding, ImGuiSelectableFlags_SpanAvailWidth)
        {
        }

        virtual void render() override;
    };

    class APPLIB_API MenuItem final : public Selectable
    {
    public:
        std::function<void()> callback;
        std::string shortcut;

        MenuItem(const std::string &name, const std::function<void()> &callback = nullptr,
                 const std::string &shortcut = "")
            : Selectable(name, false, style::g_VMenu.rounding), callback(callback), shortcut(shortcut)
        {
        }

        virtual void render() override;
    };

    struct MenuNode
    {
        enum class FlagBits
        {
            data = 0x0,
            group = 0x1,
            category = 0x2
        };
        using Flags = ::Flags<FlagBits>;

        Flags flags;
        astl::unique_ptr<Selectable> widget;
        astl::vector<MenuNode> nodes;
    };

    class MenuBar : public Widget
    {
    public:
        astl::vector<MenuNode> nodes;

        MenuBar(const std::string &name) : Widget(name) {}
        virtual ~MenuBar() = default;

        virtual void render() override
        {
            auto &style = style::g_HMenu;
            ImGui::PushStyleColor(ImGuiCol_MenuBarBg, style.backgroundColor);
            ImGui::PushStyleColor(ImGuiCol_Header, style.hoverColor);
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, style.hoverColor);
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, style.backgroundColor);
            renderMenuNodes(nodes);
            ImGui::PopStyleColor(4);
        }

        static APPLIB_API void renderMenuNodes(const astl::vector<MenuNode> &nodes);

    protected:
        MenuBar(MenuBar &&other, const std::string &id) noexcept : Widget(id), nodes(std::move(other.nodes)) {}
    };
} // namespace uikit

template <>
struct FlagTraits<uikit::MenuNode::FlagBits>
{
    static constexpr bool isBitmask = true;
    static constexpr uikit::MenuNode::Flags allFlags =
        uikit::MenuNode::FlagBits::data | uikit::MenuNode::FlagBits::group | uikit::MenuNode::FlagBits::category;
};

#endif