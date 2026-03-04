#pragma once

#include <acul/enum.hpp>
#include <acul/vector.hpp>
#include <amal/vector.hpp>
#include "draw.hpp"

namespace auik::v2
{
    namespace detail
    {
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

    class APPLIB_API Widget
    {
    public:
        WidgetFlags widget_flags;

        Widget(u32 id, WidgetFlags flags, Widget *parent = nullptr, amal::vec2 pos = {0.0f, 0.0f},
               amal::vec2 size = {0.0f, 0.0f}, u32 tag_id = 0)
            : widget_flags(flags), _id(id), _parent(parent), _rect(detail::make_rect_data(id, tag_id, pos, size, 0))
        {
            push_clip_rect();
        }

        virtual ~Widget() = default;

        inline u32 id() const { return _id; }
        inline Widget *parent() const { return _parent; }
        inline void set_parent(Widget *parent) { _parent = parent; }
        inline detail::RectData &get_rect() { return _rect; }
        inline const detail::RectData &get_rect() const { return _rect; }
        inline void set_rect_tag_id(u32 tag_id) { _rect.tag_id = tag_id; }

        inline f32 get_z_order() const { return _rect.depth; }
        inline const amal::vec2 &depth_range() const { return _depth_range; }
        inline amal::vec2 &position() { return _rect.position; }
        inline const amal::vec2 &position() const { return _rect.position; }
        inline amal::vec2 &size() { return _rect.size; }
        inline const amal::vec2 &size() const { return _rect.size; }
        inline void set_position(const amal::vec2 &pos) { _rect.position = pos; }
        inline void set_size(const amal::vec2 &size) { _rect.size = size; }
        inline const amal::vec2 &required_size() const { return _required_size; }
        inline void set_required_size(const amal::vec2 &size) { _required_size = size; }
        inline u16 clip_rect_id() const { return _rect.clip_rect_id; }
        inline void set_clip_rect_id(u16 id) { _rect.clip_rect_id = id; }
        inline void push_clip_rect() { _rect.clip_rect_id = auik::v2::push_clip_rect({_rect.position, _rect.size}); }

        virtual void rebuild_clip_rects() { push_clip_rect(); }

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
            _required_size = _rect.size;
            if (_rect.clip_rect_id == 0xFFFFu) push_clip_rect();
            update_clip_rect(_rect.clip_rect_id, {_rect.position, _rect.size});
        }

        virtual void update_depth(const amal::vec2 &depth_range);
        virtual void update_style() = 0;
        virtual void draw(DrawCtx &) = 0;
        virtual void on_attach();
        virtual void on_detach();
        virtual void on_scroll(u32 tag_id, const amal::vec2 &delta)
        {
            (void)tag_id;
            if (_parent) _parent->on_scroll(tag_id, delta);
        }

    protected:
        u32 _id;
        Widget *_parent = nullptr;
        amal::vec2 _depth_range{0.0f, 1.0f};
        detail::RectData _rect{};
        amal::vec2 _required_size{0.0f, 0.0f};
    };

    APPLIB_API void assign_next_depth(const amal::vec2 &parent_range, amal::vec2 &dst_range);

    inline f32 next_depth(const amal::vec2 &parent_range)
    {
        amal::vec2 next_range{};
        assign_next_depth(parent_range, next_range);
        return (next_range.x + next_range.y) * 0.5f;
    }

    APPLIB_API void record_all_commands();
    APPLIB_API void add_widget_to_root(Widget *widget);
    APPLIB_API void reset_clip_rects();
} // namespace auik::v2
