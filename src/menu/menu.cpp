#include <uikit/menu/menu.hpp>
#include <window/window.hpp>

static bool IsRootOfOpenMenuSet()
{
    ImGuiContext &g = *GImGui;
    ImGuiWindow *window = g.CurrentWindow;
    if ((g.OpenPopupStack.Size <= g.BeginPopupStack.Size) || (window->Flags & ImGuiWindowFlags_ChildMenu))
        return false;

    // Initially we used 'upper_popup->OpenParentId == window->IDStack.back()' to differentiate multiple menu sets
    // from each others (e.g. inside menu bar vs loose menu items) based on parent ID. This would however prevent
    // the use of e.g. PushID() user code submitting menus. Previously this worked between popup and a first child
    // menu because the first child menu always had the _ChildWindow flag, making hovering on parent popup possible
    // while first child menu was focused - but this was generally a bug with other side effects. Instead we don't
    // treat Popup specifically (in order to consistently support menu features in them), maybe the first child menu
    // of a Popup doesn't have the _ChildWindow flag, and we rely on this IsRootOfOpenMenuSet() check to allow
    // hovering between root window/popup and first child menu. In the end, lack of ID check made it so we could no
    // longer differentiate between separate menu sets. To compensate for that, we at least check parent window nav
    // layer. This fixes the most common case of menu opening on hover when moving between window content and menu
    // bar. Multiple different menu sets in same nav layer would still open on hover, but that should be a lesser
    // problem, because if such menus are close in proximity in window content then it won't feel weird and if they
    // are far apart it likely won't be a problem anyone runs into.
    const ImGuiPopupData *upper_popup = &g.OpenPopupStack[g.BeginPopupStack.Size];
    if (window->DC.NavLayerCurrent != upper_popup->ParentNavLayer)
        return false;
    return upper_popup->Window && (upper_popup->Window->Flags & ImGuiWindowFlags_ChildMenu) &&
           ImGui::IsWindowChildOf(upper_popup->Window, window, true);
}

namespace ui
{
    void HMenu::beginMenu()
    {
        ImGuiWindow *window = ImGui::GetCurrentWindow();
        if (window->SkipItems)
            return;

        ImGuiContext &g = *GImGui;
        const ImGuiStyle &style = g.Style;
        auto labelPtr = _label.c_str();
        const ImGuiID id = window->GetID(labelPtr);
        _selected = ImGui::IsPopupOpen(id, ImGuiPopupFlags_None);

        // Sub-menus are ChildWindow so that mouse can be hovering across them (otherwise top-most popup menu would
        // steal focus and not allow hovering on parent menu) The first menu in a hierarchy isn't so hovering doesn't
        // get across (otherwise e.g. resizing borders with ImGuiButtonFlags_FlattenChildren would react), but top-most
        // BeginMenu() will bypass that limitation.
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_ChildMenu | ImGuiWindowFlags_AlwaysAutoResize |
                                        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar |
                                        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNavFocus;
        if (window->Flags & ImGuiWindowFlags_ChildMenu)
            window_flags |= ImGuiWindowFlags_ChildWindow;

        // If a menu with same the ID was already submitted, we will append to it, matching the behavior of Begin().
        // We are relying on a O(N) search - so O(N log N) over the frame - which seems like the most efficient for the
        // expected small amount of BeginMenu() calls per frame. If somehow this is ever becoming a problem we can
        // switch to use e.g. ImGuiStorage mapping key to last frame used.
        if (g.MenusIdSubmittedThisFrame.contains(id))
        {
            if (_selected)
                _selected = ImGui::BeginPopupEx(id, window_flags); // menu_is_open can be 'false' when the popup is
                                                                   // completely clipped (e.g. zero size display)
            else
                g.NextWindowData.ClearFlags(); // we behave like Begin() and need to consume those values
            return;
        }

        // Tag menu as used. Next time BeginMenu() with same ID is called it will append to existing menu
        g.MenusIdSubmittedThisFrame.push_back(id);

        ImVec2 label_size = ImGui::CalcTextSize(labelPtr, NULL, true);

        // Odd hack to allow hovering across menus of a same menu-set (otherwise we wouldn't be able to hover parent
        // without always being a Child window) This is only done for items for the menu set and not the full parent
        // window.
        const bool menuset_is_open = IsRootOfOpenMenuSet();
        if (menuset_is_open)
            ImGui::PushItemFlag(ImGuiItemFlags_NoWindowHoverableCheck, true);

        // The reference position stored in popup_pos will be used by Begin() to find a suitable position for the child
        // menu, However the final position is going to be different! It is chosen by FindBestWindowPosForPopup(). e.g.
        // Menus tend to overlap each other horizontally to amplify relative Z-ordering.
        ImVec2 popup_pos, pos = window->DC.CursorPos;
        ImGui::PushID(labelPtr);
        const ImGuiMenuColumns *offsets = &window->DC.MenuColumns;
        popup_pos = ImVec2(pos.x - 1.0f - IM_TRUNC(style.ItemSpacing.x * 0.5f),
                           pos.y - style.FramePadding.y + window->MenuBarHeight());
        window->DC.CursorPos.x += IM_TRUNC(style.ItemSpacing.x * 0.5f);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(style.ItemSpacing.x * 2.0f, style.ItemSpacing.y));
        float w = label_size.x;
        ImVec2 text_pos(window->DC.CursorPos.x + offsets->OffsetLabel,
                        window->DC.CursorPos.y + window->DC.CurrLineTextBaseOffset);
        _size = ImVec2(w, label_size.y);
        Selectable::Params params{.label = "",
                                  .rounding = _rounding,
                                  .flags = _flags,
                                  .buttonFlags = _buttonFlags,
                                  .size = _size,
                                  .selected = _selected,
                                  .hover = &_hover,
                                  .pressed = &_pressed};
        Selectable::render(params);
        ImGui::RenderText(text_pos, labelPtr);
        ImGui::PopStyleVar();
        window->DC.CursorPos.x += IM_TRUNC(style.ItemSpacing.x * (-1.0f + 0.5f));

        _hover = (g.HoveredId == id) && !g.NavDisableMouseHover;
        if (menuset_is_open)
            ImGui::PopItemFlag();

        bool want_open = false;
        bool want_close = false;

        // Menu bar
        if (_selected && _pressed && menuset_is_open) // Click an open menu again to close it
        {
            want_close = true;
            want_open = _selected = false;
        }
        else if (_pressed || (_hover && menuset_is_open && !_selected))
            want_open = true;
        else if (g.NavId == id && g.NavMoveDir == ImGuiDir_Down) // Nav-Down to open
        {
            want_open = true;
            ImGui::NavMoveRequestCancel();
        }

        if (want_close && ImGui::IsPopupOpen(id, ImGuiPopupFlags_None))
            ImGui::ClosePopupToLevel(g.BeginPopupStack.Size, true);

        ImGui::PopID();

        if (want_open && !_selected && g.OpenPopupStack.Size > g.BeginPopupStack.Size)
            ImGui::OpenPopup(labelPtr);
        else if (want_open)
        {
            _selected = true;
            ImGui::OpenPopup(labelPtr);
        }

        if (_selected)
        {
            ImGuiLastItemData last_item_in_parent = g.LastItemData;
            ImGui::SetNextWindowPos(popup_pos, ImGuiCond_Always); // Note: misleading: the value will serve as reference
                                                                  // for FindBestWindowPosForPopup(), not actual pos.
            ImGui::PushStyleVar(
                ImGuiStyleVar_ChildRounding,
                style.PopupRounding); // First level will use _PopupRounding, subsequent will use _ChildRounding
            _selected = ImGui::BeginPopupEx(id, window_flags); // menu_is_open can be 'false' when the popup is
                                                               // completely clipped (e.g. zero size display)
            ImGui::PopStyleVar();
            if (_selected)
            {
                // Restore LastItemData so IsItemXXXX functions can work after BeginMenu()/EndMenu()
                // (This fixes using IsItemClicked() and IsItemHovered(), but IsItemHovered() also relies on its support
                // for ImGuiItemFlags_NoWindowHoverableCheck)
                g.LastItemData = last_item_in_parent;
                if (g.HoveredWindow == window)
                    g.LastItemData.StatusFlags |= ImGuiItemStatusFlags_HoveredWindow;
            }
        }
        else
            g.NextWindowData.ClearFlags();
    }

    void HMenu::render()
    {
        beginMenu();
        if (_selected)
        {
            _submenu.render();
            ImGui::EndMenu();
        }
    }

    void VMenu::destroyItems()
    {
        for (auto &group : _itemGroups)
        {
            for (auto &item : group)
            {
                delete item.menu;
                if (item.submenu)
                    delete item.submenu;
            }
        }
    }

    void BeginMenu::render()
    {
        ImGuiWindow *window = ImGui::GetCurrentWindow();
        if (window->SkipItems)
            return;

        ImGuiContext &g = *GImGui;
        const ImGuiStyle &style = g.Style;
        auto labelPtr = _label.c_str();
        const ImGuiID id = window->GetID(labelPtr);
        _selected = ImGui::IsPopupOpen(id, ImGuiPopupFlags_None);

        // Sub-menus are ChildWindow so that mouse can be hovering across them (otherwise top-most popup menu would
        // steal focus and not allow hovering on parent menu) The first menu in a hierarchy isn't so hovering doesn't
        // get across (otherwise e.g. resizing borders with ImGuiButtonFlags_FlattenChildren would react), but top-most
        // BeginMenu() will bypass that limitation.
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_ChildMenu | ImGuiWindowFlags_AlwaysAutoResize |
                                        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar |
                                        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNavFocus;
        if (window->Flags & ImGuiWindowFlags_ChildMenu)
            window_flags |= ImGuiWindowFlags_ChildWindow;

        // If a menu with same the ID was already submitted, we will append to it, matching the behavior of Begin().
        // We are relying on a O(N) search - so O(N log N) over the frame - which seems like the most efficient for the
        // expected small amount of BeginMenu() calls per frame. If somehow this is ever becoming a problem we can
        // switch to use e.g. ImGuiStorage mapping key to last frame used.
        if (g.MenusIdSubmittedThisFrame.contains(id))
        {
            if (_selected)
                _selected = ImGui::BeginPopupEx(id, window_flags); // menu_is_open can be 'false' when the popup is
                                                                   // completely clipped (e.g. zero size display)
            else
                g.NextWindowData.ClearFlags(); // we behave like Begin() and need to consume those values
        }

        // Tag menu as used. Next time BeginMenu() with same ID is called it will append to existing menu
        g.MenusIdSubmittedThisFrame.push_back(id);

        ImVec2 label_size = ImGui::CalcTextSize(labelPtr, nullptr, true);

        // Odd hack to allow hovering across menus of a same menu-set (otherwise we wouldn't be able to hover parent
        // without always being a Child window) This is only done for items for the menu set and not the full parent
        // window.
        const bool menuset_is_open = IsRootOfOpenMenuSet();
        if (menuset_is_open)
            ImGui::PushItemFlag(ImGuiItemFlags_NoWindowHoverableCheck, true);

        // The reference position stored in popup_pos will be used by Begin() to find a suitable position for the child
        // menu, However the final position is going to be different! It is chosen by FindBestWindowPosForPopup(). e.g.
        // Menus tend to overlap each other horizontally to amplify relative Z-ordering.
        ImVec2 popup_pos, pos = window->DC.CursorPos;
        ImGui::PushID(labelPtr);
        const ImGuiMenuColumns *offsets = &window->DC.MenuColumns;
        popup_pos = ImVec2(pos.x, pos.y - style.WindowPadding.y);
        float checkmark_w = IM_TRUNC(g.FontSize * 1.20f);
        float min_w =
            window->DC.MenuColumns.DeclColumns(0.0f, label_size.x, 0.0f, checkmark_w); // Feedback to next frame
        float extra_w = ImMax(0.0f, ImGui::GetContentRegionAvail().x - min_w);
        ImVec2 text_pos(window->DC.CursorPos.x + offsets->OffsetLabel + _style.padding.x,
                        window->DC.CursorPos.y + window->DC.CurrLineTextBaseOffset);
        _size = ImVec2(min_w + +_style.padding.x, label_size.y);
        Selectable::Params params{.label = "",
                                  .rounding = _rounding,
                                  .flags = _flags | ImGuiSelectableFlags_SpanAvailWidth,
                                  .buttonFlags = _buttonFlags,
                                  .size = _size,
                                  .selected = _selected,
                                  .hover = &_hover,
                                  .pressed = &_pressed};
        Selectable::render(params);
        ImVec2 arrowPos =
            pos + ImVec2(offsets->OffsetMark + extra_w + g.FontSize * 0.30f + style.ItemInnerSpacing.x * 0.5f,
                         label_size.y / 2.0f - _arrowIcon->height() / 2.0f);
        ImGui::RenderText(text_pos, labelPtr);
        _arrowIcon->render(arrowPos);

        _hover = (g.HoveredId == id) && !g.NavDisableMouseHover;
        if (menuset_is_open)
            ImGui::PopItemFlag();

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
            const float ref_unit = g.FontSize; // FIXME-DPI
            const float child_dir = (window->Pos.x < child_menu_window->Pos.x) ? 1.0f : -1.0f;
            const ImRect next_window_rect = child_menu_window->Rect();
            ImVec2 ta = (g.IO.MousePos - g.IO.MouseDelta);
            ImVec2 tb = (child_dir > 0.0f) ? next_window_rect.GetTL() : next_window_rect.GetTR();
            ImVec2 tc = (child_dir > 0.0f) ? next_window_rect.GetBL() : next_window_rect.GetBR();
            const float pad_farmost_h = ImClamp(ImFabs(ta.x - tb.x) * 0.30f, ref_unit * 0.5f, ref_unit * 2.5f);
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
        if (_selected && !_hover && g.HoveredWindow == window && !moving_toward_child_menu && !g.NavDisableMouseHover &&
            g.ActiveId == 0)
            want_close = true;

        // Open
        // (note: at this point 'hovered' actually includes the NavDisableMouseHover == false test)
        if (!_selected && _pressed) // Click/activate to open
            want_open = true;
        else if (!_selected && _hover && !moving_toward_child_menu) // Hover to open
            want_open = true;
        else if (!_selected && _hover && g.HoveredIdTimer >= 0.30f &&
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

        if (want_open && !_selected && g.OpenPopupStack.Size > g.BeginPopupStack.Size)
            ImGui::OpenPopup(labelPtr);
        else if (want_open)
        {
            _selected = true;
            ImGui::OpenPopup(labelPtr);
        }

        if (_selected)
        {
            ImGuiLastItemData last_item_in_parent = g.LastItemData;
            ImGui::SetNextWindowPos(popup_pos, ImGuiCond_Always); // Note: misleading: the value will serve as reference
                                                                  // for FindBestWindowPosForPopup(), not actual pos.
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, style.PopupRounding);
            _selected = ImGui::BeginPopupEx(id, window_flags);
            ImGui::PopStyleVar();
            if (_selected)
            {
                // Restore LastItemData so IsItemXXXX functions can work after BeginMenu()/EndMenu()
                // (This fixes using IsItemClicked() and IsItemHovered(), but IsItemHovered() also relies on its support
                // for ImGuiItemFlags_NoWindowHoverableCheck)
                g.LastItemData = last_item_in_parent;
                if (g.HoveredWindow == window)
                    g.LastItemData.StatusFlags |= ImGuiItemStatusFlags_HoveredWindow;
            }
        }
        else
            g.NextWindowData.ClearFlags(); // We behave like Begin() and need to consume those values
    }

    void MenuItem::render()
    {
        ImGuiWindow *window = ImGui::GetCurrentWindow();
        if (window->SkipItems)
            return;

        ImGuiContext &g = *GImGui;
        ImGuiStyle &style = g.Style;
        ImVec2 pos = window->DC.CursorPos;
        auto labelPtr = _label.c_str();
        ImVec2 label_size = ImGui::CalcTextSize(labelPtr, nullptr, true);

        const bool menuset_is_open = IsRootOfOpenMenuSet();
        if (menuset_is_open)
            ImGui::PushItemFlag(ImGuiItemFlags_NoWindowHoverableCheck, true);

        // We've been using the equivalent of ImGuiSelectableFlags_SetNavIdOnHover on all Selectable() since early Nav
        // system days (commit 43ee5d73), but I am unsure whether this should be kept at all. For now moved it to be an
        // opt-in feature used by menus only.
        ImGui::PushID(labelPtr);

        const ImGuiMenuColumns *offsets = &window->DC.MenuColumns;

        auto shortcutPtr = _shortcut.c_str();
        float shortcut_w = (shortcutPtr && shortcutPtr[0]) ? ImGui::CalcTextSize(shortcutPtr, nullptr).x : 0.0f;
        float checkmark_w = IM_TRUNC(g.FontSize * 1.20f);
        float min_w = window->DC.MenuColumns.DeclColumns(0.0f, label_size.x, shortcut_w,
                                                         checkmark_w); // Feedback for next frame
        float stretch_w = ImMax(0.0f, ImGui::GetContentRegionAvail().x - min_w);
        _size = ImVec2(min_w + _style.padding.x, label_size.y);
        Selectable::Params params{.label = "",
                                  .rounding = _style.rounding,
                                  .flags = _flags | ImGuiSelectableFlags_SpanAvailWidth,
                                  .buttonFlags = _buttonFlags,
                                  .size = _size,
                                  .selected = false,
                                  .hover = &_hover,
                                  .pressed = &_pressed};
        Selectable::render(params);

        if (g.LastItemData.StatusFlags & ImGuiItemStatusFlags_Visible)
        {
            ImGui::RenderText(pos + ImVec2(offsets->OffsetLabel + _style.padding.x, 0.0f), labelPtr);
            if (shortcut_w > 0.0f)
            {
                ImVec2 shortcut_pos = ImVec2(
                    window->Pos.x + window->Size.x - shortcut_w - style.ItemSpacing.x - style.WindowPadding.x, pos.y);
                ImGui::PushStyleColor(ImGuiCol_Text, style.Colors[ImGuiCol_TextDisabled]);
                ImGui::RenderText(shortcut_pos, shortcutPtr);
                ImGui::PopStyleColor();
            }
            if (_selected)
                ImGui::RenderCheckMark(
                    window->DrawList,
                    pos + ImVec2(offsets->OffsetMark + stretch_w + g.FontSize * 0.40f, g.FontSize * 0.134f * 0.5f),
                    ImGui::GetColorU32(ImGuiCol_Text), g.FontSize * 0.866f);
        }

        ImGui::PopID();
        if (menuset_is_open)
            ImGui::PopItemFlag();
    }

    VMenu::VMenu(std::initializer_list<ItemGroup> itemgroups, const Style &style,
                 const std::shared_ptr<Icon> &arrowIcon)
        : _style(style)
    {
        for (auto &group : itemgroups)
        {
            _ItemGroup g;
            for (auto &item : group)
            {
                if (item.submenu)
                    g.emplace_front(new BeginMenu(item.label, arrowIcon, _style), item.submenu);
                else
                    g.emplace_front(new MenuItem(item.label, item.shortcut, _style), nullptr, item.callback,
                                    item.beforeRender);
            }
            g.reverse();
            _itemGroups.emplace_front(g);
        }
        _itemGroups.reverse();
    }

    void VMenu::render()
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, _style.padding);
        auto &style = ImGui::GetStyle();
        auto start = _itemGroups.begin();
        auto end = _itemGroups.end();
        if (start != end)
        {
            while (true)
            {
                for (auto &item : *start)
                {
                    if (item.submenu)
                    {
                        item.menu->render();
                        if (item.menu->selected())
                        {
                            item.submenu->render();
                            ImGui::EndMenu();
                        }
                    }
                    else
                    {
                        if (item.beforeRender)
                            item.beforeRender(item.menu);
                        ImGui::PushStyleColor(
                            ImGuiCol_Text,
                            style.Colors[item.menu->flags() & ImGuiSelectableFlags_Disabled ? ImGuiCol_TextDisabled
                                                                                            : ImGuiCol_Text]);
                        item.menu->render();
                        ImGui::PopStyleColor();
                        if (item.menu->pressed())
                        {
                            item.menu->pressed(false);
                            item.callback();
                            window::pushEmptyEvent();
                        }
                    }
                }

                ++start;
                if (start == end)
                    break;
                ImGui::Separator();
            }
        }
        ImGui::PopStyleVar();
    }

    void MenuBar::render()
    {
        ImGui::PushStyleColor(ImGuiCol_MenuBarBg, _style->menubar.backgroundColor);
        ImGui::PushStyleColor(ImGuiCol_Header, _style->menubar.hoverColor);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, _style->menubar.hoverColor);
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, _style->menubar.backgroundColor);
        ImGui::PushStyleColor(ImGuiCol_PopupBg, _style->submenu.backgroundColor);
        ImGui::PushStyleColor(ImGuiCol_Separator, _style->submenu.separatorColor);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, _style->menubar.margin);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, _style->menubar.padding);

        if (ImGui::BeginMainMenuBar())
        {
            for (auto &item : _items)
                item.render();
            ImGui::EndMainMenuBar();
        }
        ImGui::PopStyleColor(6);
        ImGui::PopStyleVar(2);
    }

    MenuBar::~MenuBar()
    {
        for (auto &item : _items)
            item.destroy();
        if (_style)
            delete _style;
    }

} // namespace ui