#pragma once

#include <acul/vector.hpp>
#include <amal/geometric.hpp>
#include "widget.hpp"

#define AUIK_TAG_GRID_LAYOUT                 0x86801E7Au
#define AUIK_TAG_GRID_LAYOUT_RESIZE_HELPER_V 0x7A9C3B1Eu
#define AUIK_TAG_GRID_LAYOUT_RESIZE_HELPER_H 0x57C2104Du

namespace auik
{
    class GridLayout final : public Widget
    {
    public:
        struct Cell
        {
            Widget *widget = nullptr;
            ChildLayoutFlags layout = default_child_layout_flags();
        };

        AUIK_EXPORT GridLayout(u32 id, size_t rows, size_t columns, amal::vec2 inline_size, WidgetFlags flags);
        AUIK_EXPORT ~GridLayout() override;

        size_t row_count() const { return _rows; }
        size_t column_count() const { return _columns; }
        AUIK_EXPORT void set_cell(size_t row, size_t column, Widget *widget,
                                  ChildLayoutFlags layout = default_child_layout_flags());
        AUIK_EXPORT void clear_cell(size_t row, size_t column);
        AUIK_EXPORT void clear();
        AUIK_EXPORT void set_column_resizable(bool value);
        AUIK_EXPORT void set_row_resizable(bool value);
        bool column_resizable() const { return _column_resizable; }
        bool row_resizable() const { return _row_resizable; }
        AUIK_EXPORT void set_track_min_size(amal::vec2 value);
        AUIK_EXPORT void set_resize_helper_style_tag(u32 tag_id);

        AUIK_EXPORT StyleUpdateFlags update_style() override;
        AUIK_EXPORT void update_layout_min_size_force() override;
        AUIK_EXPORT void update_layout(bool min_size_known) override;
        AUIK_EXPORT void translate(const amal::vec2 &delta) override;
        AUIK_EXPORT void reset_clip_rect_records() override;
        AUIK_EXPORT void rebuild_clip_rects() override;
        AUIK_EXPORT void reset_draw_records() override;
        AUIK_EXPORT u32 get_depth_requirement() const override;
        AUIK_EXPORT void update_depth(const amal::vec2 &depth_range) override;
        AUIK_EXPORT void draw(DrawCtx &ctx) override;
        AUIK_EXPORT void on_hover(HoverState state) override;
        AUIK_EXPORT void on_drag(const amal::vec2 &delta, KeyPressState state) override;
        AUIK_EXPORT void on_attach() override;
        AUIK_EXPORT void on_detach() override;
        u16 content_clip_id() const override { return clip_id(); }
        amal::vec4 get_content_clip_rect() const override { return get_clip_rect(clip_id()); }
        u32 signature() const override { return AUIK_TAG_GRID_LAYOUT; }

    private:
        struct ResizeHelper
        {
            detail::RectData rect{};
            DrawDataID draw{};
            amal::axis axis = amal::axis::x;
            size_t track = 0u;
            bool visible = false;
        };

        size_t cell_index(size_t row, size_t column) const { return row * _columns + column; }
        void attach_cell(Cell &cell);
        void detach_cell(Cell &cell);
        void sync_cells();
        void layout_cell(const amal::rect &bounds, Cell &cell);
        void resolve_tracks(const acul::vector<f32> &weights, acul::vector<f32> &sizes, f32 available, f32 min_size);
        void normalize_weights(acul::vector<f32> &weights);
        void update_resize_helpers();
        ResizeHelper *helper_from_element(u32 element_id);
        const ResizeHelper *helper_from_element(u32 element_id) const;

        size_t _rows = 0u;
        size_t _columns = 0u;
        acul::vector<Cell> _cells;
        acul::vector<f32> _row_weights;
        acul::vector<f32> _column_weights;
        acul::vector<f32> _row_sizes;
        acul::vector<f32> _column_sizes;
        acul::vector<f32> _resize_basis;
        acul::vector<ResizeHelper> _helpers;
        amal::vec2 _track_min_size{80.0f, 80.0f};
        amal::vec2 _resize_drag_accum{0.0f, 0.0f};
        StyleSelector _resize_helper_style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_DOCKSPACE_RESIZE_HELPER};
        StyleSelector _resize_helper_drag_style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_DOCKSPACE_RESIZE_HELPER_DRAG};
        size_t _resizing_helper = static_cast<size_t>(-1);
        bool _column_resizable = false;
        bool _row_resizable = false;
    };

    inline GridLayout *make_grid_layout(u32 id, size_t rows, size_t columns, amal::vec2 inline_size = AUIK_SIZE_INHERIT)
    {
        return acul::alloc<GridLayout>(id, rows, columns, inline_size,
                                       WidgetFlagBits::visible | WidgetFlagBits::attachable |
                                           WidgetFlagBits::configurable | WidgetFlagBits::hittable);
    }
} // namespace auik
