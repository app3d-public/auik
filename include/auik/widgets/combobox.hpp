#pragma once

#include "../theme.hpp"
#include "widget.hpp"

#define AUIK_TAG_COMBO_BOX          0x48F5BBCAu
#define AUIK_TAG_COMBO_BOX_ICON     0xAF09C8C6u
#define AUIK_TAG_COMBO_BOX_POPUP    0xC94B7B61u
#define AUIK_TAG_COMBO_BOX_ITEM     0xF2B7E06Eu
#define AUIK_TAG_MULTIPLE_COMBO_BOX 0x9B26807Du

namespace auik
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
    class ComboBox final : public Widget
    {
    public:
        AUIK_EXPORT ComboBox(u32 id, acul::vector<acul::string> items = {}, u32 selected_index = 0u,
                             amal::vec2 size = {0.0f, 0.0f}, WidgetFlags widget_flags = get_default_combo_box_flags(),
                             Widget *parent = nullptr);
        AUIK_EXPORT ComboBox(u32 id, const acul::vector<StringView> &items, u32 selected_index = 0u,
                             amal::vec2 size = {0.0f, 0.0f}, WidgetFlags widget_flags = get_default_combo_box_flags(),
                             Widget *parent = nullptr);
        AUIK_EXPORT ~ComboBox() override;

        AUIK_EXPORT StyleUpdateFlags update_style() override;
        AUIK_EXPORT void update_layout_min_size() override;
        AUIK_EXPORT void update_layout(bool min_size_known) override;
        AUIK_EXPORT void translate(const amal::vec2 &delta) override;
        AUIK_EXPORT void rebuild_clip_rects() override;
        AUIK_EXPORT void update_depth(const amal::vec2 &depth_range) override;
        AUIK_EXPORT void back_hit_depth() override;
        AUIK_EXPORT void restore_hit_depth() override;
        AUIK_EXPORT void draw(DrawCtx &ctx) override;
        AUIK_EXPORT void on_focus(bool focused) override;
        AUIK_EXPORT void on_click(MouseKey key, KeyPressState state, u32 click_count) override;

        acul::vector<acul::string> items() const;
        AUIK_EXPORT acul::vector<StringView> item_text_views() const;
        AUIK_EXPORT void set_items(const acul::vector<acul::string> &items);
        AUIK_EXPORT void set_items(const acul::vector<StringView> &items);

        u32 selected_index() const { return _selected_index; }
        AUIK_EXPORT const acul::string &selected_text() const;
        AUIK_EXPORT void set_selected_index(u32 index);
        u32 style_tag() const { return _style.tag_id; }
        void set_style_tag(u32 tag_id)
        {
            _style = {Theme::STYLE_ID_INVALID, tag_id};
            set_rect_tag_id(tag_id);
        }

        bool is_open() const { return _open; }
        AUIK_EXPORT void open();
        AUIK_EXPORT void close();
        AUIK_EXPORT void toggle();
        virtual u32 signature() const override { return AUIK_TAG_COMBO_BOX; }

    private:
        void rebuild_control_layout();
        void sync_label_text();
        void update_popup_layout();
        void schedule_outside_click_tick();
        void tick_outside_click();
        bool has_draw_record() const;

        u32 _selected_index = 0u;
        bool _open = false;

        StyleSelector _style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_COMBO_BOX};
        detail::PopupTrigger *_trigger = nullptr;
        Text *_label = nullptr;
        Window *_popup = nullptr;

        amal::rect _label_rect{};
        amal::vec2 _content_depth_range{0.0f, 1.0f};
    };

    class MultipleComboBox final : public Widget
    {
    public:
        AUIK_EXPORT MultipleComboBox(u32 id, acul::vector<acul::string> items = {}, acul::string placeholder = {},
                                     amal::vec2 size = {0.0f, 0.0f},
                                     WidgetFlags widget_flags = get_default_combo_box_flags(),
                                     Widget *parent = nullptr);
        AUIK_EXPORT ~MultipleComboBox() override;

        AUIK_EXPORT StyleUpdateFlags update_style() override;
        AUIK_EXPORT void update_layout_min_size() override;
        AUIK_EXPORT void update_layout(bool min_size_known) override;
        AUIK_EXPORT void translate(const amal::vec2 &delta) override;
        AUIK_EXPORT void rebuild_clip_rects() override;
        AUIK_EXPORT void update_depth(const amal::vec2 &depth_range) override;
        AUIK_EXPORT void back_hit_depth() override;
        AUIK_EXPORT void restore_hit_depth() override;
        AUIK_EXPORT void draw(DrawCtx &ctx) override;
        AUIK_EXPORT void on_focus(bool focused) override;
        AUIK_EXPORT void on_click(MouseKey key, KeyPressState state, u32 click_count) override;

        acul::vector<acul::string> items() const;
        AUIK_EXPORT acul::vector<StringView> item_text_views() const;
        AUIK_EXPORT void set_items(const acul::vector<acul::string> &items);
        AUIK_EXPORT void set_items(const acul::vector<StringView> &items);
        const acul::string &placeholder() const { return _placeholder; }
        bool is_translated_placeholder() const { return _translated_placeholder; }
        const acul::string &placeholder_literal() const { return _placeholder_literal; }
        AUIK_EXPORT void set_placeholder(StringView value);
        AUIK_EXPORT void set_placeholder(acul::string value);

        const acul::vector<u32> &selected_indices() const { return _selected_indices; }
        AUIK_EXPORT void set_selected_indices(const acul::vector<u32> &indices);
        AUIK_EXPORT bool is_selected(u32 index) const;
        u32 style_tag() const { return _style.tag_id; }
        void set_style_tag(u32 tag_id)
        {
            _style = {Theme::STYLE_ID_INVALID, tag_id};
            set_rect_tag_id(tag_id);
        }

        bool is_open() const { return _open; }
        AUIK_EXPORT void open();
        AUIK_EXPORT void close();
        AUIK_EXPORT void toggle();
        virtual u32 signature() const override { return AUIK_TAG_MULTIPLE_COMBO_BOX; }

    private:
        void rebuild_control_layout();
        void sync_label_text();
        void update_popup_layout();
        void schedule_outside_click_tick();
        void tick_outside_click();
        bool has_draw_record() const;

        acul::vector<u32> _selected_indices;
        acul::string _placeholder;
        acul::string _placeholder_literal;
        bool _translated_placeholder = false;
        bool _open = false;
        StyleSelector _style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_COMBO_BOX};
        detail::PopupTrigger *_trigger = nullptr;
        Text *_label = nullptr;
        Window *_popup = nullptr;
        amal::rect _label_rect{};
        amal::vec2 _content_depth_range{0.0f, 1.0f};
    };

    inline ComboBox *make_combo_box(u32 id, acul::vector<acul::string> items = {}, u32 selected_index = 0u,
                                    amal::vec2 size = AUIK_SIZE_FIT)
    {
        return acul::alloc<ComboBox>(id, std::move(items), selected_index, size, get_default_combo_box_flags());
    }

    inline MultipleComboBox *make_multiple_combo_box(u32 id, acul::vector<acul::string> items = {},
                                                     acul::string placeholder = {},
                                                     amal::vec2 size = AUIK_SIZE_FIT)
    {
        return acul::alloc<MultipleComboBox>(id, std::move(items), std::move(placeholder), size,
                                             get_default_combo_box_flags());
    }

    namespace streams
    {
        extern AUIK_EXPORT const umbf::streams::Stream combo_box;
        extern AUIK_EXPORT const umbf::streams::Stream multiple_combo_box;
    }
} // namespace auik
