#pragma once

#include <acul/pair.hpp>
#include <acul/vector.hpp>
#include "../theme.hpp"
#include "detail/table_base.hpp"
#include "widget.hpp"

#define AUIK_TAG_TABLE                 0xB82106B6u
#define AUIK_TAG_TABLE_ROW             0x3AB13A86u
#define AUIK_TAG_TABLE_COLUMN          0x729770E6u
#define AUIK_TAG_TABLE_HEADER_CELL     0xF2C1B19Cu
#define AUIK_TAG_TABLE_CELL            0xEDEBDD0Au
#define AUIK_TAG_TABLE_RESIZE_BORDER_V 0x817A4990u
#define AUIK_TAG_TABLE_RESIZE_BORDER_H 0x2F27A33Au

#define AUIK_TABLE_FLAG_ALTERNATING_ROWS        (1u << 0u)
#define AUIK_TABLE_FLAG_COLUMN_RESIZABLE        (1u << 1u)
#define AUIK_TABLE_FLAG_ROW_RESIZABLE           (1u << 2u)
#define AUIK_TABLE_FLAG_RESIZE_INDICATOR_ACTIVE (1u << 3u)
#define AUIK_TABLE_FLAG_COLUMN_SIZE_OVERRIDES   (1u << 4u)
#define AUIK_TABLE_FLAG_ROW_SIZE_OVERRIDES      (1u << 5u)

namespace auik
{
    constexpr inline WidgetFlags get_default_table_flags()
    {
        return get_default_widget_flags() | WidgetFlagBits::hittable;
    }

    enum class TableColumnSizing : u8
    {
        shrink,
        fixed,
        stretch
    };

    struct TableColumnSettings
    {
        TableColumnSizing sizing = TableColumnSizing::stretch;
        f32 value = 1.0f;
        f32 min_width = 0.0f;
        HAlign halign = HAlign::left;
        VAlign valign = VAlign::none;
    };

    class Table final : public Widget
    {
    public:
        using Row = acul::vector<Widget *>;
        using Rows = acul::vector<Row>;

        struct CellRef
        {
            Table *table = nullptr;
            size_t row_index = 0;
            size_t column_index = 0;

            size_t row() const { return row_index; }
            size_t column() const { return column_index; }
            AUIK_EXPORT bool valid() const;
            Widget *value() const;
        };

        struct ConstCellRef
        {
            const Table *table = nullptr;
            size_t row_index = 0;
            size_t column_index = 0;

            size_t row() const { return row_index; }
            size_t column() const { return column_index; }
            AUIK_EXPORT bool valid() const;
            AUIK_EXPORT const Widget *value() const;
        };

        struct CellIterator
        {
            Table *table = nullptr;
            size_t row_index = 0;
            size_t column_index = 0;

            CellRef operator*() const { return {table, row_index, column_index}; }
            CellIterator &operator++()
            {
                ++row_index;
                return *this;
            }
            bool operator==(const CellIterator &rhs) const
            {
                return table == rhs.table && row_index == rhs.row_index && column_index == rhs.column_index;
            }
            bool operator!=(const CellIterator &rhs) const { return !(*this == rhs); }
        };

        struct ConstCellIterator
        {
            const Table *table = nullptr;
            size_t row_index = 0;
            size_t column_index = 0;

            ConstCellRef operator*() const { return {table, row_index, column_index}; }
            ConstCellIterator &operator++()
            {
                ++row_index;
                return *this;
            }
            bool operator==(const ConstCellIterator &rhs) const
            {
                return table == rhs.table && row_index == rhs.row_index && column_index == rhs.column_index;
            }
            bool operator!=(const ConstCellIterator &rhs) const { return !(*this == rhs); }
        };

        struct Column
        {
            Table *table = nullptr;
            size_t column_index = 0;

            size_t column() const { return column_index; }
            Widget *header() const;
            Widget *cell(size_t row) const;
            AUIK_EXPORT TableColumnSettings *settings() const;
            AUIK_EXPORT f32 width() const;
            AUIK_EXPORT f32 *width_override() const;
            CellIterator begin() const { return {table, 0u, column_index}; }
            AUIK_EXPORT CellIterator end() const;
        };

        struct ConstColumn
        {
            const Table *table = nullptr;
            size_t column_index = 0;

            size_t column() const { return column_index; }
            AUIK_EXPORT const Widget *header() const;
            AUIK_EXPORT const Widget *cell(size_t row) const;
            AUIK_EXPORT const TableColumnSettings *settings() const;
            AUIK_EXPORT f32 width() const;
            AUIK_EXPORT const f32 *width_override() const;
            ConstCellIterator begin() const { return {table, 0u, column_index}; }
            AUIK_EXPORT ConstCellIterator end() const;
        };

        struct ColumnIterator
        {
            Table *table = nullptr;
            size_t column_index = 0;

            Column operator*() const { return {table, column_index}; }
            ColumnIterator &operator++()
            {
                ++column_index;
                return *this;
            }
            bool operator==(const ColumnIterator &rhs) const
            {
                return table == rhs.table && column_index == rhs.column_index;
            }
            bool operator!=(const ColumnIterator &rhs) const { return !(*this == rhs); }
        };

        struct ConstColumnIterator
        {
            const Table *table = nullptr;
            size_t column_index = 0;

            ConstColumn operator*() const { return {table, column_index}; }
            ConstColumnIterator &operator++()
            {
                ++column_index;
                return *this;
            }
            bool operator==(const ConstColumnIterator &rhs) const
            {
                return table == rhs.table && column_index == rhs.column_index;
            }
            bool operator!=(const ConstColumnIterator &rhs) const { return !(*this == rhs); }
        };

        struct ColumnList
        {
            Table *table = nullptr;

            ColumnIterator begin() const { return {table, 0u}; }
            AUIK_EXPORT ColumnIterator end() const;
        };

        struct ConstColumnList
        {
            const Table *table = nullptr;

            ConstColumnIterator begin() const { return {table, 0u}; }
            AUIK_EXPORT ConstColumnIterator end() const;
        };

        AUIK_EXPORT explicit Table(u32 id, Rows rows = {}, amal::vec2 size = {0.0f, 0.0f},
                                   WidgetFlags flags = get_default_table_flags(), Widget *parent = nullptr,
                                   u32 style_tag_id = AUIK_STYLE_TAG_TABLE);
        AUIK_EXPORT ~Table() override;

        AUIK_EXPORT void clear();
        AUIK_EXPORT void set_rows(Rows rows);
        AUIK_EXPORT void add_row(Row row);
        AUIK_EXPORT void set_cell(size_t row, size_t column, Widget *value);

        AUIK_EXPORT void set_header(Row header);
        AUIK_EXPORT void clear_header();
        bool has_header() const { return !_header.empty(); }

        AUIK_EXPORT void set_alternating_rows(bool value);
        bool alternating_rows() const { return (_table_flags & AUIK_TABLE_FLAG_ALTERNATING_ROWS) != 0u; }
        AUIK_EXPORT void set_alternating_row_style_tag(u32 tag_id);
        u32 alternating_row_style_tag() const { return _alternating_row_style.tag_id; }

        AUIK_EXPORT void set_default_column_settings(TableColumnSettings settings);
        const TableColumnSettings &default_column_settings() const { return _default_column_settings; }
        AUIK_EXPORT void set_column_settings(acul::vector<TableColumnSettings> settings);
        AUIK_EXPORT void set_column_settings(size_t column, TableColumnSettings settings);
        AUIK_EXPORT void clear_column_settings();
        const acul::vector<TableColumnSettings> &column_settings() const { return _column_settings; }

        AUIK_EXPORT void set_column_resizable(bool value);
        bool column_resizable() const { return (_table_flags & AUIK_TABLE_FLAG_COLUMN_RESIZABLE) != 0u; }
        AUIK_EXPORT void set_row_resizable(bool value);
        bool row_resizable() const { return (_table_flags & AUIK_TABLE_FLAG_ROW_RESIZABLE) != 0u; }
        AUIK_EXPORT void set_resize_border_style_tag(u32 tag_id);
        u32 resize_border_style_tag() const { return _resize_border_style.tag_id; }
        u32 table_flags() const { return _table_flags; }
        const acul::vector<acul::point2D<f32>> &size_overrides() const { return _size_overrides; }
        AUIK_EXPORT void set_size_overrides(acul::vector<acul::point2D<f32>> values, bool column_overrides,
                                            bool row_overrides);

        const Row &header() const { return _header; }
        const Rows &rows() const { return _rows; }
        size_t row_count() const { return _rows.size(); }
        size_t column_count() const { return _column_count; }
        ColumnList columns() { return {this}; }
        ConstColumnList columns() const { return {this}; }
        ColumnIterator begin() { return columns().begin(); }
        ColumnIterator end() { return columns().end(); }
        ConstColumnIterator begin() const { return columns().begin(); }
        ConstColumnIterator end() const { return columns().end(); }
        Column get_column(size_t column) { return {this, column}; }
        ConstColumn get_column(size_t column) const { return {this, column}; }
        CellRef get_row(size_t column, size_t row) { return {this, row, column}; }
        ConstCellRef get_row(size_t column, size_t row) const { return {this, row, column}; }
        AUIK_EXPORT void move_rows_before(const acul::vector<size_t> &rows, size_t target_row);
        AUIK_EXPORT void move_rows_after(const acul::vector<size_t> &rows, size_t target_row);
        AUIK_EXPORT void move_columns_before(const acul::vector<size_t> &columns, size_t target_column);
        AUIK_EXPORT void move_columns_after(const acul::vector<size_t> &columns, size_t target_column);

        AUIK_EXPORT void set_style_tag(u32 tag_id);
        u32 style_tag() const { return _style.tag_id; }
        AUIK_EXPORT void set_header_cell_style_tag(u32 tag_id);
        u32 header_cell_style_tag() const { return _header_cell_style.tag_id; }
        AUIK_EXPORT void set_cell_style_tag(u32 tag_id);
        u32 cell_style_tag() const { return _cell_style.tag_id; }

        AUIK_EXPORT bool is_header_cell_hovered(size_t column) const;
        AUIK_EXPORT bool is_cell_hovered(size_t row, size_t column) const;
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
        AUIK_EXPORT void on_hover(HoverState state) override;
        AUIK_EXPORT void on_drag(const amal::vec2 &delta, KeyPressState state) override;
        u16 content_clip_id() const override { return clip_id(); }
        AUIK_EXPORT amal::vec4 get_content_clip_rect() const override;
        AUIK_EXPORT void on_attach() override;
        AUIK_EXPORT void on_detach() override;
        virtual u32 signature() const override { return AUIK_TAG_TABLE; }

    private:
        using CellVisual = detail::TableCellVisual;
        using TrackMetrics = detail::TableTrackMetrics;

        void rebuild_cells();
        void clear_cells(bool invalidate_draw = true);
        size_t resolve_column_count() const;
        u32 cell_element_id(size_t row, size_t column) const;
        Widget *header_widget(size_t column) const;
        Widget *cell_widget(size_t row, size_t column) const;
        const TableColumnSettings &settings_for_column(size_t column) const;
        void update_column_widths(f32 inner_width);
        void invalidate_layout();
        void update_own_layout();
        void update_cell_clip_rects();
        void resize_visuals();
        void sync_cell_parents();
        StyleUpdateFlags update_resize_indicator();
        bool is_resize_border_tag(u32 tag_id) const;

        Row _header;
        Rows _rows;
        acul::vector<acul::point2D<TrackMetrics>> _layout_metrics;
        acul::vector<CellVisual> _header_visuals;
        acul::vector<CellVisual> _cell_visuals;
        acul::vector<CellVisual> _alt_row_visuals;
        acul::vector<acul::point2D<CellVisual>> _resize_border_hit_visuals;
        CellVisual _resize_indicator_visual;
        DrawDataID _bg{};
        StyleSelector _style;
        StyleSelector _header_cell_style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_TABLE_HEADER_CELL};
        StyleSelector _cell_style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_TABLE_CELL};
        StyleSelector _alternating_row_style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_TABLE_ROW_ALT};
        StyleSelector _resize_border_style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_TABLE_RESIZE_BORDER};
        acul::vector<TableColumnSettings> _column_settings;
        acul::vector<acul::point2D<f32>> _size_overrides;
        acul::vector<acul::point2D<f32>> _resize_size_basis;
        TableColumnSettings _default_column_settings{};
        u32 _table_flags = 0u;
        size_t _resizing_column = static_cast<size_t>(-1);
        size_t _resizing_row = static_cast<size_t>(-1);
        amal::vec2 _resize_drag_accum{0.0f, 0.0f};
        size_t _column_count = 0;
    };

    inline Table *make_table(u32 id, Table::Rows rows = {}, amal::vec2 size = AUIK_SIZE_FIT, Widget *parent = nullptr)
    {
        return acul::alloc<Table>(id, std::move(rows), size, get_default_table_flags(), parent, AUIK_STYLE_TAG_TABLE);
    }

    namespace streams
    {
        extern AUIK_EXPORT const umbf::streams::Stream table;
    }
} // namespace auik
