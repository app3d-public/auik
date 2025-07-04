#ifndef AUIK_ICONS_H
#define AUIK_ICONS_H

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include "../image/image.hpp"

namespace auik
{
    class Icon
    {
    public:
        Icon(const acul::string &symbol) : _symbol(symbol) {}

        virtual ~Icon() = default;

        void render(ImVec2 pos) { render(pos, size()); }

        virtual void render(ImVec2 pos, ImVec2 size) = 0;

        virtual ImVec2 size() const = 0;

        acul::string symbol() const { return _symbol; }

    private:
        acul::string _symbol;
    };

    class MissingIcon : public Icon
    {
    public:
        MissingIcon() : Icon("__missing__") {}

        virtual void render(ImVec2 pos, ImVec2 size) override
        {
            ImDrawList *draw = ImGui::GetWindowDrawList();
            draw->AddRectFilled(pos, pos + size, IM_COL32(0, 0, 0, 255));
            draw->AddRect(pos, pos + size, IM_COL32(255, 255, 255, 255));
        }

        virtual ImVec2 size() const override { return {8.0f, 8.0f}; }

        static MissingIcon *instance()
        {
            static MissingIcon inst;
            return &inst;
        }
    };

    class FontIcon : public Icon
    {
    public:
        FontIcon(const acul::string &symbol, ImFont *font, const acul::string &u8sequence)
            : Icon(symbol), _u8sequence(u8sequence), _font(font)
        {
        }

        virtual void render(ImVec2 pos, ImVec2 size) override
        {
            ImGui::PushFont(_font);
            ImGui::RenderText(pos, _u8sequence.c_str());
            ImGui::PopFont();
        }

        virtual ImVec2 size() const override
        {
#if IMGUI_VERSION_NUM > 19195
            f32 sz = _font->LegacySize;
#else
            f32 sz = _font->FontSize;
#endif
            return {sz, sz};
        }

        virtual ~FontIcon() = default;

    private:
        acul::string _u8sequence;
        ImFont *_font;
    };
    class ImageIcon : public Icon
    {
    public:
        ImageIcon(const acul::string &symbol, ImTextureID id, ImVec2 size, ImVec2 uv_min = {0, 0},
                  ImVec2 uv_max = {1, 1})
            : Icon(symbol), _image(id, size, uv_min, uv_max), _size(size)
        {
        }

        virtual void render(ImVec2 pos, ImVec2 size) override { _image.render(pos, size); }

        virtual ~ImageIcon() = default;

        virtual ImVec2 size() const override { return _size; }

    private:
        Image _image;
        ImVec2 _size;
    };
} // namespace auik

#endif