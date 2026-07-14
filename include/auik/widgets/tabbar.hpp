#pragma once

#include <acul/string/string.hpp>
#include "../model.hpp"
#include "../theme.hpp"
#include "combobox.hpp"
#include "detail/popup_trigger.hpp"
#include "detail/selectable.hpp"
#include "image_button.hpp"
#include "widget.hpp"

#define AUIK_TAG_TABBAR             0xECA5E393u
#define AUIK_TAG_TABBAR_ITEM        0x6F421EACu
#define AUIK_TAG_TABBAR_POPUP_BTN   0x9041AD68u
#define AUIK_TAG_TABBAR_OVERFLOW    AUIK_TAG_TABBAR_POPUP_BTN
#define AUIK_TAG_TABBAR_POPUP       0x2E443728u
#define AUIK_TAG_TABBAR_CHANGE_ICON 0xC9B40563u
#define AUIK_TAG_CLOSE_BUTTON       0x5B258693u

namespace auik
{
    class Window;

    struct TabbarFlagBits
    {
        enum enum_type
        {
            none = 0x0,
            popup = 0x1,
            scroll = 0x2,
            multiple = 0x4,
            movable = 0x8,
            closable = 0x10
        };
        using flag_bitmask = std::true_type;
    };
    using TabbarFlags = acul::flags<TabbarFlagBits>;

    enum class TabbarChangeReason : u8
    {
        none = 0,
        selection,
        close,
        reorder
    };

    namespace detail
    {
        constexpr inline TabbarFlags get_tabbar_visual_mask() { return TabbarFlagBits::popup | TabbarFlagBits::scroll; }
        constexpr inline WidgetFlags get_tabbar_widget_flags()
        {
            return WidgetFlagBits::visible | WidgetFlagBits::attachable | WidgetFlagBits::configurable |
                   WidgetFlagBits::hittable;
        }
    } // namespace detail

    class Tabbar : public Widget
    {
    public:
        AUIK_EXPORT Tabbar(u32 id, TabbarFlags tab_flags, WidgetFlags widget_flags, amal::vec2 inline_size);
        AUIK_EXPORT Tabbar(u32 id, acul::vector<acul::string> items, TabbarFlags tab_flags, WidgetFlags widget_flags,
                           amal::vec2 inline_size);
        AUIK_EXPORT Tabbar(u32 id, const acul::vector<StringView> &items, TabbarFlags tab_flags,
                           WidgetFlags widget_flags, amal::vec2 inline_size);
        AUIK_EXPORT ~Tabbar() override;

        AUIK_EXPORT StyleUpdateFlags update_style() override;
        AUIK_EXPORT void update_layout_min_size() override;
        AUIK_EXPORT void update_layout(bool min_size_known) override;
        AUIK_EXPORT void translate(const amal::vec2 &delta) override;
        AUIK_EXPORT void reset_clip_rect_records() override;
        AUIK_EXPORT void reset_draw_records() override;
        AUIK_EXPORT void rebuild_clip_rects() override;
        AUIK_EXPORT u32 get_depth_requirement() const override;
        AUIK_EXPORT void update_depth(const amal::vec2 &depth_range) override;
        AUIK_EXPORT void back_hit_depth() override;
        AUIK_EXPORT void restore_hit_depth() override;
        AUIK_EXPORT void draw(DrawCtx &ctx) override;
        AUIK_EXPORT void on_focus(bool focused) override;
        AUIK_EXPORT void on_hover(HoverState state) override;
        AUIK_EXPORT void on_click(MouseKey key, KeyPressState state, u32 click_count) override;
        AUIK_EXPORT void on_drag(const amal::vec2 &delta, KeyPressState state) override;
        AUIK_EXPORT void on_scroll(const amal::vec2 &delta) override;
        AUIK_EXPORT void set_tab_changed(u32 element_id);
        AUIK_EXPORT void set_tab_unchanged(u32 element_id);

        u16 content_clip_id() const override
        {
            if (_drag_element_id != 0u) return get_layout_parent_clip_id();
            return _content_clip_id != 0xFFFFu ? _content_clip_id : clip_id();
        }

        using PFN_on_changed_icon_create = Widget *(*)(u32 id);
        using value_type = detail::Selectable *;
        struct Item
        {
            u32 element_id = 0u;
            detail::Selectable *tab = nullptr;
            Widget *change_icon = nullptr;
            ImageButton *close_button = nullptr;
            void *user_data = nullptr;
            bool changed = false;
        };
        class iterator
        {
        public:
            explicit iterator(Item *ptr = nullptr) : _ptr(ptr) {}
            value_type operator*() const { return _ptr ? _ptr->tab : nullptr; }
            iterator &operator++()
            {
                ++_ptr;
                return *this;
            }
            bool operator!=(const iterator &rhs) const { return _ptr != rhs._ptr; }

        private:
            Item *_ptr = nullptr;
        };
        class const_iterator
        {
        public:
            explicit const_iterator(const Item *ptr = nullptr) : _ptr(ptr) {}
            value_type operator*() const { return _ptr ? _ptr->tab : nullptr; }
            const_iterator &operator++()
            {
                ++_ptr;
                return *this;
            }
            bool operator!=(const const_iterator &rhs) const { return _ptr != rhs._ptr; }

        private:
            const Item *_ptr = nullptr;
        };

        AUIK_EXPORT void set_model_binding(ModelBinding *binding);
        AUIK_EXPORT void set_items(acul::vector<acul::string> items);
        AUIK_EXPORT void set_items(const acul::vector<StringView> &items);
        AUIK_EXPORT void insert_item(u32 index, StringView item);
        AUIK_EXPORT void remove_item(u32 index);
        AUIK_EXPORT void set_style_tag(u32 tag_id);
        void set_item_style_tag(u32 tag_id) { _item_style_tag = tag_id; }
        void set_selected_item_style_tag(u32 tag_id) { _selected_item_style_tag = tag_id; }
        void set_popup_item_style_tag(u32 tag_id) { _popup_item_style_tag = tag_id; }
        void set_close_button_style_tag(u32 tag_id) { _close_button_style_tag = tag_id; }
        AUIK_EXPORT void set_changed_icon_create_cb(PFN_on_changed_icon_create cb);
        AUIK_EXPORT Widget *change_icon(u32 element_id);
        AUIK_EXPORT const Widget *change_icon(u32 element_id) const;
        AUIK_EXPORT void update_item_tags();
        AUIK_EXPORT void sync_tags();
        u32 style_tag() const { return _style.tag_id; }
        acul::vector<acul::string> item_texts() const
        {
            acul::vector<acul::string> out;
            out.reserve(_items.size());
            for (const auto &item : _items) out.push_back(item.tab ? item.tab->text() : acul::string{});
            return out;
        }

        iterator begin() { return iterator(_items.empty() ? nullptr : &_items[0]); }
        iterator end() { return iterator(_items.empty() ? nullptr : &_items[0] + _items.size()); }
        const_iterator begin() const { return const_iterator(_items.empty() ? nullptr : &_items[0]); }
        const_iterator end() const { return const_iterator(_items.empty() ? nullptr : &_items[0] + _items.size()); }
        const_iterator cbegin() const { return begin(); }
        const_iterator cend() const { return end(); }
        bool empty() const { return _items.empty(); }
        size_t child_size() const { return _items.size(); }
        value_type front() { return _items.empty() ? nullptr : _items.front().tab; }
        value_type back() { return _items.empty() ? nullptr : _items.back().tab; }
        const value_type front() const { return _items.empty() ? nullptr : _items.front().tab; }
        const value_type back() const { return _items.empty() ? nullptr : _items.back().tab; }
        AUIK_EXPORT value_type *data();
        AUIK_EXPORT const value_type *data() const;
        value_type item_at(u32 index) { return index < _items.size() ? _items[index].tab : nullptr; }
        const value_type item_at(u32 index) const { return index < _items.size() ? _items[index].tab : nullptr; }
        u32 item_element_id(u32 index) const { return index < _items.size() ? _items[index].element_id : 0u; }
        void set_item_user_data(u32 index, void *data)
        {
            if (index < _items.size()) _items[index].user_data = data;
        }
        void *item_user_data(u32 index) const { return index < _items.size() ? _items[index].user_data : nullptr; }
        AUIK_EXPORT void *item_user_data_by_element_id(u32 element_id) const;

        TabbarFlags tab_flags() const { return _tab_flags; }
        bool content_width_fit() const { return is_size_fit(style_size().x); }
        bool clipped() const { return !(_tab_flags & detail::get_tabbar_visual_mask()) && !content_width_fit(); }
        bool popup() const { return _tab_flags & TabbarFlagBits::popup; }
        bool scroll() const { return _tab_flags & TabbarFlagBits::scroll; }
        bool multiple() const { return _tab_flags & TabbarFlagBits::multiple; }
        bool movable() const { return _tab_flags & TabbarFlagBits::movable; }
        bool closable() const { return _tab_flags & TabbarFlagBits::closable; }

        AUIK_EXPORT u32 selected_index() const;
        AUIK_EXPORT u32 selected_id() const;
        acul::vector<u32> selected_ids() const;
        AUIK_EXPORT void set_selected(u32 element_id);
        AUIK_EXPORT void set_selected(const acul::vector<u32> &element_ids);
        AUIK_EXPORT void set_selected_silent(u32 element_id);
        AUIK_EXPORT bool is_selected(u32 element_id) const;
        AUIK_EXPORT bool is_changed(u32 element_id) const;
        AUIK_EXPORT void close_item(u32 element_id);
        TabbarChangeReason change_reason() const { return _change_reason; }
        AUIK_EXPORT u32 insertion_index_at(const amal::vec2 &point) const;
        const amal::vec2 &drag_grab_offset() const { return _drag_grab_offset; }
        bool has_drag_grab_offset() const { return _drag_grab_offset_valid; }
        AUIK_EXPORT void begin_external_drag(u32 element_id);
        AUIK_EXPORT void cancel_drag();
        u32 item_style_tag() const { return _item_style_tag; }
        u32 selected_item_style_tag() const { return _selected_item_style_tag; }
        u32 popup_item_style_tag() const { return _popup_item_style_tag; }
        u32 close_button_style_tag() const { return _close_button_style_tag; }
        f32 scroll_offset() const { return _scroll_offset; }
        void set_scroll_offset(f32 value) { _scroll_offset = amal::max(value, 0.0f); }
        virtual u32 signature() const override { return AUIK_TAG_TABBAR; }

    protected:
        AUIK_EXPORT bool draw_transition_targets(DrawCtx &ctx);
        AUIK_EXPORT void rebuild_items();
        AUIK_EXPORT void update_popup_layout();
        AUIK_EXPORT void open_popup();
        AUIK_EXPORT void close_popup(bool refresh_style = true);
        void toggle_popup()
        {
            if (_open) close_popup();
            else open_popup();
        }
        AUIK_EXPORT void update_overflow_button_style();
        AUIK_EXPORT void clamp_scroll_offset();
        AUIK_EXPORT void handle_item_click(u32 element_id);
        AUIK_EXPORT u32 find_index_by_element_id(u32 element_id) const;
        AUIK_EXPORT void reorder_item(u32 from, u32 to);
        AUIK_EXPORT void begin_drag(u32 element_id);
        AUIK_EXPORT void end_drag();
        AUIK_EXPORT u32 find_drop_index_by_x(f32 x) const;
        AUIK_EXPORT u32 find_drop_index_by_dragged_center() const;
        AUIK_EXPORT virtual u16 get_layout_parent_clip_id() const;
        AUIK_EXPORT virtual amal::vec4 get_layout_parent_clip_rect() const;
        AUIK_EXPORT bool update_drag_realtime_order(f32 delta_x);
        AUIK_EXPORT bool swap_drag_with_neighbor(u32 drag_index, u32 neighbor_index);
        AUIK_EXPORT void update_drag_depth();
        virtual StyleState resolve_tab_item_state(u32 index,
                                                  const detail::WidgetStyleSelectorTransition &transition) const;
        virtual bool auto_select_first_item() const { return true; }
        bool mark_changed(TabbarChangeReason reason)
        {
            _change_reason = reason;
            const bool prevented = Widget::mark_changed();
            if (prevented) _change_reason = TabbarChangeReason::none;
            return prevented;
        }
        bool mark_changed() { return mark_changed(TabbarChangeReason::selection); }
        AUIK_EXPORT StyleUpdateFlags update_item_state(u32 index,
                                                       const detail::WidgetStyleSelectorTransition &transition);
        AUIK_EXPORT StyleUpdateFlags update_tab_item_style(u32 index,
                                                           const detail::WidgetStyleSelectorTransition &transition);
        AUIK_EXPORT StyleUpdateFlags update_close_button_style(u32 index,
                                                               const detail::WidgetStyleSelectorTransition &transition);
        AUIK_EXPORT StyleUpdateFlags update_change_icon_style(u32 index,
                                                              const detail::WidgetStyleSelectorTransition &transition);
        AUIK_EXPORT StyleUpdateFlags update_popup_item_style(u32 index,
                                                             const detail::WidgetStyleSelectorTransition &transition);
        AUIK_EXPORT void update_layout_from_current_bounds(bool min_size_known);
        AUIK_EXPORT amal::vec2 resolve_tab_required_size(u32 index);
        AUIK_EXPORT void ensure_overflow_icon_resources();
        AUIK_EXPORT amal::vec2 measure_overflow_size();
        AUIK_EXPORT void rebuild_from_model_binding();
        AUIK_EXPORT void handle_model_records(const ModelRecordsEvent &event);
        AUIK_EXPORT void update_item_text(u32 index, StringView text);
        AUIK_EXPORT void sync_selection_from_model_binding();
        AUIK_EXPORT void sync_changed_from_model_binding();
        AUIK_EXPORT Item create_item(u32 element_id, StringView text);
        AUIK_EXPORT void release_item(Item &item);
        AUIK_EXPORT void sync_items();

        acul::vector<Item> _items;
        mutable acul::vector<value_type> _item_data_cache;
        PFN_on_changed_icon_create _on_changed_icon_create = nullptr;
        detail::PopupTrigger *_overflow_button = nullptr;
        Window *_popup = nullptr;
        const TabbarFlags _tab_flags;
        u32 _selected_index = 0u;
        u32 _visible_count = 0u;
        u32 _overflow_start = 0u;
        u32 _next_element_id = 1u;
        u32 _drag_element_id = 0u;
        u32 _drag_preview_index = 0u;
        u32 _last_selected_element_id = 0u;
        amal::vec2 _drag_grab_offset{0.0f, 0.0f};
        amal::vec2 _drag_offset{0.0f, 0.0f};
        amal::vec2 _drag_applied_offset{0.0f, 0.0f};
        bool _drag_grab_offset_valid = false;
        bool _drag_moved = false;
        bool _open = false;
        f32 _scroll_offset = 0.0f;
        f32 _content_width = 0.0f;
        TabbarChangeReason _change_reason = TabbarChangeReason::none;
        u32 _item_style_tag = AUIK_STYLE_TAG_TABBAR_ITEM;
        u32 _selected_item_style_tag = AUIK_STYLE_TAG_TABBAR_ITEM_SELECTED;
        u32 _popup_item_style_tag = AUIK_STYLE_TAG_TABBAR_POPUP_ITEM;
        u32 _close_button_style_tag = AUIK_STYLE_TAG_CLOSE_BUTTON;
        u16 _full_clip_id = 0xFFFFu;
        u16 _content_clip_id = 0xFFFFu;
        StyleSelector _style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_TABBAR};
        ModelBinding *_model_binding = nullptr;
    };

    AUIK_EXPORT Widget *create_tab_changed_icon(u32 id);

    inline Tabbar *make_tabbar(u32 id, const acul::vector<StringView> &items,
                               TabbarFlags tab_flags = TabbarFlagBits::none, amal::vec2 inline_size = AUIK_SIZE_INHERIT)
    { return acul::alloc<Tabbar>(id, items, tab_flags, detail::get_tabbar_widget_flags(), inline_size); }

    inline Tabbar *make_tabbar(u32 id, TabbarFlags tab_flags = TabbarFlagBits::none,
                               amal::vec2 inline_size = AUIK_SIZE_INHERIT)
    { return acul::alloc<Tabbar>(id, tab_flags, detail::get_tabbar_widget_flags(), inline_size); }

    namespace streams
    {
        extern AUIK_EXPORT const umbf::streams::Stream tab_bar;
        extern AUIK_EXPORT const umbf::streams::Stream popup_menu;
    } // namespace streams
} // namespace auik
