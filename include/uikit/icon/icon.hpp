#ifndef UIKIT_ICONS_H
#define UIKIT_ICONS_H

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <string>
#include "../image/image.hpp"

namespace uikit
{
    class Icon
    {
    public:
        Icon(const std::string &symbol) : _symbol(symbol) {}

        virtual ~Icon() = default;

        void render(ImVec2 pos) { render(pos, size()); }

        virtual void render(ImVec2 pos, ImVec2 size) = 0;

        virtual ImVec2 size() const = 0;

        std::string symbol() const { return _symbol; }

    private:
        std::string _symbol;
    };

    class FontIcon : public Icon
    {
    public:
        FontIcon(const std::string &symbol, ImFont *font, const std::string &u8sequence)
            : Icon(symbol), _u8sequence(u8sequence), _font(font)
        {
        }

        virtual void render(ImVec2 pos, ImVec2 size) override
        {
            ImGui::PushFont(_font);
            ImGui::RenderText(pos, _u8sequence.c_str());
            ImGui::PopFont();
        }

        virtual ImVec2 size() const override { return {_font->FontSize, _font->FontSize}; }

        virtual ~FontIcon() = default;

    private:
        std::string _u8sequence;
        ImFont *_font;
    };

    class ImageIcon : public Icon
    {
    public:
        ImageIcon(const std::string &symbol, ImTextureID id, ImVec2 size, ImVec2 uvMin = {0, 0}, ImVec2 uvMax = {1, 1})
            : Icon(symbol), _image(id, size, uvMin, uvMax), _size(size)
        {
        }

        virtual void render(ImVec2 pos, ImVec2 size) override { _image.render(pos, size); }

        virtual ~ImageIcon() = default;

        virtual ImVec2 size() const override { return _size; }

    private:
        Image _image;
        ImVec2 _size;
    };
} // namespace uikit

#endif