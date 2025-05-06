#ifndef UIKIT_WIDGETS_TAB_H
#define UIKIT_WIDGETS_TAB_H

#include <acul/disposal_queue.hpp>
#include <acul/enum.hpp>
#include <acul/event.hpp>
#include "../selectable/selectable.hpp"

namespace uikit
{
    class APPLIB_API TabItem : public Selectable
    {
    public:
        struct FlagBits
        {
            enum enum_type : u8
            {
                None = 0x0,
                Closable = 0x1,
                Unsaved = 0x2
            };
            using flag_bitmask = std::true_type;
        };

        using Flags = acul::flags<FlagBits>;

        u64 id;

        TabItem(u64 id, const acul::string &label, const std::function<void()> &render_callback = nullptr,
                Flags flags = FlagBits::None, f32 rounding = 0.0f)
            : Selectable({label, false, rounding, ImGuiSelectableFlags_AllowItemOverlap, {0.0f, 0.0f}, true}),
              id(id),
              _render_callback(render_callback),
              _tab_flags(flags)
        {
        }

        bool render_item();

        std::function<void()> &render_callback() { return _render_callback; }
        void render_callback(const std::function<void()> &render_callback) { _render_callback = render_callback; }

        ImVec2 calculate_item_size();

        Flags &flags() { return _tab_flags; }

        const Flags flags() const { return _tab_flags; }

    private:
        std::function<void()> _render_callback;
        Flags _tab_flags;
    };

    class APPLIB_API TabBar : public Widget
    {
    public:
        struct event_id;
        acul::vector<TabItem> items;
        u8 active_index = 0;

        struct FlagBits
        {
            enum enum_type : u8
            {
                None = 0x0,
                Reorderable = 0x1,
                Scrollable = 0x2,
            };
            using flag_bitmask = std::true_type;
        };

        using Flags = acul::flags<FlagBits>;

        struct Style
        {
            ImVec2 size;
            f32 scroll_offset_lr;
            f32 scroll_offset_rl;
        };

        TabBar(const acul::string &id, acul::events::dispatcher *ed, acul::disposal_queue &disposal_queue,
               const acul::vector<TabItem> &items, Flags flags = FlagBits::None, const Style &style = {})
            : Widget(id),
              items(items),
              ed(ed),
              _disposal_queue(disposal_queue),
              _flags(flags),
              _height(0.0f),
              _style(style)
        {
        }

        ~TabBar() { ed->unbind_listeners(this); }

        virtual void render() override;

        void bind_events();

        void size(ImVec2 size) { _style.size = size; }

        ImVec2 size() const { return _style.size; }

        f32 height() const { return _height; }

        f32 avaliable_width() const { return _avaliable_width > 0 ? _avaliable_width : 0; }

        TabItem &active_tab() { return items[active_index]; }

        bool empty() const { return items.empty(); }

        void new_tab(const TabItem &tab);

        bool remove_tab(const TabItem &tab);

        bool is_hovered() const { return _is_hovered; }

    private:
        acul::events::dispatcher *ed;
        acul::disposal_queue &_disposal_queue;
        bool _is_hovered{false};
        Flags _flags;
        f32 _avaliable_width{0};
        f32 _height;
        Style _style;
        struct DragData
        {
            std::optional<acul::vector<TabItem>::iterator> it{std::nullopt};
            ImVec2 pos;
            f32 pos_offset{0};
            f32 offset{0};
        } _drag;

        bool render_tab(acul::vector<TabItem>::iterator &begin, int index);
        void render_dragged();
        void render_combobox();
    };

    struct TabBar::event_id
    {
        enum : u64
        {
            None = 0x0,
            Close = 0x33C84BEDF7097480,
            Switched = 0x26274151A3D94FF5,
            Changed = 0x3E025D2B3AC893E4
        };
    };

    struct TabCloseEvent : public acul::events::event
    {
        TabBar *tabbar;
        TabItem tab;
        bool confirmed;
        bool create_on_empty;
        bool batch;

        TabCloseEvent(TabBar *tabbar, const TabItem &tab, bool confirmed = false, bool create_on_empty = true,
                      bool batch = false)
            : event(TabBar::event_id::Close),
              tabbar(tabbar),
              tab(tab),
              confirmed(confirmed),
              create_on_empty(create_on_empty),
              batch(batch)
        {
        }
    };

    struct TabSwitchEvent : public acul::events::event
    {
        TabBar *tabbar;
        acul::vector<TabItem>::iterator prev;
        acul::vector<TabItem>::iterator current;

        TabSwitchEvent(TabBar *tabbar, const acul::vector<TabItem>::iterator &prev,
                       const acul::vector<TabItem>::iterator &current)
            : event(TabBar::event_id::Switched), tabbar(tabbar), prev(prev), current(current)
        {
        }
    };

    struct TabChangeEvent : public acul::events::event
    {
        TabBar *tabbar;
        acul::string display_name;
        TabItem::Flags flags;

        TabChangeEvent(TabBar *tabbar, const acul::string &display_name = {},
                       TabItem::Flags flags = TabItem::FlagBits::None)
            : event(TabBar::event_id::Changed), tabbar(tabbar), display_name(display_name), flags(flags)
        {
        }
    };

} // namespace uikit
#endif