#include <auik/menu/menu.hpp>
#include <awin/window.hpp>

namespace auik
{
    namespace style
    {
        VMenu g_vmenu;
        HMenu g_hmenu;
    } // namespace style
    static bool is_root_of_open_menu_set()
    {
        ImGuiContext &g = *GImGui;
        ImGuiWindow *window = g.CurrentWindow;
        if ((g.OpenPopupStack.Size <= g.BeginPopupStack.Size) || (window->Flags & ImGuiWindowFlags_ChildMenu))
            return false;
        const ImGuiPopupData *upper_popup = &g.OpenPopupStack[g.BeginPopupStack.Size];
        if (window->DC.NavLayerCurrent != upper_popup->ParentNavLayer) return false;
        return upper_popup->Window && (upper_popup->Window->Flags & ImGuiWindowFlags_ChildMenu) &&
               ImGui::IsWindowChildOf(upper_popup->Window, window, true);
    }

    void HMenu::render()
    {
        ImGuiWindow *window = ImGui::GetCurrentWindow();
        if (window->SkipItems) return;

        ImGuiContext &g = *GImGui;
        const ImGuiStyle &style = g.Style;
        auto label_c = name.c_str();
        const ImGuiID id = window->GetID(label_c);
        selected = ImGui::IsPopupOpen(id, ImGuiPopupFlags_None);

        // Sub-menus are ChildWindow so that mouse can be hovering across them (otherwise top-most popup menu would
        // steal focus and not allow hovering on parent menu) The first menu in a hierarchy isn't so hovering doesn't
        // get across (otherwise e.g. resizing borders with ImGuiButtonFlags_FlattenChildren would react), but top-most
        // BeginMenu() will bypass that limitation.
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_ChildMenu | ImGuiWindowFlags_AlwaysAutoResize |
                                        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar |
                                        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNavFocus;
        if (window->Flags & ImGuiWindowFlags_ChildMenu) window_flags |= ImGuiWindowFlags_ChildWindow;

        // If a menu with same the ID was already submitted, we will append to it, matching the behavior of Begin().
        // We are relying on a O(N) search - so O(N log N) over the frame - which seems like the most efficient for the
        // expected small amount of BeginMenu() calls per frame. If somehow this is ever becoming a problem we can
        // switch to use e.g. ImGuiStorage mapping key to last frame used.
        if (g.MenusIdSubmittedThisFrame.contains(id))
        {
            if (selected)
                selected = ImGui::BeginPopupEx(id, window_flags); // menu_is_open can be 'false' when the popup is
                                                                  // completely clipped (e.g. zero size display)
            else
                g.NextWindowData.ClearFlags(); // we behave like Begin() and need to consume those values
            return;
        }

        // Tag menu as used. Next time BeginMenu() with same ID is called it will append to existing menu
        g.MenusIdSubmittedThisFrame.push_back(id);

        ImVec2 label_size = ImGui::CalcTextSize(label_c, NULL, true);

        // Odd hack to allow hovering across menus of a same menu-set (otherwise we wouldn't be able to hover parent
        // without always being a Child window) This is only done for items for the menu set and not the full parent
        // window.
        const bool menuset_is_open = is_root_of_open_menu_set();
        if (menuset_is_open) ImGui::PushItemFlag(ImGuiItemFlags_NoWindowHoverableCheck, true);

        // The reference position stored in popup_pos will be used by Begin() to find a suitable position for the child
        // menu, However the final position is going to be different! It is chosen by FindBestWindowPosForPopup(). e.g.
        // Menus tend to overlap each other horizontally to amplify relative Z-ordering.
        ImVec2 popup_pos, pos = window->DC.CursorPos;
        ImGui::PushID(label_c);
        const ImGuiMenuColumns *offsets = &window->DC.MenuColumns;
        popup_pos = ImVec2(pos.x - 1.0f - IM_TRUNC(style.ItemSpacing.x * 0.5f),
                           pos.y - style.FramePadding.y + window->MenuBarHeight);
        window->DC.CursorPos.x += IM_TRUNC(style.ItemSpacing.x * 0.5f);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(style.ItemSpacing.x * 2.0f, style.ItemSpacing.y));
        f32 w = label_size.x;
        ImVec2 text_pos(window->DC.CursorPos.x + offsets->OffsetLabel,
                        window->DC.CursorPos.y + window->DC.CurrLineTextBaseOffset);
        size = ImVec2(w, label_size.y);
        Selectable::render("", *this);

        ImGui::RenderText(text_pos, label_c);
        ImGui::PopStyleVar();
        window->DC.CursorPos.x += IM_TRUNC(style.ItemSpacing.x * (-1.0f + 0.5f));

        hover = (g.HoveredId == id) && enabled && !g.NavHighlightItemUnderNav;
        if (menuset_is_open) ImGui::PopItemFlag();

        bool want_open = false;
        bool want_close = false;

        // Menu bar
        if (selected && pressed && menuset_is_open) // Click an open menu again to close it
        {
            want_close = true;
            want_open = selected = false;
        }
        else if (pressed || (hover && menuset_is_open && !selected))
            want_open = true;
        else if (g.NavId == id && g.NavMoveDir == ImGuiDir_Down) // Nav-Down to open
        {
            want_open = true;
            ImGui::NavMoveRequestCancel();
        }

        if (want_close && ImGui::IsPopupOpen(id, ImGuiPopupFlags_None))
            ImGui::ClosePopupToLevel(g.BeginPopupStack.Size, true);

        ImGui::PopID();

        if (want_open && !selected && g.OpenPopupStack.Size > g.BeginPopupStack.Size)
            ImGui::OpenPopup(label_c);
        else if (want_open)
        {
            selected = true;
            ImGui::OpenPopup(label_c);
        }

        if (selected)
        {
            ImGuiLastItemData last_item_in_parent = g.LastItemData;
            ImGui::SetNextWindowPos(popup_pos, ImGuiCond_Always); // Note: misleading: the value will serve as reference
                                                                  // for FindBestWindowPosForPopup(), not actual pos.
            ImGui::PushStyleVar(
                ImGuiStyleVar_ChildRounding,
                style.PopupRounding); // First level will use _PopupRounding, subsequent will use _ChildRounding
            selected =
                ImGui::BeginPopupMenuEx(id, label_c, window_flags); // menu_is_open can be 'false' when the popup is
                                                                    // completely clipped (e.g. zero size display)
            ImGui::PopStyleVar();
            if (selected)
            {
                // Restore LastItemData so IsItemXXXX functions can work after BeginMenu()/EndMenu()
                // (This fixes using IsItemClicked() and IsItemHovered(), but IsItemHovered() also relies on its support
                // for ImGuiItemFlags_NoWindowHoverableCheck)
                g.LastItemData = last_item_in_parent;
                if (g.HoveredWindow == window) g.LastItemData.StatusFlags |= ImGuiItemStatusFlags_HoveredWindow;
            }
        }
        else
            g.NextWindowData.ClearFlags();
    }

    void VMenu::render()
    {
        ImGuiWindow *window = ImGui::GetCurrentWindow();
        if (window->SkipItems) return;
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, style::g_vmenu.hover_color);
        ImGuiContext &g = *GImGui;
        const ImGuiStyle &style = g.Style;
        auto label_c = name.c_str();
        const ImGuiID id = window->GetID(label_c);
        selected = ImGui::IsPopupOpen(id, ImGuiPopupFlags_None);

        // Sub-menus are ChildWindow so that mouse can be hovering across them (otherwise top-most popup menu would
        // steal focus and not allow hovering on parent menu) The first menu in a hierarchy isn't so hovering doesn't
        // get across (otherwise e.g. resizing borders with ImGuiButtonFlags_FlattenChildren would react), but top-most
        // BeginMenu() will bypass that limitation.
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_ChildMenu | ImGuiWindowFlags_AlwaysAutoResize |
                                        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar |
                                        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNavFocus;
        if (window->Flags & ImGuiWindowFlags_ChildMenu) window_flags |= ImGuiWindowFlags_ChildWindow;

        // If a menu with same the ID was already submitted, we will append to it, matching the behavior of Begin().
        // We are relying on a O(N) search - so O(N log N) over the frame - which seems like the most efficient for the
        // expected small amount of BeginMenu() calls per frame. If somehow this is ever becoming a problem we can
        // switch to use e.g. ImGuiStorage mapping key to last frame used.
        if (g.MenusIdSubmittedThisFrame.contains(id))
        {
            if (selected)
                selected = ImGui::BeginPopupEx(id, window_flags); // menu_is_open can be 'false' when the popup is
                                                                  // completely clipped (e.g. zero size display)
            else
                g.NextWindowData.ClearFlags(); // we behave like Begin() and need to consume those values
        }

        // Tag menu as used. Next time BeginMenu() with same ID is called it will append to existing menu
        g.MenusIdSubmittedThisFrame.push_back(id);

        ImVec2 label_size = ImGui::CalcTextSize(label_c, nullptr, true);

        // Odd hack to allow hovering across menus of a same menu-set (otherwise we wouldn't be able to hover parent
        // without always being a Child window) This is only done for items for the menu set and not the full parent
        // window.
        const bool menuset_is_open = is_root_of_open_menu_set();
        if (menuset_is_open) ImGui::PushItemFlag(ImGuiItemFlags_NoWindowHoverableCheck, true);

        // The reference position stored in popup_pos will be used by Begin() to find a suitable position for the child
        // menu, However the final position is going to be different! It is chosen by FindBestWindowPosForPopup(). e.g.
        // Menus tend to overlap each other horizontally to amplify relative Z-ordering.
        ImVec2 popup_pos, pos = window->DC.CursorPos;
        ImGui::PushID(label_c);
        const ImGuiMenuColumns *offsets = &window->DC.MenuColumns;
        popup_pos = ImVec2(pos.x, pos.y - style.WindowPadding.y);
        f32 checkmark_w = IM_TRUNC(g.FontSize * 1.20f);
        f32 min_w = window->DC.MenuColumns.DeclColumns(0.0f, label_size.x, 0.0f, checkmark_w); // Feedback to next frame
        f32 extra_w = ImMax(0.0f, ImGui::GetContentRegionAvail().x - min_w);
        ImVec2 text_pos(window->DC.CursorPos.x + offsets->OffsetLabel + style::g_vmenu.padding.x,
                        window->DC.CursorPos.y + window->DC.CurrLineTextBaseOffset);
        size = ImVec2(min_w + style::g_vmenu.padding.x, label_size.y);
        Selectable::render("", *this);

        ImVec2 arrowPos =
            pos + ImVec2(offsets->OffsetMark + extra_w + g.FontSize * 0.30f + style.ItemInnerSpacing.x * 0.5f,
                         label_size.y / 2.0f - style::g_vmenu.arrow_right->size().y / 2.0f);
        ImGui::RenderText(text_pos, label_c);
        style::g_vmenu.arrow_right->render(arrowPos);

        hover = (g.HoveredId == id) && enabled && !g.NavHighlightItemUnderNav;
        if (menuset_is_open) ImGui::PopItemFlag();

        bool want_open = false;
        bool want_close = false;

        // Close menu when not hovering it anymore unless we are moving roughly in the direction of the menu
        // Implement http://bjk5.com/post/44698559168/breaking-down-amazons-mega-dropdown to avoid using timers, so
        // menus feels more reactive.
        bool moving_toward_child_menu = false;
        ImGuiPopupData *child_popup =
            (g.BeginPopupStack.Size < g.OpenPopupStack.Size) ? &g.OpenPopupStack[g.BeginPopupStack.Size] : nullptr;
        ImGuiWindow *child_menu_window =
            (child_popup && child_popup->Window && child_popup->Window->ParentWindow == window) ? child_popup->Window
                                                                                                : nullptr;
        if (g.HoveredWindow == window && child_menu_window != nullptr)
        {
            const f32 ref_unit = g.FontSize; // FIXME-DPI
            const f32 child_dir = (window->Pos.x < child_menu_window->Pos.x) ? 1.0f : -1.0f;
            const ImRect next_window_rect = child_menu_window->Rect();
            ImVec2 ta = (g.IO.MousePos - g.IO.MouseDelta);
            ImVec2 tb = (child_dir > 0.0f) ? next_window_rect.GetTL() : next_window_rect.GetTR();
            ImVec2 tc = (child_dir > 0.0f) ? next_window_rect.GetBL() : next_window_rect.GetBR();
            const f32 pad_farmost_h = ImClamp(ImFabs(ta.x - tb.x) * 0.30f, ref_unit * 0.5f, ref_unit * 2.5f);
            ta.x += child_dir * -0.5f;
            tb.x += child_dir * ref_unit;
            tc.x += child_dir * ref_unit;
            tb.y = ta.y + ImMax((tb.y - pad_farmost_h) - ta.y, -ref_unit * 8.0f);
            tc.y = ta.y + ImMin((tc.y + pad_farmost_h) - ta.y, +ref_unit * 8.0f);
            moving_toward_child_menu = ImTriangleContainsPoint(ta, tb, tc, g.IO.MousePos);
        }

        // The 'HovereWindow == window' check creates an inconsistency (e.g. moving away from menu slowly tends to
        // hit same window, whereas moving away fast does not) But we also need to not close the top-menu menu when
        // moving over void. Perhaps we should extend the triangle check to a larger polygon. (Remember to test this
        // on BeginPopup("A")->BeginMenu("B") sequence which behaves slightly differently as B isn't a Child of A
        // and hovering isn't shared.)
        if (selected && !hover && g.HoveredWindow == window && !moving_toward_child_menu &&
            !g.NavHighlightItemUnderNav && g.ActiveId == 0)
            want_close = true;

        // Open
        // (note: at this point 'hovered' actually includes the NavDisableMouseHover == false test)
        if (!selected && pressed) // Click/activate to open
            want_open = true;
        else if (!selected && hover && !moving_toward_child_menu) // Hover to open
            want_open = true;
        else if (!selected && hover && g.HoveredIdTimer >= 0.30f &&
                 g.MouseStationaryTimer >= 0.30f) // Hover to open (timer fallback)
            want_open = true;
        if (g.NavId == id && g.NavMoveDir == ImGuiDir_Right) // Nav-Right to open
        {
            want_open = true;
            ImGui::NavMoveRequestCancel();
        }

        if (want_close && ImGui::IsPopupOpen(id, ImGuiPopupFlags_None))
            ImGui::ClosePopupToLevel(g.BeginPopupStack.Size, true);
        ImGui::PopID();

        if (want_open && !selected && g.OpenPopupStack.Size > g.BeginPopupStack.Size)
            ImGui::OpenPopup(label_c);
        else if (want_open)
        {
            selected = true;
            ImGui::OpenPopup(label_c);
        }

        if (selected)
        {
            ImGuiLastItemData last_item_in_parent = g.LastItemData;
            ImGui::SetNextWindowPos(popup_pos, ImGuiCond_Always); // Note: misleading: the value will serve as reference
                                                                  // for FindBestWindowPosForPopup(), not actual pos.
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, style.PopupRounding);
            selected = ImGui::BeginPopupMenuEx(id, label_c, window_flags);
            ImGui::PopStyleVar();
            if (selected)
            {
                // Restore LastItemData so IsItemXXXX functions can work after BeginMenu()/EndMenu()
                // (This fixes using IsItemClicked() and IsItemHovered(), but IsItemHovered() also relies on its support
                // for ImGuiItemFlags_NoWindowHoverableCheck)
                g.LastItemData = last_item_in_parent;
                if (g.HoveredWindow == window) g.LastItemData.StatusFlags |= ImGuiItemStatusFlags_HoveredWindow;
            }
        }
        else
            g.NextWindowData.ClearFlags(); // We behave like Begin() and need to consume those values
        ImGui::PopStyleColor();
    }

    void MenuItem::render()
    {
        ImGuiWindow *window = ImGui::GetCurrentWindow();
        if (window->SkipItems) return;

        ImGuiContext &g = *GImGui;
        ImGuiStyle &style = g.Style;
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, style::g_vmenu.padding);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, style::g_vmenu.hover_color);
        ImVec2 pos = window->DC.CursorPos;
        auto label_c = name.c_str();
        ImVec2 label_size = ImGui::CalcTextSize(label_c, nullptr, true);

        const bool menuset_is_open = is_root_of_open_menu_set();
        if (menuset_is_open) ImGui::PushItemFlag(ImGuiItemFlags_NoWindowHoverableCheck, true);

        // We've been using the equivalent of ImGuiSelectableFlags_SetNavIdOnHover on all Selectable() since early Nav
        // system days (commit 43ee5d73), but I am unsure whether this should be kept at all. For now moved it to be an
        // opt-in feature used by menus only.
        ImGui::PushID(label_c);

        const ImGuiMenuColumns *offsets = &window->DC.MenuColumns;

        auto shortcut_ptr = shortcut.c_str();
        f32 shortcut_w = (shortcut_ptr && shortcut_ptr[0]) ? ImGui::CalcTextSize(shortcut_ptr, nullptr).x : 0.0f;
        f32 checkmark_w = IM_TRUNC(g.FontSize * 1.20f);
        f32 min_w = window->DC.MenuColumns.DeclColumns(0.0f, label_size.x, shortcut_w, checkmark_w);
        f32 stretch_w = ImMax(0.0f, ImGui::GetContentRegionAvail().x - min_w);
        size = ImVec2(min_w + style::g_vmenu.padding.x, label_size.y);
        Selectable::render("", *this);

        if (g.LastItemData.StatusFlags & ImGuiItemStatusFlags_Visible)
        {
            const ImVec4 text_color =
                style.Colors[selectable_flags & ImGuiSelectableFlags_Disabled ? ImGuiCol_TextDisabled : ImGuiCol_Text];
            ImGui::PushStyleColor(ImGuiCol_Text, text_color);
            ImGui::RenderText(pos + ImVec2(offsets->OffsetLabel + style::g_vmenu.padding.x, 0.0f), label_c);
            ImGui::PopStyleColor();
            if (shortcut_w > 0.0f)
            {
                ImVec2 shortcut_pos = ImVec2(
                    window->Pos.x + window->Size.x - shortcut_w - style.ItemSpacing.x - style.WindowPadding.x, pos.y);
                if (hover)
                    ImGui::PushStyleColor(ImGuiCol_Text, style::g_vmenu.disabled_hover_color);
                else
                    ImGui::PushStyleColor(ImGuiCol_Text, style.Colors[ImGuiCol_TextDisabled]);
                ImGui::RenderText(shortcut_pos, shortcut_ptr);
                ImGui::PopStyleColor();
            }
            if (selected)
            {
                ImVec2 checkmark_pos =
                    pos + ImVec2(offsets->OffsetMark + stretch_w + g.FontSize * 0.30f + style.ItemInnerSpacing.x * 0.5f,
                                 label_size.y / 2.0f - style::g_vmenu.arrow_right->size().y / 2.0f);
                style::g_hmenu.checkmark->render(checkmark_pos);
            }
        }

        ImGui::PopID();
        if (menuset_is_open) ImGui::PopItemFlag();
        if (pressed)
        {
            if (callback) callback();
            pressed = false;
            awin::push_empty_event();
        }

        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }

    void MenuBar::render_menu_nodes(const acul::vector<MenuNode> &nodes)
    {
        for (size_t i = 0; i < nodes.size(); i++)
        {
            auto &node = nodes[i];
            if (node.flags & MenuNode::FlagBits::group)
            {
                if (node.flags & MenuNode::FlagBits::category)
                {
                    render_menu_nodes(node.nodes);
                    if (i != nodes.size() - 1)
                    {
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + style::g_vmenu.padding.y / 2.0f);
                        ImGui::Separator();
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + style::g_vmenu.padding.y / 2.0f);
                    }
                }
                else
                {
                    node.widget->render();
                    if (node.widget->selected)
                    {
                        render_menu_nodes(node.nodes);
                        ImGui::EndMenu();
                    }
                }
            }
            else
                node.widget->render();
        }
    }
} // namespace auik