#pragma once

#include <acul/enum.hpp>
#include <acul/string/string.hpp>
#include <acul/vector.hpp>
#include <amal/vector.hpp>
#include "../detail/events.hpp"
#include "../draw.hpp"
#include "../theme.hpp"

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
            configurable = 0x2,
            attachable = 0x4,
            foreground = 0x8,
            background = 0x10,
            fixed = 0x20,
            hittable = 0x40
        };
        using flag_bitmask = std::true_type;
    };

    using WidgetFlags = acul::flags<WidgetFlagBits>;
    struct StyleUpdateFlagBits
    {
        enum enum_type
        {
            none = 0x0,
            redraw = 0x1,
            layout = 0x2,
            parent_layout = 0x4
        };
        using flag_bitmask = std::true_type;
    };
    using StyleUpdateFlags = acul::flags<StyleUpdateFlagBits>;
    constexpr inline detail::StylePropertyFlags g_style_layout_mask =
        detail::StylePropertiesBits::padding | detail::StylePropertiesBits::text_size |
        detail::StylePropertiesBits::border_thickness | detail::StylePropertiesBits::border_radius;
    constexpr inline detail::StylePropertyFlags g_style_parent_layout_mask = detail::StylePropertiesBits::margin;

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
        }

        virtual ~Widget() = default;

        inline u32 id() const { return _id; }
        inline Widget *parent() const { return _parent; }
        inline void set_parent(Widget *parent) { _parent = parent; }
        inline Widget *focus_parent() const { return _focus_parent; }
        inline void set_focus_parent(Widget *parent) { _focus_parent = parent; }
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
        inline u16 clip_id() const { return _rect.clip_id; }
        inline void set_clip_id(u16 id) { _rect.clip_id = id; }
        inline void inherit_parent_clip_rect() { _rect.clip_id = _parent ? _parent->clip_id() : 0xFFFFu; }
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

        virtual void update_layout_min_size() { set_required_size(size()); }
        virtual void update_layout(bool min_size_known = true)
        {
            (void)min_size_known;
            detail::get_context().dirty_flags |= DirtyFlagBits::hit_rect_update;
        }

        virtual void update_depth(const amal::vec2 &depth_range);
        virtual StyleUpdateFlags update_style() = 0;
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

        virtual void on_focus(bool focused) { (void)focused; }

        virtual void on_hover(HoverState state, u32 prev_tag_id)
        {
            (void)prev_tag_id;
            (void)state;
        }
        virtual void on_click(MouseKey key, KeyPressState state, u32 click_count) {}
        virtual void on_drag(const amal::vec2 &delta, KeyPressState state) {}
        virtual void on_key(u32 key, KeyPressState state, u32 mods)
        {
            (void)key;
            (void)state;
            (void)mods;
        }
        virtual void on_char(const acul::string &text, u32 count)
        {
            (void)text;
            (void)count;
        }

    protected:
        u32 _id;
        Widget *_parent = nullptr;
        Widget *_focus_parent = nullptr;
        amal::vec2 _depth_range{0.0f, 1.0f};
        detail::RectData _rect{};
        amal::vec2 _required_size{0.0f, 0.0f};
        StyleState _style_state = StyleState::normal;
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
        if (widget.style_state() == StyleState::active || widget.style_state() == StyleState::focus) return false;
        if (state == HoverState::leave) return widget.set_style_state(StyleState::normal);
        return widget.set_style_state(StyleState::hover);
    }

    inline StyleUpdateFlags make_style_update_flags(const Style &prev_style, const Style &next_style)
    {
        const auto union_mask = prev_style.mask() | next_style.mask();
        detail::StylePropertyFlags changed = detail::StylePropertiesBits::none;
        if ((union_mask & detail::StylePropertiesBits::padding) && prev_style.padding() != next_style.padding())
            changed |= detail::StylePropertiesBits::padding;
        if ((union_mask & detail::StylePropertiesBits::margin) && prev_style.margin() != next_style.margin())
            changed |= detail::StylePropertiesBits::margin;
        if ((union_mask & detail::StylePropertiesBits::background_color) &&
            prev_style.background_color() != next_style.background_color())
            changed |= detail::StylePropertiesBits::background_color;
        if ((union_mask & detail::StylePropertiesBits::text_color) &&
            prev_style.text_color() != next_style.text_color())
            changed |= detail::StylePropertiesBits::text_color;
        if ((union_mask & detail::StylePropertiesBits::border_color) &&
            prev_style.border_color() != next_style.border_color())
            changed |= detail::StylePropertiesBits::border_color;
        if ((union_mask & detail::StylePropertiesBits::border_radius) &&
            prev_style.border_radius() != next_style.border_radius())
            changed |= detail::StylePropertiesBits::border_radius;
        if ((union_mask & detail::StylePropertiesBits::border_thickness) &&
            prev_style.border_thickness() != next_style.border_thickness())
            changed |= detail::StylePropertiesBits::border_thickness;
        if ((union_mask & detail::StylePropertiesBits::corner_mask) &&
            prev_style.corner_mask() != next_style.corner_mask())
            changed |= detail::StylePropertiesBits::corner_mask;
        if ((union_mask & detail::StylePropertiesBits::text_size) && prev_style.text_size() != next_style.text_size())
            changed |= detail::StylePropertiesBits::text_size;

        if (changed == detail::StylePropertiesBits::none) return StyleUpdateFlagBits::none;
        StyleUpdateFlags out = StyleUpdateFlagBits::redraw;
        if (changed & g_style_layout_mask) out |= StyleUpdateFlagBits::layout;
        if (changed & g_style_parent_layout_mask) out |= StyleUpdateFlagBits::parent_layout;
        return out;
    }

    inline StyleUpdateFlags resolve_style_selector(StyleSelector &selector, u32 self_id, u32 parent_id,
                                                   StyleState state = StyleState::normal)
    {
        auto *theme = get_theme();
        const StyleID prev_style_id = selector.id;
        const StyleID next_style_id = theme->get_resolved_style(selector.tag_id, self_id, parent_id, state);
        selector.id = next_style_id;
        if (prev_style_id == Theme::STYLE_ID_INVALID)
        {
            const Style empty_style{};
            return make_style_update_flags(empty_style, theme->get_style(next_style_id));
        }
        if (prev_style_id == next_style_id) return StyleUpdateFlagBits::none;
        return make_style_update_flags(theme->get_style(prev_style_id), theme->get_style(next_style_id));
    }

} // namespace auik::v2
