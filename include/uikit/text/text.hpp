#ifndef UIKIT_WIDGETS_TEXT_H
#define UIKIT_WIDGETS_TEXT_H

#include "../widget.hpp"

namespace uikit
{
    class Text : public Widget
    {
    public:
        Text(const std::string &text, bool wrapped = false) : _text(text), _wrapped(wrapped), _text_size{0, 0} {}

        virtual void render() override;

        std::string text() const { return _text; }

        bool wrapped() const { return _wrapped; }

        f32 width() const { return _text_size.x; }

        f32 height() const { return _text_size.y; }

    private:
        std::string _text;
        bool _wrapped;
        ImVec2 _text_size;

        void renderTextWrapped(const ImVec2 text_pos);

        void renderText(const ImVec2 text_pos);
    };
} // namespace ui

#endif