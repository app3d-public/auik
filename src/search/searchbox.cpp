#include <uikit/search/searchbox.hpp>
#include <uikit/text/text.hpp>

namespace uikit
{
    namespace style
    {
        SearchBox g_Search;
    }

    void SearchBox::render()
    {
        auto &style = ImGui::GetStyle();
        f32 width = ImGui::CalcItemWidth();
        f32 input_width = width < 100 ? 10
                                      : width - style::g_Search.filterIcon->size().x -
                                            style::g_Search.searchIcon->size().x - style.ItemSpacing.x * 2;
        auto *window = ImGui::GetCurrentWindow();
        f32 frame_height = ImGui::GetFrameHeight();

        // Background for search icon
        ImRect background;
        background.Min = window->DC.CursorPos;
        background.Max.x = background.Min.x + input_width;
        background.Max.y = background.Min.y + frame_height;
        auto *draw_list = window->DrawList;
        ImU32 col = ImGui::GetColorU32(ImGuiCol_FrameBg);
        draw_list->AddRectFilled(background.Min, background.Max, col, style.FrameRounding);

        // Search icon
        ImVec2 search_icon_pos = window->DC.CursorPos;
        search_icon_pos.x += style.FramePadding.x;
        search_icon_pos.y += (ImGui::GetFrameHeight() - style::g_Search.searchIcon->size().y) / 2.0f;
        style::g_Search.searchIcon->render(search_icon_pos);

        // Input
        ImGui::PushStyleColor(ImGuiCol_TextDisabled, style::g_Search.hintColor);
        ImVec2 pos = window->DC.CursorPos;
        pos.x += style::g_Search.searchIcon->size().x + style.ItemSpacing.x;
        ImGui::SetCursorScreenPos(pos);
        ImGui::SetNextItemWidth(input_width);
        inputTextWithHint(name.c_str(), _hint.c_str(), &_text);
        ImGui::PopStyleColor();

        // Filter icon
        pos.x += input_width + style.ItemSpacing.x;
        pos.y += (ImGui::GetFrameHeight() - style::g_Search.filterIcon->size().y) / 2.0f;
        style::g_Search.filterIcon->render(pos);
    }
} // namespace uikit