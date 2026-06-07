#pragma once

#include <acul/functional/unique_function.hpp>
#include "detail/popup_trigger.hpp"
#include "tabbar.hpp"

#define AUIK_TAG_WINDOW_MENU_BAR   0xC37C8F4Bu
#define AUIK_TAG_MENU_BAR_ITEM     0x62F8CAAAu
#define AUIK_TAG_MENU_POPUP        0x0EF2EE30u
#define AUIK_TAG_MENU_SHORTCUT     0x94020AC8u
#define AUIK_TAG_MAIN_MENU_BAR     0x5F42BA15u
#define AUIK_TAG_MAIN_MENU_ITEM    0xDD0667CDu
#define AUIK_TAG_POPUP_MENU        0x2AF466E1u
#define AUIK_TAG_POPUP_MENU_BUTTON 0x34EF928Au

namespace auik::v2
{
    class Window;

    constexpr inline WidgetFlags get_default_menu_bar_flags()
    {
        return get_default_tab_bar_flags() | WidgetFlagBits::fixed_layout;
    }

    class APPLIB_API MenuBar final : public TabBar
    {
        friend class PopupMenu;

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
            GroupRef add_group(const acul::vector<acul::string> &items) const;
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

        MenuBar(u32 id, acul::vector<acul::string> items = {}, amal::vec2 size = AUIK_SIZE_IGNORE,
                WidgetFlags widget_flags = get_default_menu_bar_flags(), Widget *parent = nullptr);
        ~MenuBar() override;

        StyleUpdateFlags update_style() override;
        void update_layout(bool min_size_known) override;
        void update_depth(const amal::vec2 &depth_range) override;
        void back_hit_depth() override;
        void restore_hit_depth() override;
        void translate(const amal::vec2 &delta) override;
        void reset_clip_rect_records() override;
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
        PopupDepthMode popup_depth_mode() const { return _popup_depth_mode; }
        void set_selected_enabled(bool value);
        bool selected_enabled() const { return _selected_enabled; }
        void sync_selection_mode_changes();
        u32 append_item(acul::string text, acul::unique_function<void(ClickEvent &)> callback = nullptr);
        bool open_root_items_at(const amal::rect &anchor, const acul::vector<u32> &ids);
        bool open_root_at(u32 element_id, const amal::rect &anchor);
        void close_all();
        void discard_popups();
        ItemRef item(u32 element_id) { return ItemRef{this, element_id}; }
        ItemRef operator[](size_t index)
        {
            return index < _element_ids.size() ? item(_element_ids[index]) : ItemRef{nullptr, 0u};
        }
        GroupRef add_group(u32 element_id, const acul::vector<acul::string> &items)
        {
            return GroupRef{this, append_item_group(element_id, std::move(items))};
        }
        void draw_popups(DrawCtx &ctx);

    private:
        friend class PopupMenu;

        struct ItemData
        {
            u32 id = 0u;
            acul::string text;
            acul::string shortcut;
            acul::vector<u32> next;
            acul::unique_function<void(ClickEvent &)> callback = nullptr;
            bool separator = false;
            bool selected = false;
            bool runtime_suffix = false;
            bool runtime_suffix_root = false;
            u32 runtime_suffix_group = 0u;
        };

        class PopupItem;
        struct RuntimeSuffixGroup
        {
            u32 count = 0u;
            bool empty() const { return count == 0u; }
            u32 size() const { return count; }
        };

        ItemData *find_item(u32 element_id);
        const ItemData *find_item(u32 element_id) const;
        ItemData &ensure_item(u32 element_id, const acul::string &text = {});
        ItemData &ensure_separator(u32 element_id);
        RuntimeSuffixGroup &ensure_runtime_suffix_group(u32 group_index);
        const RuntimeSuffixGroup *runtime_suffix_group(u32 group_index) const;
        void increment_runtime_suffix_group(u32 group_index);
        u32 push_root_suffix_group();
        u32 root_suffix_group_count() const { return static_cast<u32>(_runtime_suffix_groups.size()); }
        void pop_root_suffix_group();
        u32 append_root_suffix_item(acul::string text, acul::unique_function<void(ClickEvent &)> callback = nullptr);
        u32 append_root_suffix_separator();
        u32 append_root_suffix_item(u32 group_index, acul::string text,
                                    acul::unique_function<void(ClickEvent &)> callback = nullptr);
        u32 append_root_suffix_separator(u32 group_index);
        void erase_root_suffix_group(u32 group_index);
        bool has_root_suffix(u32 group_index) const;
        bool root_suffix_ends_with_separator(u32 group_index) const;
        acul::vector<u32> append_item_group(u32 element_id, acul::vector<acul::string> items);
        acul::vector<u32> set_item_next(u32 element_id, acul::vector<acul::string> items);
        acul::vector<acul::vector<u32>> set_item_next_groups(u32 element_id, MenuGroups groups);
        acul::vector<u32> root_ids() const;
        void open_root(u32 element_id);
        bool open_next(u32 element_id, u32 depth, const amal::rect &anchor);
        void close_from_depth(u32 depth);
        StyleUpdateFlags update_root_item_state(u32 element_id);
        bool is_item_open(u32 element_id) const;
        bool is_popup_item_focused(u32 element_id) const;
        StyleState resolve_popup_item_state(Widget *child) const;
        void sync_popup_item_states();
        Widget *find_popup_child(u32 element_id) const;
        Widget *find_popup_child_with_depth(u32 element_id, u32 *out_depth = nullptr) const;
        Widget *find_popup_child_by_transition_id(ElementID id);
        u32 popup_child_item_id(const Widget *child) const;
        StyleUpdateFlags update_popup_transition(ElementID id);
        StyleState resolve_tab_item_state(u32 index,
                                          const detail::WidgetStyleSelectorTransition &transition) const override;
        bool auto_select_first_item() const override { return false; }
        Window *ensure_popup(u32 depth);
        void layout_popup(u32 depth, const amal::rect &anchor, const acul::vector<u32> &ids);
        amal::vec2 resolve_popup_position(u32 depth, const amal::rect &anchor, const amal::vec2 &popup_size,
                                          u32 popup_id) const;
        void refresh_menu_clip_rects();
        amal::vec4 get_popup_bounds_rect() const
        {
            if (_popup_depth_mode != PopupDepthMode::workzone_overlay) return get_main_viewport();
            return get_widget_viewport_rect(this);
        }
        amal::vec4 get_popup_clip_rect(Window *popup) const;
        void refresh_popup_clip_rect(Window *popup);
        void request_redraw();
        bool draw_popup_child(const ElementID &element_id, DrawCtx &ctx);
        u16 get_layout_parent_clip_id() const override;
        amal::vec4 get_layout_parent_clip_rect() const override;

        DrawDataID _bg{};
        acul::vector<ItemData> _items;
        acul::vector<RuntimeSuffixGroup> _runtime_suffix_groups;
        acul::vector<Window *> _popups;
        acul::vector<u32> _open_path;
        StyleSelector _menu_style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_WINDOW_MENU_BAR};
        PopupDepthMode _popup_depth_mode = PopupDepthMode::workzone_overlay;
        bool _selected_enabled = false;
    };

    inline MenuBar::GroupRef MenuBar::ItemRef::add_group(const acul::vector<acul::string> &items) const
    {
        if (!_owner || _element_id == 0u) return MenuBar::GroupRef{nullptr, {}};
        return _owner->add_group(_element_id, std::move(items));
    }

    inline MenuBar *make_menu_bar(u32 id, const acul::vector<acul::string> &items = {})
    {
        return acul::alloc<MenuBar>(id, std::move(items));
    }

    inline MenuBar *make_main_menu_bar(u32 id, const acul::vector<acul::string> &items = {})
    {
        auto *menu_bar = acul::alloc<MenuBar>(id, std::move(items));
        menu_bar->set_menu_style_tag(AUIK_STYLE_TAG_MAIN_MENU_BAR);
        menu_bar->set_menu_item_style_tag(AUIK_STYLE_TAG_MAIN_MENU_ITEM);
        menu_bar->set_popup_depth_mode(MenuBar::PopupDepthMode::root_overlay);
        return menu_bar;
    }

    class APPLIB_API PopupMenu final : public Widget
    {
    public:
        using ItemRef = MenuBar::ItemRef;
        using GroupRef = MenuBar::GroupRef;
        using MenuGroup = MenuBar::MenuGroup;

        PopupMenu(u32 id, acul::vector<acul::string> items = {},
                  WidgetFlags widget_flags = get_default_fixed_tab_bar_flags(), Widget *parent = nullptr,
                  bool selected_enabled = false);
        explicit PopupMenu(MenuBar *menu, WidgetFlags widget_flags = get_default_fixed_tab_bar_flags(),
                           Widget *parent = nullptr, bool selected_enabled = false);
        ~PopupMenu() override;

        ItemRef item(u32 element_id) { return _menu ? _menu->item(element_id) : ItemRef{nullptr, 0u}; }
        ItemRef operator[](size_t index) { return _menu ? (*_menu)[index] : ItemRef{nullptr, 0u}; }
        const acul::vector<u32> &element_ids() const { return _menu->element_ids(); }

        u32 push_suffix_group() { return _menu ? _menu->push_root_suffix_group() : 0u; }
        u32 suffix_group_count() const { return _menu ? _menu->root_suffix_group_count() : 0u; }
        void pop_suffix_group()
        {
            if (_menu) _menu->pop_root_suffix_group();
        }

        u32 add_suffix_item(u32 group_index, acul::string text,
                            acul::unique_function<void(ClickEvent &)> callback = nullptr)
        {
            return _menu ? _menu->append_root_suffix_item(group_index, std::move(text), std::move(callback)) : 0u;
        }

        u32 add_suffix_separator(u32 group_index)
        {
            return _menu ? _menu->append_root_suffix_separator(group_index) : 0u;
        }

        void erase_suffix_group(u32 group_index)
        {
            if (_menu) _menu->erase_root_suffix_group(group_index);
        }

        bool has_suffix_items(u32 group_index) const { return _menu && _menu->has_root_suffix(group_index); }

        u32 suffix_item_count(u32 group_index) const
        {
            if (!_menu) return 0u;
            const auto *group = _menu->runtime_suffix_group(group_index);
            return group ? group->size() : 0u;
        }

        void set_popup_anchor_override(const amal::rect &anchor)
        {
            _popup_anchor_override = anchor;
            _popup_anchor_overridden = true;
            if (_open) open_menu();
        }

        void clear_popup_anchor_override()
        {
            _popup_anchor_overridden = false;
            if (_open) open_menu();
        }

        void set_button_update_target(Widget *target) { _button.set_update_target(target ? target : this); }
        void set_button_hit_id(ElementID id)
        {
            _button_hit_id = id;
            _button.set_hit_id(id);
        }

        void set_button_style_tag(u32 style_tag) { _button.set_style_tag(style_tag); }
        void set_selected_enabled(bool value)
        {
            if (_menu) _menu->set_selected_enabled(value);
        }

        void sync_selection_mode_changes()
        {
            if (_menu) _menu->sync_selection_mode_changes();
        }

        void set_open(bool value);
        void discard_popup();
        bool is_open() const { return _open; }
        MenuBar *menu_model() const { return _menu; }

        StyleUpdateFlags update_style() override;
        void update_layout_min_size() override;
        void update_layout(bool min_size_known) override;
        void update_depth(const amal::vec2 &depth_range) override;
        void back_hit_depth() override;
        void restore_hit_depth() override;
        void translate(const amal::vec2 &delta) override;
        void reset_clip_rect_records() override;
        void rebuild_clip_rects() override;
        void reset_draw_records() override;
        void draw(DrawCtx &ctx) override;
        void draw_button(DrawCtx &ctx);
        void draw_popups(DrawCtx &ctx);
        void on_click(MouseKey key, KeyPressState state, u32 click_count) override;
        void on_focus(bool focused) override;
        void on_attach() override;
        void on_detach() override;
        u32 get_depth_requirement() const override;

    private:
        bool menu_attached() const
        {
            if (!_menu) return false;
            const auto &map = detail::get_context().id_map;
            const auto it = map.find(_menu->id());
            return it != map.end() && it->second == _menu;
        }

        void attach_menu_for_popup()
        {
            if (!_menu || !(_menu->widget_flags & WidgetFlagBits::attachable) || menu_attached()) return;
            _menu->on_attach();
        }

        void detach_menu_from_popup()
        {
            if (!_menu || !(_menu->widget_flags & WidgetFlagBits::attachable) || !menu_attached()) return;
            _menu->on_detach();
        }

        void open_menu();
        bool is_button_hit(ElementID id) const { return id == _button_hit_id; }
        acul::vector<u32> root_items();
        StyleState resolve_button_state() const
        {
            const auto &ctx = detail::get_context();
            if (ctx.io.drag_id == _button_hit_id) return StyleState::active;
            if (ctx.hover_id == _button_hit_id) return StyleState::hover;
            return StyleState::normal;
        }

        detail::PopupTrigger _button;
        MenuBar *_menu = nullptr;
        ElementID _button_hit_id{};
        amal::rect _popup_anchor_override{};
        bool _popup_anchor_overridden = false;
        bool _open = false;
    };

    inline PopupMenu *make_popup_menu(u32 id, acul::vector<acul::string> items = {}, bool selected_enabled = false)
    {
        return acul::alloc<PopupMenu>(id, std::move(items), get_default_fixed_tab_bar_flags(), nullptr,
                                      selected_enabled);
    }

    class MenuProxy
    {
    public:
        using value_type = MenuBar::value_type;
        using iterator = MenuBar::iterator;
        using const_iterator = MenuBar::const_iterator;
        using ItemRef = MenuBar::ItemRef;
        using GroupRef = MenuBar::GroupRef;

        MenuProxy() = default;
        explicit MenuProxy(Widget *widget) : _widget(widget) {}
        ~MenuProxy()
        {
            if (_widget) acul::release(_widget);
        }

        MenuProxy(const MenuProxy &) = delete;
        MenuProxy &operator=(const MenuProxy &) = delete;
        MenuProxy(MenuProxy &&other) noexcept : _widget(other._widget) { other._widget = nullptr; }
        MenuProxy &operator=(MenuProxy &&other) noexcept
        {
            if (this == &other) return *this;
            reset();
            _widget = other._widget;
            other._widget = nullptr;
            return *this;
        }

        Widget *get_widget() const { return _widget; }
        Widget *operator->() const { return _widget; }
        void reset(Widget *widget = nullptr)
        {
            if (_widget == widget) return;
            if (_widget) acul::release(_widget);
            _widget = widget;
        }
        Widget *release()
        {
            auto *widget = _widget;
            _widget = nullptr;
            return widget;
        }
        void set_menu_bar(MenuBar *menu) { reset(menu); }
        void set_popup_menu(PopupMenu *menu) { reset(menu); }
        bool valid() const { return menu_model() != nullptr; }
        explicit operator bool() const { return valid(); }
        bool is_menu_bar() const { return _widget && _widget->get_rect().id.tag_id == AUIK_TAG_WINDOW_MENU_BAR; }
        bool is_popup_menu() const { return _widget && _widget->get_rect().id.tag_id == AUIK_TAG_POPUP_MENU; }

        MenuBar *menu_model() const
        {
            if (is_menu_bar()) return static_cast<MenuBar *>(_widget);
            if (is_popup_menu()) return static_cast<PopupMenu *>(_widget)->menu_model();
            return nullptr;
        }
        MenuBar *menu_bar() const { return menu_model(); }
        MenuBar *get_menu_bar() const { return menu_model(); }

        const acul::vector<u32> &element_ids() const
        {
            static const acul::vector<u32> empty;
            auto *model = menu_model();
            return model ? model->element_ids() : empty;
        }

        size_t item_count() const { return element_ids().size(); }
        iterator begin()
        {
            auto *model = menu_model();
            return model ? model->begin() : empty_items().begin();
        }
        iterator end()
        {
            auto *model = menu_model();
            return model ? model->end() : empty_items().end();
        }
        const_iterator begin() const
        {
            auto *model = menu_model();
            return model ? model->begin() : empty_items().begin();
        }
        const_iterator end() const
        {
            auto *model = menu_model();
            return model ? model->end() : empty_items().end();
        }
        const_iterator cbegin() const
        {
            auto *model = menu_model();
            return model ? model->cbegin() : empty_items().cbegin();
        }
        const_iterator cend() const
        {
            auto *model = menu_model();
            return model ? model->cend() : empty_items().cend();
        }
        bool empty() const
        {
            auto *model = menu_model();
            return !model || model->empty();
        }
        size_t child_size() const
        {
            auto *model = menu_model();
            return model ? model->child_size() : 0u;
        }
        value_type front()
        {
            auto *model = menu_model();
            return model ? model->front() : nullptr;
        }
        value_type back()
        {
            auto *model = menu_model();
            return model ? model->back() : nullptr;
        }
        const value_type front() const
        {
            auto *model = menu_model();
            return model ? model->front() : nullptr;
        }
        const value_type back() const
        {
            auto *model = menu_model();
            return model ? model->back() : nullptr;
        }
        value_type *data()
        {
            auto *model = menu_model();
            return model ? model->data() : nullptr;
        }
        const value_type *data() const
        {
            auto *model = menu_model();
            return model ? model->data() : nullptr;
        }
        ItemRef item(u32 element_id) const
        {
            auto *model = menu_model();
            return model ? model->item(element_id) : ItemRef{nullptr, 0u};
        }
        ItemRef operator[](size_t index) const
        {
            auto *model = menu_model();
            return model ? (*model)[index] : ItemRef{nullptr, 0u};
        }
        GroupRef add_group(u32 element_id, acul::vector<acul::string> items) const
        {
            auto *model = menu_model();
            return model ? model->add_group(element_id, std::move(items)) : GroupRef{nullptr, {}};
        }

    private:
        static acul::vector<value_type> &empty_items()
        {
            static acul::vector<value_type> items;
            return items;
        }

        Widget *_widget = nullptr;
    };
} // namespace auik::v2
