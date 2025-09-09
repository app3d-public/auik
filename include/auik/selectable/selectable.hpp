#ifndef AUIK_WIDGETS_SELECTABLE_H
#define AUIK_WIDGETS_SELECTABLE_H

#include <acul/vector.hpp>
#include <imgui/imgui_internal.h>
#include "../widget.hpp"

namespace auik
{
    struct SelectableParams
    {
        f32 rounding = 0.0f;
        ImGuiSelectableFlags selectable_flags = ImGuiSelectableFlags_None; //< Selectable flags
        ImGuiButtonFlags btn_flags = ImGuiButtonFlags_None;                //< Button flags
        ImDrawFlags draw_flags = ImDrawFlags_None;                         //< Draw flags
        ImVec2 size = ImVec2(0, 0);
        bool selected = false;
        bool hover = false;
        bool pressed = false;
        bool show_background = false;
    };

    class APPLIB_API Selectable : public Widget, public SelectableParams
    {
    public:
        Selectable(const acul::string &label = "", bool selected = false, f32 rounding = 0.0f,
                   ImGuiSelectableFlags selectable_flags = 0, const ImVec2 &size = ImVec2(0, 0),
                   bool show_background = false)
            : Widget(label), SelectableParams{rounding,
                                              selectable_flags,
                                              load_button_flags(selectable_flags),
                                              ImDrawFlags_None,
                                              size,
                                              selected,
                                              false,
                                              false,
                                              false}
        {
        }

        virtual void render() override { render(name.c_str(), *this); }

        static APPLIB_API void render(const char *label, SelectableParams &params);

    protected:
        static APPLIB_API ImGuiButtonFlags load_button_flags(ImGuiSelectableFlags flags);
    };

    class APPLIB_API RubberBandSelection final : public Widget
    {
    public:
        RubberBandSelection(const acul::string &name)
            : Widget(name), _is_active(false), _is_selected(false), _start(0, 0), _end(0, 0)
        {
        }

        virtual void render() override;

        bool active() const { return _is_active; }

        bool selected() const { return _is_selected; }

        void flush()
        {
            _is_selected = false;
            _item_ids.clear();
        }

        void try_push(int id, const ImRect &bb)
        {
            if (_rect.Overlaps(bb)) _item_ids.push_back(id);
        }

        const acul::vector<int> &get_items() const { return _item_ids; }

    private:
        bool _is_active;
        bool _is_selected;
        acul::vector<int> _item_ids;
        ImVec2 _start, _end;
        ImRect _rect;
    };
} // namespace auik

#endif // APP_UI_WIDGETS_SELECTABLE_H