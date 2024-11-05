#ifndef UIKIT_WIDGETS_SELECTABLE_H
#define UIKIT_WIDGETS_SELECTABLE_H

#include <core/api.hpp>
#include <string>
#include "../widget.hpp"

namespace uikit
{
    struct SelectableParams
    {
        float rounding = 0.0f;
        ImGuiSelectableFlags sFlags = ImGuiSelectableFlags_None; //< Selectable flags
        ImGuiButtonFlags bFlags = ImGuiButtonFlags_None;         //< Button flags
        ImDrawFlags dFlags = ImDrawFlags_None;                   //< Draw flags
        ImVec2 size = ImVec2(0, 0);
        bool selected = false;
        bool hover = false;
        bool pressed = false;
        bool showBackground = false;
    };

    class APPLIB_API Selectable : public Widget, public SelectableParams
    {
    public:
        Selectable(const std::string &label = "", bool selected = false, float rounding = 0.0f,
                   ImGuiSelectableFlags sFlags = 0, const ImVec2 &size = ImVec2(0, 0), bool showBackground = false)
            : Widget(label),
              SelectableParams(rounding, sFlags, loadButtonFlags(sFlags), ImDrawFlags_None, size, selected, false,
                               false, false)
        {
        }

        virtual void render() override { render(name.c_str(), *this); }

        static APPLIB_API void render(const char *label, SelectableParams &params);

    protected:
        static APPLIB_API ImGuiButtonFlags loadButtonFlags(ImGuiSelectableFlags flags);
    };
} // namespace uikit

#endif // APP_UI_WIDGETS_SELECTABLE_H