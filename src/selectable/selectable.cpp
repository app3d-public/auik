#include <imgui/imgui_internal.h>
#include <auik/selectable/selectable.hpp>

namespace auik
{
    ImGuiButtonFlags Selectable::load_button_flags(ImGuiSelectableFlags flags)
    {
        ImGuiContext &g = *GImGui;
        ImGuiButtonFlags button_flags = 0;
        if (flags & ImGuiSelectableFlags_NoHoldingActiveID) button_flags |= ImGuiButtonFlags_NoHoldingActiveId;
        if (flags & ImGuiSelectableFlags_NoSetKeyOwner) button_flags |= ImGuiButtonFlags_NoSetKeyOwner;
        if (flags & ImGuiSelectableFlags_SelectOnClick) button_flags |= ImGuiButtonFlags_PressedOnClick;
        if (flags & ImGuiSelectableFlags_SelectOnRelease) button_flags |= ImGuiButtonFlags_PressedOnRelease;
        if (flags & ImGuiSelectableFlags_AllowDoubleClick)
            button_flags |= ImGuiButtonFlags_PressedOnClickRelease | ImGuiButtonFlags_PressedOnDoubleClick;
        if ((flags & ImGuiSelectableFlags_AllowOverlap) || (g.LastItemData.ItemFlags & ImGuiItemFlags_AllowOverlap))
            button_flags |= ImGuiButtonFlags_AllowOverlap;
        return button_flags;
    }

    void Selectable::render(const char *label, SelectableParams &params)
    {
        ImGuiWindow *window = ImGui::GetCurrentWindow();
        if (window->SkipItems) return;

        ImGuiContext &g = *GImGui;
        const ImGuiStyle &style = g.Style;

        // Submit label or explicit size to ItemSize(), whereas ItemAdd() will submit a larger/spanning rectangle.
        ImGuiID id = window->GetID(label);
        ImVec2 label_size = ImGui::CalcTextSize(label, nullptr, true);
        ImVec2 size(params.size.x != 0.0f ? params.size.x : label_size.x,
                    params.size.y != 0.0f ? params.size.y : label_size.y);
        if (params.size.x == 0.0f) size.x += style.ItemSpacing.x * 2.0f;
        ImVec2 pos = window->DC.CursorPos;
        pos.y += window->DC.CurrLineTextBaseOffset;
        ImGui::ItemSize(size, 0.0f);

        // Fill horizontal space
        // We don't support (size < 0.0f) in Selectable() because the ItemSpacing extension would make explicitly
        // right-aligned sizes not visibly match other widgets.
        const bool span_all_columns = (params.selectable_flags & ImGuiSelectableFlags_SpanAllColumns) != 0;
        const f32 min_x = span_all_columns ? window->ParentWorkRect.Min.x : pos.x;
        const f32 max_x = span_all_columns ? window->ParentWorkRect.Max.x : window->WorkRect.Max.x;
        if (params.size.x == 0.0f || (params.selectable_flags & ImGuiSelectableFlags_SpanAvailWidth))
            size.x = ImMax(label_size.x, max_x - min_x);

        // Text stays at the submission position, but bounding box may be extended on both sides
        const ImVec2 text_min{pos.x + style.ItemSpacing.x, pos.y};
        const ImVec2 text_max(min_x + size.x, pos.y + size.y);

        // Selectables are meant to be tightly packed together with no click-gap, so we extend their box to cover
        // spacing between selectable.
        ImRect bb(min_x, pos.y, text_max.x, text_max.y);
        if ((params.selectable_flags & ImGuiSelectableFlags_NoPadWithHalfSpacing) == 0)
        {
            const f32 spacing_x = span_all_columns ? 0.0f : style.ItemSpacing.x;
            const f32 spacing_y = style.ItemSpacing.y;
            const f32 spacing_L = IM_TRUNC(spacing_x * 0.50f);
            const f32 spacing_U = IM_TRUNC(spacing_y * 0.50f);
            bb.Min.x -= spacing_L;
            bb.Min.y -= spacing_U;
            bb.Max.x += (spacing_x - spacing_L);
            bb.Max.y += (spacing_y - spacing_U);
        }

        // Modify ClipRect for the ItemAdd(), faster than doing a PushColumnsBackground/PushTableBackgroundChannel
        // for every Selectable..
        const f32 backup_clip_rect_min_x = window->ClipRect.Min.x;
        const f32 backup_clip_rect_max_x = window->ClipRect.Max.x;
        if (span_all_columns)
        {
            window->ClipRect.Min.x = window->ParentWorkRect.Min.x;
            window->ClipRect.Max.x = window->ParentWorkRect.Max.x;
        }

        const bool disabled_item = (params.selectable_flags & ImGuiSelectableFlags_Disabled) != 0;
        const bool item_add = ImGui::ItemAdd(
            bb, id, NULL, disabled_item ? (ImGuiItemFlags_)ImGuiItemFlags_Disabled : ImGuiItemFlags_None);

        if (span_all_columns)
        {
            window->ClipRect.Min.x = backup_clip_rect_min_x;
            window->ClipRect.Max.x = backup_clip_rect_max_x;
        }
        if (!item_add) return;

        const bool disabled_global = (g.CurrentItemFlags & ImGuiItemFlags_Disabled) != 0;
        if (disabled_item && !disabled_global) ImGui::BeginDisabled();

        // FIXME: We can standardize the behavior of those two, we could also keep the fast path of override
        // ClipRect + full push on render only, which would be advantageous since most selectable are not selected.
        if (span_all_columns)
        {
            if (g.CurrentTable)
                ImGui::TablePushBackgroundChannel();
            else if (window->DC.CurrentColumns)
                ImGui::PushColumnsBackground();
            g.LastItemData.StatusFlags |= ImGuiItemStatusFlags_HasClipRect;
            g.LastItemData.ClipRect = window->ClipRect;
        }

        const bool was_selected = params.selected;
        bool held;
        bool pressed, hovered;
        pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held, params.btn_flags);

        // Auto-select when moved into
        // - This will be more fully fleshed in the range-select branch
        // - This is not exposed as it won't nicely work with some user side handling of shift/control
        // - We cannot do 'if (g.NavJustMovedToId != id) { selected = false; pressed = was_selected; }' for two
        // reasons
        //   - (1) it would require focus scope to be set, need exposing PushFocusScope() or equivalent (e.g.
        //   BeginSelection() calling PushFocusScope())
        //   - (2) usage will fail with clipped items
        //   The multi-select API aim to fix those issues, e.g. may be replaced with a BeginSelection() API.
        if ((params.selectable_flags & ImGuiSelectableFlags_SelectOnNav) && g.NavJustMovedToId != 0 &&
            g.NavJustMovedToFocusScopeId == g.CurrentFocusScopeId)
            if (g.NavJustMovedToId == id) params.selected = pressed = true;

        // Update NavId when clicking or when Hovering (this doesn't happen on most widgets), so navigation can be
        // resumed with keyboard/gamepad
        if (pressed || (hovered && (params.selectable_flags & ImGuiSelectableFlags_SetNavIdOnHover)))
        {
            if (!g.NavHighlightItemUnderNav && g.NavWindow == window && g.NavLayer == window->DC.NavLayerCurrent)
            {
                ImGui::SetNavID(id, window->DC.NavLayerCurrent, g.CurrentFocusScopeId,
                                ImGui::WindowRectAbsToRel(window, bb)); // (bb == NavRect)
                if (g.IO.ConfigNavCursorVisibleAuto) g.NavCursorVisible = false;
            }
        }
        if (pressed) ImGui::MarkItemEdited(id);

        // In this branch, Selectable() cannot toggle the selection so this will never trigger.
        if (params.selected != was_selected) g.LastItemData.StatusFlags |= ImGuiItemStatusFlags_ToggledSelection;

        // Render
        if (hovered || params.selected || params.show_background)
        {
            const ImU32 col = ImGui::GetColorU32((held && hovered) ? ImGuiCol_HeaderActive
                                                 : hovered         ? ImGuiCol_HeaderHovered
                                                 : params.selected ? ImGuiCol_Header
                                                                   : ImGuiCol_PopupBg);

            render_frame(bb.Min, bb.Max, col, false, params.rounding, params.draw_flags);
        }
        if (g.NavId == id)
            ImGui::RenderNavHighlight(bb, id, ImGuiNavHighlightFlags_Compact | ImGuiNavHighlightFlags_NoRounding);
        if (span_all_columns)
        {
            if (g.CurrentTable)
                ImGui::TablePopBackgroundChannel();
            else if (window->DC.CurrentColumns)
                ImGui::PopColumnsBackground();
        }

        ImGui::RenderTextClipped(text_min, text_max, label, nullptr, &label_size, style.SelectableTextAlign, &bb);

        // Automatically close popups
        if (pressed && (window->Flags & ImGuiWindowFlags_Popup) &&
            !(params.selectable_flags & ImGuiSelectableFlags_NoAutoClosePopups) &&
            (g.LastItemData.ItemFlags & ImGuiItemFlags_AutoClosePopups))
            ImGui::CloseCurrentPopup();
        if (disabled_item && !disabled_global) ImGui::EndDisabled();
        params.pressed = pressed;
        params.hover = hovered;
    }

    void RubberBandSelection::render()
    {
        auto &g = *GImGui;
        auto *window = g.CurrentWindow;
        assert(window);
        if (window->SkipItems) return;
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            auto &io = g.IO;
            if (!_is_active)
            {
                _is_active = true;
                _start = io.MousePos - ImGui::GetMouseDragDelta();
            }
            _end = io.MousePos;
            auto *draw_list = ImGui::GetWindowDrawList();
            _rect.Min = {std::min(_start.x, _end.x), std::min(_start.y, _end.y)};
            _rect.Max = {std::max(_start.x, _end.x), std::max(_start.y, _end.y)};
            ImU32 col = ImGui::GetColorU32(g.Style.Colors[ImGuiCol_TextSelectedBg]);
            draw_list->AddRectFilled(_rect.Min, _rect.Max, col, g.Style.FrameRounding);
        }
        if (ImGui::IsMouseReleased(0) && _is_active)
        {
            _is_active = false;
            _is_selected = true;
        }
    }
    // }
} // namespace auik