#pragma once

#include <awin/window.hpp>
#include "../menu/menu.hpp"
#include "../tab/tab.hpp"

namespace auik
{
    class Titlebar;

    struct TitlebarStyle
    {
        ImVec4 hover_color;
        ImVec2 submenu_padding;
        ImVec4 tab_active_color;
        ImVec4 tab_background_color;
    };

    class APPLIB_API TitlebarDecorator
    {
        enum DecorationIcons
        {
            ICON_MIN,
            ICON_MAX,
            ICON_RESTORE,
            ICON_CLOSE,
            ICON_APP,
            ICON_MAX_VALUE
        };

        enum class ControlState
        {
            idle,
            hover,
            active
        };

        enum class ControlArea
        {
            none,
            client,
            caption,
            min,
            max,
            close
        };

    public:
        struct Style
        {
            ImVec4 close_color;
            Icon *icons[ICON_MAX_VALUE];
        } style;

        TitlebarDecorator(const ImVec2 &control_size,
                          void (*bind_events_call)(acul::events::dispatcher *, awin::Window &, Titlebar *))
            : _controls{{ControlState::idle, ControlArea::min},
                        {ControlState::idle, ControlArea::max},
                        {ControlState::idle, ControlArea::close}},
              _control_size(control_size),
              bind_events_call(bind_events_call)
        {
        }

        void render_controls(awin::Window &window, const TitlebarStyle &style, f32 caption_width);

        void render_app_icon()
        {
            auto *icon_app = style.icons[ICON_APP];
            const struct ImVec2 app_pos =
                ImGui::GetCursorScreenPos() + ImVec2(0, (_control_size.y - icon_app->size().y) * 0.5f);
            icon_app->render(app_pos);
        }

        void process_drag(awin::Window &window)
        {
            if (ImGui::IsMouseDragging(ImGuiMouseButton_Left))
            {
                acul::point2D<i32> cursor_pos = window.cursor_position();
                acul::point2D<i32> window_pos = window.position();
                acul::point2D<i32> new_pos{window_pos.x + cursor_pos.x - _drag_offset.x,
                                           window_pos.y + cursor_pos.y - _drag_offset.y};
                window.position(new_pos);
            }
            else
                _drag_offset = window.cursor_position();
        }

        ImVec2 control_size() const { return _control_size; }

        void bind_events(acul::events::dispatcher *ed, awin::Window &window, Titlebar *self)
        {
            if (bind_events_call) bind_events_call(ed, window, self);
        }

    private:
        struct ControlButton
        {
            ControlState state;
            ControlArea area;
        } _controls[3];
        ControlArea _active_area{ControlArea::none};
        ImVec2 _control_size;
        acul::point2D<i32> _drag_offset;

        void (*bind_events_call)(acul::events::dispatcher *, awin::Window &, Titlebar *);
        friend void bind_events(acul::events::dispatcher *, awin::Window &, Titlebar *);
    };

    class APPLIB_API Titlebar final : public MenuBar
    {
    public:
        class Decorator;
        TitlebarStyle style;
        TabBar tabbar;

        Titlebar(awin::Window &window, MenuBar *menubar, const TabBar &tabbar);

        ~Titlebar() { acul::release(_decorator); }

        bool is_client_decorated() const { return _decorator != nullptr; }

        void bind_events(acul::events::dispatcher *ed);

        virtual void render() override;

        void bind_decoration_style(const TitlebarDecorator::Style &style) { _decorator->style = style; }

    private:
        awin::Window &_window;
        f32 _caption_width;
        f32 _client_width;
        TitlebarDecorator *_decorator;

        friend void bind_events(acul::events::dispatcher *, awin::Window &, Titlebar *);
    };

    inline void Titlebar::bind_events(acul::events::dispatcher *ed)
    {
        tabbar.bind_events();
        if (_decorator) _decorator->bind_events(ed, _window, this);
    }
} // namespace auik