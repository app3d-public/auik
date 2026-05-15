#pragma once

#include "../text.hpp"

#define AUIK_TAG_COMBO_BOX_ITEM_SELECTED 0xB7EA954Du

namespace auik::v2::detail
{
    constexpr inline WidgetFlags get_selectable_item_flags()
    {
        return WidgetFlagBits::visible | WidgetFlagBits::hittable;
    }

    class APPLIB_API Selectable final : public Text
    {
    public:
        Selectable(u32 id, u32 tag_id, u32 element_id, const acul::string &text, amal::vec2 size, Widget *parent,
                   u32 style_tag_id, WidgetFlags flags, u32 selected_style_tag_id = AUIK_TAG_COMBO_BOX_ITEM_SELECTED,
                   StyleState selected_style_state = StyleState::normal)
            : Text(AUIK_TAG_TEXT, text, size, flags & ~WidgetFlagBits::attachable, parent, style_tag_id),
              _selected_style({Theme::STYLE_ID_INVALID, selected_style_tag_id}),
              _selected_style_state(selected_style_state)
        {
            _rect.tag_id = tag_id;
            _rect.element_id = element_id;
            set_vertical_align(TextVerticalAlign::center);
        }

        StyleUpdateFlags update_style() override;
        void rebuild_clip_rects() override;
        void update_layout(bool min_size_known) override;
        void translate(const amal::vec2 &delta) override;
        void reset_draw_records() override;
        void update_depth(const amal::vec2 &depth_range) override;
        void draw(DrawCtx &ctx) override;
        bool selected() const { return _selected; }
        void set_selected(bool value) { _selected = value; }
        void set_style_tag(u32 tag_id)
        {
            _rect.tag_id = tag_id;
            _style = {Theme::STYLE_ID_INVALID, tag_id};
        }
        void set_selected_style_tag(u32 tag_id) { _selected_style = {Theme::STYLE_ID_INVALID, tag_id}; }

    private:
        DrawDataID _bg{};
        DrawDataID _selected_bg{};
        StyleSelector _selected_style{};
        StyleState _selected_style_state = StyleState::normal;
        f32 _bg_z = 0.0f;
        f32 _selected_bg_z = 0.0f;
        bool _selected = false;

        void update_content_clip_rect();
    };
} // namespace auik::v2::detail
