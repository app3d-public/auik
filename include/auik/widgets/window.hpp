#pragma once

#include "containers.hpp"
#include "menu.hpp"

#define AUIK_TAG_WINDOW          0xB4382179u
#define AUIK_TAG_VIEWPORT_WINDOW 0x965E5D42u
#define AUIK_TAG_WINDOW_HEADER   0x663566BEu
#define AUIK_TAG_WINDOW_CONTENT  0x2E80C7A1u

namespace auik
{
    class PopupMenu;
    class RubberBand;

    struct RubberBandCommitEvent
    {
        RubberBand *band = nullptr;
        KeyMode mods{};
    };

    struct WindowFlagBits
    {
        enum enum_type
        {
            none = 0x0,
            resizable = 0x1,
            movable = 0x2,
            decorated = 0x4,
            docked = 0x8,
            dockable = 0x10,
            scrollable = 0x20,
            no_scrollbar_x = 0x40,
            no_scrollbar_y = 0x80,
            rubber_band = 0x100
        };

        using flag_bitmask = std::true_type;
    };

    using WindowFlags = acul::flags<WindowFlagBits>;
    enum class WindowMenuType : u8
    {
        menu_bar,
        popup
    };

    constexpr inline WindowFlags get_decorated_window_flags()
    {
        return WindowFlagBits::resizable | WindowFlagBits::movable | WindowFlagBits::decorated |
               WindowFlagBits::scrollable | WindowFlagBits::dockable;
    }
    constexpr inline WindowFlags get_fixed_window_flags()
    {
        return WindowFlagBits::decorated | WindowFlagBits::scrollable;
    }
    constexpr inline WindowFlags get_popup_window_flags()
    {
        return WindowFlagBits::movable | WindowFlagBits::scrollable;
    }

    class Window : public Widget
    {
    private:
        DrawBlock *_content_block = nullptr;

    public:
        acul::vector<Widget *> &children;
        WindowFlags window_flags;

        AUIK_EXPORT Window(u32 id, StringView title, const amal::rect &bounds, WindowFlags window_flags,
                           WidgetFlags widget_flags);
        AUIK_EXPORT ~Window() override;

        AUIK_EXPORT void clear_children();
        AUIK_EXPORT void add_child(Widget *child, ChildLayoutFlags layout = default_child_layout_flags());
        AUIK_EXPORT void add_children(const acul::vector<Widget *> &new_children);
        AUIK_EXPORT void
        set_menu(MenuProxy &&menu,
                 PFN_window_menu_suffix_create window_menu_suffix_create = detail::get_default_menu_suffix_create_cb());
        AUIK_EXPORT void
        set_menu(detail::MenuBase *menu,
                 PFN_window_menu_suffix_create window_menu_suffix_create = detail::get_default_menu_suffix_create_cb());
        AUIK_EXPORT Widget *take_menu_widget();
        AUIK_EXPORT void set_menu_widget(Widget *menu);
        AUIK_EXPORT void override_content_clip_rect(const amal::vec4 &rect);
        const amal::vec2 &min_size() const { return _min_size; }
        void set_auto_size(bool width = true, bool height = true)
        {
            _auto_size = {width, height};
            set_size({width ? AUIK_SIZE_X_FIT : size().x, height ? AUIK_SIZE_Y_FIT : size().y});
        }
        void set_auto_position(bool x = true, bool y = true)
        {
            _auto_position = {x, y};
            _auto_position_resolved = false;
        }
        void begin_external_move_drag()
        {
            _move_drag_active = true;
            _resize_dir = {0, 0};
        }
        MenuProxy *get_menu() { return _menu ? &_menu : nullptr; }
        const MenuProxy *get_menu() const { return _menu ? &_menu : nullptr; }
        AUIK_EXPORT bool is_popup_menu() const;
        AUIK_EXPORT PopupMenu *header_popup_menu() const;
        RubberBand *rubber_band() const { return _rubber_band; }
        RubberBand *get_rubber_band() const { return _rubber_band; }
        void on_rubber_band_commit(acul::unique_function<void(RubberBandCommitEvent &)> callback)
        {
            _on_rubber_band_commit = std::move(callback);
        }
        const acul::string &title() const { return _title; }
        AUIK_EXPORT void set_title(StringView title);
        AUIK_EXPORT const Text *title_text() const;
        DrawBlock *content_block() { return _content_block; }
        const DrawBlock *content_block() const { return _content_block; }
        const acul::vector<ChildLayoutFlags> &child_layouts() const { return _content_block->child_layouts(); }
        using value_type = Widget *;
        using iterator = acul::vector<value_type>::iterator;
        using const_iterator = acul::vector<value_type>::const_iterator;

        iterator begin() { return children.begin(); }
        iterator end() { return children.end(); }
        const_iterator begin() const { return children.begin(); }
        const_iterator end() const { return children.end(); }
        const_iterator cbegin() const { return children.cbegin(); }
        const_iterator cend() const { return children.cend(); }
        bool empty() const { return children.empty(); }
        size_t child_size() const { return children.size(); }
        value_type front() { return children.front(); }
        value_type back() { return children.back(); }
        const value_type front() const { return children.front(); }
        const value_type back() const { return children.back(); }
        value_type *data() { return children.data(); }
        const value_type *data() const { return children.data(); }
        void set_window_style_tag(u32 tag_id)
        {
            _window_style_tag = tag_id;
            _window_style = {Theme::STYLE_ID_INVALID, tag_id};
        }
        u32 window_style_tag() const { return _window_style_tag; }

        AUIK_EXPORT virtual StyleUpdateFlags update_style() override;
        AUIK_EXPORT void reset_clip_rect_records() override;
        AUIK_EXPORT void rebuild_clip_rects() override;
        AUIK_EXPORT void reset_draw_records() override;
        AUIK_EXPORT virtual void translate(const amal::vec2 &delta) override;
        AUIK_EXPORT u32 get_depth_requirement() const override;
        AUIK_EXPORT virtual void update_depth(const amal::vec2 &depth_range) override;
        AUIK_EXPORT virtual void back_hit_depth() override;
        AUIK_EXPORT virtual void restore_hit_depth() override;
        AUIK_EXPORT virtual void update_layout_min_size() override;
        AUIK_EXPORT void update_layout(bool min_size_known) override;
        inline void sync_widget_flags() override
        {
            Widget::sync_widget_flags(requested_event_flags | _window_event_flags);
        }
        AUIK_EXPORT virtual void draw(DrawCtx &ctx) override;
        u32 signature() const override { return AUIK_TAG_WINDOW; }

    private:
        acul::string _title;
        amal::vec2 _min_size{0.0f, 0.0f};
        amal::bvec2 _auto_size{false, false};
        amal::bvec2 _auto_position{false, false};
        bool _auto_position_resolved = false;
        class WindowHeader *_header = nullptr;
        MenuProxy _menu{};
        PopupMenu *_default_header_menu = nullptr;
        PFN_window_menu_suffix_create _window_menu_suffix_create = nullptr;
        u32 _header_menu_suffix_group = 0xFFFFu;
        RubberBand *_rubber_band = nullptr;
        acul::unique_function<void(RubberBandCommitEvent &)> _on_rubber_band_commit = nullptr;
        amal::ivec2 _resize_dir{0, 0};
        bool _move_drag_active = false;
        detail::RectData _resize_hit_rect{};
        f32 _resize_hit_depth = 0.0f;
        DrawDataID _resize_hit_draw_id{};
        DrawDataID _bg_draw_id{};
        EventFlags _window_event_flags = EventFlagBits::none;
        u32 _window_style_tag = AUIK_STYLE_TAG_WINDOW;
        StyleSelector _window_style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_WINDOW};

        AUIK_EXPORT virtual bool accepts_focus_on_mouse_press(ElementID hit_id) const override;
        virtual u16 content_clip_id() const override { return clip_id(); }
        virtual amal::vec4 get_content_clip_rect() const override
        {
            if (_content_block && _content_block->content_clip_id() != 0xFFFFu)
                return get_clip_rect(_content_block->content_clip_id());
            if (clip_id() != 0xFFFFu) return get_clip_rect(clip_id());
            return get_main_viewport_rect();
        }
        AUIK_EXPORT virtual void on_attach() override;
        AUIK_EXPORT virtual void on_detach() override;
        void sync_window_event_flags(bool scroll, bool hover);
        void sync_rubber_band();
        void commit_rubber_band();
        void redraw_decorations(DrawReasonFlags reason = DrawReasonBits::none);

        AUIK_EXPORT virtual void on_scroll(const amal::vec2 &delta) override;
        AUIK_EXPORT virtual void on_drag(const amal::vec2 &delta, KeyPressState state) override;
        AUIK_EXPORT virtual void on_focus(bool focused) override;
        AUIK_EXPORT virtual void on_hover(HoverState state) override;
        AUIK_EXPORT virtual void on_click(MouseKey key, KeyPressState state, u32 click_count) override;
        u32 effective_window_style_tag() const;
        const Style &resolved_window_style() const;
        PopupMenu *ensure_header_popup_menu();
        void install_header_menu_suffix();
        void remove_header_menu_suffix();
        void sync_header_popup_menu();
    };

    inline Window *make_decorated_window(u32 id, StringView title = "",
                                         const amal::rect &bounds = {AUIK_POS_IGNORE, AUIK_SIZE_AUTO})
    {
        constexpr WidgetFlags widget_flags = WidgetFlagBits::visible | WidgetFlagBits::attachable |
                                             WidgetFlagBits::configurable | WidgetFlagBits::hittable;
        return acul::alloc<Window>(id, title, bounds, get_decorated_window_flags(), widget_flags);
    }

    inline Window *make_popup_window(u32 id, const amal::rect &bounds = {AUIK_POS_IGNORE, AUIK_SIZE_AUTO})
    {
        constexpr WidgetFlags widget_flags = WidgetFlagBits::visible | WidgetFlagBits::attachable |
                                             WidgetFlagBits::configurable | WidgetFlagBits::hittable;
        return acul::alloc<Window>(id, "", bounds, get_popup_window_flags(), widget_flags);
    }

    inline Window *make_viewport_window(u32 id, StringView title = "")
    {
        constexpr WidgetFlags widget_flags = WidgetFlagBits::visible | WidgetFlagBits::attachable |
                                             WidgetFlagBits::configurable | WidgetFlagBits::hittable;
        auto *window = acul::alloc<Window>(id, title, amal::rect{AUIK_POS_IGNORE, AUIK_SIZE_FILL},
                                           get_popup_window_flags(), widget_flags);
        window->set_rect_tag_id(AUIK_TAG_VIEWPORT_WINDOW);
        return window;
    }

    namespace streams
    {
        extern AUIK_EXPORT const umbf::streams::Stream window;
    }
} // namespace auik
