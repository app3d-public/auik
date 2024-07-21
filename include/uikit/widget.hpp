#ifndef APP_UI_WIDGETS_H
#define APP_UI_WIDGETS_H

#include <core/api.hpp>
#include <core/std/basic_types.hpp>
#include <imgui/imgui.h>

namespace uikit
{
    class Widget
    {
    public:
        bool enabled;

        Widget(const std::string &name, bool enabled = true) : enabled(enabled), _name(name) {}

        virtual ~Widget() = default;

        virtual void render() = 0;

        std::string name() const { return _name; }

    protected:
        std::string _name;
    };

    namespace style
    {
        extern APPLIB_API struct General
        {
            f32 windowBorderSize;
            f32 popupBorderSize;
            f32 windowRounding;
            f32 frameRounding;
        } general;

        extern APPLIB_API struct Button
        {
            ImVec4 color;
            ImVec4 colorActive;
            ImVec4 colorHovered;
            ImVec2 padding;
        } button;

        extern APPLIB_API struct CheckBox
        {
            f32 spacing;
            f32 sizePadding;
            ImVec4 mark;
        } checkbox;

        extern APPLIB_API struct Colors
        {
            ImVec4 windowBg;
            ImVec4 frameBg;
            ImVec4 frameHovered;
            ImVec4 frameActive;
            ImVec4 header;
            ImVec4 headerHovered;
            ImVec4 headerActive;
            ImVec4 border;
        } colors;

        APPLIB_API void setupNativeStyle();
    } // namespace style
}; // namespace uikit

static inline ImVec2 operator*(const ImVec2 &lhs, const float rhs) { return ImVec2(lhs.x * rhs, lhs.y * rhs); }
static inline ImVec2 operator/(const ImVec2 &lhs, const float rhs) { return ImVec2(lhs.x / rhs, lhs.y / rhs); }
static inline ImVec2 operator+(const ImVec2 &lhs, const ImVec2 &rhs) { return ImVec2(lhs.x + rhs.x, lhs.y + rhs.y); }
static inline ImVec2 operator-(const ImVec2 &lhs, const ImVec2 &rhs) { return ImVec2(lhs.x - rhs.x, lhs.y - rhs.y); }
static inline ImVec2 operator*(const ImVec2 &lhs, const ImVec2 &rhs) { return ImVec2(lhs.x * rhs.x, lhs.y * rhs.y); }
static inline ImVec2 operator/(const ImVec2 &lhs, const ImVec2 &rhs) { return ImVec2(lhs.x / rhs.x, lhs.y / rhs.y); }
static inline ImVec2 operator-(const ImVec2 &lhs) { return ImVec2(-lhs.x, -lhs.y); }
static inline ImVec2 &operator*=(ImVec2 &lhs, const float rhs)
{
    lhs.x *= rhs;
    lhs.y *= rhs;
    return lhs;
}
static inline ImVec2 &operator/=(ImVec2 &lhs, const float rhs)
{
    lhs.x /= rhs;
    lhs.y /= rhs;
    return lhs;
}
static inline ImVec2 &operator+=(ImVec2 &lhs, const ImVec2 &rhs)
{
    lhs.x += rhs.x;
    lhs.y += rhs.y;
    return lhs;
}
static inline ImVec2 &operator-=(ImVec2 &lhs, const ImVec2 &rhs)
{
    lhs.x -= rhs.x;
    lhs.y -= rhs.y;
    return lhs;
}
static inline ImVec2 &operator*=(ImVec2 &lhs, const ImVec2 &rhs)
{
    lhs.x *= rhs.x;
    lhs.y *= rhs.y;
    return lhs;
}
static inline ImVec2 &operator/=(ImVec2 &lhs, const ImVec2 &rhs)
{
    lhs.x /= rhs.x;
    lhs.y /= rhs.y;
    return lhs;
}
static inline bool operator==(const ImVec2 &lhs, const ImVec2 &rhs) { return lhs.x == rhs.x && lhs.y == rhs.y; }
static inline bool operator!=(const ImVec2 &lhs, const ImVec2 &rhs) { return lhs.x != rhs.x || lhs.y != rhs.y; }
static inline ImVec4 operator+(const ImVec4 &lhs, const ImVec4 &rhs)
{
    return ImVec4(lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z, lhs.w + rhs.w);
}
static inline ImVec4 operator-(const ImVec4 &lhs, const ImVec4 &rhs)
{
    return ImVec4(lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z, lhs.w - rhs.w);
}
static inline ImVec4 operator*(const ImVec4 &lhs, const ImVec4 &rhs)
{
    return ImVec4(lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z, lhs.w * rhs.w);
}
static inline bool operator==(const ImVec4 &lhs, const ImVec4 &rhs)
{
    return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z && lhs.w == rhs.w;
}
static inline bool operator!=(const ImVec4 &lhs, const ImVec4 &rhs)
{
    return lhs.x != rhs.x || lhs.y != rhs.y || lhs.z != rhs.z || lhs.w != rhs.w;
}

#endif