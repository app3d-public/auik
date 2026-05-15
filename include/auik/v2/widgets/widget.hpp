#pragma once

#include <acul/enum.hpp>
#include <acul/functional/unique_function.hpp>
#include <acul/string/string.hpp>
#include <acul/vector.hpp>
#include <amal/vector.hpp>
#include "../detail/events.hpp"
#include "../draw.hpp"
#include "../theme.hpp"

namespace auik::v2
{
    struct WidgetFlagBits
    {
        enum enum_type
        {
            none = 0x0,
            visible = 0x1,
            configurable = 0x2,
            attachable = 0x4,
            fixed = 0x20,
            hittable = 0x40,
            viewport_reserved = 0x80
        };
        using flag_bitmask = std::true_type;
    };

    using WidgetFlags = acul::flags<WidgetFlagBits>;
    struct DepthZone
    {
        enum enum_type
        {
            foreground,
            work,
            background
        };
    };

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
    constexpr inline DrawReasonFlags get_draw_reason_from_style_update(StyleUpdateFlags flags)
    {
        DrawReasonFlags reason = DrawReasonBits::none;
        if ((flags & StyleUpdateFlagBits::layout) || (flags & StyleUpdateFlagBits::parent_layout))
            reason |= DrawReasonBits::layout;
        return reason;
    }

    constexpr inline detail::StylePropertyFlags g_style_layout_mask =
        detail::StylePropertiesBits::padding | detail::StylePropertiesBits::text_size |
        detail::StylePropertiesBits::border_thickness | detail::StylePropertiesBits::border_radius |
        detail::StylePropertiesBits::font | detail::StylePropertiesBits::inline_spacing;
    constexpr inline detail::StylePropertyFlags g_style_parent_layout_mask = detail::StylePropertiesBits::margin;

    constexpr inline WidgetFlags get_default_widget_flags()
    {
        return WidgetFlagBits::visible | WidgetFlagBits::attachable | WidgetFlagBits::configurable;
    }

    class APPLIB_API Widget
    {
    public:
        struct UserBind
        {
            explicit UserBind(Widget *owner) : _owner(owner) {}

            UserBind &on_hover(acul::unique_function<void(HoverEvent &)> fn)
            {
                on_hover_fn = std::move(fn);
                if (_owner && on_hover_fn) _owner->add_event_flags(EventFlagBits::hover);
                return *this;
            }

            UserBind &on_click(acul::unique_function<void(ClickEvent &)> fn)
            {
                on_click_fn = std::move(fn);
                if (_owner && on_click_fn) _owner->add_event_flags(EventFlagBits::click);
                return *this;
            }

            UserBind &on_drag(acul::unique_function<void(DragEvent &)> fn)
            {
                on_drag_fn = std::move(fn);
                if (_owner && on_drag_fn) _owner->add_event_flags(EventFlagBits::drag);
                return *this;
            }

            UserBind &on_scroll(acul::unique_function<void(ScrollEvent &)> fn)
            {
                on_scroll_fn = std::move(fn);
                if (_owner && on_scroll_fn) _owner->add_event_flags(EventFlagBits::scroll);
                return *this;
            }

            UserBind &on_focus(acul::unique_function<void(FocusEvent &)> fn)
            {
                on_focus_fn = std::move(fn);
                if (_owner && on_focus_fn) _owner->add_event_flags(EventFlagBits::focus);
                return *this;
            }

            UserBind &on_key(acul::unique_function<void(KeyEvent &)> fn)
            {
                on_key_fn = std::move(fn);
                if (_owner && on_key_fn) _owner->add_event_flags(EventFlagBits::key_input);
                return *this;
            }

            UserBind &on_char(acul::unique_function<void(CharEvent &)> fn)
            {
                on_char_fn = std::move(fn);
                if (_owner && on_char_fn) _owner->add_event_flags(EventFlagBits::char_input);
                return *this;
            }

            UserBind &on_change(acul::unique_function<void(ChangeEvent &)> fn)
            {
                on_change_fn = std::move(fn);
                if (_owner && on_change_fn) _owner->add_event_flags(EventFlagBits::change);
                return *this;
            }

            acul::unique_function<void(HoverEvent &)> on_hover_fn = nullptr;
            acul::unique_function<void(ClickEvent &)> on_click_fn = nullptr;
            acul::unique_function<void(DragEvent &)> on_drag_fn = nullptr;
            acul::unique_function<void(ScrollEvent &)> on_scroll_fn = nullptr;
            acul::unique_function<void(FocusEvent &)> on_focus_fn = nullptr;
            acul::unique_function<void(KeyEvent &)> on_key_fn = nullptr;
            acul::unique_function<void(CharEvent &)> on_char_fn = nullptr;
            acul::unique_function<void(ChangeEvent &)> on_change_fn = nullptr;

        private:
            Widget *_owner = nullptr;
        };

        WidgetFlags widget_flags;
        EventFlags event_flags = EventFlagBits::none;

        Widget(u32 id, WidgetFlags flags, EventFlags event_flags = EventFlagBits::none, Widget *parent = nullptr,
               amal::rect bounds = {}, u32 tag_id = 0)
            : widget_flags(flags),
              event_flags(event_flags),
              _id(id),
              _parent(parent),
              _rect(detail::make_rect_data(id, tag_id, bounds))
        {
        }

        virtual ~Widget();
        UserBind &bind()
        {
            if (!_user_bind) _user_bind = acul::alloc<UserBind>(this);
            return *_user_bind;
        }

        inline u32 id() const { return _id; }
        inline Widget *parent() const { return _parent; }
        inline void set_parent(Widget *parent) { _parent = parent; }
        inline Widget *focus_parent() const { return _focus_parent; }
        inline void set_focus_parent(Widget *parent) { _focus_parent = parent; }
        inline DepthZone::enum_type depth_zone() const { return _depth_zone; }
        inline void set_depth_zone(DepthZone::enum_type zone) { _depth_zone = zone; }
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
            _external_draw_culled = false;
            _external_draw_invalidated = false;
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
        inline amal::rect bounds() { return _rect.bounds; }
        inline const amal::rect &bounds() const { return _rect.bounds; }
        inline amal::vec2 &position() { return _rect.bounds.offset; }
        inline const amal::vec2 &position() const { return _rect.bounds.offset; }
        inline amal::vec2 &size() { return _rect.bounds.size; }
        inline const amal::vec2 &size() const { return _rect.bounds.size; }
        inline void set_position(const amal::vec2 &pos) { _rect.bounds.offset = pos; }
        virtual void translate(const amal::vec2 &delta) { set_position(position() + delta); }
        inline void set_size(const amal::vec2 &size) { _rect.bounds.size = size; }
        inline const amal::vec2 &required_size() const { return _required_size; }
        inline void set_required_size(const amal::vec2 &size) { _required_size = size; }
        inline bool is_fixed() const { return widget_flags & WidgetFlagBits::fixed; }
        inline bool is_hittable() const { return widget_flags & WidgetFlagBits::hittable; }
        inline bool is_viewport_reserved() const { return widget_flags & WidgetFlagBits::viewport_reserved; }
        inline const amal::vec2 &root_viewport_origin() const { return _root_viewport_origin; }
        inline void set_root_viewport_origin(const amal::vec2 &origin) { _root_viewport_origin = origin; }
        inline bool has_event_handler(EventFlagBits::enum_type event_flag) const { return event_flags & event_flag; }
        inline void set_event_flags(EventFlags value) { event_flags = value; }
        inline void add_event_flags(EventFlags value) { event_flags |= value; }
        inline void remove_event_flags(EventFlags value) { event_flags &= ~value; }
        inline u16 clip_id() const { return _rect.clip_id; }
        inline void set_clip_id(u16 id) { _rect.clip_id = id; }
        inline void ensure_own_clip_rect(const amal::vec4 &rect)
        {
            if (_rect.clip_id == 0xFFFFu) _rect.clip_id = auik::v2::push_clip_rect(rect);
            else update_clip_rect(_rect.clip_id, rect);
        }

        virtual void rebuild_clip_rects() {}
        virtual void reset_draw_records() {}

        void update_draw_commands(DrawReasonFlags reason = DrawReasonBits::none)
        {
            const bool external_only = (reason & DrawReasonBits::external) && !(reason & ~DrawReasonBits::external);
            if (!external_only)
            {
                _external_draw_culled = false;
                _external_draw_invalidated = false;
            }
            DrawCtx draw_ctx{};
            draw_ctx.emit_fn = (reason & DrawReasonBits::record) ? &emit_draw_record : &emit_draw_update;
            draw_ctx.emit_hit_rect = is_hittable();
            draw_ctx.reason = reason;
            draw(draw_ctx);
        }

        void invalidate_draw_commands(DrawReasonFlags reason = DrawReasonBits::none)
        {
            DrawCtx draw_ctx{};
            draw_ctx.emit_fn = &emit_draw_invalidate;
            draw_ctx.emit_hit_rect = is_hittable();
            draw_ctx.reason = reason;
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
        // Controls whether a mouse press on this hit target replaces ctx.focus_id.
        // Return false for technical targets that should preserve the current focus leaf.
        virtual bool accepts_focus_on_mouse_press(detail::ElementID) const { return true; }
        virtual u16 content_clip_id() const { return clip_id(); }
        virtual amal::vec4 get_content_clip_rect() const { return get_clip_rect(content_clip_id()); }
        bool should_skip_external_draw_update(const amal::vec4 &clip_rect)
        {
            const auto &rect = bounds();
            const f32 rect_l = rect.offset.x;
            const f32 rect_t = rect.offset.y;
            const f32 rect_r = rect_l + rect.size.x;
            const f32 rect_b = rect_t + rect.size.y;
            const f32 clip_l = clip_rect.x;
            const f32 clip_t = clip_rect.y;
            const f32 clip_r = clip_l + clip_rect.z;
            const f32 clip_b = clip_t + clip_rect.w;
            const bool culled = rect_r <= clip_l || rect_b <= clip_t || rect_l >= clip_r || rect_t >= clip_b;
            const bool skip = _external_draw_culled && _external_draw_invalidated && culled;
            _external_draw_culled = culled;
            if (!culled) _external_draw_invalidated = false;
            return skip;
        }
        bool is_external_draw_culled() const { return _external_draw_culled; }
        void mark_external_draw_invalidated() { _external_draw_invalidated = true; }
        void reset_external_draw_cull_state()
        {
            _external_draw_culled = false;
            _external_draw_invalidated = false;
        }
        virtual void on_attach()
        {
            auto &ctx = detail::get_context();
            ctx.id_map.emplace(id(), this);
        }
        virtual void on_detach() { detail::get_context().id_map.erase(id()); }
        virtual void on_scroll(const amal::vec2 &delta) {}
        virtual void on_focus(bool focused) {}
        virtual void on_hover(HoverState state) {}
        virtual void on_click(MouseKey key, KeyPressState state, u32 click_count) {}
        virtual void on_drag(const amal::vec2 &delta, KeyPressState state) {}
        virtual void on_key(Key key, KeyPressState state, KeyMode mods) {}
        virtual void on_char_input(u32 char_code, u32 count) {}

        inline void dispatch_hover(HoverState state)
        {
            HoverEvent e{};
            e.state = state;
            if (_user_bind && _user_bind->on_hover_fn)
            {
                _user_bind->on_hover_fn(e);
                if (e.is_prevented_default()) return;
            }
            on_hover(state);
        }

        inline void dispatch_click(MouseKey key, KeyPressState state, u32 click_count)
        {
            ClickEvent e{};
            e.key = key;
            e.state = state;
            e.click_count = click_count;
            if (_user_bind && _user_bind->on_click_fn)
            {
                _user_bind->on_click_fn(e);
                if (e.is_prevented_default()) return;
            }
            on_click(key, state, click_count);
        }

        inline void dispatch_drag(const amal::vec2 &delta, KeyPressState state)
        {
            DragEvent e{};
            e.delta = delta;
            e.state = state;
            if (_user_bind && _user_bind->on_drag_fn)
            {
                _user_bind->on_drag_fn(e);
                if (e.is_prevented_default()) return;
            }
            on_drag(delta, state);
        }

        inline void dispatch_scroll(const amal::vec2 &delta)
        {
            ScrollEvent e{};
            e.delta = delta;
            if (_user_bind && _user_bind->on_scroll_fn)
            {
                _user_bind->on_scroll_fn(e);
                if (e.is_prevented_default()) return;
            }
            on_scroll(delta);
        }

        inline void dispatch_focus(bool focused)
        {
            FocusEvent e{};
            e.focused = focused;
            if (_user_bind && _user_bind->on_focus_fn)
            {
                _user_bind->on_focus_fn(e);
                if (e.is_prevented_default()) return;
            }
            on_focus(focused);
        }

        inline void dispatch_key(Key key, KeyPressState state, KeyMode mods)
        {
            KeyEvent e{};
            e.key = key;
            e.state = state;
            e.mods = mods;
            if (_user_bind && _user_bind->on_key_fn)
            {
                _user_bind->on_key_fn(e);
                if (e.is_prevented_default()) return;
            }
            on_key(key, state, mods);
        }

        inline void dispatch_char(u32 char_code, u32 count)
        {
            CharEvent e{};
            e.char_code = char_code;
            e.count = count;
            if (_user_bind && _user_bind->on_char_fn)
            {
                _user_bind->on_char_fn(e);
                if (e.is_prevented_default()) return;
            }
            on_char_input(char_code, count);
        }

    protected:
        inline bool dispatch_change()
        {
            if (!_user_bind || !_user_bind->on_change_fn) return false;
            ChangeEvent e{};
            _user_bind->on_change_fn(e);
            return e.is_prevented_default();
        }

        inline void redraw_external(bool has_record, DrawReasonFlags update_reason = DrawReasonBits::external)
        {
            if (has_record) update_draw_commands(update_reason);
            else update_draw_commands(DrawReasonBits::external | DrawReasonBits::record);
            detail::get_context().dirty_flags |= DirtyFlagBits::redraw;
        }

        u32 _id;
        Widget *_parent = nullptr;
        Widget *_focus_parent = nullptr;
        DepthZone::enum_type _depth_zone = DepthZone::work;
        amal::vec2 _depth_range{0.0f, 1.0f};
        amal::vec2 _root_viewport_origin{0.0f, 0.0f};
        detail::RectData _rect{};
        amal::vec2 _required_size{0.0f, 0.0f};
        StyleState _style_state = StyleState::normal;
        bool _external_draw_culled = false;
        bool _external_draw_invalidated = false;
        UserBind *_user_bind = nullptr;
    };

    APPLIB_API void assign_next_depth(const amal::vec2 &parent_range, amal::vec2 &dst_range);

    inline f32 next_depth(const amal::vec2 &parent_range)
    {
        amal::vec2 next_range{};
        assign_next_depth(parent_range, next_range);
        return (next_range.x + next_range.y) * 0.5f;
    }

    inline bool has_widget_state_style(const Widget &widget, StyleState state)
    {
        if (state == StyleState::normal) return true;
        auto *theme = detail::get_context().theme;
        const u32 parent_id = widget.parent() ? widget.parent()->id() : 0u;
        return theme && theme->has_state_style(widget.get_rect().tag_id, widget.id(), parent_id, state);
    }

    inline StyleState resolve_widget_visual_state(const Widget &widget, StyleState state)
    {
        return has_widget_state_style(widget, state) ? state : StyleState::normal;
    }

    inline bool apply_hover_style_state(Widget &widget, HoverState state)
    {
        if (widget.style_state() == StyleState::active || widget.style_state() == StyleState::focus) return false;
        if (state == HoverState::leave) return widget.set_style_state(StyleState::normal);
        if (!has_widget_state_style(widget, StyleState::hover)) return false;
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
        if ((union_mask & detail::StylePropertiesBits::font) && prev_style.font() != next_style.font())
            changed |= detail::StylePropertiesBits::font;
        if ((union_mask & detail::StylePropertiesBits::inline_spacing) &&
            prev_style.inline_spacing() != next_style.inline_spacing())
            changed |= detail::StylePropertiesBits::inline_spacing;

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
