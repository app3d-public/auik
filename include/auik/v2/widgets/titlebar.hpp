#pragma once

#include <auik/v2/detail/fwd.hpp>
#include <auik/v2/detail/gpu_context.hpp>
#include <auik/v2/widgets/menubar.hpp>
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

namespace auik::v2
{
    inline bool is_titlebar_customizable()
    {
#ifdef _WIN32
        return true;
#else
        return false;
#endif
    }

    class APPLIB_API Titlebar final : public Widget
    {
    public:
        APPLIB_API explicit Titlebar(u32 id = AUIK_TAG_TITLEBAR,
                                     WidgetFlags widget_flags = get_default_widget_flags() | WidgetFlagBits::fixed_layout);
        ~Titlebar() override;

        void set_show_icon(bool value);
        bool show_icon() const { return _show_icon; }
        void set_leading_count(u32 count);
        u32 leading_count() const { return _leading_count; }
        void add_child(Widget *child);
        void add_children(const acul::vector<Widget *> &children);

        StyleUpdateFlags update_style() override;
        void update_layout_min_size() override;
        void update_layout(bool min_size_known) override;
        void update_depth(const amal::vec2 &depth_range) override;
        void back_hit_depth() override;
        void restore_hit_depth() override;
        void reset_clip_rect_records() override;
        void rebuild_clip_rects() override;
        void draw(DrawCtx &ctx) override;
        void translate(const amal::vec2 &delta) override;
        void on_attach() override;
        void on_detach() override;
        void set_titlebar_state(struct TitlebarState *state) { _state = state; }
        struct TitlebarState *titlebar_state() const { return _state; }

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
        bool _show_icon = false;
        struct TitlebarState *_state = nullptr;
    };

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

    APPLIB_API bool adjust_window_by_titlebar_settings(Titlebar *titlebar, TitlebarCreateFlags flags,
                                                       const FontRegistry &fonts);

    inline Titlebar *make_titlebar(u32 id = AUIK_TAG_TITLEBAR)
    {
        return acul::alloc<Titlebar>(id, get_default_widget_flags() | WidgetFlagBits::fixed_layout);
    }

    inline MenuBar *make_titlebar_menu_bar(u32 id, acul::vector<acul::string> items = {})
    {
        auto *menu_bar = acul::alloc<MenuBar>(id, std::move(items));
        menu_bar->widget_flags &= ~WidgetFlagBits::fixed_layout;
        menu_bar->set_menu_style_tag(AUIK_STYLE_TAG_TITLEBAR_MENU_BAR);
        menu_bar->set_menu_item_style_tag(AUIK_STYLE_TAG_TITLEBAR_MENU_ITEM);
        menu_bar->set_popup_depth_mode(MenuBar::PopupDepthMode::root_overlay);
        return menu_bar;
    }
} // namespace auik::v2
