#pragma once

#include <acul/memory/alloc.hpp>
#include <amal/geometric.hpp>
#include "../theme.hpp"
#include "widget.hpp"

#define AUIK_TAG_SEPARATOR 0x3EF7C2B1u

namespace auik
{
    class Separator : public Widget
    {
    public:
        AUIK_EXPORT Separator(amal::axis axis, WidgetFlags widget_flags = WidgetFlagBits::visible, Widget *parent = nullptr,
                  u32 style_tag = AUIK_STYLE_TAG_SEPARATOR);

        AUIK_EXPORT StyleUpdateFlags update_style() override;
        AUIK_EXPORT void update_layout_min_size() override;
        AUIK_EXPORT void update_layout(bool min_size_known) override;
        AUIK_EXPORT void translate(const amal::vec2 &delta) override;
        AUIK_EXPORT void rebuild_clip_rects() override;
        AUIK_EXPORT void update_depth(const amal::vec2 &depth_range) override;
        AUIK_EXPORT void draw(DrawCtx &ctx) override;

    protected:
        amal::axis _axis = amal::axis::x;
        StyleSelector _style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_SEPARATOR};
        DrawDataID _line{};
        amal::rect _line_rect{};
    };

    class HSeparator final : public Separator
    {
    public:
        AUIK_EXPORT HSeparator(WidgetFlags widget_flags = WidgetFlagBits::visible, Widget *parent = nullptr,
                   u32 style_tag = AUIK_STYLE_TAG_SEPARATOR)
            : Separator(amal::axis::x, widget_flags, parent, style_tag)
        {
        }

        virtual u32 signature() const override { return AUIK_TAG_SEPARATOR; }
    };

    class VSeparator final : public Separator
    {
    public:
        AUIK_EXPORT VSeparator(WidgetFlags widget_flags = WidgetFlagBits::visible, Widget *parent = nullptr,
                   u32 style_tag = AUIK_STYLE_TAG_SEPARATOR)
            : Separator(amal::axis::y, widget_flags, parent, style_tag)
        {
        }

        virtual u32 signature() const override { return AUIK_TAG_SEPARATOR; }
    };

    inline HSeparator *make_h_separator() { return acul::alloc<HSeparator>(); }

    inline VSeparator *make_v_separator() { return acul::alloc<VSeparator>(); }

    namespace streams
    {
        extern AUIK_EXPORT const umbf::streams::Stream h_separator;
        extern AUIK_EXPORT const umbf::streams::Stream v_separator;
    }
} // namespace auik
