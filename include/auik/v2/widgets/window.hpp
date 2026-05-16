#pragma once

#include <acul/memory/alloc.hpp>
#include <acul/string/string.hpp>
#include "../theme.hpp"
#include "detail/scrollbar.hpp"
#include "widget.hpp"

#define AUIK_TAG_WINDOW        0xB4382179
#define AUIK_TAG_WINDOW_HEADER 0x663566BE
namespace auik::v2
{
    class MenuBar;
    class RubberBand;

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
    enum class WindowChildLayout : u8
    {
        block = 0,
        inline_layout
    };

    constexpr inline WindowFlags get_default_window_flags()
    {
        return WindowFlagBits::resizable | WindowFlagBits::movable | WindowFlagBits::decorated |
               WindowFlagBits::scrollable;
    }

    constexpr inline WindowFlags get_decorated_window_flags() { return get_default_window_flags(); }

    constexpr inline WindowFlags get_fixed_window_flags()
    {
        return get_default_window_flags() & ~WindowFlagBits::resizable;
    }

    constexpr inline WindowFlags get_popup_window_flags()
    {
        return get_default_window_flags() & ~(WindowFlagBits::decorated | WindowFlagBits::resizable);
    }

    class Window : public Widget
    {
    public:
        WindowFlags window_flags;
        acul::vector<Widget *> children;

        APPLIB_API Window(u32 id, acul::string title = "", const amal::rect &bounds = {},
                          WindowFlags window_flags = get_default_window_flags(),
                          WidgetFlags widget_flags = get_default_widget_flags() | WidgetFlagBits::hittable,
                          Widget *parent = nullptr);
        APPLIB_API ~Window() override;

        APPLIB_API void clear_children();
        APPLIB_API void add_child(Widget *child, WindowChildLayout layout = WindowChildLayout::block);
        APPLIB_API void add_children(const acul::vector<Widget *> &new_children);
        APPLIB_API void set_menu_bar(MenuBar *menu_bar);
        APPLIB_API void override_content_clip_rect(const amal::vec4 &rect);
        void set_min_size(const amal::vec2 &value) { _min_size = value; }
        const amal::vec2 &min_size() const { return _min_size; }
        MenuBar *menu_bar() const { return _menu_bar; }
        RubberBand *rubber_band() const { return _rubber_band; }
        RubberBand *get_rubber_band() const { return _rubber_band; }
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
            _window_style = {Theme::STYLE_ID_INVALID, tag_id};
            set_rect_tag_id(tag_id);
        }

        virtual StyleUpdateFlags update_style() override;
        void rebuild_clip_rects() override;
        virtual void translate(const amal::vec2 &delta) override;
        virtual void update_depth(const amal::vec2 &depth_range) override;
        virtual void update_layout_min_size() override;
        virtual void update_layout(bool min_size_known) override;
        virtual void draw(DrawCtx &ctx) override;

    private:
        DrawDataID _bg;
        f32 _header_height = 0.0f;
        amal::vec2 _min_size{0.0f, 0.0f};
        amal::vec2 _content_offset{0.0f};
        u16 _content_clip_id = 0xFFFFu;
        amal::vec4 _content_clip_rect{0.0f, 0.0f, 0.0f, 0.0f};
        StyleSelector _window_style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_WINDOW};
        class WindowHeader *_header = nullptr;
        MenuBar *_menu_bar = nullptr;
        RubberBand *_rubber_band = nullptr;
        detail::Scrollbar *_scrollbar_x = nullptr;
        detail::Scrollbar *_scrollbar_y = nullptr;
        detail::HitboxZone _resize_zone = detail::HitboxZoneBits::none;
        bool _move_drag_active = false;
        acul::vector<WindowChildLayout> _child_layouts;

        virtual bool accepts_focus_on_mouse_press(detail::ElementID hit_id) const override;
        virtual u16 content_clip_id() const override { return _content_clip_id; }
        virtual amal::vec4 get_content_clip_rect() const override { return _content_clip_rect; }
        virtual void on_attach() override;
        virtual void on_detach() override;
        void sync_rubber_band();
        void redraw_decorations(DrawReasonFlags reason = DrawReasonBits::none);

        virtual void on_scroll(const amal::vec2 &delta) override;
        virtual void on_drag(const amal::vec2 &delta, KeyPressState state) override;
        virtual void on_focus(bool focused) override;
        virtual void on_hover(HoverState state) override;
        virtual void on_click(MouseKey key, KeyPressState state, u32 click_count) override;
        void relayout_children(f32 available_width, const amal::vec2 &content_inset);
    };

    inline Window *make_decorated_window(u32 id, const acul::string &title = "", const amal::rect &bounds = {},
                                         WidgetFlags widget_flags = get_default_widget_flags() |
                                                                    WidgetFlagBits::hittable,
                                         Widget *parent = nullptr)
    {
        return acul::alloc<Window>(id, title, bounds, get_decorated_window_flags(), widget_flags, parent);
    }

    inline Window *make_fixed_window(u32 id, const acul::string &title = "", const amal::rect &bounds = {},
                                     WidgetFlags widget_flags = get_default_widget_flags() | WidgetFlagBits::hittable,
                                     Widget *parent = nullptr)
    {
        return acul::alloc<Window>(id, title, bounds, get_fixed_window_flags(), widget_flags, parent);
    }

    inline Window *make_popup_window(u32 id, const amal::rect &bounds = {},
                                     WidgetFlags widget_flags = get_default_widget_flags() | WidgetFlagBits::hittable,
                                     Widget *parent = nullptr)
    {
        return acul::alloc<Window>(id, "", bounds, get_popup_window_flags(), widget_flags, parent);
    }
} // namespace auik::v2
