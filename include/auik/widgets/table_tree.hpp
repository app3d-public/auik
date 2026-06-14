#pragma once

#include "../post_effects.hpp"
#include "detail/table_base.hpp"
#include "table.hpp"
#include "text.hpp"
#include "widget.hpp"

#define AUIK_TAG_TABLE_TREE       0x933619CCu
#define AUIK_TAG_TABLE_TREE_ARROW 0x0FB27EE1u

#define AUIK_TABLE_TREE_FLAG_ALTERNATING_ROWS        0x1u
#define AUIK_TABLE_TREE_FLAG_COLUMN_RESIZABLE        0x2u
#define AUIK_TABLE_TREE_FLAG_RESIZE_INDICATOR_ACTIVE 0x4u
#define AUIK_TABLE_TREE_FLAG_COLUMN_SIZE_OVERRIDES   0x8u

namespace auik
{
    constexpr inline WidgetFlags get_default_table_tree_flags()
    {
        return get_default_widget_flags() | WidgetFlagBits::hittable;
    }

    class TableTree final : public Widget
    {
    public:
        using Row = acul::vector<acul::string>;
        static AUIK_EXPORT constexpr size_t invalid_node = static_cast<size_t>(-1);

        struct Node
        {
            acul::string label;
            Row cells;
            size_t parent = invalid_node;
            bool expanded = true;
        };

        AUIK_EXPORT explicit TableTree(u32 id, amal::vec2 size = {0.0f, 0.0f}, WidgetFlags flags = get_default_table_tree_flags(),
                           Widget *parent = nullptr, u32 style_tag_id = AUIK_STYLE_TAG_TABLE_TREE);
        AUIK_EXPORT ~TableTree() override;

        AUIK_EXPORT void clear();
        size_t add_node(acul::string label, Row cells = {}, size_t parent = invalid_node);
        AUIK_EXPORT void set_node_expanded(size_t node, bool expanded);
        AUIK_EXPORT bool node_expanded(size_t node) const;
        AUIK_EXPORT bool node_has_children(size_t node) const;

        AUIK_EXPORT void set_alternating_rows(bool value);
        bool alternating_rows() const
        {
            return detail::has_table_flag(_tree_flags, AUIK_TABLE_TREE_FLAG_ALTERNATING_ROWS);
        }
        AUIK_EXPORT void set_alternating_row_style_tag(u32 tag_id);
        u32 alternating_row_style_tag() const { return _alternating_row_style.tag_id; }

        AUIK_EXPORT void set_default_column_settings(TableColumnSettings settings);
        const TableColumnSettings &default_column_settings() const { return _default_column_settings; }
        AUIK_EXPORT void set_column_settings(acul::vector<TableColumnSettings> settings);
        AUIK_EXPORT void set_column_settings(size_t column, TableColumnSettings settings);
        AUIK_EXPORT void clear_column_settings();
        const acul::vector<TableColumnSettings> &column_settings() const { return _column_settings; }
        AUIK_EXPORT void set_column_resizable(bool value);
        bool column_resizable() const
        {
            return detail::has_table_flag(_tree_flags, AUIK_TABLE_TREE_FLAG_COLUMN_RESIZABLE);
        }
        AUIK_EXPORT void set_resize_border_style_tag(u32 tag_id);
        u32 resize_border_style_tag() const { return _resize_border_style.tag_id; }

        AUIK_EXPORT void set_style_tag(u32 tag_id);
        u32 style_tag() const { return _style.tag_id; }
        AUIK_EXPORT void set_cell_style_tag(u32 tag_id);
        u32 cell_style_tag() const { return _cell_style.tag_id; }

        const acul::vector<Node> &nodes() const { return _nodes; }
        size_t visible_row_count() const { return _visible_nodes.size(); }
        size_t column_count() const { return _column_count; }
        AUIK_EXPORT bool is_row_hovered(size_t visible_row) const;
        AUIK_EXPORT bool is_arrow_hovered(size_t node) const;
        AUIK_EXPORT bool is_resize_border_hovered(size_t element_id) const;

        AUIK_EXPORT StyleUpdateFlags update_style() override;
        AUIK_EXPORT void update_layout_min_size() override;
        AUIK_EXPORT void update_layout(bool min_size_known) override;
        AUIK_EXPORT void translate(const amal::vec2 &delta) override;
        AUIK_EXPORT void rebuild_clip_rects() override;
        AUIK_EXPORT void reset_draw_records() override;
        AUIK_EXPORT void update_depth(const amal::vec2 &depth_range) override;
        AUIK_EXPORT void back_hit_depth() override;
        AUIK_EXPORT void restore_hit_depth() override;
        AUIK_EXPORT void draw(DrawCtx &ctx) override;
        AUIK_EXPORT void on_click(MouseKey key, KeyPressState state, u32 click_count) override;
        AUIK_EXPORT void on_hover(HoverState state) override;
        AUIK_EXPORT void on_drag(const amal::vec2 &delta, KeyPressState state) override;
        u16 content_clip_id() const override { return clip_id(); }
        AUIK_EXPORT amal::vec4 get_content_clip_rect() const override;
        AUIK_EXPORT void on_attach() override;
        AUIK_EXPORT void on_detach() override;

    private:
        struct ArrowVisual
        {
            detail::RectData rect{};
            DrawDataID hit_draw{};
            DrawDataID icon_draw{};
            size_t node = invalid_node;
        };

        struct ArrowAnimation
        {
            size_t node = invalid_node;
            u32 rotate_post_id = AUIK_INVALID_POST_EFFECT_DATA_ID;
            DrawDataID draw{};
        };

        void rebuild_visible_nodes();
        void rebuild_cells();
        void clear_cells(bool invalidate_draw = true);
        void invalidate_visual_draw_records();
        void clear_tree_line_draw_records();
        void draw_tree_lines(DrawCtx &ctx, DrawStream *stream);
        Text *make_cell_text(const acul::string &value);
        size_t node_depth(size_t node) const;
        bool node_is_last_sibling(size_t node) const;
        size_t node_ancestor_at_depth(size_t node, size_t depth) const;
        size_t resolve_column_count() const;
        u32 cell_element_id(size_t visible_row, size_t column) const;
        Text *cell_text(size_t visible_row, size_t column) const;
        const TableColumnSettings &settings_for_column(size_t column) const;
        void update_column_widths(f32 inner_width);
        void resize_visuals();
        void update_cell_clip_rects();
        void sync_cell_parents();
        void invalidate_layout();
        void update_own_layout();
        StyleUpdateFlags update_resize_indicator();

        void ensure_arrow_resources();
        ArrowAnimation *find_arrow_animation(size_t node);
        const ArrowAnimation *find_arrow_animation(size_t node) const;
        void start_arrow_animation(size_t node, bool opening);
        void schedule_arrow_tick();
        void tick_arrow_animations();
        void clear_arrow_animation_draw(ArrowAnimation &animation);
        void release_arrow_animations();
        void draw_arrow(DrawCtx &ctx, ArrowVisual &visual);

        acul::vector<Node> _nodes;
        acul::vector<size_t> _visible_nodes;
        acul::vector<acul::vector<Text *>> _cells;
        acul::vector<acul::point2D<detail::TableTrackMetrics>> _layout_metrics;
        acul::vector<detail::TableCellVisual> _row_visuals;
        acul::vector<detail::TableCellVisual> _cell_visuals;
        acul::vector<detail::TableCellVisual> _alt_row_visuals;
        acul::vector<detail::TableCellVisual> _resize_border_hit_visuals;
        detail::TableCellVisual _resize_indicator_visual;
        acul::vector<QuadsInstanceData> _tree_line_data;
        acul::vector<DrawDataID> _tree_line_draws;
        acul::vector<ArrowVisual> _arrow_visuals;
        acul::vector<ArrowAnimation> _arrow_animations;
        DrawDataID _bg{};
        StyleSelector _style;
        StyleSelector _cell_style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_TABLE_TREE_CELL};
        StyleSelector _alternating_row_style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_TABLE_TREE_ROW_ALT};
        StyleSelector _line_style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_TABLE_TREE_LINE};
        StyleSelector _collapse_icon_style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_TABLE_TREE_COLLAPSE_ICON};
        StyleSelector _resize_border_style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_TABLE_TREE_RESIZE_BORDER};
        acul::vector<TableColumnSettings> _column_settings;
        acul::vector<acul::point2D<f32>> _size_overrides;
        acul::vector<acul::point2D<f32>> _resize_size_basis;
        TableColumnSettings _default_column_settings{};
        TextureID _arrow_texture{};
        amal::rect _arrow_uv_rect{{0.0f, 0.0f}, {1.0f, 1.0f}};
        amal::vec2 _arrow_size{0.0f, 0.0f};
        u32 _tree_flags = 0u;
        size_t _resizing_column = static_cast<size_t>(-1);
        amal::vec2 _resize_drag_accum{0.0f, 0.0f};
        size_t _column_count = 0u;
        f32 _indent_width = 16.0f;
        bool _arrow_tick_scheduled = false;
    };

    inline TableTree *make_table_tree(u32 id, Widget *parent = nullptr)
    {
        return acul::alloc<TableTree>(id, amal::vec2{0.0f, 0.0f}, get_default_table_tree_flags(), parent,
                                      AUIK_STYLE_TAG_TABLE_TREE);
    }

    inline TableTree *make_fixed_table_tree(u32 id, amal::vec2 size, Widget *parent = nullptr)
    {
        return acul::alloc<TableTree>(id, size, get_default_table_tree_flags() | WidgetFlagBits::fixed_layout, parent,
                                      AUIK_STYLE_TAG_TABLE_TREE);
    }
} // namespace auik
