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
            background = 0x20,
            scrolable = 0x40
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

        Widget(u31 id, WidgetFlags flags, Widget *parent = nullptr, amal::vec2 pos = {0.0f, 0.0f},
               amal::vec2 size = {0.0f, 0.0f})
            : widget_flags(flags), _id(id), _parent(parent), _pos(pos), _size(size)
        {
            if (detail::g_context && detail::get_context().gpu_ctx && detail::get_context().gpu_ctx->push_clip_rect)
                _clip_rect_id = push_clip_rect({_pos.x, _pos.y, _size.x, _size.y});
        }
        virtual ~Widget() = default;

        inline u31 id() const { return _id; }
        inline Widget *parent() const { return _parent; }
        inline void set_parent(Widget *parent) { _parent = parent; }

        inline f32 get_hitbox_depth() const { return _depth.hitbox; }
        inline f32 get_z_order() const { return _depth.z_order; }
        inline const amal::vec2 &position() const { return _pos; }
        inline const amal::vec2 &size() const { return _size; }
        inline void set_position(const amal::vec2 &pos) { _pos = pos; }
        inline void set_size(const amal::vec2 &size) { _size = size; }
        inline const amal::vec2 &required_size() const { return _required_size; }
        inline void set_required_size(const amal::vec2 &size) { _required_size = size; }
        inline u16 clip_rect_id() const { return _clip_rect_id; }
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

        virtual void update_layout()
        {
            _required_size = _size;
            if (_clip_rect_id == 0xFFFFu)
            {
                if (!(detail::g_context && detail::get_context().gpu_ctx &&
                      detail::get_context().gpu_ctx->push_clip_rect))
                    return;
                _clip_rect_id = push_clip_rect({_pos, _size});
            }
            get_clip_rect(_clip_rect_id) = {_pos, _size};
        }
        virtual void update_depth(const amal::vec2 &depth_range);
        virtual void update_style() = 0;
        virtual void draw(DrawCtx &) = 0;

    protected:

        u31 _id;
        Widget *_parent = nullptr;
        detail::DepthData _depth;
        u16 _clip_rect_id = 0xFFFFu;
        amal::vec2 _pos{0.0f, 0.0f};
        amal::vec2 _size{0.0f, 0.0f};
        amal::vec2 _required_size{0.0f, 0.0f};
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
