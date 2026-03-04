#pragma once

#include "scrollbar.hpp"
#include "theme.hpp"
#include "widget.hpp"

#define AUIK_TAG_WINDOW        0xB4382179
#define AUIK_TAG_WINDOW_HEADER 0x663566BE

namespace auik::v2
{
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
            fixed = 0x20,
            scrollable = 0x40,
            no_scrollbar_x = 0x80,
            no_scrollbar_y = 0x100
        };

        using flag_bitmask = std::true_type;
    };

    using WindowFlags = acul::flags<WindowFlagBits>;

    constexpr inline WindowFlags get_default_window_flags()
    {
        return WindowFlagBits::resizable | WindowFlagBits::movable | WindowFlagBits::decorated |
               WindowFlagBits::scrollable;
    }

    class APPLIB_API Window : public Widget
    {
    public:
        WindowFlags window_flags;
        acul::vector<Widget *> children;

        Window(u32 id, amal::vec2 pos = amal::vec2(0.0f), amal::vec2 size = amal::vec2(0.0f),
               WindowFlags window_flags = get_default_window_flags(),
               WidgetFlags widget_flags = get_default_widget_flags(), Widget *parent = nullptr);
        ~Window() override;

        void add_child(Widget *child);
        void add_children(const acul::vector<Widget *> &new_children);

        virtual void update_style() override;
        void rebuild_clip_rects() override;

    private:
        DrawDataID _bg;
        f32 _header_height = 0.0f;
        amal::vec2 _content_offset{0.0f};
        StyleSelector _window_style{0, AUIK_TAG_WINDOW};
        class WindowHeader *_header = nullptr;
        Scrollbar *_scrollbar_x = nullptr;
        Scrollbar *_scrollbar_y = nullptr;

        virtual void update_depth(const amal::vec2 &depth_range) override;
        virtual void update_layout() override;

        virtual void draw(DrawCtx &ctx) override;
        virtual void on_attach() override
        {
            auto &map = detail::get_context().id_map;
            map.emplace(id(), this);
            for (auto *child : children) map.emplace(child->id(), child);
        }
        
        virtual void on_detach() override
        {
            auto &map = detail::get_context().id_map;
            map.erase(id());
            for (auto *child : children) map.erase(child->id());
        }

        virtual void on_scroll(u32 tag_id, const amal::vec2 &delta) override;
    };
} // namespace auik::v2
