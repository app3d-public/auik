#pragma once

#include <acul/enum.hpp>
#include <acul/functional/unique_function.hpp>
#include <acul/string/string.hpp>
#include <acul/vector.hpp>
#include <amal/vector.hpp>
#include "../detail/context.hpp"
#include "../detail/events.hpp"
#include "../draw.hpp"
#include "../post_effects.hpp"
#include "../theme.hpp"
#include "../viewport.hpp"

#define AUIK_UD_CUSTOM_DATA     0xFFFFu
#define AUIK_UD_ROOT_DATA       0xFFFEu
#define AUIK_UD_LOCALE_LITERAL  0xFFFCu
#define AUIK_WIDGET_SIGN_IGNORE 0xA087D252u

namespace auik
{
    struct StringView
    {
        const char *str = "";
        bool is_translated = false;

        StringView() = default;
        StringView(const char *value) : str(value ? value : ""), is_translated(false) {}
        StringView(const char *value, bool translated) : str(value ? value : ""), is_translated(translated) {}
        StringView(const acul::string &value) : str(value.c_str()), is_translated(false) {}
    };

    constexpr inline StringView tr(const char *literal) { return {literal, true}; }
    AUIK_EXPORT void rebuild_root_widget_depths();
    inline acul::string translate_string(const StringView &value)
    {
        const auto locale_cb = detail::get_default_string_locale_cb();
        if (!value.is_translated || !locale_cb) return acul::string(value.str ? value.str : "");
        const char *translated = locale_cb(value.str ? value.str : "");
        return acul::string(translated ? translated : "");
    }

    constexpr inline bool is_size_min_fit(f32 value) { return value == AUIK_SIZE_X_MIN_FIT; }
    constexpr inline bool is_size_min_fit_require(f32 value) { return value == AUIK_SIZE_X_MIN_FIT_REQUIRE; }
    constexpr inline bool is_size_fill(f32 value) { return value == AUIK_SIZE_X_FILL; }
    constexpr inline bool is_size_inherit(f32 value) { return value == AUIK_SIZE_X_INHERIT; }
    constexpr inline bool is_size_auto(f32 value) { return is_size_min_fit(value); }
    constexpr inline bool is_size_fit(f32 value) { return is_size_min_fit_require(value); }
    constexpr inline bool is_size_concrete(f32 value) { return value < AUIK_SIZE_X_FILL; }
    constexpr inline bool is_size_static_layout(f32 value) { return is_size_concrete(value) && value > 0.0f; }

    constexpr inline bool is_size_dynamic(f32 value) { return is_size_min_fit(value) || value >= AUIK_SIZE_X_FILL; }

    constexpr inline bool is_size_dynamic(const amal::vec2 &size)
    {
        return is_size_dynamic(size.x) || is_size_dynamic(size.y);
    }

    struct WidgetFlagBits
    {
        enum enum_type
        {
            none = 0x0,
            visible = 0x1,
            configurable = 0x2,
            attachable = 0x4,
            hittable = 0x8,
            read_only = 0x10,
            disabled = 0x20
        };
        using flag_bitmask = std::true_type;
    };

    using WidgetFlags = acul::flags<WidgetFlagBits>;

    struct WidgetStateFlagBits
    {
        enum enum_type : u8
        {
            none = 0x0,
            attached = 0x1,
            transient = 0x2
        };
        using flag_bitmask = std::true_type;
    };

    using WidgetStateFlags = acul::flags<WidgetStateFlagBits>;

    struct ChildLayoutFlagBits
    {
        enum enum_type : u32
        {
            none = 0x0,
            linline = 0x1,
            aright = 0x2,
            top = 0x4,
            vcenter = 0x8,
            bottom = 0x10,
            hcenter = 0x20,
            hleft = 0x40,
            block = 0x80
        };
        using flag_bitmask = std::true_type;
    };
    using ChildLayoutFlags = acul::flags<ChildLayoutFlagBits>;

    enum class ChildLayout : u8
    {
        block,
        inline_
    };

    constexpr inline ChildLayoutFlags default_child_layout_flags() { return ChildLayoutFlagBits::none; }

    inline ChildLayoutFlags make_layout_flags(ChildLayout layout = ChildLayout::block, HAlign halign = HAlign::none,
                                              VAlign valign = VAlign::none)
    {
        ChildLayoutFlags flags = ChildLayoutFlagBits::none;
        if (layout == ChildLayout::block) flags |= ChildLayoutFlagBits::block;
        if (layout == ChildLayout::inline_) flags |= ChildLayoutFlagBits::linline;
        if (halign == HAlign::left) flags |= ChildLayoutFlagBits::hleft;
        if (halign == HAlign::center) flags |= ChildLayoutFlagBits::hcenter;
        if (halign == HAlign::right) flags |= ChildLayoutFlagBits::aright;
        if (valign == VAlign::top) flags |= ChildLayoutFlagBits::top;
        if (valign == VAlign::center) flags |= ChildLayoutFlagBits::vcenter;
        else if (valign == VAlign::bottom) flags |= ChildLayoutFlagBits::bottom;
        return flags;
    }

    constexpr inline ChildLayoutFlags child_layout_display_mask()
    {
        return ChildLayoutFlagBits::block | ChildLayoutFlagBits::linline;
    }

    constexpr inline ChildLayoutFlags child_layout_halign_mask()
    {
        return ChildLayoutFlagBits::hleft | ChildLayoutFlagBits::hcenter | ChildLayoutFlagBits::aright;
    }

    constexpr inline ChildLayoutFlags child_layout_valign_mask()
    {
        return ChildLayoutFlagBits::top | ChildLayoutFlagBits::vcenter | ChildLayoutFlagBits::bottom;
    }

    inline ChildLayoutFlags merge_child_layout_flags(ChildLayoutFlags style, ChildLayoutFlags layout)
    {
        ChildLayoutFlags out = style;
        if (layout & child_layout_display_mask())
            out = (out & ~child_layout_display_mask()) | (layout & child_layout_display_mask());
        if (layout & child_layout_halign_mask())
            out = (out & ~child_layout_halign_mask()) | (layout & child_layout_halign_mask());
        if (layout & child_layout_valign_mask())
            out = (out & ~child_layout_valign_mask()) | (layout & child_layout_valign_mask());
        return out;
    }

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

    class Widget;
    class DrawBlock;
    namespace detail
    {
        struct WidgetStateAccess;
        AUIK_EXPORT void request_style_refresh(Widget *widget);
        AUIK_EXPORT void request_widget_refresh(Widget *widget, StyleUpdateFlags flags);
    } // namespace detail

    struct WidgetUserData
    {
        u32 tag_id = AUIK_UD_CUSTOM_DATA;
        void *handle = nullptr;
        void (*destroy)(void *) = nullptr;
        WidgetUserData *pNext = nullptr;
    };

    namespace detail
    {
        using RootWidgetUserData = DepthZone;
    }

    constexpr inline DrawReasonFlags get_draw_reason_from_style_update(StyleUpdateFlags flags)
    {
        DrawReasonFlags reason = DrawReasonBits::none;
        if ((flags & StyleUpdateFlagBits::layout) || (flags & StyleUpdateFlagBits::parent_layout))
            reason |= DrawReasonBits::layout;
        return reason;
    }

    constexpr inline detail::StylePropertyFlags g_style_layout_mask =
        detail::StylePropertiesBits::padding | detail::StylePropertiesBits::text_size |
        detail::StylePropertiesBits::border_thickness | detail::StylePropertiesBits::font |
        detail::StylePropertiesBits::inline_spacing | detail::StylePropertiesBits::width |
        detail::StylePropertiesBits::height | detail::StylePropertiesBits::min_width |
        detail::StylePropertiesBits::min_height | detail::StylePropertiesBits::extra;
    constexpr inline detail::StylePropertyFlags g_style_parent_layout_mask = detail::StylePropertiesBits::margin;

    constexpr inline WidgetFlags get_default_widget_flags()
    {
        return WidgetFlagBits::visible | WidgetFlagBits::attachable | WidgetFlagBits::configurable;
    }

    class Widget : public umbf::Block
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

            UserBind &prepend_click(acul::unique_function<void(ClickEvent &)> fn)
            {
                auto next = std::move(on_click_fn);
                on_click_fn = [first = std::move(fn), next = std::move(next)](ClickEvent &event) mutable {
                    if (first) first(event);
                    if (!event.is_prevented_default() && next) next(event);
                };
                if (_owner && on_click_fn) _owner->add_event_flags(EventFlagBits::click);
                return *this;
            }

            UserBind &on_drag(acul::unique_function<void(DragEvent &)> fn)
            {
                on_drag_fn = std::move(fn);
                if (_owner && on_drag_fn) _owner->add_event_flags(EventFlagBits::drag);
                return *this;
            }

            UserBind &on_drop(acul::unique_function<void(DropEvent &)> fn)
            {
                on_drop_fn = std::move(fn);
                if (_owner && on_drop_fn) _owner->add_event_flags(EventFlagBits::drop);
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
            acul::unique_function<void(DropEvent &)> on_drop_fn = nullptr;
            acul::unique_function<void(ScrollEvent &)> on_scroll_fn = nullptr;
            acul::unique_function<void(FocusEvent &)> on_focus_fn = nullptr;
            acul::unique_function<void(KeyEvent &)> on_key_fn = nullptr;
            acul::unique_function<void(CharEvent &)> on_char_fn = nullptr;
            acul::unique_function<void(ChangeEvent &)> on_change_fn = nullptr;

        private:
            Widget *_owner = nullptr;
        };

        WidgetFlags widget_flags;
        EventFlags requested_event_flags = EventFlagBits::none;
        EventFlags event_flags = EventFlagBits::none;

        Widget(u32 id, WidgetFlags flags, EventFlags event_flags = EventFlagBits::none,
               amal::rect bounds = {{0.0f, 0.0f}, AUIK_SIZE_INHERIT}, u32 tag_id = 0)
            : widget_flags(flags),
              requested_event_flags(event_flags),
              event_flags(resolve_event_flags(event_flags)),
              _synced_widget_flags(flags),
              _id(id),
              _parent(nullptr),
              _inline_size(bounds.size),
              _requested_size(resolve_style_size_from_inline(bounds.size, {AUIK_SIZE_X_FILL, AUIK_SIZE_Y_FIT})),
              _viewport(get_main_viewport()),
              _rect(detail::make_rect_data(id, tag_id,
                                           amal::rect{bounds.offset,
                                                      {is_size_concrete(bounds.size.x) ? bounds.size.x : 0.0f,
                                                       is_size_concrete(bounds.size.y) ? bounds.size.y : 0.0f}}))
        {
            assert(_viewport && "main viewport must be set before creating widgets");
        }

        AUIK_EXPORT virtual ~Widget();

        u32 signature() const override { return AUIK_WIDGET_SIGN_IGNORE; }

        template <class T, class... Args>
        T *emplace_user_data(Args &&...args)
        {
            return emplace_user_data_tagged<T>(AUIK_UD_CUSTOM_DATA, std::forward<Args>(args)...);
        }

        template <class T, class... Args>
        T *emplace_user_data_tagged(u32 tag, Args &&...args)
        {
            auto *value = acul::alloc<T>(std::forward<Args>(args)...);
            auto *node = acul::alloc<WidgetUserData>();
            node->tag_id = tag;
            node->handle = value;
            node->destroy = [](void *ptr) { acul::release(static_cast<T *>(ptr)); };
            append_user_data_node(node);
            return value;
        }

        void *get_user_data(u32 tag = AUIK_UD_CUSTOM_DATA) const
        {
            for (auto *node = _user_data; node; node = node->pNext)
                if (node->tag_id == tag) return node->handle;
            return nullptr;
        }

        template <class T>
        T *get_user_data(u32 tag = AUIK_UD_CUSTOM_DATA) const
        {
            return static_cast<T *>(get_user_data(tag));
        }

        const WidgetUserData *user_data_head() const { return _user_data; }
        AUIK_EXPORT void emplace_user_data_ref_head(u32 tag, void *handle);
        AUIK_EXPORT void emplace_user_data_ref_after_head(u32 tag, void *handle);

        template <class T, class... Args>
        T *emplace_user_data_head(u32 tag, Args &&...args)
        {
            auto *value = acul::alloc<T>(std::forward<Args>(args)...);
            auto *node = acul::alloc<WidgetUserData>();
            node->tag_id = tag;
            node->handle = value;
            node->destroy = [](void *ptr) { acul::release(static_cast<T *>(ptr)); };
            node->pNext = _user_data;
            _user_data = node;
            return value;
        }

        AUIK_EXPORT void pop_user_data_head(u32 expected_tag);

        UserBind &bind()
        {
            if (!_user_bind) _user_bind = acul::alloc<UserBind>(this);
            return *_user_bind;
        }

        inline u32 id() const { return _id; }
        inline bool is_attached() const { return _widget_state_flags & WidgetStateFlagBits::attached; }
        inline bool is_transient() const { return _widget_state_flags & WidgetStateFlagBits::transient; }
        inline Widget *parent() const { return _parent; }
        inline void set_parent(Widget *parent) { _parent = parent; }
        inline Widget *focus_parent() const { return _focus_parent; }
        inline void set_focus_parent(Widget *parent) { _focus_parent = parent; }
        inline detail::RectData &get_rect() { return _rect; }
        inline const detail::RectData &get_rect() const { return _rect; }
        inline void set_rect_tag_id(u32 tag_id) { _rect.id.tag_id = tag_id; }
        inline bool is_visible() const { return (widget_flags & WidgetFlagBits::visible); }
        inline void set_visible() { set_widget_flag(WidgetFlagBits::visible); }
        inline void unset_visible() { unset_widget_flag(WidgetFlagBits::visible); }
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
        inline const amal::vec2 &inline_size() const { return _inline_size; }
        virtual amal::vec2 requested_size() const { return _requested_size; }
        virtual amal::vec2 style_size() const { return requested_size(); }
        inline bool fill_width() const { return is_size_fill(requested_size().x); }
        inline bool fill_height() const { return is_size_fill(requested_size().y); }
        inline void set_position(const amal::vec2 &pos)
        {
            if (_rect.bounds.offset.x == pos.x && _rect.bounds.offset.y == pos.y) return;
            _rect.bounds.offset = pos;
            reset_external_draw_cull_state();
            detail::get_context().dirty_flags |= DirtyFlagBits::hit_rect_update;
        }
        virtual void translate(const amal::vec2 &delta)
        {
            set_position(position() + delta);
            detail::get_context().dirty_flags |= DirtyFlagBits::hit_rect_update;
        }
        inline void set_size(const amal::vec2 &size)
        {
            if (_inline_size == size) return;
            _inline_size = size;
            set_requested_size(resolve_style_size_from_inline(_inline_size, _requested_size));
        }
        inline void set_layout_size(const amal::vec2 &size)
        {
            const amal::vec2 resolved = {is_size_concrete(size.x) ? size.x : 0.0f,
                                         is_size_concrete(size.y) ? size.y : 0.0f};
            if (_rect.bounds.size.x == resolved.x && _rect.bounds.size.y == resolved.y) return;
            const bool width_changed = _rect.bounds.size.x != resolved.x;
            if (width_changed) _layout_dirty_flags |= layout_dirty_width;
            if (_rect.bounds.size.y != resolved.y) _layout_dirty_flags |= layout_dirty_height;
            _rect.bounds.size = resolved;
            reset_external_draw_cull_state();
            detail::get_context().dirty_flags |= DirtyFlagBits::hit_rect_update;
        }
        inline const amal::vec2 &required_size() const { return _required_size; }
        inline void set_required_size(const amal::vec2 &size)
        {
            _required_size = {amal::max(size.x, 0.0f), amal::max(size.y, 0.0f)};
            _layout_dirty_flags &= ~layout_dirty_measure;
            _layout_dirty_flags |= layout_measure_updated;
        }
        inline void apply_style_layout(const Style &style) { apply_style_layout(style, style.mask()); }
        inline void apply_style_layout(const Style &style, detail::StylePropertyFlags mask)
        {
            mask &= style.mask();
            amal::vec2 next_style_size = _requested_size;
            if (mask & detail::StylePropertiesBits::width) next_style_size.x = style.width();
            if (mask & detail::StylePropertiesBits::height) next_style_size.y = style.height();
            const amal::vec2 next_size = resolve_style_size_from_inline(_inline_size, next_style_size);
            if (next_size != _requested_size) set_requested_size(next_size);
        }
        inline amal::vec2 resolve_layout_size_from_required() const
        {
            amal::vec2 out = _rect.bounds.size;
            if ((!is_width_fixed() && !fill_width()) || out.x <= 0.0f) out.x = _required_size.x;
            if ((!is_height_fixed() && !fill_height()) || out.y <= 0.0f) out.y = _required_size.y;
            return out;
        }
        inline bool is_width_fixed() const { return is_size_static_layout(requested_size().x); }
        inline bool is_height_fixed() const { return is_size_static_layout(requested_size().y); }
        inline bool is_fixed() const { return is_width_fixed() && is_height_fixed(); }
        inline bool is_hittable() const { return widget_flags & WidgetFlagBits::hittable; }
        inline bool can_emit_hit(const DrawCtx &ctx) const { return is_hittable() && ctx.is_hit_allowed; }
        inline bool is_read_only() const { return widget_flags & WidgetFlagBits::read_only; }
        inline bool is_disabled() const { return widget_flags & WidgetFlagBits::disabled; }
        inline PostFxChain *post_fx_chain() const { return _post_fx_chain; }
        inline PostEffect *post_effect() const { return _post_fx_chain ? _post_fx_chain->post_effect : nullptr; }
        inline const void *post_data() const { return _post_fx_chain ? _post_fx_chain->post_data : nullptr; }
        inline u32 post_id() const { return _post_fx_chain ? _post_fx_chain->id : AUIK_INVALID_POST_EFFECT_DATA_ID; }
        AUIK_EXPORT PostFxChain *add_post_effect(PostEffect *effect, const void *instance_data = nullptr,
                                                 const void *post_data = nullptr);
        AUIK_EXPORT bool remove_post_effect(PostFxChain *chain);
        AUIK_EXPORT void clear_post_effects();
        inline void set_post_effect(PostEffect *effect, const void *data = nullptr)
        {
            clear_post_effects();
            if (effect) add_post_effect(effect, nullptr, data);
        }
        inline void set_post_id(u32 id)
        {
            if (_post_fx_chain) _post_fx_chain->id = id;
        }
        inline void set_post_data(const void *data)
        {
            if (!_post_fx_chain) _post_fx_chain = acul::alloc<PostFxChain>();
            _post_fx_chain->post_data = data;
        }
        inline void clear_post_effect() { clear_post_effects(); }
        inline void set_widget_flag(WidgetFlagBits::enum_type flag) { widget_flags |= flag; }
        inline void unset_widget_flag(WidgetFlagBits::enum_type flag) { widget_flags &= ~flag; }
        inline void set_read_only() { set_widget_flag(WidgetFlagBits::read_only); }
        inline void set_mutable() { unset_widget_flag(WidgetFlagBits::read_only); }
        inline void set_disabled() { set_widget_flag(WidgetFlagBits::disabled); }
        inline void unset_disabled() { unset_widget_flag(WidgetFlagBits::disabled); }
        inline void unset_disbled() { unset_disabled(); }
        inline void set_configurable() { set_widget_flag(WidgetFlagBits::configurable); }
        inline void unset_configurable() { unset_widget_flag(WidgetFlagBits::configurable); }
        inline Viewport *viewport() const { return _viewport; }
        inline void set_viewport(Viewport *viewport) { _viewport = viewport; }
        inline void attach_to_viewport(Viewport *viewport) { set_viewport(viewport); }
        inline const amal::vec2 &root_viewport_origin() const { return _root_viewport_origin; }
        inline void set_root_viewport_origin(const amal::vec2 &origin) { _root_viewport_origin = origin; }
        inline bool has_event_handler(EventFlagBits::enum_type event_flag) const { return event_flags & event_flag; }
        inline void set_event_flags(EventFlags value)
        {
            requested_event_flags = value;
            sync_widget_flags();
        }
        inline void add_event_flags(EventFlags value)
        {
            requested_event_flags |= value;
            sync_widget_flags();
        }
        inline void remove_event_flags(EventFlags value)
        {
            requested_event_flags &= ~value;
            sync_widget_flags();
        }
        inline u16 clip_id() const { return _rect.clip_id; }
        inline void set_clip_id(u16 id) { _rect.clip_id = id; }
        inline void ensure_own_clip_rect(const amal::vec4 &rect)
        {
            if (_rect.clip_id == 0xFFFFu) _rect.clip_id = auik::push_clip_rect(rect);
            else update_clip_rect(_rect.clip_id, rect);
        }

        virtual void reset_clip_rect_records() { _rect.clip_id = 0xFFFFu; }
        virtual void rebuild_clip_rects() {}
        virtual void reset_draw_records() { reset_external_draw_cull_state(); }
        virtual void sync_widget_flags() { sync_widget_flags(resolve_event_flags(requested_event_flags)); }

        // Setters only change local widget state. Use these explicit synchronization points when an attached
        // widget must immediately apply a changed style or rebuild its layout/draw data.
        inline void sync_widget_style() { detail::request_style_refresh(this); }
        inline void sync_widget(StyleUpdateFlags flags) { detail::request_widget_refresh(this, flags); }

        void update_draw_commands(DrawReasonFlags reason = DrawReasonBits::none)
        {
            if (reason & (DrawReasonBits::full_redraw | DrawReasonBits::record))
            {
                _external_draw_culled = false;
                _external_draw_invalidated = false;
            }
            DrawCtx draw_ctx{reason};
            draw_local(draw_ctx);
        }

        void invalidate_draw_commands(DrawReasonFlags reason = DrawReasonBits::none)
        {
            DrawCtx draw_ctx{reason | DrawReasonBits::invalidate};
            draw_local(draw_ctx);
        }

        void draw_local(DrawCtx &owner_ctx)
        {
            if (!is_visible() && !(owner_ctx.reason & DrawReasonBits::invalidate)) return;
            DrawCtx local_ctx(owner_ctx);
            if (is_disabled()) local_ctx.is_hit_allowed = false;
            if (_post_fx_chain)
            {
                auto *tail = _post_fx_chain;
                while (tail->next) tail = tail->next;
                tail->next = local_ctx.post_fx_chain;
                local_ctx.post_fx_chain = _post_fx_chain;
                draw(local_ctx);
                tail->next = nullptr;
                return;
            }
            draw(local_ctx);
        }

        virtual void update_layout_min_size_force() { set_required_size(size()); }
        void invalidate_layout_measure()
        {
            for (Widget *widget = this; widget; widget = widget->_parent)
            {
                widget->_layout_dirty_flags |= layout_dirty_measure;
                widget->_layout_dirty_flags &= ~layout_measure_updated;
            }
        }

        inline bool update_layout_min_size()
        {
            if (!layout_measure_required(true)) return false;
            update_layout_min_size_force();
            return true;
        }
        // min_size_known means update_layout_min_size_force() has already been run for this
        // subtree in the current measure/arrange transaction.
        virtual void update_layout(bool min_size_known = true)
        {
            (void)min_size_known;
            _layout_dirty_flags = layout_dirty_none;
            detail::get_context().dirty_flags |= DirtyFlagBits::hit_rect_update;
        }
        virtual u32 get_depth_requirement() const { return 1u; }
        AUIK_EXPORT virtual void update_depth(const amal::vec2 &depth_range);
        AUIK_EXPORT virtual void back_hit_depth();
        AUIK_EXPORT virtual void restore_hit_depth();
        virtual StyleUpdateFlags update_style() = 0;
        StyleUpdateFlags update_style_invalidated()
        {
            const StyleUpdateFlags flags = update_style();
            if (flags & (StyleUpdateFlagBits::layout | StyleUpdateFlagBits::parent_layout)) invalidate_layout_measure();
            return flags;
        }
        // A container may suppress event-driven style updates for a child that it does not currently draw.
        virtual bool accepts_child_style_update(const Widget *) const { return true; }
        virtual void draw(DrawCtx &) = 0;

        // Controls whether a mouse press on this hit target replaces ctx.focus_id.
        // Return false for technical targets that should preserve the current focus leaf.
        virtual bool accepts_focus_on_mouse_press(ElementID) const { return true; }
        // Allows selected widgets to receive hover transitions while another element is being dragged.
        virtual bool accepts_drag_hover(ElementID drag_id, ElementID hover_id) const
        {
            (void)drag_id;
            (void)hover_id;
            return false;
        }
        // Relative drags may continue with a hidden cursor after reaching a host-window boundary.
        // The input dispatcher owns the raw-mouse lifecycle; widgets only opt into that policy.
        virtual bool allows_unbounded_drag() const { return false; }

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
            _widget_state_flags |= WidgetStateFlagBits::attached;
            // Embedded implementation widgets participate in the live tree without owning an id-map entry.
            if (!(widget_flags & WidgetFlagBits::attachable)) return;
            auto &ctx = detail::get_context();
            ctx.id_map.emplace(id(), this);
            if (is_disabled() && !post_effect()) apply_disabled_post_effect();
            if (const auto attach_cb = detail::get_default_widget_attach_cb()) attach_cb(this);
        }
        virtual void on_detach()
        {
            if (!(widget_flags & WidgetFlagBits::attachable))
            {
                _widget_state_flags &= ~WidgetStateFlagBits::attached;
                return;
            }
            auto &map = detail::get_context().id_map;
            auto it = map.find(id());
            if (it != map.end() && it->second == this) map.erase(it);
            _widget_state_flags &= ~WidgetStateFlagBits::attached;
        }
        virtual void on_scroll(const amal::vec2 &delta) {}
        virtual void on_focus(bool focused) {}
        virtual void on_hover(HoverState state) {}
        virtual void on_click(MouseKey key, KeyPressState state, u32 click_count) {}
        virtual void on_drag(const amal::vec2 &delta, KeyPressState state) {}
        virtual void on_drop(ElementID drag_id, ElementID drop_id) {}
        virtual void on_key(Key key, KeyPressState state, KeyMode mods) {}
        virtual void on_char_input(u32 char_code, u32 count) {}
        virtual void on_change(ChangeEvent &) {}

        inline void dispatch_hover(HoverState state)
        {
            HoverEvent e{};
            e.state = state;
            e.target = detail::get_style_selector_id();
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
            const auto &io = detail::get_context().io;
            e.target = io.clicked_id;
            e.drag_id = io.drag_id;
            e.mods = io.active_mods;
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
            const auto &io = detail::get_context().io;
            e.origin = io.drag_id;
            e.mods = io.active_mods;
            if (_user_bind && _user_bind->on_drag_fn)
            {
                _user_bind->on_drag_fn(e);
                if (e.is_prevented_default()) return;
            }
            on_drag(delta, state);
        }

        inline void dispatch_drop(ElementID drag_id, ElementID drop_id)
        {
            DropEvent e{};
            e.drag_id = drag_id;
            e.drop_id = drop_id;
            if (_user_bind && _user_bind->on_drop_fn)
            {
                _user_bind->on_drop_fn(e);
                if (e.is_prevented_default()) return;
            }
            on_drop(drag_id, drop_id);
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

        inline bool mark_changed()
        {
            invalidate_layout_measure();
            ChangeEvent event{};
            event.target = id();
            for (Widget *widget = this; widget; widget = widget->parent())
                widget->dispatch_change(event);
            return event.is_prevented_default();
        }

    protected:
        // required_size() and the arranged bounds are independent layout caches. These
        // flags keep their validity in Widget so every widget can use the same contract.
        bool layout_measure_required(bool min_size_known) const
        {
            return !min_size_known || (_layout_dirty_flags & layout_dirty_measure);
        }
        bool layout_width_dirty() const { return _layout_dirty_flags & layout_dirty_width; }
        bool layout_height_dirty() const { return _layout_dirty_flags & layout_dirty_height; }
        bool layout_measure_was_updated() const { return _layout_dirty_flags & layout_measure_updated; }
        void invalidate_layout_arrange() { _layout_dirty_flags |= layout_dirty_width | layout_dirty_height; }

        inline bool dispatch_change()
        {
            ChangeEvent e{};
            e.target = id();
            e.current_target = id();
            return dispatch_change(e);
        }

        inline bool dispatch_change(ChangeEvent &e)
        {
            e.current_target = id();
            if (_user_bind && _user_bind->on_change_fn)
                _user_bind->on_change_fn(e);
            if (!e.is_prevented_default()) on_change(e);
            return e.is_prevented_default();
        }

        inline void redraw_external(bool has_record, DrawReasonFlags update_reason = DrawReasonBits::external)
        {
            if (has_record) update_draw_commands(update_reason);
            else update_draw_commands(DrawReasonBits::external | DrawReasonBits::record);
            detail::get_context().dirty_flags |= DirtyFlagBits::redraw;
        }

        AUIK_EXPORT void sync_widget_flags(EventFlags flags);

        WidgetFlags _synced_widget_flags = WidgetFlagBits::none;
        PostFxChain *_post_fx_chain = nullptr;
        PostFxChain *_disabled_post_fx = nullptr;

        virtual void on_disabled_changed(bool disabled) { (void)disabled; }

        static inline amal::vec2 resolve_style_size_from_inline(const amal::vec2 &inline_size,
                                                                const amal::vec2 &style_size)
        {
            return {is_size_inherit(inline_size.x) ? style_size.x : inline_size.x,
                    is_size_inherit(inline_size.y) ? style_size.y : inline_size.y};
        }

        inline void set_requested_size(const amal::vec2 &size)
        {
            if (_requested_size == size) return;
            _requested_size = size;
            set_layout_size(size);
        }

        enum LayoutDirtyBits : u8
        {
            layout_dirty_none = 0,
            layout_dirty_measure = 1u << 0u,
            layout_dirty_width = 1u << 1u,
            layout_dirty_height = 1u << 2u,
            layout_measure_updated = 1u << 3u,
            layout_dirty_all = layout_dirty_measure | layout_dirty_width | layout_dirty_height
        };

        u32 _id;
        Widget *_parent = nullptr;
        Widget *_focus_parent = nullptr;
        amal::vec2 _depth_range{0.0f, 1.0f};
        amal::vec2 _root_viewport_origin{0.0f, 0.0f};
        amal::vec2 _inline_size = AUIK_SIZE_INHERIT;
        amal::vec2 _requested_size{AUIK_SIZE_X_FILL, AUIK_SIZE_Y_FIT};
        Viewport *_viewport = nullptr;
        detail::RectData _rect{};
        amal::vec2 _required_size{0.0f, 0.0f};
        u8 _layout_dirty_flags = layout_dirty_all;
        StyleState _style_state = StyleState::normal;
        bool _external_draw_culled = false;
        bool _external_draw_invalidated = false;
        WidgetStateFlags _widget_state_flags = WidgetStateFlagBits::none;
        UserBind *_user_bind = nullptr;

    private:
        friend struct detail::WidgetStateAccess;

        inline EventFlags resolve_event_flags(EventFlags flags) const
        {
            if (is_disabled()) return EventFlagBits::none;
            if (is_read_only()) return EventFlagBits::none;
            return flags;
        }

        inline void apply_disabled_post_effect()
        {
            if (!_disabled_post_fx) _disabled_post_fx = add_post_effect(get_disabled_post_effect());
        }

        inline void restore_pre_disabled_post_effect()
        {
            if (!_disabled_post_fx) return;
            remove_post_effect(_disabled_post_fx);
            _disabled_post_fx = nullptr;
        }

        void append_user_data_node(WidgetUserData *node)
        {
            if (!_user_data)
            {
                _user_data = node;
                return;
            }
            auto *tail = _user_data;
            while (tail->pNext) tail = tail->pNext;
            tail->pNext = node;
        }

        void clear_user_data();
        WidgetUserData *_user_data = nullptr;
    };

    namespace detail
    {
        struct WidgetStateAccess
        {
            static void set_transient(Widget *widget, bool transient)
            {
                if (transient) widget->_widget_state_flags |= WidgetStateFlagBits::transient;
                else widget->_widget_state_flags &= ~WidgetStateFlagBits::transient;
            }
        };

        inline RootWidgetUserData *root_widget_user_data(Widget *widget)
        {
            auto *head = widget ? widget->user_data_head() : nullptr;
            if (!head || head->tag_id != AUIK_UD_ROOT_DATA) return nullptr;
            return static_cast<RootWidgetUserData *>(head->handle);
        }

        inline const RootWidgetUserData *root_widget_user_data(const Widget *widget)
        {
            auto *head = widget ? widget->user_data_head() : nullptr;
            if (!head || head->tag_id != AUIK_UD_ROOT_DATA) return nullptr;
            return static_cast<const RootWidgetUserData *>(head->handle);
        }

        inline DepthZone root_widget_depth_zone(const Widget *widget)
        {
            auto *data = root_widget_user_data(widget);
            return data ? *data : DepthZone::work;
        }

        AUIK_EXPORT void setup_root_window(Widget *widget);
        AUIK_EXPORT void teardown_root_window(Widget *widget);
    } // namespace detail

    AUIK_EXPORT void assign_next_depth(const amal::vec2 &parent_range, amal::vec2 &dst_range);

    struct DepthCursor
    {
        amal::vec2 range{0.0f, 1.0f};
        u32 slots = 1u;
        u32 used = 0u;

        DepthCursor() = default;
        DepthCursor(const amal::vec2 &range, u32 slots) : range(range), slots(slots ? slots : 1u) {}

        amal::vec2 next(u32 requirement = 1u)
        {
            requirement = requirement ? requirement : 1u;
            if (used >= slots) return {range.y, range.y};
            const u32 begin = used;
            const u32 end = (used + requirement < slots) ? used + requirement : slots;
            used = end;

            const f32 span = range.y - range.x;
            const f32 step = span / static_cast<f32>(slots);
            return {range.x + step * static_cast<f32>(begin), range.x + step * static_cast<f32>(end)};
        }

        amal::vec2 shared() const { return range; }
    };

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
        return theme && theme->has_state_style(widget.get_rect().id.tag_id, widget.id(), parent_id, state);
    }

    inline StyleState resolve_widget_visual_state(const Widget &widget, StyleState state)
    {
        return has_widget_state_style(widget, state) ? state : StyleState::normal;
    }

    inline Widget *resolve_parent_layout_update_target(Widget *widget)
    {
        if (!widget) return nullptr;
        Widget *target = widget->parent() ? widget->parent() : widget;
        while (target->parent() && !target->is_fixed()) target = target->parent();
        return target;
    }

    inline bool style_extra_equal(const Style &left, const Style &right)
    {
        const StyleExtra *left_node = left.extra();
        const StyleExtra *right_node = right.extra();
        while (left_node || right_node)
        {
            if (!left_node || !right_node) return false;
            if (left_node->id != right_node->id) return false;
            if (left_node->id == AUIK_STYLE_EXTRA_ALIGN)
            {
                auto *left_align = static_cast<const StyleExtraAlign *>(left_node->data);
                auto *right_align = static_cast<const StyleExtraAlign *>(right_node->data);
                if (!left_align || !right_align || left_align->flags != right_align->flags) return false;
            }
            else if (left_node->id == AUIK_STYLE_EXTRA_TEXT)
            {
                auto *left_text = static_cast<const StyleExtraText *>(left_node->data);
                auto *right_text = static_cast<const StyleExtraText *>(right_node->data);
                if (!left_text || !right_text || left_text->wrap != right_text->wrap ||
                    left_text->overflow != right_text->overflow)
                    return false;
            }
            else if (left_node->data != right_node->data) return false;
            left_node = left_node->next;
            right_node = right_node->next;
        }
        return true;
    }

    inline bool apply_hover_style_state(Widget &widget, HoverState state)
    {
        if (widget.style_state() == StyleState::active || widget.style_state() == StyleState::focus) return false;
        if (state == HoverState::leave) return widget.set_style_state(StyleState::normal);
        if (!has_widget_state_style(widget, StyleState::hover)) return false;
        return widget.set_style_state(StyleState::hover);
    }

    inline StyleUpdateFlags make_style_update_flags(
        const Style &prev_style, const Style &next_style,
        detail::StylePropertyFlags property_mask = acul::flag_traits<detail::StylePropertiesBits>::all_flags)
    {
        const auto union_mask = (prev_style.mask() | next_style.mask()) & property_mask;
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
        if ((union_mask & detail::StylePropertiesBits::width) && prev_style.width() != next_style.width())
            changed |= detail::StylePropertiesBits::width;
        if ((union_mask & detail::StylePropertiesBits::height) && prev_style.height() != next_style.height())
            changed |= detail::StylePropertiesBits::height;
        if ((union_mask & detail::StylePropertiesBits::min_width) && prev_style.min_width() != next_style.min_width())
            changed |= detail::StylePropertiesBits::min_width;
        if ((union_mask & detail::StylePropertiesBits::min_height) &&
            prev_style.min_height() != next_style.min_height())
            changed |= detail::StylePropertiesBits::min_height;
        if ((union_mask & detail::StylePropertiesBits::extra) && !style_extra_equal(prev_style, next_style))
            changed |= detail::StylePropertiesBits::extra;

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
        const bool invalidate_previous = detail::get_context().dirty_flags & DirtyFlagBits::styles;
        const StyleID prev_style_id = invalidate_previous ? Theme::STYLE_ID_INVALID : selector.id;
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

} // namespace auik
