#pragma once

#include <acul/enum.hpp>
#include "draw.hpp"
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

    constexpr inline WidgetFlags get_default_widget_flags() { return WidgetFlagBits::visible; }

    using u31 = u32;

    class Widget
    {
    public:
        WidgetFlags widget_flags;
        struct
        {
            StyleID id = 0;
            u32 type_id = 0;
        } style;

        Widget(u31 id, WidgetFlags flags, Widget *parent = nullptr) : widget_flags(flags), _id(id), _parent(parent) {}
        virtual ~Widget() = default;

        inline u31 id() const { return _id; }
        inline Widget *parent() const { return _parent; }

        void record_draw_commands()
        {
            DrawCtx ctx{};
            ctx.emit = &emit_draw_record;
            draw(ctx);
        }

        void update_draw_commands()
        {
            DrawCtx ctx{};
            ctx.emit = &emit_draw_update;
            draw(ctx);
        }

        virtual void update_layout() {}

    protected:
        virtual void draw(DrawCtx &) = 0;

        u31 _id;
        Widget *_parent = nullptr;
        f32 _z_order = 0.0f;
    };

    APPLIB_API void record_all_commands();
    APPLIB_API void record_all_commands_force();
    APPLIB_API void update_widget_style(Widget *widget);
    APPLIB_API void add_widget_to_root(Widget *widget);
} // namespace auik::v2
