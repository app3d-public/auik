#pragma once

#include <acul/functional/unique_function.hpp>
#include <acul/string/string.hpp>
#include "../theme.hpp"
#include "containers.hpp"
#include "menubar.hpp"

#define AUIK_TAG_WINDOW               0xB4382179
#define AUIK_TAG_VIEWPORT_WINDOW      0x965E5D42u
#define AUIK_TAG_WINDOW_HEADER        0x663566BE
#define AUIK_TAG_WINDOW_CONTENT       0x2E80C7A1u
#define AUIK_TAG_WINDOW_BUILDER       0x31D9C0B7u

namespace auik
{
    class PopupMenu;
    class RubberBand;
    class WindowBuilder;

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

    constexpr inline WindowFlags get_default_window_flags()
    {
        return WindowFlagBits::resizable | WindowFlagBits::movable | WindowFlagBits::decorated |
               WindowFlagBits::scrollable | WindowFlagBits::dockable;
    }

    constexpr inline WindowFlags get_decorated_window_flags() { return get_default_window_flags(); }

    constexpr inline WindowFlags get_fixed_window_flags()
    {
        return get_default_window_flags() & ~WindowFlagBits::resizable;
    }

    constexpr inline WindowFlags get_popup_window_flags()
    {
        return get_default_window_flags() &
               ~(WindowFlagBits::decorated | WindowFlagBits::resizable | WindowFlagBits::dockable);
    }

    class Window : public Widget
    {
    private:
        DrawBlock *_content_block = nullptr;

    public:
        acul::vector<Widget *> &children;
        WindowFlags window_flags;

        AUIK_EXPORT Window(u32 id, acul::string title = "", const amal::rect &bounds = {},
                          WindowFlags window_flags = get_default_window_flags(),
                          WidgetFlags widget_flags = get_default_widget_flags() | WidgetFlagBits::hittable,
                          Widget *parent = nullptr);
        AUIK_EXPORT ~Window() override;

        AUIK_EXPORT void clear_children();
        AUIK_EXPORT void add_child(Widget *child, ChildLayoutFlags layout = default_child_layout_flags());
        AUIK_EXPORT void add_children(const acul::vector<Widget *> &new_children);
        AUIK_EXPORT void set_menu(MenuBar *menu);
        AUIK_EXPORT Widget *take_menu_widget();
        AUIK_EXPORT void set_menu_widget(Widget *menu);
        AUIK_EXPORT void set_window_builder(WindowBuilder *builder);
        WindowBuilder *window_builder() const { return _window_builder; }
        AUIK_EXPORT void override_content_clip_rect(const amal::vec4 &rect);
        void set_min_size(const amal::vec2 &value) { _min_size = value; }
        const amal::vec2 &min_size() const { return _min_size; }
        void set_auto_size(bool width = true, bool height = true)
        {
            _auto_size = {width, height};
            sync_fixed_bounds_flag();
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
        const acul::string &title() const { return _title; }
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
            set_rect_tag_id(tag_id);
        }

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
        AUIK_EXPORT virtual void update_layout(bool min_size_known) override;
        AUIK_EXPORT virtual void draw(DrawCtx &ctx) override;

    private:
        acul::string _title;
        amal::vec2 _min_size{0.0f, 0.0f};
        amal::bvec2 _auto_size{false, false};
        amal::bvec2 _auto_position{false, false};
        bool _auto_position_resolved = false;
        class WindowHeader *_header = nullptr;
        MenuProxy _menu{};
        WindowBuilder *_window_builder = nullptr;
        PopupMenu *_default_header_menu = nullptr;
        bool _header_menu_suffix_installed = false;
        RubberBand *_rubber_band = nullptr;
        amal::ivec2 _resize_dir{0, 0};
        bool _move_drag_active = false;
        detail::RectData _resize_hit_rect{};
        f32 _resize_hit_depth = 0.0f;
        DrawDataID _resize_hit_draw_id{};
        DrawDataID _bg_draw_id{};
        u32 _window_style_tag = AUIK_STYLE_TAG_WINDOW;
        StyleSelector _window_style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_WINDOW};

        virtual bool accepts_focus_on_mouse_press(ElementID hit_id) const override;
        virtual u16 content_clip_id() const override { return clip_id(); }
        virtual amal::vec4 get_content_clip_rect() const override
        {
            return _content_block ? _content_block->get_content_clip_rect() : get_clip_rect(clip_id());
        }
        virtual void on_attach() override;
        virtual void on_detach() override;
        void sync_rubber_band();
        void redraw_decorations(DrawReasonFlags reason = DrawReasonBits::none);

        virtual void on_scroll(const amal::vec2 &delta) override;
        virtual void on_drag(const amal::vec2 &delta, KeyPressState state) override;
        virtual void on_focus(bool focused) override;
        virtual void on_hover(HoverState state) override;
        virtual void on_click(MouseKey key, KeyPressState state, u32 click_count) override;
        void add_child_with_flags(Widget *child, ChildLayoutFlags layout);
        void sync_fixed_bounds_flag();
        u32 effective_window_style_tag() const;
        const Style &resolved_window_style() const;
        PopupMenu *ensure_header_popup_menu();
        void install_header_menu_suffix();
        void remove_header_menu_suffix();
        void sync_header_menu_suffix();
        void sync_header_popup_menu();
    };

    class WindowBuilder : public Widget
    {
    public:
        using PFN_menu_suffix_create = acul::unique_function<void(WindowBuilder *, Window *, MenuBar *)>;

        AUIK_EXPORT explicit WindowBuilder(u32 id, Widget *parent = nullptr);
        AUIK_EXPORT ~WindowBuilder() override;

        AUIK_EXPORT Window *add_window(Window *window);
        void set_menu_type(WindowMenuType type) { _menu_type = type; }
        WindowMenuType menu_type() const { return _menu_type; }
        bool is_menu_popup() const { return _menu_type == WindowMenuType::popup; }
        bool is_menu_bar() const { return _menu_type == WindowMenuType::menu_bar; }
        AUIK_EXPORT void set_on_menu_suffix_create_cb(PFN_menu_suffix_create callback);
        bool has_menu_suffix() const { return static_cast<bool>(_on_menu_suffix_create); }
        AUIK_EXPORT void build_menu_suffix(Window *window, MenuBar *menu);

        StyleUpdateFlags update_style() override { return StyleUpdateFlagBits::none; }
        void update_layout_min_size() override { set_required_size({0.0f, 0.0f}); }
        AUIK_EXPORT void update_layout(bool min_size_known = true) override;
        u32 get_depth_requirement() const override { return 1u; }
        void draw(DrawCtx &) override {}

    private:
        PFN_menu_suffix_create _on_menu_suffix_create = nullptr;
        WindowMenuType _menu_type = WindowMenuType::menu_bar;
    };

    inline WindowBuilder *make_window_builder(u32 id, Widget *parent = nullptr)
    {
        return acul::alloc<WindowBuilder>(id, parent);
    }

    inline Window *make_decorated_window(u32 id, const acul::string &title = "", const amal::rect &bounds = {},
                                         WidgetFlags widget_flags = get_default_widget_flags() |
                                                                    WidgetFlagBits::hittable,
                                         Widget *parent = nullptr)
    {
        return acul::alloc<Window>(id, title, bounds, get_decorated_window_flags(), widget_flags, parent);
    }

    inline Window *make_popup_window(u32 id, const amal::rect &bounds = {},
                                     WidgetFlags widget_flags = get_default_widget_flags() | WidgetFlagBits::hittable,
                                     Widget *parent = nullptr)
    {
        return acul::alloc<Window>(id, "", bounds, get_popup_window_flags(), widget_flags, parent);
    }

    inline Window *make_viewport_window(u32 id, const acul::string &title = "",
                                        WidgetFlags widget_flags = get_default_widget_flags() |
                                                                   WidgetFlagBits::hittable,
                                        Widget *parent = nullptr)
    {
        auto *window = acul::alloc<Window>(id, title, amal::rect{AUIK_POS_IGNORE, AUIK_SIZE_FILL},
                                           get_decorated_window_flags(), widget_flags, parent);
        window->set_rect_tag_id(AUIK_TAG_VIEWPORT_WINDOW);
        return window;
    }
} // namespace auik
