#pragma once

#include <acul/enum.hpp>
#include "theme.hpp"

namespace auik::v2
{
    struct WidgetFlagBits
    {
        enum enum_type
        {
            none = 0x0,
            visible = 0x1,
            active = 0x2,
            hovered = 0x4,
            configurable = 0x8
        };
        using flag_bitmask = std::true_type;
    };

    using WidgetFlags = acul::flags<WidgetFlagBits>;

    class Widget
    {
    public:
        WidgetFlags widget_flags;
        struct
        {
            StyleID id = 0;
            u32 type_id = 0;
        } style;

        Widget(u32 id, WidgetFlags flags, Widget *parent = nullptr) : widget_flags(flags), _id(id), _parent(parent) {}
        virtual ~Widget() = default;

        u32 id() const { return _id; }

        virtual void record_commands() {}
        virtual void update_immediate_commands() {}

    protected:
        u32 _id;
        Widget *_parent = nullptr;
        f32 _z_order = 0.0f;
    };
} // namespace auik::v2
