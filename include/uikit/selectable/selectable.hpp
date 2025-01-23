#ifndef UIKIT_WIDGETS_SELECTABLE_H
#define UIKIT_WIDGETS_SELECTABLE_H

#include <astl/vector.hpp>
#include <core/api.hpp>
#include <imgui/imgui_internal.h>
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

    class APPLIB_API RubberBandSelection final : public Widget
    {
    public:
        RubberBandSelection(const std::string &name)
            : Widget(name), _isActive(false), _isSelected(false), _start(0, 0), _end(0, 0)
        {
        }

        virtual void render() override;

        bool active() const { return _isActive; }

        bool selected() const { return _isSelected; }

        void flush()
        {
            _isSelected = false;
            _item_ids.clear();
        }

        void try_push(int id, const ImRect &bb)
        {
            if (_rect.Overlaps(bb)) _item_ids.push_back(id);
        }

        const astl::vector<int> &get_items() const { return _item_ids; }

    private:
        bool _isActive;
        bool _isSelected;
        astl::vector<int> _item_ids;
        ImVec2 _start, _end;
        ImRect _rect;
    };
} // namespace uikit

#endif // APP_UI_WIDGETS_SELECTABLE_H