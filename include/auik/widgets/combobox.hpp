#pragma once

#include "../model.hpp"
#include "../theme.hpp"
#include "widget.hpp"

#define AUIK_TAG_COMBO_BOX            0x48F5BBCAu
#define AUIK_TAG_COMBO_BOX_ICON       0xAF09C8C6u
#define AUIK_TAG_COMBO_BOX_POPUP      0xC94B7B61u
#define AUIK_TAG_COMBO_BOX_ITEM       0xF2B7E06Eu
#define AUIK_TAG_MULTIPLE_COMBO_BOX   0x9B26807Du
#define AUIK_COMBO_BOX_TEXT_FIELD     1u
#define AUIK_COMBO_BOX_SELECTED_FIELD 2u

namespace auik
{
    class Text;
    class Window;
    namespace detail
    {
        class PopupTrigger;

        constexpr inline WidgetFlags get_combobox_widget_flags()
        {
            return WidgetFlagBits::visible | WidgetFlagBits::attachable | WidgetFlagBits::configurable |
                   WidgetFlagBits::hittable;
        }
    } // namespace detail

    class Combobox final : public Widget
    {
    public:
        AUIK_EXPORT Combobox(u32 id, const acul::vector<StringView> &items, u32 selected_index, amal::vec2 inline_size,
                             WidgetFlags widget_flags);
        AUIK_EXPORT ~Combobox() override;

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
        AUIK_EXPORT void set_model_binding(ModelBinding *binding);
        AUIK_EXPORT void set_items(const acul::vector<StringView> &items);
        AUIK_EXPORT void add_item(StringView item);
        AUIK_EXPORT void add_items(const acul::vector<StringView> &items);

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
        void rebuild_from_model_binding();
        void request_model_selected_index(u32 index);

        u32 _selected_index = 0u;
        bool _open = false;

        StyleSelector _style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_COMBO_BOX};
        detail::PopupTrigger *_trigger = nullptr;
        Text *_label = nullptr;
        Window *_popup = nullptr;

        amal::rect _label_rect{};
        amal::vec2 _content_depth_range{0.0f, 1.0f};
        ModelBinding *_model_binding = nullptr;
    };

    class MultipleCombobox final : public Widget
    {
    public:
        AUIK_EXPORT MultipleCombobox(u32 id, const acul::vector<StringView> &items, StringView placeholder,
                                     amal::vec2 inline_size,
                                     WidgetFlags widget_flags);
        AUIK_EXPORT ~MultipleCombobox() override;

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

    inline Combobox *make_combobox(u32 id, const acul::vector<StringView> &items = {}, u32 selected_index = 0u,
                                   amal::vec2 inline_size = AUIK_SIZE_INHERIT)
    { return acul::alloc<Combobox>(id, items, selected_index, inline_size, detail::get_combobox_widget_flags()); }

    inline Model *make_combobox_value_model(ModelDB *db, ModelID model_id, const acul::vector<StringView> &items,
                                            u32 selected_index = 0u)
    {
        if (!db) return nullptr;
        if (model_id == 0u) model_id = make_generated_model_id();
        if (find_model(db, model_id)) return nullptr;

        Model model{};
        model.make_record_id_cb = make_generated_model_record_id;
        for (u32 i = 0; i < items.size(); ++i)
        {
            ModelRecord record{};
            add_model_field(record, make_model_field<acul::string>(
                                        AUIK_COMBO_BOX_TEXT_FIELD, items[i].str ? items[i].str : ""));
            add_model_field(record, make_model_field<bool>(AUIK_COMBO_BOX_SELECTED_FIELD, i == selected_index));
            model.add_record(std::move(record));
        }

        if (!register_model(db, model_id, std::move(model), destroy_model_fields)) return nullptr;
        return find_model(db, model_id);
    }

    inline ModelBinding *make_combobox_value_model_binding(ModelID model_id)
    { return make_model_binding(model_id); }

    inline MultipleCombobox *make_multiple_combobox(u32 id, const acul::vector<StringView> &items = {},
                                                     StringView placeholder = {},
                                                     amal::vec2 inline_size = AUIK_SIZE_INHERIT)
    { return acul::alloc<MultipleCombobox>(id, items, placeholder, inline_size, detail::get_combobox_widget_flags()); }

    namespace streams
    {
        extern AUIK_EXPORT const umbf::streams::Stream combobox;
        extern AUIK_EXPORT const umbf::streams::Stream multiple_combobox;
    } // namespace streams
} // namespace auik
