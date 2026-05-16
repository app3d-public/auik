#pragma once

#include "widget.hpp"

#define AUIK_TAG_RUBBER_BAND 0x46F839BFu

namespace auik::v2
{
    enum class RubberBandMatchMode : u8
    {
        overlap,
        contains
    };

    struct RubberBandSearch
    {
        Widget **widgets = nullptr;
        u32 count = 0;
        amal::rect rect = {};
        RubberBandMatchMode mode = RubberBandMatchMode::overlap;
    };

    class APPLIB_API RubberBand final : public Widget
    {
    public:
        explicit RubberBand(u32 id, WidgetFlags widget_flags = WidgetFlagBits::none, Widget *parent = nullptr);

        bool active() const { return _active; }
        bool committed() const { return _committed; }
        const amal::rect &selection_rect() const { return _selection_rect; }
        void clear_commit() { _committed = false; }

        void filter_elements(RubberBandSearch &search) const;

        StyleUpdateFlags update_style() override;
        void update_layout_min_size() override;
        void update_layout(bool min_size_known) override;
        void translate(const amal::vec2 &delta) override;
        void rebuild_clip_rects() override;
        void update_depth(const amal::vec2 &depth_range) override;
        void draw(DrawCtx &ctx) override;
        void on_drag(const amal::vec2 &delta, KeyPressState state) override;

    private:
        static amal::rect make_rect_from_points(const amal::vec2 &a, const amal::vec2 &b);
        bool has_draw_record() const { return _draw_id.render_id != AUIK_INVALID_DRAW_DATA_ID; }
        void update_selection_rect();
        void redraw_band();

        StyleSelector _style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_RUBBER_BAND};
        DrawDataID _draw_id;
        amal::vec2 _start{0.0f, 0.0f};
        amal::vec2 _end{0.0f, 0.0f};
        amal::rect _selection_rect{};
        bool _active = false;
        bool _committed = false;
    };

} // namespace auik::v2
