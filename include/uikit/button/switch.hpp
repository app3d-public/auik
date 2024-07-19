#pragma once

#include <core/api.hpp>
#include "../widget.hpp"

namespace uikit
{
    namespace style
    {
        extern APPLIB_API struct Switch
        {
            ImVec4 bgColor;
            ImColor switchColor;
            ImVec4 bgColorActive;
        } switchStyle;
    } // namespace style

    class APPLIB_API Switch final : public Widget
    {
    public:
        static constexpr f32 width = 40.0f;
        static constexpr f32 height = 20.0f;

        Switch(const std::string &label, bool toogled = false)
            : _label(label), _pressed(false), _hovered(false), _toggled(toogled)
        {
        }

        virtual void render() override;

        std::string label() const { return _label; }

        bool pressed() const { return _pressed; }

        bool hovered() const { return _hovered; }

        bool toogled() const { return _toggled; }
        void toogled(bool toggled) { _toggled = toggled; }

    private:
        std::string _label;
        bool _pressed;
        bool _hovered;
        bool _toggled;
    };
} // namespace ui