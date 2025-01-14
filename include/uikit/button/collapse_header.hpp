#pragma once

#include "../widget.hpp"

namespace uikit
{
    class APPLIB_API CollapseHeader
    {
    public:
        CollapseHeader(Widget *content, bool collapsed = true) : _collapsed(collapsed), _content(content) {}

        void render();
        bool collapsed() const { return _collapsed; }
        void collapsed(bool collapsed) { _collapsed = collapsed; }

        bool pressed() const { return _pressed; }

    private:
        bool _collapsed;
        bool _pressed;
        Widget *_content;
    };
} // namespace uikit