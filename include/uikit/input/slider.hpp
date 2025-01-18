#pragma once

#include <imgui/imgui_internal.h>
#include <window/window.hpp>
#include "../widget.hpp"

namespace uikit
{
    namespace style
    {
        extern APPLIB_API struct Slider
        {
            float height;
            float circle_radius;
            ImU32 fill_color;
            ImU32 circle_color;
        } g_Slider;
    } // namespace style

    template <typename T>
    class InputSlider final : public Widget
    {
    public:
        T *value;

        InputSlider(const std::string &name, T *value, T min, T max, bool round = false)
            : Widget(name), value(value), _min(min), _max(max), _round(round)
        {
        }

        void render() override
        {
            ImGuiWindow *window = ImGui::GetCurrentWindow();
            if (window->SkipItems) return;

            ImGuiContext &g = *GImGui;
            const ImGuiStyle &style = g.Style;
            const ImGuiID id = window->GetID(name.c_str());
            const float w = ImGui::CalcItemWidth();
            const float frame = ImGui::GetFrameHeight();
            const ImRect frame_bb(window->DC.CursorPos, window->DC.CursorPos + ImVec2(w, frame));
            ImGui::ItemSize(frame_bb, style.FramePadding.y);
            if (!ImGui::ItemAdd(frame_bb, id, &frame_bb, 0)) return;
            float factor = (*value - _min) / (_max - _min);
            if (*value < _min) factor = 0;
            if (*value > _max) factor = 1;
            ImRect filled;
            filled.Min = frame_bb.Min;
            filled.Min.y += (frame - style::g_Slider.height) * 0.5f;
            filled.Max.x = filled.Min.x + w * factor;
            filled.Max.y = filled.Min.y + style::g_Slider.height;
            auto *draw_list = g.CurrentWindow->DrawList;
            if (filled.Max.x > filled.Min.x)
                draw_list->AddRectFilled(filled.Min, filled.Max, style::g_Slider.fill_color, style.FrameRounding);

            ImRect avaliable{{filled.Max.x, filled.Min.y}, {filled.Min.x + w, filled.Max.y}};
            if (avaliable.Max.x > avaliable.Min.x)
            {
                ImU32 aval_color = ImGui::GetColorU32(style.Colors[ImGuiCol_FrameBg]);
                draw_list->AddRectFilled(avaliable.Min, avaliable.Max, aval_color, style.FrameRounding);
            }

            float circle_diam = style::g_Slider.circle_radius * 2;
            ImVec2 circle_center(filled.Max.x, filled.Min.y + (style::g_Slider.height / 2.0f));
            draw_list->AddCircleFilled(circle_center, style::g_Slider.circle_radius, style::g_Slider.circle_color);

            processMouseBehavior(frame_bb, w, id, g.IO);
        }

    private:
        T _min;
        T _max;
        bool _round;

        void processMouseBehavior(const ImRect &frame_bb, f32 w, ImGuiID id, ImGuiIO &IO)
        {
            bool hovered, held;
            ImGui::ButtonBehavior(frame_bb, id, &hovered, &held, ImGuiButtonFlags_PressedOnClickReleaseAnywhere);
            float mouse_pos = IO.MousePos.x;
            if (hovered && ImGui::IsMouseClicked(0))
            {
                float diff = mouse_pos - frame_bb.Min.x;
                float factor = diff / w;
                *value = ImClamp(_min + (_max - _min) * factor, _min, _max);
                if (_round) *value = round(*value);
            }
            if (held && ImGui::IsMouseDragging(0))
            {
                float drag_delta = ImGui::GetMouseDragDelta(0).x / w;
                float step_change = (_max - _min) * drag_delta;
                if (_round)
                {
                    int steps = static_cast<int>(round(step_change));
                    if (steps != 0)
                    {
                        *value = ImClamp(*value + steps, _min, _max);
                        IO.MouseClickedPos[0].x += steps * w / (_max - _min);
                    }
                }
                else
                {
                    *value = ImClamp(*value + step_change, _min, _max);
                    ImGui::ResetMouseDragDelta();
                }
            }
        }
    };
} // namespace uikit