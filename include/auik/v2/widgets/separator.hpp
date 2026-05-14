#pragma once

#include <acul/memory/alloc.hpp>
#include <amal/geometric.hpp>
#include "../theme.hpp"
#include "widget.hpp"

#define AUIK_TAG_SEPARATOR 0x3EF7C2B1u

namespace auik::v2
{
    class APPLIB_API Separator : public Widget
    {
    public:
        Separator(amal::axis axis, WidgetFlags widget_flags = WidgetFlagBits::visible, Widget *parent = nullptr,
                  u32 style_tag = AUIK_TAG_SEPARATOR);

        StyleUpdateFlags update_style() override;
        void update_layout_min_size() override;
        void update_layout(bool min_size_known) override;
        void translate(const amal::vec2 &delta) override;
        void rebuild_clip_rects() override;
        void update_depth(const amal::vec2 &depth_range) override;
        void draw(DrawCtx &ctx) override;

    protected:
        amal::axis _axis = amal::axis::x;
        StyleSelector _style{Theme::STYLE_ID_INVALID, AUIK_TAG_SEPARATOR};
        DrawDataID _line{};
        amal::rect _line_rect{};
    };

    class APPLIB_API HSeparator final : public Separator
    {
    public:
        HSeparator(WidgetFlags widget_flags = WidgetFlagBits::visible, Widget *parent = nullptr,
                   u32 style_tag = AUIK_TAG_SEPARATOR)
            : Separator(amal::axis::x, widget_flags, parent, style_tag)
        {
        }
    };

    class APPLIB_API VSeparator final : public Separator
    {
    public:
        VSeparator(WidgetFlags widget_flags = WidgetFlagBits::visible, Widget *parent = nullptr,
                   u32 style_tag = AUIK_TAG_SEPARATOR)
            : Separator(amal::axis::y, widget_flags, parent, style_tag)
        {
        }
    };

    inline HSeparator *make_h_separator() { return acul::alloc<HSeparator>(); }

    inline VSeparator *make_v_separator() { return acul::alloc<VSeparator>(); }
} // namespace auik::v2
