#pragma once

#include "../theme.hpp"
#include "widget.hpp"

#define AUIK_TAG_COMBO_BOX       0x48F5BBCAu
#define AUIK_TAG_COMBO_BOX_ICON  0xAF09C8C6u
#define AUIK_TAG_COMBO_BOX_POPUP 0xC94B7B61u
#define AUIK_TAG_COMBO_BOX_ITEM  0xF2B7E06Eu

namespace auik::v2
{
    class Text;
    class Window;
    namespace detail
    {
        class PopupTrigger;
    }

    constexpr inline WidgetFlags get_default_combo_box_flags()
    {
        return get_default_widget_flags() | WidgetFlagBits::hittable;
    }
    constexpr inline WidgetFlags get_default_fixed_combo_box_flags()
    {
        return get_default_combo_box_flags() | WidgetFlagBits::fixed;
    }

    class APPLIB_API ComboBox final : public Widget
    {
    public:
        ComboBox(u32 id, acul::vector<acul::string> items = {}, u32 selected_index = 0u,
                 amal::vec2 size = {0.0f, 0.0f}, WidgetFlags widget_flags = get_default_combo_box_flags(),
                 Widget *parent = nullptr);
        ~ComboBox() override;

        StyleUpdateFlags update_style() override;
        void update_layout_min_size() override;
        void update_layout(bool min_size_known) override;
        void translate(const amal::vec2 &delta) override;
        void rebuild_clip_rects() override;
        void update_depth(const amal::vec2 &depth_range) override;
        void draw(DrawCtx &ctx) override;
        void on_focus(bool focused) override;
        void on_click(MouseKey key, KeyPressState state, u32 click_count) override;

        acul::vector<acul::string> items() const;
        void set_items(const acul::vector<acul::string>& items);

        u32 selected_index() const { return _selected_index; }
        const acul::string &selected_text() const;
        void set_selected_index(u32 index);

        bool is_open() const { return _open; }
        void open();
        void close();
        void toggle();

    private:
        void rebuild_control_layout();
        void sync_label_text();
        void update_popup_layout();
        void schedule_outside_click_tick();
        void tick_outside_click();
        bool has_draw_record() const;

        u32 _selected_index = 0u;
        bool _open = false;

        StyleSelector _style{Theme::STYLE_ID_INVALID, AUIK_TAG_COMBO_BOX};
        detail::PopupTrigger *_trigger = nullptr;
        Text *_label = nullptr;
        Window *_popup = nullptr;

        amal::rect _label_rect{};
        amal::vec2 _content_depth_range{0.0f, 1.0f};
    };

    class APPLIB_API MultipleComboBox final : public Widget
    {
    public:
        MultipleComboBox(u32 id, acul::vector<acul::string> items = {}, acul::string placeholder = {},
                         amal::vec2 size = {0.0f, 0.0f}, WidgetFlags widget_flags = get_default_combo_box_flags(),
                         Widget *parent = nullptr);
        ~MultipleComboBox() override;

        StyleUpdateFlags update_style() override;
        void update_layout_min_size() override;
        void update_layout(bool min_size_known) override;
        void translate(const amal::vec2 &delta) override;
        void rebuild_clip_rects() override;
        void update_depth(const amal::vec2 &depth_range) override;
        void draw(DrawCtx &ctx) override;
        void on_focus(bool focused) override;
        void on_click(MouseKey key, KeyPressState state, u32 click_count) override;

        acul::vector<acul::string> items() const;
        void set_items(const acul::vector<acul::string> &items);
        const acul::string &placeholder() const { return _placeholder; }
        void set_placeholder(acul::string value);

        const acul::vector<u32> &selected_indices() const { return _selected_indices; }
        void set_selected_indices(const acul::vector<u32> &indices);
        bool is_selected(u32 index) const;

        bool is_open() const { return _open; }
        void open();
        void close();
        void toggle();

    private:
        void rebuild_control_layout();
        void sync_label_text();
        void update_popup_layout();
        void schedule_outside_click_tick();
        void tick_outside_click();
        bool has_draw_record() const;

        acul::vector<u32> _selected_indices;
        acul::string _placeholder;
        bool _open = false;
        StyleSelector _style{Theme::STYLE_ID_INVALID, AUIK_TAG_COMBO_BOX};
        detail::PopupTrigger *_trigger = nullptr;
        Text *_label = nullptr;
        Window *_popup = nullptr;
        amal::rect _label_rect{};
        amal::vec2 _content_depth_range{0.0f, 1.0f};
    };

    inline ComboBox *make_combo_box(u32 id, acul::vector<acul::string> items = {}, u32 selected_index = 0u)
    {
        return acul::alloc<ComboBox>(id, std::move(items), selected_index, amal::vec2{0.0f, 0.0f},
                                     get_default_combo_box_flags());
    }

    inline ComboBox *make_fixed_combo_box(u32 id, acul::vector<acul::string> items = {}, u32 selected_index = 0u,
                                          amal::vec2 size = {0.0f, 0.0f})
    {
        return acul::alloc<ComboBox>(id, std::move(items), selected_index, size, get_default_fixed_combo_box_flags());
    }

    inline MultipleComboBox *make_multiple_combo_box(u32 id, acul::vector<acul::string> items = {},
                                                     acul::string placeholder = {})
    {
        return acul::alloc<MultipleComboBox>(id, std::move(items), std::move(placeholder), amal::vec2{0.0f, 0.0f},
                                             get_default_combo_box_flags());
    }

    inline MultipleComboBox *make_fixed_multiple_combo_box(u32 id, acul::vector<acul::string> items = {},
                                                           acul::string placeholder = {},
                                                           amal::vec2 size = {0.0f, 0.0f})
    {
        return acul::alloc<MultipleComboBox>(id, std::move(items), std::move(placeholder), size,
                                             get_default_fixed_combo_box_flags());
    }
} // namespace auik::v2
