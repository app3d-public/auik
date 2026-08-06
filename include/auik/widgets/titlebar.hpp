#pragma once

#include <auik/detail/fwd.hpp>
#include <auik/detail/gpu_context.hpp>
#include <auik/widgets/menu.hpp>
#include "widget.hpp"

#define AUIK_TAG_TITLEBAR                    0xDBECC2C6u
#define AUIK_TAG_TITLEBAR_ICON               0x5A7E4E91u
#define AUIK_TAG_TITLEBAR_LEADING_REGION     0xB20D45EFu
#define AUIK_TAG_WINDOW_CAPTION_BUTTON       0xEF0A4C09u
#define AUIK_TAG_WINDOW_CAPTION_CLOSE_BUTTON 0x561CF733u
#define AUIK_WINDOW_CAPTION_BTN_MIN          0
#define AUIK_WINDOW_CAPTION_BTN_MAX          1
#define AUIK_WINDOW_CAPTION_BTN_CLOSE        2
#define AUIK_WINDOW_CAPTION_BTN_COUNT        3
#define AUIK_TAG_TITLEBAR_MENU_BAR           0x6541ADB5u
#define AUIK_TAG_TITLEBAR_MENU_ITEM          0xD9F76C8Au
#define AUIK_ICON_CAP_MINIMIZE               0x7CB8CE8Du
#define AUIK_ICON_CAP_MAXIMIZE               0x28392EA5u
#define AUIK_ICON_CAP_RESTORE                0x2F89CAF9u
#define AUIK_ICON_CAP_CLOSE                  0x6D0C422D

namespace auik
{
    class Titlebar;

    // Flags for window creation, stored as u16 for memory efficiency.
    struct TitlebarCreateFlagBits
    {
        enum enum_type : u16
        {
            resizable = 0x0001,     // Allows window resizing.
            decorated = 0x0004,     // Adds decorations like title bar and borders.
            fullscreen = 0x0008,    // Enables fullscreen mode.
            minimize_box = 0x00010, // Includes a minimize button.
            maximize_box = 0x00020, // Includes a maximize button.
            hidden = 0x00040,       // Does not show the window on creation.
            minimized = 0x00080,    // Minimized.
            maximized = 0x00100,    // Maximized.
            extended_nc_area = 0x00200,
        };
        using flag_bitmask = std::true_type;
    };

    using TitlebarCreateFlags = acul::flags<TitlebarCreateFlagBits>;

    struct TitlebarState
    {
        Titlebar *titlebar = nullptr;
        TitlebarCreateFlags flags;
        f32 height = 0.0f;
        i32 padding = 0;
        acul::point2D<i32> frame;
        amal::vec2 caption_button_size{};
        bool caption_buttons[AUIK_WINDOW_CAPTION_BTN_COUNT]{};
        ImageButton *caption_button_widgets[AUIK_WINDOW_CAPTION_BTN_COUNT]{};
        f32 content_end_x = 0.0f;
        f32 caption_buttons_width = 0.0f;
        i32 hover_button = -1;
        i32 active_button = -1;
        void (*destroy)(TitlebarState *state) = nullptr;
    };

    class Titlebar final : public Widget
    {
        friend struct TitlebarStreamAccess;

    public:
        AUIK_EXPORT explicit Titlebar(u32 id = AUIK_TAG_TITLEBAR,
                                      WidgetFlags widget_flags = get_default_widget_flags());
        AUIK_EXPORT ~Titlebar() override;

        AUIK_EXPORT void set_show_icon(bool value);
        bool show_icon() const { return _show_icon; }
        AUIK_EXPORT void set_leading_count(u32 count);
        u32 leading_count() const { return _leading_count; }
        AUIK_EXPORT void set_menu_popup_viewport(Viewport *viewport);
        AUIK_EXPORT void add_child(Widget *child);
        AUIK_EXPORT void add_children(const acul::vector<Widget *> &children);

        AUIK_EXPORT StyleUpdateFlags update_style() override;
        AUIK_EXPORT void update_layout_min_size() override;
        AUIK_EXPORT void update_layout(bool min_size_known) override;
        AUIK_EXPORT void update_depth(const amal::vec2 &depth_range) override;
        AUIK_EXPORT void back_hit_depth() override;
        AUIK_EXPORT void restore_hit_depth() override;
        AUIK_EXPORT void reset_clip_rect_records() override;
        AUIK_EXPORT void rebuild_clip_rects() override;
        AUIK_EXPORT void reset_draw_records() override;
        AUIK_EXPORT void draw(DrawCtx &ctx) override;
        AUIK_EXPORT void translate(const amal::vec2 &delta) override;
        AUIK_EXPORT void on_attach() override;
        AUIK_EXPORT void on_detach() override;
        void set_titlebar_state(TitlebarState *state)
        {
            _state = state;
            if (_state) _state->titlebar = this;
        }
        TitlebarState *titlebar_state() const { return _state; }
        AUIK_EXPORT bool reload_caption_button_icons(f32 dpi, const FontRegistry &fonts);
        AUIK_EXPORT void set_caption_hover_button(i32 index);
        AUIK_EXPORT void set_caption_active_button(i32 index);
        virtual u32 signature() const override { return AUIK_TAG_TITLEBAR; }

    private:
        void ensure_icon_widget();
        void ensure_caption_buttons();

        DrawDataID _bg;
        DrawDataID _icon_bg;
        DrawDataID _leading_region_bg;
        StyleSelector _style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_TITLEBAR};
        StyleSelector _icon_style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_TITLEBAR_ICON};
        StyleSelector _leading_region_style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_TITLEBAR_LEADING_REGION};
        acul::vector<Widget *> _children;
        Image *_icon = nullptr;
        ImageButton *_caption_buttons[AUIK_WINDOW_CAPTION_BTN_COUNT]{};
        detail::ImageTextureResource _icon_texture{};
        f32 _caption_buttons_width = 0.0f;
        u32 _leading_count = 0u;
        amal::rect _leading_region_rect{};
        Viewport *_menu_popup_viewport = nullptr;
        bool _show_icon = false;
        TitlebarState *_state = nullptr;
    };

    AUIK_EXPORT bool adjust_window_by_titlebar_settings(Titlebar *titlebar, TitlebarCreateFlags flags,
                                                        const FontRegistry &fonts);

    inline Titlebar *make_titlebar(u32 id = AUIK_TAG_TITLEBAR)
    {
        constexpr WidgetFlags widget_flags = WidgetFlagBits::visible | WidgetFlagBits::attachable |
                                             WidgetFlagBits::configurable | WidgetFlagBits::hittable;
        return acul::alloc<Titlebar>(id, widget_flags);
    }

    inline MenuBar *make_titlebar_menubar(u32 id, const acul::vector<StringView> &items = {})
    {
        auto *menu_bar = acul::alloc<MenuBar>(id, items);
        menu_bar->set_menu_style_tag(AUIK_STYLE_TAG_TITLEBAR_MENU_BAR);
        menu_bar->set_menu_item_style_tag(AUIK_STYLE_TAG_TITLEBAR_MENU_ITEM);
        menu_bar->set_popup_depth_mode(MenuBar::PopupDepthMode::root_overlay);
        return menu_bar;
    }

    namespace streams
    {
        extern AUIK_EXPORT const umbf::streams::Stream titlebar;
    }
} // namespace auik
