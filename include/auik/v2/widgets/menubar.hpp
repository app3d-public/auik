#pragma once

#include <acul/functional/unique_function.hpp>
#include "tabbar.hpp"
#include "window.hpp"

#define AUIK_TAG_WINDOW_MENU_BAR 0xC37C8F4Bu
#define AUIK_TAG_MENU_BAR_ITEM   0x62F8CAAAu
#define AUIK_TAG_MENU_POPUP      0x0EF2EE30u
#define AUIK_TAG_MENU_SHORTCUT   0x94020AC8u
#define AUIK_TAG_MAIN_MENU_BAR   0x5F42BA15u
#define AUIK_TAG_MAIN_MENU_ITEM  0xDD0667CDu

namespace auik::v2
{
    constexpr inline WidgetFlags get_default_menu_bar_flags()
    {
        return get_default_tab_bar_flags() | WidgetFlagBits::fixed;
    }

    class APPLIB_API MenuBar final : public TabBar
    {
    public:
        enum class PopupDepthMode : u8
        {
            workzone_overlay,
            root_overlay,
            root_overlay_next
        };

        class GroupRef;
        class ItemRef
        {
        public:
            ItemRef(MenuBar *owner, u32 element_id) : _owner(owner), _element_id(element_id) {}

            ItemRef &set_shortcut(acul::string shortcut)
            {
                if (_owner && _element_id != 0u) _owner->set_item_shortcut(_element_id, std::move(shortcut));
                return *this;
            }
            ItemRef &set_callback(acul::unique_function<void(ClickEvent &)> callback)
            {
                if (_owner && _element_id != 0u) _owner->ensure_item(_element_id).callback = std::move(callback);
                return *this;
            }
            ItemRef &set_selected(bool selected = true)
            {
                if (_owner && _element_id != 0u) _owner->set_item_selected(_element_id, selected);
                return *this;
            }
            GroupRef add_group(acul::vector<acul::string> items) const;
            u32 id() const { return _element_id; }

        private:
            MenuBar *_owner = nullptr;
            u32 _element_id = 0u;
        };

        class GroupRef
        {
        public:
            GroupRef(MenuBar *owner, acul::vector<u32> ids) : _owner(owner), _ids(std::move(ids)) {}

            size_t size() const { return _ids.size(); }
            bool empty() const { return _ids.empty(); }
            ItemRef operator[](size_t index) const
            {
                return index < _ids.size() ? ItemRef{_owner, _ids[index]} : ItemRef{nullptr, 0u};
            }
            const acul::vector<u32> &ids() const { return _ids; }

        private:
            MenuBar *_owner = nullptr;
            acul::vector<u32> _ids;
        };

        MenuBar(u32 id, acul::vector<acul::string> items = {}, amal::vec2 size = {0.0f, 0.0f},
                WidgetFlags widget_flags = get_default_menu_bar_flags(), Widget *parent = nullptr);
        ~MenuBar() override;

        StyleUpdateFlags update_style() override;
        void update_layout(bool min_size_known) override;
        void update_depth(const amal::vec2 &depth_range) override;
        void translate(const amal::vec2 &delta) override;
        void rebuild_clip_rects() override;
        void draw(DrawCtx &ctx) override;
        void on_click(MouseKey key, KeyPressState state, u32 click_count) override;
        void on_hover(HoverState state) override;
        void on_focus(bool focused) override;

        using MenuGroup = acul::vector<acul::string>;
        using MenuGroups = acul::vector<MenuGroup>;

        void set_item_shortcut(u32 element_id, acul::string shortcut);
        void set_item_selected(u32 element_id, bool selected = true);
        void set_selected_items(const acul::vector<u32> &element_ids);
        bool is_item_selected(u32 element_id) const;
        void set_menu_style_tag(u32 tag_id);
        void set_menu_item_style_tag(u32 tag_id);
        void set_popup_depth_mode(PopupDepthMode mode);
        const acul::vector<u32> &selected_item_ids() const { return _selected_item_ids; }
        ItemRef item(u32 element_id) { return ItemRef{this, element_id}; }
        ItemRef operator[](size_t index)
        {
            return index < _element_ids.size() ? item(_element_ids[index]) : ItemRef{nullptr, 0u};
        }
        GroupRef add_group(u32 element_id, acul::vector<acul::string> items)
        {
            return GroupRef{this, append_item_group(element_id, std::move(items))};
        }
        void draw_popups(DrawCtx &ctx);

    private:
        struct ItemData
        {
            u32 id = 0u;
            acul::string text;
            acul::string shortcut;
            acul::vector<u32> next;
            acul::unique_function<void(ClickEvent &)> callback = nullptr;
            bool separator = false;
        };

        class PopupItem;

        ItemData *find_item(u32 element_id);
        const ItemData *find_item(u32 element_id) const;
        ItemData &ensure_item(u32 element_id, const acul::string &text = {});
        ItemData &ensure_separator(u32 element_id);
        acul::vector<u32> append_item_group(u32 element_id, acul::vector<acul::string> items);
        acul::vector<u32> set_item_next(u32 element_id, acul::vector<acul::string> items);
        acul::vector<acul::vector<u32>> set_item_next_groups(u32 element_id, MenuGroups groups);
        acul::vector<u32> root_ids() const;
        void open_root(u32 element_id);
        bool open_next(u32 element_id, u32 depth, const amal::rect &anchor);
        void close_from_depth(u32 depth);
        void close_all();
        void set_active_root(u32 element_id);
        void clear_active_root();
        bool is_item_open(u32 element_id) const;
        bool is_popup_item_focused(u32 element_id) const;
        StyleState resolve_popup_item_state(Widget *child) const;
        void sync_popup_item_states();
        Widget *find_popup_child(u32 element_id) const;
        Widget *find_popup_child_at(u32 element_id, const amal::vec2 &pos, u32 *out_depth = nullptr) const;
        Widget *find_popup_child_by_transition_id(detail::ElementID id, bool prefer_hovered_at_cursor);
        u32 popup_child_item_id(const Widget *child) const;
        StyleUpdateFlags update_popup_transition(detail::ElementID id);
        Window *ensure_popup(u32 depth);
        void layout_popup(u32 depth, const amal::vec2 &pos, const acul::vector<u32> &ids);
        void refresh_menu_clip_rects();
        amal::vec4 get_popup_clip_rect(Window *popup) const;
        void refresh_popup_clip_rect(Window *popup);
        void request_redraw();
        bool draw_popup_child(const detail::ElementID &element_id, DrawCtx &ctx, bool prefer_cursor_hit);
        u16 get_layout_parent_clip_id() const override;
        amal::vec4 get_layout_parent_clip_rect() const override;

        DrawDataID _bg{};
        acul::vector<ItemData> _items;
        acul::vector<Window *> _popups;
        acul::vector<u32> _open_path;
        acul::vector<u32> _selected_item_ids;
        StyleSelector _menu_style{Theme::STYLE_ID_INVALID, AUIK_TAG_WINDOW_MENU_BAR};
        PopupDepthMode _popup_depth_mode = PopupDepthMode::workzone_overlay;
    };

    inline MenuBar::GroupRef MenuBar::ItemRef::add_group(acul::vector<acul::string> items) const
    {
        if (!_owner || _element_id == 0u) return MenuBar::GroupRef{nullptr, {}};
        return _owner->add_group(_element_id, std::move(items));
    }

    inline MenuBar *make_menu_bar(u32 id, acul::vector<acul::string> items = {})
    {
        return acul::alloc<MenuBar>(id, std::move(items));
    }

    inline MenuBar *make_main_menu_bar(u32 id, acul::vector<acul::string> items = {})
    {
        auto *menu_bar = acul::alloc<MenuBar>(id, std::move(items));
        menu_bar->set_menu_style_tag(AUIK_TAG_MAIN_MENU_BAR);
        menu_bar->set_menu_item_style_tag(AUIK_TAG_MAIN_MENU_ITEM);
        menu_bar->set_popup_depth_mode(MenuBar::PopupDepthMode::root_overlay);
        return menu_bar;
    }
} // namespace auik::v2
