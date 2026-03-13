#pragma once

#include <acul/enum.hpp>
#include <acul/vector.hpp>
#include <amal/vector.hpp>
#include "../detail/events.hpp"
#include "../draw.hpp"
#include "../theme.hpp"

namespace auik::v2
{
    constexpr inline u32 AUIK_INVALID_RENDER_SLOT_ID = ~0u;

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
            configurable = 0x2,
            attachable = 0x4,
            foreground = 0x8,
            background = 0x10,
            active_from_child = 0x20,
            active_to_child = 0x40,
            fixed = 0x80,
            hittable = 0x100
        };
        using flag_bitmask = std::true_type;
    };

    using WidgetFlags = acul::flags<WidgetFlagBits>;
    constexpr inline WidgetFlags get_default_widget_flags()
    {
        return WidgetFlagBits::visible | WidgetFlagBits::attachable | WidgetFlagBits::configurable;
    }

    class APPLIB_API Widget
    {
    public:
        WidgetFlags widget_flags;

        Widget(u32 id, WidgetFlags flags, Widget *parent = nullptr, amal::vec2 pos = {0.0f, 0.0f},
               amal::vec2 size = {0.0f, 0.0f}, u32 tag_id = 0)
            : widget_flags(flags), _id(id), _parent(parent), _rect(detail::make_rect_data(id, tag_id, pos, size))
        {
            auto *ctx = detail::g_context;
            if (!ctx) return;
            if (!ctx->free_render_slot_ids.empty())
            {
                _render_slot_id = ctx->free_render_slot_ids.back();
                ctx->free_render_slot_ids.pop_back();
                return;
            }
            _render_slot_id = ctx->pending_task_count++;
        }

        virtual ~Widget()
        {
            auto *ctx = detail::g_context;
            if (!ctx) return;
            if (_render_slot_id == AUIK_INVALID_RENDER_SLOT_ID) return;
            const u32 word_id = _render_slot_id >> 6;
            if (word_id < ctx->pending_task_bits.size())
                ctx->pending_task_bits[word_id] &= ~(1ull << (_render_slot_id & 63));
            ctx->free_render_slot_ids.push_back(_render_slot_id);
            _render_slot_id = AUIK_INVALID_RENDER_SLOT_ID;
        }

        inline u32 id() const { return _id; }
        inline Widget *parent() const { return _parent; }
        inline void set_parent(Widget *parent) { _parent = parent; }
        inline detail::RectData &get_rect() { return _rect; }
        inline const detail::RectData &get_rect() const { return _rect; }
        inline void set_rect_tag_id(u32 tag_id) { _rect.tag_id = tag_id; }
        inline bool is_visible() const { return (widget_flags & WidgetFlagBits::visible); }
        inline void set_visible(bool value)
        {
            const bool visible = is_visible();
            if (visible == value) return;
            if (value) widget_flags |= WidgetFlagBits::visible;
            else widget_flags &= ~WidgetFlagBits::visible;
            auto &ctx = detail::get_context();
            ctx.dirty_flags |= DirtyFlagBits::redraw;
        }
        inline void show() { set_visible(true); }
        inline void hide() { set_visible(false); }
        inline StyleState style_state() const { return _style_state; }
        inline bool set_style_state(StyleState value)
        {
            if (_style_state == value) return false;
            _style_state = value;
            return true;
        }

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
        inline bool is_fixed() const { return widget_flags & WidgetFlagBits::fixed; }
        inline bool is_hittable() const { return widget_flags & WidgetFlagBits::hittable; }
        inline u32 render_slot_id() const { return _render_slot_id; }
        inline void set_render_slot_id(u32 slot_id) { _render_slot_id = slot_id; }
        inline u16 clip_id() const { return _rect.clip_id; }
        inline void set_clip_id(u16 id) { _rect.clip_id = id; }
        inline void inherit_parent_clip_rect()
        {
            _rect.clip_id = _parent ? _parent->clip_id() : 0xFFFFu;
        }
        inline void inherit_parent_content_clip_rect()
        {
            _rect.clip_id = _parent ? _parent->content_clip_id() : 0xFFFFu;
        }
        inline void ensure_own_clip_rect(const amal::vec4 &rect)
        {
            if (_rect.clip_id == 0xFFFFu) _rect.clip_id = auik::v2::push_clip_rect(rect);
            else update_clip_rect(_rect.clip_id, rect);
        }

        virtual void rebuild_clip_rects() {}

        void record_draw_commands()
        {
            DrawCtx draw_ctx{};
            draw_ctx.emit = &emit_draw_record;
            draw_ctx.emit_hit_rect = is_hittable();
            draw(draw_ctx);
        }

        void update_draw_commands()
        {
            DrawCtx draw_ctx{};
            draw_ctx.emit = &emit_draw_update;
            draw_ctx.emit_hit_rect = is_hittable();
            draw(draw_ctx);
        }

        virtual void update_layout()
        {
            detail::get_context().dirty_flags |= DirtyFlagBits::hit_rect_update;
        }

        virtual void update_depth(const amal::vec2 &depth_range);
        virtual void update_style() = 0;
        virtual void draw(DrawCtx &) = 0;
        virtual u16 content_clip_id() const { return clip_id(); }
        virtual amal::vec4 get_content_clip_rect() const { return get_clip_rect(content_clip_id()); }
        virtual void on_attach()
        {
            auto &ctx = detail::get_context();
            ctx.id_map.emplace(id(), this);
        }
        virtual void on_detach() { detail::get_context().id_map.erase(id()); }
        virtual void on_scroll(const amal::vec2 &delta)
        {
            if (_parent) _parent->on_scroll(delta);
        }

        virtual void on_active()
        {
            auto &ctx = detail::get_context();
            ctx.active_id = id();
            set_style_state(StyleState::active);
            if (_parent && (_parent->widget_flags & WidgetFlagBits::active_from_child)) _parent->on_active();
        }
        virtual void on_parent_active() {}

        virtual void on_hover(HoverState state, u32 prev_tag_id)
        {
            (void)prev_tag_id;
            (void)state;
        }
        virtual void on_click(MouseKey key, KeyPressState state, u32 click_count) {}
        virtual void on_drag(const amal::vec2 &delta, KeyPressState state) {}

    protected:
        u32 _id;
        Widget *_parent = nullptr;
        amal::vec2 _depth_range{0.0f, 1.0f};
        detail::RectData _rect{};
        amal::vec2 _required_size{0.0f, 0.0f};
        StyleState _style_state = StyleState::normal;
        u32 _render_slot_id = AUIK_INVALID_RENDER_SLOT_ID;
    };

    APPLIB_API void assign_next_depth(const amal::vec2 &parent_range, amal::vec2 &dst_range);

    inline f32 next_depth(const amal::vec2 &parent_range)
    {
        amal::vec2 next_range{};
        assign_next_depth(parent_range, next_range);
        return (next_range.x + next_range.y) * 0.5f;
    }

    inline bool apply_hover_style_state(Widget &widget, HoverState state)
    {
        if (widget.style_state() == StyleState::active) return false;
        if (state == HoverState::leave) return widget.set_style_state(StyleState::normal);
        return widget.set_style_state(StyleState::hover);
    }

} // namespace auik::v2
