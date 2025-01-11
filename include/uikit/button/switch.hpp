#pragma once

#include <astl/basic_types.hpp>
#include <core/api.hpp>
#include "../style.hpp" // IWYU pragma: keep
#include "../widget.hpp"

namespace uikit
{
    class APPLIB_API Switch final : public Widget
    {
    public:
        static constexpr f32 width = 28.0f;
        static constexpr f32 height = 15.0f;

        Switch(const std::string &label, bool toogled = false)
            : Widget(label), _pressed(false), _hovered(false), _toggled(toogled)
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