#ifndef AUIK_WIDGETS_TEXT_H
#define AUIK_WIDGETS_TEXT_H

#include "../widget.hpp"

namespace auik
{
    class APPLIB_API Text : public Widget
    {
    public:
        Text(const acul::string &text, bool wrapped = false) : Widget(text), _wrapped(wrapped), _text_size{0, 0} {}

        Text &operator=(const acul::string &text)
        {
            name = text;
            return *this;
        }

        virtual void render() override;

        bool wrapped() const { return _wrapped; }

        f32 width() const { return _text_size.x; }

        f32 height() const { return _text_size.y; }

    private:
        bool _wrapped;
        ImVec2 _text_size;

        void render_text_wrapped(const ImVec2 text_pos);

        void render_text(const ImVec2 text_pos);
    };

    inline f32 get_max_text_width(acul::string items[], size_t size)
    {
        f32 max = 0;
        for (size_t i = 0; i < size; ++i) max = std::max(max, ImGui::CalcTextSize(items[i].c_str()).x);
        return max;
    }

    APPLIB_API bool input_text(const char *label, acul::string *str, ImGuiInputTextFlags flags = 0,
                              ImGuiInputTextCallback callback = nullptr, void *user_data = nullptr);
    APPLIB_API bool input_text_with_hint(const char *label, const char *hint, acul::string *str,
                                      ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = nullptr,
                                      void *user_data = nullptr);
} // namespace auik

#endif