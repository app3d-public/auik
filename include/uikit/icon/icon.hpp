#ifndef UIKIT_ICONS_H
#define UIKIT_ICONS_H

#include <imgui/imgui.h>
#include <string>
#include "../image/image.hpp"

namespace ui
{
    class Icon
    {
    public:
        Icon(const std::string &symbol) : _symbol(symbol) {}

        virtual ~Icon() = default;

        virtual void render(ImVec2 pos) = 0;

        virtual size_t width() const = 0;
        virtual size_t height() const = 0;

        std::string symbol() const { return _symbol; }

    private:
        std::string _symbol;
    };

    class FontIcon : public Icon
    {
    public:
        FontIcon(const std::string &symbol, ImFont *font, const std::string &u8sequence)
            : Icon(symbol), _font(font), _u8sequence(u8sequence)
        {
        }

        virtual void render(ImVec2 pos) override;

        virtual size_t width() const override { return _font->FontSize; }

        virtual size_t height() const override { return _font->FontSize; }

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

        virtual void render(ImVec2 pos) override { _image.render(pos); }

        virtual ~ImageIcon() = default;

        virtual size_t width() const override { return _size.x; }

        virtual size_t height() const override { return _size.y; }

    private:
        Image _image;
        ImVec2 _size;
    };

    // class IconResolver
    // {
    //     struct Impl;

    // public:
    //     IconResolver();
    //     ~IconResolver();

    //     void loadSymbols();

    //     std::shared_ptr<Icon> getIcon(const std::string &symbol) const;

    // private:
    //     emhash5::HashMap<std::string, std::shared_ptr<Icon>> _symbolMap;
    //     Impl *_impl{nullptr};

    //     void addAppIcons(const std::shared_ptr<graphics::Atlas> &appIcons);
    // };

    // inline float dpiAwareScale(float value) { return value * window::getDpi() / 0.96f; }
} // namespace ui

#endif