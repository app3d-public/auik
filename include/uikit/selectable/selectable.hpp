#ifndef UIKIT_WIDGETS_SELECTABLE_H
#define UIKIT_WIDGETS_SELECTABLE_H

#include <string>
#include "../widget.hpp"
#include <core/api.hpp>

namespace ui
{
    class APPLIB_API Selectable : public Widget
    {
    public:
        struct Params
        {
            const char *label = "";
            float rounding = 0.0f;
            ImGuiSelectableFlags flags = ImGuiSelectableFlags_None;
            ImGuiButtonFlags buttonFlags = ImGuiButtonFlags_None;
            ImVec2 size = ImVec2(0, 0);
            bool selected = false;
            bool *hover = nullptr;
            bool *pressed = nullptr;
            bool showBackground = false;
        };

        Selectable(const std::string &label = "", bool selected = false, float rounding = 0.0f,
                   ImGuiSelectableFlags flags = 0, const ImVec2 &size = ImVec2(0, 0), bool showBackground = false)
            : _size(size),
              _rounding(rounding),
              _selected(selected),
              _label(label),
              _hover(false),
              _pressed(false),
              _flags(flags),
              _buttonFlags(loadFlags(flags)),
              _showBackground(showBackground)
        {
        }

        virtual void render() override;
        static APPLIB_API void render(Params &params);

        ImVec2 size() const { return _size; }
        void size(ImVec2 size) { _size = size; }

        float rounding() const { return _rounding; }
        void rounding(float rounding) { _rounding = rounding; }

        bool selected() const { return _selected; }
        void selected(bool selected) { _selected = selected; }

        std::string label() const { return _label; }

        bool hover() const { return _hover; }

        bool pressed() const { return _pressed; }
        void pressed(bool pressed) { _pressed = pressed; }

        bool showBackground() const { return _showBackground; }

        void showBackground(bool showBackground) { _showBackground = showBackground; }

        ImGuiSelectableFlags &flags() { return _flags; }

    protected:
        ImVec2 _size;
        float _rounding;
        bool _selected;
        std::string _label;
        bool _hover;
        bool _pressed;
        ImGuiSelectableFlags _flags;
        ImGuiButtonFlags _buttonFlags;
        bool _showBackground;

        ImGuiButtonFlags loadFlags(ImGuiSelectableFlags flags);
    };
} // namespace ui

#endif // APP_UI_WIDGETS_SELECTABLE_H