#ifndef UIKIT_WIDGETS_IMAGE_H
#define UIKIT_WIDGETS_IMAGE_H

#include "../widget.hpp"
#include <core/api.hpp>

namespace ui
{
    class APPLIB_API Image : public Widget
    {
    public:
        Image(ImTextureID id, ImVec2 size = {0, 0}, ImVec2 uvMin = {0, 0}, ImVec2 uvMax = {1, 1})
            : _id(id), _size(size), _uvMin(uvMin), _uvMax(uvMax)
        {
        }

        ImVec2 size() const { return _size; }
        void size(ImVec2 size) { _size = size; }

        ImVec2 uvMin() const { return _uvMin; }
        void uvMin(ImVec2 uvMin) { _uvMin = uvMin; }

        ImVec2 uvMax() const { return _uvMax; }
        void uvMax(ImVec2 uvMax) { _uvMax = uvMax; }

        virtual void render() override;
        void render(ImVec2 pos);

    private:
        ImTextureID _id;
        ImVec2 _size;
        ImVec2 _uvMin;
        ImVec2 _uvMax;
    };
} // namespace ui

#endif