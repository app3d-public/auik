#pragma once

#include <acul/string/string.hpp>
#include <acul/vector.hpp>
#include "../model.hpp"
#include "../theme.hpp"
#include "combobox.hpp"
#include "detail/popup_trigger.hpp"
#include "detail/selectable.hpp"
#include "image_button.hpp"
#include "widget.hpp"

#define AUIK_TAG_TAB_BAR           0xECA5E393u
#define AUIK_TAG_TAB_BAR_ITEM      0x6F421EACu
#define AUIK_TAG_TABBAR_POPUP_BTN  0x9041AD68u
#define AUIK_TAG_TAB_BAR_OVERFLOW  AUIK_TAG_TABBAR_POPUP_BTN
#define AUIK_TAG_TAB_BAR_POPUP     0x2E443728u
#define AUIK_TAG_TAB_BAR_TAB_WIDTH 0xC72A3851u
#define AUIK_TAG_CLOSE_BUTTON      0x5B258693u

namespace auik
{
    class Window;

    struct TabBarFlagBits
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
    using TabBarFlags = acul::flags<TabBarFlagBits>;
    constexpr inline TabBarFlags g_tab_bar_visual_mask = TabBarFlagBits::popup | TabBarFlagBits::scroll;

    constexpr inline WidgetFlags get_default_tab_bar_flags()
    {
        return get_default_widget_flags() | WidgetFlagBits::hittable;
    }

    class TabBar : public Widget
    {
    public:
        AUIK_EXPORT TabBar(u32 id, acul::vector<acul::string> items = {}, TabBarFlags tab_flags = TabBarFlagBits::none,
               amal::vec2 size = {0.0f, 0.0f}, WidgetFlags widget_flags = get_default_tab_bar_flags(),
               Widget *parent = nullptr, f32 tab_width = 0.0f, u32 tab_width_key = 0u,
               u32 item_style_tag = AUIK_STYLE_TAG_TAB_BAR_ITEM,
               u32 selected_item_style_tag = AUIK_STYLE_TAG_TAB_BAR_ITEM_SELECTED,
               u32 popup_item_style_tag = AUIK_STYLE_TAG_COMBO_BOX_ITEM);
        AUIK_EXPORT TabBar(u32 id, const acul::vector<StringView> &items,
               TabBarFlags tab_flags = TabBarFlagBits::none, amal::vec2 size = {0.0f, 0.0f},
               WidgetFlags widget_flags = get_default_tab_bar_flags(), Widget *parent = nullptr,
               f32 tab_width = 0.0f, u32 tab_width_key = 0u,
               u32 item_style_tag = AUIK_STYLE_TAG_TAB_BAR_ITEM,
               u32 selected_item_style_tag = AUIK_STYLE_TAG_TAB_BAR_ITEM_SELECTED,
               u32 popup_item_style_tag = AUIK_STYLE_TAG_COMBO_BOX_ITEM);
        AUIK_EXPORT ~TabBar() override;

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
        u16 content_clip_id() const override { return _content_clip_id != 0xFFFFu ? _content_clip_id : clip_id(); }

        using value_type = detail::Selectable *;
        using iterator = acul::vector<value_type>::iterator;
        using const_iterator = acul::vector<value_type>::const_iterator;

        const acul::vector<u32> &element_ids() const { return _element_ids; }
        AUIK_EXPORT void set_model_binding(ModelBinding *binding);
        AUIK_EXPORT void set_items(acul::vector<acul::string> items);
        AUIK_EXPORT void set_items(const acul::vector<StringView> &items);
        AUIK_EXPORT void set_style_tag(u32 tag_id);
        u32 style_tag() const { return _style.tag_id; }
        acul::vector<acul::string> item_texts() const
        {
            acul::vector<acul::string> out;
            out.reserve(_tabs.size());
            for (auto *tab : _tabs) out.push_back(tab ? tab->text() : acul::string{});
            return out;
        }

        iterator begin() { return _tabs.begin(); }
        iterator end() { return _tabs.end(); }
        const_iterator begin() const { return _tabs.begin(); }
        const_iterator end() const { return _tabs.end(); }
        const_iterator cbegin() const { return _tabs.cbegin(); }
        const_iterator cend() const { return _tabs.cend(); }
        bool empty() const { return _tabs.empty(); }
        size_t child_size() const { return _tabs.size(); }
        value_type front() { return _tabs.front(); }
        value_type back() { return _tabs.back(); }
        const value_type front() const { return _tabs.front(); }
        const value_type back() const { return _tabs.back(); }
        value_type *data() { return _tabs.data(); }
        const value_type *data() const { return _tabs.data(); }

        TabBarFlags tab_flags() const { return _tab_flags; }
        bool content_width_fit() const { return is_size_fit(requested_size().x); }
        bool clipped() const { return !(_tab_flags & g_tab_bar_visual_mask) && !content_width_fit(); }
        bool popup() const { return _tab_flags & TabBarFlagBits::popup; }
        bool scroll() const { return _tab_flags & TabBarFlagBits::scroll; }
        bool multiple() const { return _tab_flags & TabBarFlagBits::multiple; }
        bool movable() const { return _tab_flags & TabBarFlagBits::movable; }
        bool closable() const { return _tab_flags & TabBarFlagBits::closable; }

        AUIK_EXPORT u32 selected_index() const;
        AUIK_EXPORT u32 selected_id() const;
        acul::vector<u32> selected_ids() const;
        AUIK_EXPORT void set_selected(u32 element_id);
        AUIK_EXPORT void set_selected(const acul::vector<u32> &element_ids);
        AUIK_EXPORT void set_selected_silent(u32 element_id);
        AUIK_EXPORT bool is_selected(u32 element_id) const;
        AUIK_EXPORT u32 insertion_index_at(const amal::vec2 &point) const;
        const amal::vec2 &drag_grab_offset() const { return _drag_grab_offset; }
        bool has_drag_grab_offset() const { return _drag_grab_offset_valid; }
        AUIK_EXPORT void begin_external_drag(u32 element_id);
        AUIK_EXPORT void cancel_drag();
        u32 item_style_tag() const { return _item_style_tag; }
        u32 selected_item_style_tag() const { return _selected_item_style_tag; }
        u32 popup_item_style_tag() const { return _popup_item_style_tag; }
        f32 tab_width() const { return _tab_width; }
        u32 tab_width_key() const { return _tab_width_key; }
        AUIK_EXPORT void set_tab_width(f32 value);
        AUIK_EXPORT void set_tab_width_key(u32 key);
        f32 scroll_offset() const { return _scroll_offset; }
        void set_scroll_offset(f32 value) { _scroll_offset = amal::max(value, 0.0f); }
        virtual u32 signature() const override { return AUIK_TAG_TAB_BAR; }

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
        AUIK_EXPORT void close_item(u32 element_id);
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
        AUIK_EXPORT StyleUpdateFlags update_item_state(u32 index, const detail::WidgetStyleSelectorTransition &transition);
        AUIK_EXPORT StyleUpdateFlags update_tab_item_style(u32 index, const detail::WidgetStyleSelectorTransition &transition);
        AUIK_EXPORT StyleUpdateFlags update_close_button_style(u32 index, const detail::WidgetStyleSelectorTransition &transition);
        AUIK_EXPORT StyleUpdateFlags update_popup_item_style(u32 index, const detail::WidgetStyleSelectorTransition &transition);
        AUIK_EXPORT void update_layout_from_current_bounds(bool min_size_known);
        AUIK_EXPORT f32 resolve_tab_width() const;
        AUIK_EXPORT amal::vec2 resolve_tab_required_size(u32 index);
        AUIK_EXPORT void ensure_overflow_icon_resources();
        AUIK_EXPORT amal::vec2 measure_overflow_size();
        AUIK_EXPORT void rebuild_from_model_binding();

        acul::vector<u32> _element_ids;
        acul::vector<detail::Selectable *> _tabs;
        acul::vector<ImageButton *> _close_buttons;
        detail::PopupTrigger *_overflow_button = nullptr;
        Window *_popup = nullptr;
        const TabBarFlags _tab_flags;
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
        f32 _tab_width = 0.0f;
        f32 _resolved_tab_width = 0.0f;
        u32 _tab_width_key = 0u;
        u32 _item_style_tag = AUIK_STYLE_TAG_TAB_BAR_ITEM;
        u32 _selected_item_style_tag = AUIK_STYLE_TAG_TAB_BAR_ITEM_SELECTED;
        u32 _popup_item_style_tag = AUIK_STYLE_TAG_COMBO_BOX_ITEM;
        u16 _full_clip_id = 0xFFFFu;
        u16 _content_clip_id = 0xFFFFu;
        StyleSelector _style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_GLOBAL};
        ModelBinding *_model_binding = nullptr;
    };

    inline TabBar *make_tab_bar(u32 id, acul::vector<acul::string> items = {},
                                TabBarFlags tab_flags = TabBarFlagBits::none,
                                amal::vec2 size = AUIK_SIZE_FIT, f32 tab_width = 0.0f,
                                u32 tab_width_key = 0u)
    {
        return acul::alloc<TabBar>(id, std::move(items), tab_flags, size, get_default_tab_bar_flags(),
                                   nullptr, tab_width, tab_width_key);
    }

    inline TabBar *make_tab_bar(u32 id, const acul::vector<StringView> &items,
                                TabBarFlags tab_flags = TabBarFlagBits::none,
                                amal::vec2 size = AUIK_SIZE_FIT, f32 tab_width = 0.0f,
                                u32 tab_width_key = 0u)
    {
        return acul::alloc<TabBar>(id, items, tab_flags, size, get_default_tab_bar_flags(), nullptr, tab_width,
                                   tab_width_key);
    }

    namespace streams
    {
        extern AUIK_EXPORT const umbf::streams::Stream tab_bar;
        extern AUIK_EXPORT const umbf::streams::Stream popup_menu;
    }
} // namespace auik
