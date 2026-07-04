#pragma once

#include <acul/functional/unique_function.hpp>
#include "detail/menu_base.hpp"
#include "detail/popup_trigger.hpp"
#include "tabbar.hpp"

#define AUIK_TAG_MENU              0x53F486E7u
#define AUIK_TAG_MENU_BAR          0xC37C8F4Bu
#define AUIK_TAG_MENU_BAR_ITEM     0x62F8CAAAu
#define AUIK_TAG_MENU_POPUP        0x0EF2EE30u
#define AUIK_TAG_MENU_SHORTCUT     0x94020AC8u
#define AUIK_TAG_MAIN_MENU_BAR     0x5F42BA15u
#define AUIK_TAG_MAIN_MENU_ITEM    0xDD0667CDu
#define AUIK_TAG_POPUP_MENU        0x2AF466E1u
#define AUIK_TAG_POPUP_MENU_BUTTON 0x34EF928Au

namespace auik
{
    class Window;

    class MenuBar final : public Tabbar
    {
        friend class PopupMenu;

    public:
        enum class PopupDepthMode : u8
        {
            workzone_overlay,
            root_overlay,
            root_overlay_next
        };

        using MenuItem = detail::MenuItem;
        using MenuGroup = detail::MenuGroup;
        using MenuGroupLayer = detail::MenuGroupLayer;

        AUIK_EXPORT MenuBar(u32 id, const acul::vector<StringView> &items = {},
                            amal::vec2 inline_size = AUIK_SIZE_INHERIT,
                            WidgetFlags widget_flags = detail::get_tabbar_widget_flags());
        AUIK_EXPORT ~MenuBar() override;

        AUIK_EXPORT StyleUpdateFlags update_style() override;
        AUIK_EXPORT void update_layout(bool min_size_known) override;
        AUIK_EXPORT void update_depth(const amal::vec2 &depth_range) override;
        AUIK_EXPORT void back_hit_depth() override;
        AUIK_EXPORT void restore_hit_depth() override;
        AUIK_EXPORT void translate(const amal::vec2 &delta) override;
        AUIK_EXPORT void reset_clip_rect_records() override;
        AUIK_EXPORT void rebuild_clip_rects() override;
        AUIK_EXPORT void draw(DrawCtx &ctx) override;
        AUIK_EXPORT void on_click(MouseKey key, KeyPressState state, u32 click_count) override;
        AUIK_EXPORT void on_hover(HoverState state) override;
        AUIK_EXPORT void on_focus(bool focused) override;

        using MenuGroupStrings = acul::vector<acul::string>;
        using MenuGroups = acul::vector<MenuGroupStrings>;
        struct SerializedItem
        {
            u32 id = 0u;
            acul::string text;
            bool translated = false;
            acul::string shortcut;
            acul::vector<acul::vector<u32>> next_groups;
            bool selected = false;
        };

        AUIK_EXPORT void set_item_shortcut(u32 element_id, acul::string shortcut);
        AUIK_EXPORT void set_item_selected(u32 element_id, bool selected = true);
        AUIK_EXPORT void set_selected_items(const acul::vector<u32> &element_ids);
        AUIK_EXPORT bool is_item_selected(u32 element_id) const;
        AUIK_EXPORT void set_menu_style_tag(u32 tag_id);
        u32 menu_style_tag() const { return _menu_style.tag_id; }
        AUIK_EXPORT void set_menu_item_style_tag(u32 tag_id);
        u32 menu_item_style_tag() const { return _item_style_tag; }
        AUIK_EXPORT void set_popup_depth_mode(PopupDepthMode mode);
        PopupDepthMode popup_depth_mode() const { return _popup_depth_mode; }
        AUIK_EXPORT void set_selected_enabled(bool value);
        bool selected_enabled() const { return _selected_enabled; }
        AUIK_EXPORT void sync_selection_mode_changes();
        AUIK_EXPORT MenuItem *append_item(const char *text,
                                          acul::unique_function<void(ClickEvent &)> callback = nullptr);
        AUIK_EXPORT MenuItem *append_item(acul::string text,
                                          acul::unique_function<void(ClickEvent &)> callback = nullptr);
        AUIK_EXPORT MenuItem *append_item(StringView text,
                                          acul::unique_function<void(ClickEvent &)> callback = nullptr);
        detail::MenuBase *menu_base() { return &_menu_base; }
        const detail::MenuBase *menu_base() const { return &_menu_base; }
        AUIK_EXPORT bool open_root_items_at(const amal::rect &anchor, MenuGroupLayer *groups);
        AUIK_EXPORT bool open_root_at(u32 element_id, const amal::rect &anchor);
        AUIK_EXPORT void close_all();
        AUIK_EXPORT void discard_popups();
        AUIK_EXPORT void add_items(u32 count);
        AUIK_EXPORT void add_items(const acul::vector<StringView> &items);
        AUIK_EXPORT void add_items(std::initializer_list<const char *> items);
        AUIK_EXPORT MenuItem *item(u32 element_id);
        AUIK_EXPORT const MenuItem *item(u32 element_id) const;
        MenuItem *operator[](size_t index) { return (*_menu_base.root_layer()[0])[index]; }
        const MenuItem *operator[](size_t index) const { return (*_menu_base.root_layer()[0])[index]; }
        AUIK_EXPORT void draw_popups(DrawCtx &ctx);
        AUIK_EXPORT acul::vector<SerializedItem> serialized_items() const;
        AUIK_EXPORT void restore_serialized_items(const acul::vector<u32> &root_ids,
                                                  const acul::vector<SerializedItem> &items);
        virtual u32 signature() const override { return AUIK_TAG_MENU_BAR; }

    private:
        friend class PopupMenu;

        class PopupItem;
        struct RuntimeSuffixGroup
        {
            MenuGroup *group = nullptr;
            bool empty() const { return !group || group->empty(); }
            u32 size() const { return group ? static_cast<u32>(group->size()) : 0u; }
        };

        MenuItem *create_item(StringView text, Widget *parent = nullptr);
        MenuItem *find_item(u32 element_id);
        const MenuItem *find_item(u32 element_id) const;
        RuntimeSuffixGroup &ensure_runtime_suffix_group(u32 group_index);
        const RuntimeSuffixGroup *runtime_suffix_group(u32 group_index) const;
        u32 push_root_suffix_group();
        u32 root_suffix_group_count() const { return static_cast<u32>(_runtime_suffix_groups.size()); }
        void pop_root_suffix_group();
        MenuItem *append_root_suffix_item(acul::string text,
                                          acul::unique_function<void(ClickEvent &)> callback = nullptr);
        MenuItem *append_root_suffix_item(StringView text,
                                          acul::unique_function<void(ClickEvent &)> callback = nullptr);
        u32 append_root_suffix_separator();
        MenuItem *append_root_suffix_item(u32 group_index, acul::string text,
                                          acul::unique_function<void(ClickEvent &)> callback = nullptr);
        MenuItem *append_root_suffix_item(u32 group_index, StringView text,
                                          acul::unique_function<void(ClickEvent &)> callback = nullptr);
        u32 append_root_suffix_separator(u32 group_index);
        void erase_root_suffix_group(u32 group_index);
        bool has_root_suffix(u32 group_index) const;
        bool root_suffix_ends_with_separator(u32 group_index) const;
        acul::vector<acul::vector<u32>> group_layer_ids(const MenuGroupLayer *layer) const;
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
        void layout_popup(u32 depth, const amal::rect &anchor, const acul::vector<acul::vector<u32>> &groups);
        amal::vec2 resolve_popup_position(u32 depth, const amal::rect &anchor, const amal::vec2 &popup_size,
                                          u32 popup_id) const;
        void refresh_menu_clip_rects();
        amal::vec4 get_popup_bounds_rect() const
        {
            if (_popup_depth_mode != PopupDepthMode::workzone_overlay) return get_main_viewport_rect();
            return get_widget_viewport_rect(this);
        }
        Viewport *get_popup_viewport() const
        {
            if (_popup_depth_mode != PopupDepthMode::workzone_overlay) return get_main_viewport();
            return this->viewport();
        }
        amal::vec4 get_popup_clip_rect(Window *popup) const;
        void refresh_popup_clip_rect(Window *popup);
        void request_redraw();
        bool draw_popup_child(const ElementID &element_id, DrawCtx &ctx);
        u16 get_layout_parent_clip_id() const override;
        amal::vec4 get_layout_parent_clip_rect() const override;

        DrawDataID _bg{};
        detail::MenuBase _menu_base;
        acul::vector<RuntimeSuffixGroup> _runtime_suffix_groups;
        acul::vector<Window *> _popups;
        acul::vector<u32> _open_path;
        StyleSelector _menu_style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_WINDOW_MENU_BAR};
        PopupDepthMode _popup_depth_mode = PopupDepthMode::workzone_overlay;
        bool _selected_enabled = false;
    };

    inline MenuBar *make_menu_bar(u32 id, const acul::vector<StringView> &items = {})
    { return acul::alloc<MenuBar>(id, items); }

    inline MenuBar *make_menu_bar(u32 id, std::initializer_list<const char *> items)
    {
        acul::vector<StringView> values;
        values.reserve(items.size());
        for (const char *item : items) values.push_back(StringView{item});
        return acul::alloc<MenuBar>(id, values);
    }

    inline MenuBar *make_main_menu_bar(u32 id, const acul::vector<StringView> &items = {})
    {
        auto *menu_bar = acul::alloc<MenuBar>(id, items);
        menu_bar->set_menu_style_tag(AUIK_STYLE_TAG_MAIN_MENU_BAR);
        menu_bar->set_menu_item_style_tag(AUIK_STYLE_TAG_MAIN_MENU_ITEM);
        menu_bar->set_popup_depth_mode(MenuBar::PopupDepthMode::root_overlay);
        return menu_bar;
    }

    inline MenuBar *make_main_menu_bar(u32 id, std::initializer_list<const char *> items)
    {
        acul::vector<StringView> values;
        values.reserve(items.size());
        for (const char *item : items) values.push_back(StringView{item});
        return make_main_menu_bar(id, values);
    }

    class PopupMenu final : public Widget
    {
    public:
        using MenuGroup = MenuBar::MenuGroup;

        AUIK_EXPORT PopupMenu(u32 id, const acul::vector<StringView> &items, WidgetFlags widget_flags,
                              bool selected_enabled);
        AUIK_EXPORT explicit PopupMenu(MenuBar *menu, WidgetFlags widget_flags = detail::get_tabbar_widget_flags(),
                                       bool selected_enabled = false);
        AUIK_EXPORT ~PopupMenu() override;

        MenuBar::MenuItem *item(u32 element_id) { return _menu ? _menu->item(element_id) : nullptr; }
        MenuBar::MenuItem *operator[](size_t index) { return _menu ? (*_menu)[index] : nullptr; }
        const acul::vector<u32> &element_ids() const { return _menu->element_ids(); }

        u32 push_suffix_group() { return _menu ? _menu->push_root_suffix_group() : 0u; }
        u32 suffix_group_count() const { return _menu ? _menu->root_suffix_group_count() : 0u; }
        void pop_suffix_group()
        {
            if (_menu) _menu->pop_root_suffix_group();
        }

        MenuBar::MenuItem *add_suffix_item(u32 group_index, acul::string text,
                                           acul::unique_function<void(ClickEvent &)> callback = nullptr)
        { return _menu ? _menu->append_root_suffix_item(group_index, std::move(text), std::move(callback)) : nullptr; }

        u32 add_suffix_separator(u32 group_index)
        { return _menu ? _menu->append_root_suffix_separator(group_index) : 0u; }

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

        AUIK_EXPORT void set_open(bool value);
        AUIK_EXPORT void discard_popup();
        bool is_open() const { return _open; }
        MenuBar *menu_model() const { return _menu; }

        AUIK_EXPORT StyleUpdateFlags update_style() override;
        AUIK_EXPORT void update_layout_min_size() override;
        AUIK_EXPORT void update_layout(bool min_size_known) override;
        AUIK_EXPORT void update_depth(const amal::vec2 &depth_range) override;
        AUIK_EXPORT void back_hit_depth() override;
        AUIK_EXPORT void restore_hit_depth() override;
        AUIK_EXPORT void translate(const amal::vec2 &delta) override;
        AUIK_EXPORT void reset_clip_rect_records() override;
        AUIK_EXPORT void rebuild_clip_rects() override;
        AUIK_EXPORT void reset_draw_records() override;
        AUIK_EXPORT void draw(DrawCtx &ctx) override;
        AUIK_EXPORT void draw_button(DrawCtx &ctx);
        AUIK_EXPORT void draw_popups(DrawCtx &ctx);
        AUIK_EXPORT void on_click(MouseKey key, KeyPressState state, u32 click_count) override;
        AUIK_EXPORT void on_focus(bool focused) override;
        AUIK_EXPORT void on_attach() override;
        AUIK_EXPORT void on_detach() override;
        AUIK_EXPORT u32 get_depth_requirement() const override;
        virtual u32 signature() const override { return AUIK_TAG_POPUP_MENU; }

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
        MenuBar::MenuGroupLayer *root_items();
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

    inline PopupMenu *make_popup_menu(u32 id, const acul::vector<StringView> &items = {}, bool selected_enabled = false)
    {
        constexpr WidgetFlags widget_flags = WidgetFlagBits::visible | WidgetFlagBits::attachable |
                                             WidgetFlagBits::configurable | WidgetFlagBits::hittable;
        return acul::alloc<PopupMenu>(id, items, widget_flags, selected_enabled);
    }

    class MenuProxy
    {
    public:
        using value_type = MenuBar::value_type;
        using iterator = MenuBar::iterator;
        using const_iterator = MenuBar::const_iterator;

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
        bool is_menu_bar() const { return _widget && _widget->signature() == AUIK_TAG_MENU_BAR; }
        bool is_popup_menu() const { return _widget && _widget->signature() == AUIK_TAG_POPUP_MENU; }

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
        MenuBar::MenuItem *item(u32 element_id) const
        {
            auto *model = menu_model();
            return model ? model->item(element_id) : nullptr;
        }
        MenuBar::MenuItem *operator[](size_t index) const
        {
            auto *model = menu_model();
            return model ? (*model)[index] : nullptr;
        }
        MenuBar::MenuGroup *add_group(u32 element_id, const acul::vector<StringView> &items) const
        {
            auto *model = menu_model();
            auto *item = model ? model->item(element_id) : nullptr;
            if (!item) return nullptr;
            auto *layer = item->add_group_layer(1u);
            auto *group = layer ? (*layer)[0] : nullptr;
            if (group) group->add_items(items);
            return group;
        }

    private:
        static acul::vector<value_type> &empty_items()
        {
            static acul::vector<value_type> items;
            return items;
        }

        Widget *_widget = nullptr;
    };

    namespace streams
    {
        extern AUIK_EXPORT const umbf::streams::Stream menu_bar;
        extern AUIK_EXPORT const umbf::streams::Stream popup_menu;
    } // namespace streams
} // namespace auik
