#pragma once

#include "../icon/icon.hpp"

namespace uikit
{
    namespace style
    {
        extern APPLIB_API struct SearchBox
        {
            Icon *searchIcon;
            Icon *filterIcon;
            ImU32 hintColor;
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
} // namespace uikit