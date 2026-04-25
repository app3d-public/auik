#pragma once

#include "../text.hpp"

namespace auik::v2::detail
{
    class APPLIB_API Selectable final : public Text
    {
    public:
        Selectable(u32 id, u32 tag_id, u32 element_id, const acul::string &text, amal::vec2 size, Widget *parent,
                   u32 style_tag_id, WidgetFlags flags)
            : Text(AUIK_TAG_TEXT, text, size, flags, parent, style_tag_id)
        {
            _rect.tag_id = tag_id;
            _rect.element_id = element_id;
            set_vertical_align(TextVerticalAlign::center);
        }

        StyleUpdateFlags update_style() override;
        void rebuild_clip_rects() override;
        void update_layout(bool min_size_known) override;
        void update_depth(const amal::vec2 &depth_range) override;
        void draw(DrawCtx &ctx) override;

    private:
        DrawDataID _bg{};
        f32 _bg_z = 0.0f;
    };
} // namespace auik::v2::detail
