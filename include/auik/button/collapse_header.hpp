#pragma once

#include "../widget.hpp"

namespace auik
{
    class APPLIB_API CollapseHeader
    {
    public:
        CollapseHeader(Widget *content, bool collapsed = true) : _collapsed(collapsed), _content(content) {}

        void render();
        bool collapsed() const { return _collapsed; }
        void collapsed(bool collapsed) { _collapsed = collapsed; }

        bool pressed() const { return _pressed; }

        Widget* content() {return _content;}

        ~CollapseHeader() { acul::release(_content); }

    private:
        bool _collapsed;
        bool _pressed;
        Widget *_content;
    };
} // namespace auik