#ifndef UIKIT_WIDGETS_TAB_H
#define UIKIT_WIDGETS_TAB_H

#include <core/disposal_queue.hpp>
#include <core/event/event.hpp>
#include <core/std/basic_types.hpp>
#include <core/std/enum.hpp>
#include "../icon/icon.hpp"
#include "../selectable/selectable.hpp"

namespace uikit
{
    class APPLIB_API TabItem : public Selectable
    {
    public:
        enum class FlagBits : u8
        {
            none = 0x0,
            closable = 0x1,
            unsaved = 0x2
        };

        using Flags = ::Flags<FlagBits>;

        TabItem(const std::string &id, const std::string &label, const std::function<void()> &onRender = nullptr,
                Flags flags = FlagBits::none, f32 rounding = 0.0f)
            : Selectable(label, false, rounding, ImGuiSelectableFlags_AllowItemOverlap, {0.0f, 0.0f}, true),
              _id(id),
              _onRender(onRender),
              _tabFlags(flags)
        {
        }

        TabItem(const std::string &id, const std::function<void()> &onRender = nullptr, Flags flags = FlagBits::none)
            : TabItem(id, id, onRender, flags)
        {
        }

        bool renderItem();

        std::function<void()> &onRender() { return _onRender; }
        void onRender(const std::function<void()> &onRender) { _onRender = onRender; }

        ImVec2 calculateItemSize();

        Flags &flags() { return _tabFlags; }

        const Flags flags() const { return _tabFlags; }

        std::string id() const { return _id; }

    private:
        std::string _id;
        std::function<void()> _onRender;
        Flags _tabFlags;
    };

    class APPLIB_API TabBar : public Widget
    {
    public:
        DArray<TabItem> items;
        u8 activeIndex = 0;

        enum class FlagBits : u8
        {
            none = 0x0,
            reorderable = 0x1,
            scrollable = 0x2,
        };

        using Flags = ::Flags<FlagBits>;

        struct Style
        {
            std::shared_ptr<Icon> comboIcon;
            ImVec2 size;
            f32 scrollOffsetLR;
            f32 scrollOffsetRL;
        };

        TabBar(const std::string &id, DisposalQueue &disposalQueue, const DArray<TabItem> &items,
               Flags flags = FlagBits::none, const Style &style = {}, bool mainTabbar = false)
            : Widget(id),
              items(items),
              _disposalQueue(disposalQueue),
              _flags(flags),
              _isMainTabbar(mainTabbar),
              _style(style)
        {
        }

        ~TabBar() { events::unbindListeners(this); }

        virtual void render() override;

        void bindEvents();

        void size(ImVec2 size) { _style.size = size; }

        ImVec2 size() const { return _style.size; }

        f32 avaliableWidth() const { return _avaliableWidth > 0 ? _avaliableWidth : 0; }

        TabItem &activeTab() { return items[activeIndex]; }

        bool empty() const { return items.empty(); }

        void newTab(const TabItem &tab);

        bool removeTab(const TabItem &tab);

    private:
        DisposalQueue &_disposalQueue;
        bool _isHovered{false};
        Flags _flags;
        bool _isMainTabbar;
        f32 _avaliableWidth{0};
        Style _style;
        struct DragData
        {
            std::optional<DArray<TabItem>::iterator> it{std::nullopt};
            ImVec2 pos;
            f32 posOffset{0};
            f32 offset{0};
        } _drag;

        bool renderTab(DArray<TabItem>::iterator &begin, int index);
        void renderDragged();
        void renderCombobox();
    };

    struct TabRemoveEvent : public events::Event
    {
        TabItem tab;
        bool confirmed;
        bool createOnEmpty;
        bool batch;

        TabRemoveEvent(const std::string &eventName, const TabItem &tab, bool confirmed = false,
                       bool createOnEmpty = true, bool batch = false)
            : Event(eventName), tab(tab), confirmed(confirmed), createOnEmpty(createOnEmpty), batch(batch)
        {
        }
    };

    struct TabChangeEvent : public events::Event
    {
        DArray<TabItem>::iterator prev;
        DArray<TabItem>::iterator current;

        TabChangeEvent(const std::string &eventName, const DArray<TabItem>::iterator &prev,
                       const DArray<TabItem>::iterator &current)
            : Event(eventName), prev(prev), current(current)
        {
        }
    };

} // namespace uikit

template <>
struct FlagTraits<uikit::TabItem::FlagBits>
{
    static constexpr bool isBitmask = true;
    static constexpr uikit::TabItem::Flags allFlags =
        uikit::TabItem::FlagBits::none | uikit::TabItem::FlagBits::closable | uikit::TabItem::FlagBits::unsaved;
};

template <>
struct FlagTraits<uikit::TabBar::FlagBits>
{
    static constexpr bool isBitmask = true;
    static constexpr uikit::TabBar::Flags allFlags =
        uikit::TabBar::FlagBits::none | uikit::TabBar::FlagBits::reorderable | uikit::TabBar::FlagBits::scrollable;
};

#endif