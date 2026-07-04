#pragma once

#include "detail/dock_base.hpp"

#define AUIK_TAG_DOCK_LAYOUT 0xD8A1014Au

namespace auik
{
    using DockLayoutNodeID = u32;

    struct DockLayoutNodeSettings : detail::DockBaseNodeSettings
    {
        DockLayoutNodeSettings() = default;
        DockLayoutNodeSettings(const amal::vec2 &node_size, const amal::vec2 &node_min_size = {0.0f, 0.0f})
        {
            size = node_size;
            min_size = node_min_size;
        }
    };

    class DockLayout final : public Widget
    {
    public:
        AUIK_EXPORT explicit DockLayout(u32 id, const amal::vec2 &inline_size, WidgetFlags widget_flags);
        AUIK_EXPORT ~DockLayout() override;

        DockLayoutNodeID root_node() const { return _dock.root_node(); }
        AUIK_EXPORT DockLayoutNodeID create_split(DockLayoutNodeID parent, amal::axis axis,
                                                  DockLayoutNodeSettings settings = {});
        AUIK_EXPORT DockLayoutNodeID create_leaf(DockLayoutNodeID parent, DockLayoutNodeSettings settings = {});
        AUIK_EXPORT void set_split_axis(DockLayoutNodeID node, amal::axis axis);
        AUIK_EXPORT void set_node_settings(DockLayoutNodeID node, DockLayoutNodeSettings settings);
        AUIK_EXPORT void add_child(DockLayoutNodeID node, Widget *child,
                                   ChildLayoutFlags layout = default_child_layout_flags());
        AUIK_EXPORT bool remove_child(DockLayoutNodeID node, Widget *child);
        AUIK_EXPORT void clear_node(DockLayoutNodeID node);
        AUIK_EXPORT void clear();

        AUIK_EXPORT StyleUpdateFlags update_style() override;
        AUIK_EXPORT void update_layout_min_size() override;
        AUIK_EXPORT void update_layout(bool min_size_known) override;
        AUIK_EXPORT void translate(const amal::vec2 &delta) override;
        AUIK_EXPORT void reset_clip_rect_records() override;
        AUIK_EXPORT void rebuild_clip_rects() override;
        AUIK_EXPORT void reset_draw_records() override;
        AUIK_EXPORT u32 get_depth_requirement() const override;
        AUIK_EXPORT void update_depth(const amal::vec2 &depth_range) override;
        AUIK_EXPORT void draw(DrawCtx &ctx) override;
        AUIK_EXPORT void on_attach() override;
        AUIK_EXPORT void on_detach() override;
        u16 content_clip_id() const override { return clip_id(); }
        amal::vec4 get_content_clip_rect() const override { return get_clip_rect(clip_id()); }
        u32 signature() const override { return AUIK_TAG_DOCK_LAYOUT; }

    private:
        struct Item
        {
            Widget *widget = nullptr;
            ChildLayoutFlags layout = default_child_layout_flags();
        };

        struct Policy
        {
            using Owner = DockLayout;
            using ItemWidget = Widget;

            static Widget *item_widget(const Item &item) { return item.widget; }
            static amal::vec2 item_required_size(const Item &item)
            { return item.widget ? item.widget->required_size() : amal::vec2{0.0f, 0.0f}; }
            static void attach_item(DockLayout *owner, Item &item);
            static void detach_item(DockLayout *owner, Item &item);
            static void update_item_min_size(DockLayout *owner, Item &item);
            static void layout_item(DockLayout *owner, const amal::rect &bounds, Item &item);
        };

        detail::DockBase<Item, Policy> _dock;
    };

    inline DockLayout *make_dock_layout(u32 id, const amal::vec2 &inline_size = AUIK_SIZE_INHERIT)
    {
        return acul::alloc<DockLayout>(id, inline_size, WidgetFlagBits::visible | WidgetFlagBits::attachable |
                                                            WidgetFlagBits::configurable);
    }
} // namespace auik
