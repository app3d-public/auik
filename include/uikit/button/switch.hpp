#pragma once

#include <acul/api.hpp>
#include <awin/window.hpp>
#include "../widget.hpp"

namespace uikit
{
    class APPLIB_API Switch final : public Widget
    {
    public:
        const f32 width;
        const f32 height;

        Switch(const acul::string &label, bool toogled = false)
            : Widget(label),
              width(28 * awin::getDpi()),
              height(15 * awin::getDpi()),
              _pressed(false),
              _hovered(false),
              _toggled(toogled)
        {
        }

        virtual void render() override;

        bool pressed() const { return _pressed; }

        bool hovered() const { return _hovered; }

        bool toogled() const { return _toggled; }
        void toogled(bool toggled) { _toggled = toggled; }

    private:
        bool _pressed;
        bool _hovered;
        bool _toggled;
    };

    namespace style
    {
        extern APPLIB_API struct Switch
        {
            ImVec4 bg;
            ImColor color;
            ImVec4 colorActive;
        } g_Switch;
    } // namespace style
} // namespace uikit