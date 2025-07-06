#pragma once

#include "../icon/icon.hpp"

namespace auik
{
    namespace style
    {
        extern APPLIB_API struct SearchBox
        {
            Icon *search_icon;
            Icon *filter_icon;
            ImU32 hint_color;
        } g_Search;
    } // namespace style

    class APPLIB_API SearchBox final : public Widget
    {
    public:
        SearchBox(const acul::string &name, const acul::string &hint) : Widget(name), _hint(hint) {}

        virtual void render() override;

    private:
        acul::string _hint, _text;
    };
} // namespace auik