#ifndef UIKIT_WIDGETS_TAB_H
#define UIKIT_WIDGETS_TAB_H

#include <astl/enum.hpp>
#include <astl/scalars.hpp>
#include <core/disposal_queue.hpp>
#include <core/event.hpp>
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
                none = 0x0,
                closable = 0x1,
                unsaved = 0x2
            };
            using flag_bitmask = std::true_type;
        };

        using Flags = astl::flags<FlagBits>;

        u64 id;

        TabItem(u64 id, const std::string &label, const std::function<void()> &onRender = nullptr,
                Flags flags = FlagBits::none, f32 rounding = 0.0f)
            : Selectable({label, false, rounding, ImGuiSelectableFlags_AllowItemOverlap, {0.0f, 0.0f}, true}),
              id(id),
              _onRender(onRender),
              _tabFlags(flags)
        {
        }

        bool renderItem();

        std::function<void()> &onRender() { return _onRender; }
        void onRender(const std::function<void()> &onRender) { _onRender = onRender; }

        ImVec2 calculateItemSize();

        Flags &flags() { return _tabFlags; }

        const Flags flags() const { return _tabFlags; }

    private:
        std::function<void()> _onRender;
        Flags _tabFlags;
    };

    class APPLIB_API TabBar : public Widget
    {
    public:
        struct event_id;
        astl::vector<TabItem> items;
        u8 activeIndex = 0;

        struct FlagBits
        {
            enum enum_type : u8
            {
                none = 0x0,
                reorderable = 0x1,
                scrollable = 0x2,
            };
            using flag_bitmask = std::true_type;
        };

        using Flags = astl::flags<FlagBits>;

        struct Style
        {
            ImVec2 size;
            f32 scrollOffsetLR;
            f32 scrollOffsetRL;
        };

        TabBar(const std::string &id, events::Manager *e, DisposalQueue &disposalQueue,
               const astl::vector<TabItem> &items, Flags flags = FlagBits::none, const Style &style = {})
            : Widget(id), items(items), e(e), _disposalQueue(disposalQueue), _flags(flags), _height(0.0f), _style(style)
        {
        }

        ~TabBar() { e->unbindListeners(this); }

        virtual void render() override;

        void bindEvents();

        void size(ImVec2 size) { _style.size = size; }

        ImVec2 size() const { return _style.size; }

        f32 height() const { return _height; }

        f32 avaliableWidth() const { return _avaliableWidth > 0 ? _avaliableWidth : 0; }

        TabItem &activeTab() { return items[activeIndex]; }

        bool empty() const { return items.empty(); }

        void newTab(const TabItem &tab);

        bool removeTab(const TabItem &tab);

        bool isHovered() const { return _isHovered; }

    private:
        events::Manager *e;
        DisposalQueue &_disposalQueue;
        bool _isHovered{false};
        Flags _flags;
        f32 _avaliableWidth{0};
        f32 _height;
        Style _style;
        struct DragData
        {
            std::optional<astl::vector<TabItem>::iterator> it{std::nullopt};
            ImVec2 pos;
            f32 posOffset{0};
            f32 offset{0};
        } _drag;

        bool renderTab(astl::vector<TabItem>::iterator &begin, int index);
        void renderDragged();
        void renderCombobox();
    };

    struct TabBar::event_id
    {
        enum : u64
        {
            none = 0x0,
            close = 0x33C84BEDF7097480,
            switched = 0x26274151A3D94FF5,
            changed = 0x3E025D2B3AC893E4
        };
    };

    struct TabCloseEvent : public events::IEvent
    {
        TabBar *tabbar;
        TabItem tab;
        bool confirmed;
        bool createOnEmpty;
        bool batch;

        TabCloseEvent(TabBar *tabbar, const TabItem &tab, bool confirmed = false, bool createOnEmpty = true,
                      bool batch = false)
            : IEvent(TabBar::event_id::close),
              tabbar(tabbar),
              tab(tab),
              confirmed(confirmed),
              createOnEmpty(createOnEmpty),
              batch(batch)
        {
        }
    };

    struct TabSwitchEvent : public events::IEvent
    {
        TabBar *tabbar;
        astl::vector<TabItem>::iterator prev;
        astl::vector<TabItem>::iterator current;

        TabSwitchEvent(TabBar *tabbar, const astl::vector<TabItem>::iterator &prev,
                       const astl::vector<TabItem>::iterator &current)
            : IEvent(TabBar::event_id::switched), tabbar(tabbar), prev(prev), current(current)
        {
        }
    };

    struct TabChangeEvent : public events::IEvent
    {
        TabBar *tabbar;
        std::string displayName;
        TabItem::Flags flags;

        TabChangeEvent(TabBar *tabbar, const std::string &displayName = "",
                       TabItem::Flags flags = TabItem::FlagBits::none)
            : IEvent(TabBar::event_id::changed), tabbar(tabbar), displayName(displayName), flags(flags)
        {
        }
    };

} // namespace uikit
#endif