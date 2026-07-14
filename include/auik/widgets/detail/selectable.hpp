#pragma once

#include "../text.hpp"

#define AUIK_TAG_COMBO_BOX_ITEM_SELECTED 0xB7EA954Du

namespace auik::detail
{
    struct SelectableStyleScope;
    constexpr inline u32 AUIK_SELECTABLE_STYLE_NONE = 0xFFFFu;

    struct SelectableStyleOptions
    {
        u32 tag_id = AUIK_SELECTABLE_STYLE_NONE;
        u32 icon_id = AUIK_SELECTABLE_STYLE_NONE;
        u32 item_tag_id = AUIK_SELECTABLE_STYLE_NONE;
    };

    constexpr inline SelectableStyleOptions make_selectable_highlight_options(u32 tag_id = AUIK_SELECTABLE_STYLE_NONE)
    { return {tag_id, AUIK_SELECTABLE_STYLE_NONE}; }

    constexpr inline SelectableStyleOptions make_selectable_icon_options(u32 icon_id = AUIK_ICON_CHECKMARK)
    { return {AUIK_SELECTABLE_STYLE_NONE, icon_id}; }

    constexpr inline SelectableStyleOptions
    make_selectable_multi_icon_options(u32 icon_id = AUIK_ICON_CHECKMARK,
                                       u32 item_tag_id = AUIK_STYLE_TAG_COMBO_BOX_ITEM_MULTI)
    { return {AUIK_SELECTABLE_STYLE_NONE, icon_id, item_tag_id}; }

    constexpr inline WidgetFlags get_selectable_item_flags()
    { return WidgetFlagBits::visible | WidgetFlagBits::hittable; }

    class Selectable : public Text
    {
    public:
        Selectable(ElementID id, StringView text, bool selected, amal::vec2 size, Widget *parent, WidgetFlags flags)
            : Text(id.widget_id, text, size, flags), _selected(selected)
        {
            set_parent(parent);
            _rect.id = id;
        }

        AUIK_EXPORT StyleUpdateFlags update_style() override;
        AUIK_EXPORT void update_layout_min_size() override;
        AUIK_EXPORT void rebuild_clip_rects() override;
        AUIK_EXPORT void update_layout(bool min_size_known) override;
        AUIK_EXPORT void translate(const amal::vec2 &delta) override;
        AUIK_EXPORT void reset_draw_records() override;
        u32 get_depth_requirement() const override { return 3u; }
        AUIK_EXPORT void update_depth(const amal::vec2 &depth_range) override;
        AUIK_EXPORT void draw(DrawCtx &ctx) override;
        bool has_draw_record() const
        {
            if (Text::draw_record_count() == 0) return false;
            if (_selected_bg.render_id == AUIK_INVALID_DRAW_DATA_ID) return false;
            if (selected_icon_enabled() && _selected_icon_draw.render_id == AUIK_INVALID_DRAW_DATA_ID) return false;
            return true;
        }

        bool selected() const { return _selected; }
        void set_selected(bool value)
        {
            if (_selected == value) return;
            _selected = value;
        }
        AUIK_EXPORT void set_selected_style_options(const SelectableStyleOptions *options);
        void set_style_tag(u32 tag_id)
        {
            _base_style_tag_id = tag_id;
            _style = {Theme::STYLE_ID_INVALID, active_item_style_tag()};
        }
        void set_selected_style_tag(u32 tag_id)
        {
            _selected_style_options_storage = make_selectable_highlight_options(tag_id);
            set_selected_style_options(&_selected_style_options_storage);
        }

        void set_selected_icon(u32 icon_id)
        {
            _selected_style_options_storage = make_selectable_multi_icon_options(icon_id);
            set_selected_style_options(&_selected_style_options_storage);
        }
    private:
        friend struct SelectableStyleScope;

        DrawDataID _bg{};
        DrawDataID _selected_bg{};
        DrawDataID _selected_icon_draw{};
        StyleSelector _selected_style{};
        SelectableStyleOptions _selected_style_options_storage{};
        const SelectableStyleOptions *_selected_style_options = nullptr;
        u32 _base_style_tag_id = AUIK_SELECTABLE_STYLE_NONE;
        TextureID _selected_icon_texture{};
        amal::rect _selected_icon_uv_rect{{0.0f, 0.0f}, {1.0f, 1.0f}};
        amal::vec2 _selected_icon_size{0.0f, 0.0f};
        RectData _selected_icon_rect{};
        f32 _bg_z = 0.0f;
        f32 _selected_bg_z = 0.0f;
        f32 _selected_icon_z = 0.0f;
        StyleID _layout_style_id = Theme::STYLE_ID_INVALID;
        bool _selected = false;

        StyleID effective_layout_style_id() const
        { return _layout_style_id != Theme::STYLE_ID_INVALID ? _layout_style_id : _style.id; }

        u32 active_item_style_tag() const
        {
            if (_selected_style_options && _selected_style_options->item_tag_id != AUIK_SELECTABLE_STYLE_NONE)
                return _selected_style_options->item_tag_id;
            return _base_style_tag_id;
        }

        bool selected_style_enabled() const
        { return _selected_style_options && _selected_style_options->tag_id != AUIK_SELECTABLE_STYLE_NONE; }

        bool selected_icon_enabled() const
        { return _selected_style_options && _selected_style_options->icon_id != AUIK_SELECTABLE_STYLE_NONE; }
        void ensure_selected_icon_resource();
        amal::vec2 selected_icon_size(const Style &style)
        {
            if (_selected_icon_size.x > 0.0f && _selected_icon_size.y > 0.0f) return _selected_icon_size;
            return {style.text_size(), style.text_size()};
        }

        f32 selected_icon_slot_width(const Style &style)
        {
            if (!selected_icon_enabled()) return 0.0f;
            ensure_selected_icon_resource();
            return selected_icon_size(style).x + amal::max(style.inline_spacing(), 8.0f);
        }

        void rebuild_selected_icon_layout(const Style &style);
        void draw_selected_icon(DrawCtx &ctx);
        void update_content_clip_rect()
        {
            const u16 next_clip_id = parent() ? parent()->content_clip_id() : clip_id();
            if (next_clip_id == 0xFFFFu) return;
            set_clip_id(next_clip_id);
        }
    };
} // namespace auik::detail
