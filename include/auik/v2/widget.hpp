#pragma once

#include <acul/enum.hpp>
#include <acul/vector.hpp>
#include <amal/vector.hpp>
#include "draw.hpp"

namespace auik::v2
{
    namespace detail
    {
        struct DepthData
        {
            amal::vec2 range{0.0f, 1.0f};
            // Cached values used frequently in draw/hit passes.
            f32 z_order = 0.0f;
            f32 hitbox = 0.0f;
        };

        APPLIB_API amal::vec2 get_depth_workzone_range(const amal::vec2 &r);
    } // namespace detail

    struct WidgetFlagBits
    {
        enum enum_type
        {
            none = 0x0,
            visible = 0x1,
            active = 0x2,
            hovered = 0x4,
            configurable = 0x8,
            foreground = 0x10,
            background = 0x20
        };
        using flag_bitmask = std::true_type;
    };

    using WidgetFlags = acul::flags<WidgetFlagBits>;
    constexpr inline WidgetFlags get_default_widget_flags() { return WidgetFlagBits::visible; }
    using u31 = u32;

    class APPLIB_API Widget
    {
    public:
        WidgetFlags widget_flags;

        Widget(u31 id, WidgetFlags flags, Widget *parent = nullptr) : widget_flags(flags), _id(id), _parent(parent) {}
        virtual ~Widget() = default;

        inline u31 id() const { return _id; }
        inline Widget *parent() const { return _parent; }
        inline void set_parent(Widget *parent) { _parent = parent; }

        inline f32 get_hitbox_depth() const { return _depth.hitbox; }
        inline f32 get_z_order() const { return _depth.z_order; }
        inline detail::DepthData &depth_data() { return _depth; }
        inline const detail::DepthData &depth_data() const { return _depth; }

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
        virtual void update_depth(const amal::vec2 &depth_range);
        virtual void update_style() = 0;

    protected:
        virtual void draw(DrawCtx &) = 0;

        u31 _id;
        Widget *_parent = nullptr;
        detail::DepthData _depth;
    };

    APPLIB_API void assign_next_depth(const detail::DepthData &parent, detail::DepthData &dst);

    inline f32 next_depth(const detail::DepthData &parent)
    {
        detail::DepthData next{};
        assign_next_depth(parent, next);
        return next.z_order;
    }

    APPLIB_API void record_all_commands();
    APPLIB_API void record_all_commands_force();
    APPLIB_API void add_widget_to_root(Widget *widget);
} // namespace auik::v2
