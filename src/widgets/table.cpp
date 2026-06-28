#include <auik/auik.hpp>
#include <auik/detail/rect.hpp>
#include <auik/detail/depth.hpp>
#include <auik/pipelines.hpp>
#include <auik/widgets/detail/draw_cull.hpp>
#include <auik/widgets/table.hpp>
#include <auik/widgets/text.hpp>
#include "../core/session_stream_utils.hpp"

namespace auik
{
    static inline bool has_table_flag(u32 flags, u32 flag) { return (flags & flag) != 0u; }

    static inline void set_table_flag(u32 &flags, u32 flag, bool value)
    {
        if (value) flags |= flag;
        else flags &= ~flag;
    }

    static inline void resize_size_points(acul::vector<acul::point2D<f32>> &values, size_t size)
    {
        const size_t old_size = values.size();
        values.resize(size);
        for (size_t index = old_size; index < size; ++index) values[index] = {0.0f, 0.0f};
    }

    static inline detail::TableColumnLayoutSettings to_layout_settings(const TableColumnSettings &settings)
    {
        u8 sizing = 2u;
        switch (settings.sizing)
        {
            case TableColumnSizing::shrink: sizing = 0u; break;
            case TableColumnSizing::fixed: sizing = 1u; break;
            case TableColumnSizing::stretch: sizing = 2u; break;
        }
        return {sizing, settings.value, settings.min_width};
    }

    static inline const Style &table_resolved_style(const StyleSelector &selector)
    {
        auto *theme = get_theme();
        const StyleID style_id = selector.id != Theme::STYLE_ID_INVALID
                                     ? selector.id
                                     : theme->get_resolved_style(selector.tag_id, selector.tag_id, 0u,
                                                                 StyleState::normal);
        return theme->get_style(style_id);
    }

    static StyleState resolve_element_state(const detail::RectData &rect)
    {
        const auto &ctx = detail::get_context();
        if (ctx.io.drag_id == rect.id) return StyleState::active;
        if (ctx.hover_id == rect.id) return StyleState::hover;
        return StyleState::normal;
    }

    template <class Visual>
    static void draw_cell_visual(DrawCtx &ctx, DrawStream *stream, Visual &visual, const Style &style, u16 clip_id,
                                 bool is_hit_allowed)
    {
        QuadsInstanceData data{};
        data.rect = visual.rect.bounds;
        data.z_order = visual.rect.depth;
        if (!(ctx.reason & DrawReasonBits::invalidate) && clip_id != 0xFFFFu)
        {
            const auto clip = get_clip_rect(clip_id);
            const auto &rect = visual.rect.bounds;
            const bool culled = rect.offset.x + rect.size.x <= clip.x || rect.offset.y + rect.size.y <= clip.y ||
                                rect.offset.x >= clip.x + clip.z || rect.offset.y >= clip.y + clip.w;
            if (culled)
            {
                DrawCtx invalidate_ctx = ctx;
                invalidate_ctx.reason |= DrawReasonBits::invalidate;
                emit_quads_instance(invalidate_ctx, stream, visual.draw, data, visual.rect, false, is_hit_allowed);
                return;
            }
        }
        const bool visible = fill_quads_instance_by_style(style, clip_id, data);
        emit_quads_instance(ctx, stream, visual.draw, data, visual.rect, visible, is_hit_allowed);
    }

    static inline bool contains_index(const acul::vector<size_t> &values, size_t value)
    {
        for (size_t item : values)
            if (item == value) return true;
        return false;
    }

    static acul::vector<size_t> make_move_order(const acul::vector<size_t> &selected, size_t target,
                                                bool insert_after_target, size_t count)
    {
        acul::vector<size_t> order;
        if (count == 0u || selected.empty() || target >= count || contains_index(selected, target)) return order;
        for (size_t index : selected)
            if (index >= count) return order;

        size_t insert_index = insert_after_target ? target + 1u : target;
        for (size_t index : selected)
            if (index < insert_index) --insert_index;

        acul::vector<size_t> remaining;
        remaining.reserve(count - selected.size());
        for (size_t index = 0; index < count; ++index)
            if (!contains_index(selected, index)) remaining.push_back(index);

        if (insert_index > remaining.size()) insert_index = remaining.size();
        order.reserve(count);
        for (size_t i = 0; i < insert_index; ++i) order.push_back(remaining[i]);
        for (size_t index : selected) order.push_back(index);
        for (size_t i = insert_index; i < remaining.size(); ++i) order.push_back(remaining[i]);
        return order;
    }

    Table::Table(u32 id, Rows rows, amal::vec2 size, WidgetFlags flags, Widget *parent, u32 style_tag_id)
        : Widget(id, flags, EventFlagBits::click | EventFlagBits::hover | EventFlagBits::drag, parent,
                 {{0.0f, 0.0f}, size}, style_tag_id),
          _rows(std::move(rows)),
          _style({Theme::STYLE_ID_INVALID, style_tag_id})
    {
        rebuild_cells();
    }

    Table::~Table()
    {
        if (_model_binding)
        {
            _model_binding->on_records = nullptr;
            detach_model_binding(*_model_binding);
        }
        clear_cells(false);
    }

    void Table::clear()
    {
        for (auto &row : _rows)
            for (auto *cell : row)
                if (cell) acul::release(cell);
        _rows.clear();
        set_table_flag(_table_flags, AUIK_TABLE_FLAG_ROW_SIZE_OVERRIDES, false);
        if (!has_table_flag(_table_flags, AUIK_TABLE_FLAG_COLUMN_SIZE_OVERRIDES)) _size_overrides.clear();
        rebuild_cells();
        invalidate_layout();
    }

    void Table::set_model_binding(ModelBinding *binding, acul::vector<ModelFieldID> field_ids)
    {
        if (_model_binding)
        {
            _model_binding->on_records = nullptr;
            _model_binding->on_field_change = nullptr;
            detach_model_binding(*_model_binding);
        }
        _model_binding = binding;
        if (!_model_binding) return;

        _model_binding->presenter.field_ids = std::move(field_ids);
        if (_model_binding->presenter.field_ids.empty()) _model_binding->presenter.field_ids.push_back(1u);
        if (!_model_binding->presenter.present_field)
        {
            _model_binding->presenter.data = nullptr;
            _model_binding->presenter.present_field = present_model_text_field;
        }
        _model_binding->on_records = [this](const ModelRecordsEvent &) { rebuild_from_model_binding(); };
        _model_binding->on_field_change = [this](ModelRecordID, ModelFieldID) { rebuild_from_model_binding(); };
        attach_model_binding(*_model_binding);
        rebuild_model_binding_records(*_model_binding);
        rebuild_from_model_binding();
    }

    void Table::set_rows(Rows rows)
    {
        for (auto &row : _rows)
            for (auto *cell : row)
                if (cell) acul::release(cell);
        _rows = std::move(rows);
        set_table_flag(_table_flags, AUIK_TABLE_FLAG_ROW_SIZE_OVERRIDES, false);
        if (!has_table_flag(_table_flags, AUIK_TABLE_FLAG_COLUMN_SIZE_OVERRIDES)) _size_overrides.clear();
        rebuild_cells();
        invalidate_layout();
    }

    void Table::rebuild_from_model_binding()
    {
        if (!_model_binding || !is_model_binding_valid(*_model_binding))
        {
            set_rows({});
            return;
        }

        auto *model = find_model(_model_binding->db, _model_binding->model_id);
        clear();
        if (model)
        {
            for (ModelRecordID record_id : _model_binding->records)
            {
                auto *record = model->find_record(record_id);
                Row row;
                if (record)
                {
                    const auto &field_ids = _model_binding->presenter.field_ids;
                    row.reserve(field_ids.size());
                    for (ModelFieldID field_id : field_ids)
                        if (auto *widget = present_model_field(*_model_binding, *record, field_id))
                            row.push_back(widget);
                }
                add_row(std::move(row));
            }
        }
    }

    void Table::add_row(Row row)
    {
        _rows.push_back(std::move(row));
        rebuild_cells();
        invalidate_layout();
    }

    void Table::set_cell(size_t row, size_t column, Widget *value)
    {
        if (row >= _rows.size()) _rows.resize(row + 1);
        if (column >= _rows[row].size()) _rows[row].resize(column + 1);
        if (_rows[row][column]) acul::release(_rows[row][column]);
        _rows[row][column] = std::move(value);
        rebuild_cells();
        invalidate_layout();
    }

    void Table::set_header(Row header)
    {
        for (auto *cell : _header)
            if (cell) acul::release(cell);
        _header = std::move(header);
        rebuild_cells();
        invalidate_layout();
    }

    void Table::clear_header()
    {
        for (auto *cell : _header)
            if (cell) acul::release(cell);
        _header.clear();
        rebuild_cells();
        invalidate_layout();
    }

    void Table::set_alternating_rows(bool value)
    {
        if (alternating_rows() == value) return;
        set_table_flag(_table_flags, AUIK_TABLE_FLAG_ALTERNATING_ROWS, value);
        invalidate_layout();
    }

    void Table::set_alternating_row_style_tag(u32 tag_id)
    {
        if (_alternating_row_style.tag_id == tag_id) return;
        _alternating_row_style = {Theme::STYLE_ID_INVALID, tag_id};
        invalidate_layout();
    }

    void Table::set_default_column_settings(TableColumnSettings settings)
    {
        _default_column_settings = settings;
        invalidate_layout();
    }

    void Table::set_column_settings(acul::vector<TableColumnSettings> settings)
    {
        _column_settings = std::move(settings);
        invalidate_layout();
    }

    void Table::set_column_settings(size_t column, TableColumnSettings settings)
    {
        if (column >= _column_settings.size()) _column_settings.resize(column + 1);
        _column_settings[column] = settings;
        invalidate_layout();
    }

    void Table::clear_column_settings()
    {
        _column_settings.clear();
        set_table_flag(_table_flags, AUIK_TABLE_FLAG_COLUMN_SIZE_OVERRIDES, false);
        if (!has_table_flag(_table_flags, AUIK_TABLE_FLAG_ROW_SIZE_OVERRIDES)) _size_overrides.clear();
        invalidate_layout();
    }

    void Table::set_column_resizable(bool value)
    {
        if (column_resizable() == value) return;
        set_table_flag(_table_flags, AUIK_TABLE_FLAG_COLUMN_RESIZABLE, value);
        invalidate_layout();
    }

    void Table::set_row_resizable(bool value)
    {
        if (row_resizable() == value) return;
        set_table_flag(_table_flags, AUIK_TABLE_FLAG_ROW_RESIZABLE, value);
        invalidate_layout();
    }

    void Table::set_resize_border_style_tag(u32 tag_id)
    {
        if (_resize_border_style.tag_id == tag_id) return;
        _resize_border_style = {Theme::STYLE_ID_INVALID, tag_id};
        invalidate_layout();
    }

    void Table::set_size_overrides(acul::vector<acul::point2D<f32>> values, bool column_overrides, bool row_overrides)
    {
        _size_overrides = std::move(values);
        set_table_flag(_table_flags, AUIK_TABLE_FLAG_COLUMN_SIZE_OVERRIDES, column_overrides);
        set_table_flag(_table_flags, AUIK_TABLE_FLAG_ROW_SIZE_OVERRIDES, row_overrides);
        invalidate_layout();
    }

    void Table::set_style_tag(u32 tag_id)
    {
        if (_style.tag_id == tag_id) return;
        _style = {Theme::STYLE_ID_INVALID, tag_id};
        set_rect_tag_id(tag_id);
        invalidate_layout();
    }

    void Table::set_header_cell_style_tag(u32 tag_id)
    {
        if (_header_cell_style.tag_id == tag_id) return;
        _header_cell_style = {Theme::STYLE_ID_INVALID, tag_id};
        invalidate_layout();
    }

    void Table::set_cell_style_tag(u32 tag_id)
    {
        if (_cell_style.tag_id == tag_id) return;
        _cell_style = {Theme::STYLE_ID_INVALID, tag_id};
        invalidate_layout();
    }

    bool Table::is_header_cell_hovered(size_t column) const
    {
        const auto hover = detail::get_context().hover_id;
        return hover.widget_id == id() && hover.tag_id == AUIK_TAG_TABLE_HEADER_CELL && hover.element_id == column;
    }

    bool Table::is_cell_hovered(size_t row, size_t column) const
    {
        const auto hover = detail::get_context().hover_id;
        return hover.widget_id == id() && hover.tag_id == AUIK_TAG_TABLE_CELL &&
               hover.element_id == cell_element_id(row, column);
    }

    bool Table::is_resize_border_hovered(size_t element_id) const
    {
        const auto hover = detail::get_context().hover_id;
        return hover.widget_id == id() && is_resize_border_tag(hover.tag_id) && hover.element_id == element_id;
    }

    bool Table::CellRef::valid() const
    {
        return table && row_index < table->_rows.size() && column_index < table->_rows[row_index].size();
    }

    Widget *Table::CellRef::value() const { return valid() ? table->_rows[row_index][column_index] : nullptr; }

    bool Table::ConstCellRef::valid() const
    {
        return table && row_index < table->_rows.size() && column_index < table->_rows[row_index].size();
    }

    const Widget *Table::ConstCellRef::value() const
    {
        return valid() ? table->_rows[row_index][column_index] : nullptr;
    }

    Widget *Table::Column::header() const
    {
        return table && column_index < table->_header.size() ? table->_header[column_index] : nullptr;
    }

    Widget *Table::Column::cell(size_t row) const
    {
        if (!table || row >= table->_rows.size() || column_index >= table->_rows[row].size()) return nullptr;
        return table->_rows[row][column_index];
    }

    TableColumnSettings *Table::Column::settings() const
    {
        if (!table) return nullptr;
        if (column_index >= table->_column_settings.size()) table->_column_settings.resize(column_index + 1u);
        return &table->_column_settings[column_index];
    }

    f32 Table::Column::width() const
    {
        return table && column_index < table->_column_count ? table->_layout_metrics[column_index].x.value : 0.0f;
    }

    f32 *Table::Column::width_override() const
    {
        if (!table || !has_table_flag(table->_table_flags, AUIK_TABLE_FLAG_COLUMN_SIZE_OVERRIDES) ||
            column_index >= table->_size_overrides.size())
            return nullptr;
        return &table->_size_overrides[column_index].x;
    }

    Table::CellIterator Table::Column::end() const { return {table, table ? table->_rows.size() : 0u, column_index}; }

    const Widget *Table::ConstColumn::header() const
    {
        return table && column_index < table->_header.size() ? table->_header[column_index] : nullptr;
    }

    const Widget *Table::ConstColumn::cell(size_t row) const
    {
        if (!table || row >= table->_rows.size() || column_index >= table->_rows[row].size()) return nullptr;
        return table->_rows[row][column_index];
    }

    const TableColumnSettings *Table::ConstColumn::settings() const
    {
        return table ? &table->settings_for_column(column_index) : nullptr;
    }

    f32 Table::ConstColumn::width() const
    {
        return table && column_index < table->_column_count ? table->_layout_metrics[column_index].x.value : 0.0f;
    }

    const f32 *Table::ConstColumn::width_override() const
    {
        if (!table || !has_table_flag(table->_table_flags, AUIK_TABLE_FLAG_COLUMN_SIZE_OVERRIDES) ||
            column_index >= table->_size_overrides.size())
            return nullptr;
        return &table->_size_overrides[column_index].x;
    }

    Table::ConstCellIterator Table::ConstColumn::end() const
    {
        return {table, table ? table->_rows.size() : 0u, column_index};
    }

    Table::ColumnIterator Table::ColumnList::end() const { return {table, table ? table->resolve_column_count() : 0u}; }

    Table::ConstColumnIterator Table::ConstColumnList::end() const
    {
        return {table, table ? table->resolve_column_count() : 0u};
    }

    void Table::move_rows_before(const acul::vector<size_t> &rows, size_t target_row)
    {
        const auto order = make_move_order(rows, target_row, false, _rows.size());
        if (order.empty()) return;

        Rows next_rows;
        next_rows.reserve(_rows.size());
        for (size_t index : order) next_rows.push_back(std::move(_rows[index]));
        _rows = std::move(next_rows);

        if (has_table_flag(_table_flags, AUIK_TABLE_FLAG_ROW_SIZE_OVERRIDES))
        {
            acul::vector<acul::point2D<f32>> next_overrides = _size_overrides;
            if (next_overrides.size() < order.size()) resize_size_points(next_overrides, order.size());
            for (size_t row = 0; row < order.size(); ++row)
            {
                const size_t src = order[row];
                next_overrides[row].y = src < _size_overrides.size() ? _size_overrides[src].y : 0.0f;
            }
            _size_overrides = std::move(next_overrides);
        }
        _resize_size_basis.clear();
        _resizing_row = static_cast<size_t>(-1);
        rebuild_cells();
        invalidate_layout();
    }

    void Table::move_rows_after(const acul::vector<size_t> &rows, size_t target_row)
    {
        const auto order = make_move_order(rows, target_row, true, _rows.size());
        if (order.empty()) return;

        Rows next_rows;
        next_rows.reserve(_rows.size());
        for (size_t index : order) next_rows.push_back(std::move(_rows[index]));
        _rows = std::move(next_rows);

        if (has_table_flag(_table_flags, AUIK_TABLE_FLAG_ROW_SIZE_OVERRIDES))
        {
            acul::vector<acul::point2D<f32>> next_overrides = _size_overrides;
            if (next_overrides.size() < order.size()) resize_size_points(next_overrides, order.size());
            for (size_t row = 0; row < order.size(); ++row)
            {
                const size_t src = order[row];
                next_overrides[row].y = src < _size_overrides.size() ? _size_overrides[src].y : 0.0f;
            }
            _size_overrides = std::move(next_overrides);
        }
        _resize_size_basis.clear();
        _resizing_row = static_cast<size_t>(-1);
        rebuild_cells();
        invalidate_layout();
    }

    void Table::move_columns_before(const acul::vector<size_t> &columns, size_t target_column)
    {
        const size_t columns_count = resolve_column_count();
        const auto order = make_move_order(columns, target_column, false, columns_count);
        if (order.empty()) return;

        if (has_header())
        {
            Row next_header;
            next_header.resize(columns_count);
            for (size_t column = 0; column < order.size(); ++column)
            {
                const size_t src = order[column];
                if (src < _header.size()) next_header[column] = std::move(_header[src]);
            }
            _header = std::move(next_header);
        }

        for (auto &row : _rows)
        {
            Row next_row;
            next_row.resize(columns_count);
            for (size_t column = 0; column < order.size(); ++column)
            {
                const size_t src = order[column];
                if (src < row.size()) next_row[column] = std::move(row[src]);
            }
            row = std::move(next_row);
        }

        if (!_column_settings.empty())
        {
            acul::vector<TableColumnSettings> next_settings;
            next_settings.resize(columns_count);
            for (size_t column = 0; column < order.size(); ++column)
                next_settings[column] = settings_for_column(order[column]);
            _column_settings = std::move(next_settings);
        }

        if (has_table_flag(_table_flags, AUIK_TABLE_FLAG_COLUMN_SIZE_OVERRIDES))
        {
            acul::vector<acul::point2D<f32>> next_overrides = _size_overrides;
            if (next_overrides.size() < columns_count) resize_size_points(next_overrides, columns_count);
            for (size_t column = 0; column < order.size(); ++column)
            {
                const size_t src = order[column];
                next_overrides[column].x = src < _size_overrides.size() ? _size_overrides[src].x : 0.0f;
            }
            _size_overrides = std::move(next_overrides);
        }
        _resize_size_basis.clear();
        _resizing_column = static_cast<size_t>(-1);
        rebuild_cells();
        invalidate_layout();
    }

    void Table::move_columns_after(const acul::vector<size_t> &columns, size_t target_column)
    {
        const size_t columns_count = resolve_column_count();
        const auto order = make_move_order(columns, target_column, true, columns_count);
        if (order.empty()) return;

        if (has_header())
        {
            Row next_header;
            next_header.resize(columns_count);
            for (size_t column = 0; column < order.size(); ++column)
            {
                const size_t src = order[column];
                if (src < _header.size()) next_header[column] = std::move(_header[src]);
            }
            _header = std::move(next_header);
        }

        for (auto &row : _rows)
        {
            Row next_row;
            next_row.resize(columns_count);
            for (size_t column = 0; column < order.size(); ++column)
            {
                const size_t src = order[column];
                if (src < row.size()) next_row[column] = std::move(row[src]);
            }
            row = std::move(next_row);
        }

        if (!_column_settings.empty())
        {
            acul::vector<TableColumnSettings> next_settings;
            next_settings.resize(columns_count);
            for (size_t column = 0; column < order.size(); ++column)
                next_settings[column] = settings_for_column(order[column]);
            _column_settings = std::move(next_settings);
        }

        if (has_table_flag(_table_flags, AUIK_TABLE_FLAG_COLUMN_SIZE_OVERRIDES))
        {
            acul::vector<acul::point2D<f32>> next_overrides = _size_overrides;
            if (next_overrides.size() < columns_count) resize_size_points(next_overrides, columns_count);
            for (size_t column = 0; column < order.size(); ++column)
            {
                const size_t src = order[column];
                next_overrides[column].x = src < _size_overrides.size() ? _size_overrides[src].x : 0.0f;
            }
            _size_overrides = std::move(next_overrides);
        }
        _resize_size_basis.clear();
        _resizing_column = static_cast<size_t>(-1);
        rebuild_cells();
        invalidate_layout();
    }

    StyleUpdateFlags Table::update_style()
    {
        const u32 parent_id = parent() ? parent()->id() : 0u;
        const auto transition = detail::get_widget_style_selector_transition(id());
        StyleUpdateFlags out = resolve_style_selector(_style, id(), parent_id, style_state());
        out |= resolve_style_selector(_header_cell_style, AUIK_TAG_TABLE_HEADER_CELL, id(), StyleState::normal);
        out |= resolve_style_selector(_cell_style, AUIK_TAG_TABLE_CELL, id(), StyleState::normal);
        out |= resolve_style_selector(_alternating_row_style, _alternating_row_style.tag_id, id(), StyleState::normal);
        out |= resolve_style_selector(_resize_border_style, _resize_border_style.tag_id, id(), StyleState::normal);
        if (is_resize_border_tag(transition.prev_id.tag_id) || is_resize_border_tag(transition.current_id.tag_id))
            out |= StyleUpdateFlagBits::redraw;
        out |= update_resize_indicator();

        for (auto *cell : _header)
            if (cell) out |= cell->update_style();
        for (auto &row : _rows)
            for (auto *cell : row)
                if (cell) out |= cell->update_style();
        return out;
    }

    void Table::update_layout_min_size()
    {
        _column_count = resolve_column_count();
        _layout_metrics.assign(amal::max(_column_count, _rows.size()), {});

        f32 header_height = 0.0f;
        const amal::vec4 header_cell_padding = table_resolved_style(_header_cell_style).padding();
        const amal::vec4 body_cell_padding = table_resolved_style(_cell_style).padding();
        for (size_t column = 0; column < _column_count; ++column)
        {
            auto *cell = header_widget(column);
            if (!cell) continue;
            cell->update_layout(false);
            _layout_metrics[column].x.min_value =
                amal::max(_layout_metrics[column].x.min_value,
                          cell->required_size().x + header_cell_padding.x + header_cell_padding.z);
            header_height = amal::max(header_height,
                                      cell->required_size().y + header_cell_padding.y + header_cell_padding.w);
        }

        for (size_t row = 0; row < _rows.size(); ++row)
        {
            for (size_t column = 0; column < _column_count; ++column)
            {
                auto *cell = cell_widget(row, column);
                if (!cell) continue;
                cell->update_layout(false);
                _layout_metrics[column].x.min_value =
                    amal::max(_layout_metrics[column].x.min_value,
                              cell->required_size().x + body_cell_padding.x + body_cell_padding.z);
                _layout_metrics[row].y.min_value =
                    amal::max(_layout_metrics[row].y.min_value,
                              cell->required_size().y + body_cell_padding.y + body_cell_padding.w);
            }
            _layout_metrics[row].y.value = _layout_metrics[row].y.min_value;
            if (has_table_flag(_table_flags, AUIK_TABLE_FLAG_ROW_SIZE_OVERRIDES) && row < _size_overrides.size())
                _layout_metrics[row].y.value = amal::max(_layout_metrics[row].y.value, _size_overrides[row].y);
        }

        const auto &style = get_theme()->get_style(_style.id);
        const amal::vec4 margin = style.margin();
        const amal::vec4 padding = style.padding();

        update_column_widths(0.0f);

        f32 content_w = 0.0f;
        for (size_t column = 0; column < _column_count; ++column) content_w += _layout_metrics[column].x.value;
        f32 content_h = has_header() ? header_height : 0.0f;
        for (size_t row = 0; row < _rows.size(); ++row) content_h += _layout_metrics[row].y.value;

        const f32 required_width =
            detail::resolve_table_required_axis(requested_size().x, fill_width(), content_w + padding.x + padding.z);
        const f32 required_height =
            detail::resolve_table_required_axis(requested_size().y, fill_height(), content_h + padding.y + padding.w);

        set_required_size({required_width + margin.x + margin.z, required_height + margin.y + margin.w});
    }

    void Table::update_layout(bool min_size_known)
    {
        if (!min_size_known) update_layout_min_size();

        const auto &style = get_theme()->get_style(_style.id);
        const amal::vec4 margin = style.margin();
        const amal::vec4 padding = style.padding();
        const amal::vec2 layout_origin = position();
        const amal::vec2 outer_pos = {layout_origin.x + margin.x, layout_origin.y + margin.y};
        const amal::vec2 required_inner = {amal::max(required_size().x - margin.x - margin.z, 0.0f),
                                           amal::max(required_size().y - margin.y - margin.w, 0.0f)};

        amal::vec2 outer_size = size();
        if (!is_width_fixed()) outer_size.x = amal::max(outer_size.x - margin.x - margin.z, required_inner.x);
        else if (outer_size.x <= 0.0f) outer_size.x = required_inner.x;
        if (outer_size.y <= 0.0f) outer_size.y = required_inner.y;
        if (!is_height_fixed()) outer_size.y = amal::max(outer_size.y, required_inner.y);

        set_position(outer_pos);
        set_layout_size(outer_size);
        Widget::update_layout(true);
        rebuild_clip_rects();

        const amal::vec2 inner_pos = outer_pos + amal::vec2{padding.x, padding.y};
        const amal::vec2 inner_size = {amal::max(outer_size.x - padding.x - padding.z, 0.0f),
                                       amal::max(outer_size.y - padding.y - padding.w, 0.0f)};

        update_column_widths(inner_size.x);

        f32 cursor_y = inner_pos.y;
        resize_visuals();
        for (size_t index = 0; index < _resize_border_hit_visuals.size(); ++index)
        {
            auto &visuals = _resize_border_hit_visuals[index];
            visuals.x.rect = detail::make_rect_data(id(), AUIK_TAG_TABLE_RESIZE_BORDER_V, {{0.0f, 0.0f}, {0.0f, 0.0f}},
                                                    clip_id(), next_depth(detail::depth_work_range(depth_range())), 0u,
                                                    static_cast<u32>(index));
            visuals.y.rect = detail::make_rect_data(id(), AUIK_TAG_TABLE_RESIZE_BORDER_H, {{0.0f, 0.0f}, {0.0f, 0.0f}},
                                                    clip_id(), next_depth(detail::depth_work_range(depth_range())), 0u,
                                                    static_cast<u32>(index));
        }

        if (has_header())
        {
            f32 header_h = 0.0f;
            const amal::vec4 header_cell_padding = table_resolved_style(_header_cell_style).padding();
            for (auto *cell : _header)
                if (cell)
                    header_h = amal::max(header_h,
                                         cell->required_size().y + header_cell_padding.y + header_cell_padding.w);

            f32 cursor_x = inner_pos.x;
            for (size_t column = 0; column < _column_count; ++column)
            {
                const f32 column_w = _layout_metrics[column].x.value;
                auto *cell = header_widget(column);
                if (cell)
                {
                    const auto &settings = settings_for_column(column);
                    detail::apply_table_cell_alignment(cell, settings.halign, settings.valign);
                    cell->set_position({cursor_x + header_cell_padding.x, cursor_y + header_cell_padding.y});
                    cell->set_layout_size({amal::max(column_w - header_cell_padding.x - header_cell_padding.z, 0.0f),
                                           amal::max(header_h - header_cell_padding.y - header_cell_padding.w, 0.0f)});
                    cell->update_layout(true);
                }
                if (column < _header_visuals.size())
                {
                    auto &visual = _header_visuals[column];
                    visual.rect = detail::make_rect_data(id(), AUIK_TAG_TABLE_HEADER_CELL,
                                                         {{cursor_x, cursor_y}, {column_w, header_h}}, clip_id(),
                                                         next_depth(detail::depth_work_range(depth_range())), 0u,
                                                         static_cast<u32>(column));
                }
                cursor_x += column_w;
            }
            cursor_y += header_h;
        }

        size_t visual_index = 0;
        for (size_t row = 0; row < _rows.size(); ++row)
        {
            const f32 row_h = _layout_metrics[row].y.value;
            if (row < _alt_row_visuals.size())
            {
                _alt_row_visuals[row].rect = detail::make_rect_data(id(), AUIK_STYLE_TAG_TABLE_ROW_ALT,
                                                                    {{inner_pos.x, cursor_y}, {inner_size.x, row_h}},
                                                                    clip_id(),
                                                                    next_depth(detail::depth_work_range(depth_range())));
            }

            f32 cursor_x = inner_pos.x;
            for (size_t column = 0; column < _column_count; ++column)
            {
                const f32 column_w = _layout_metrics[column].x.value;
                auto *cell = cell_widget(row, column);
                if (cell)
                {
                    const auto &settings = settings_for_column(column);
                    const amal::vec4 cell_padding = table_resolved_style(_cell_style).padding();
                    detail::apply_table_cell_alignment(cell, settings.halign, settings.valign);
                    cell->set_position({cursor_x + cell_padding.x, cursor_y + cell_padding.y});
                    cell->set_layout_size({amal::max(column_w - cell_padding.x - cell_padding.z, 0.0f),
                                           amal::max(row_h - cell_padding.y - cell_padding.w, 0.0f)});
                    cell->update_layout(true);
                }
                if (visual_index < _cell_visuals.size())
                {
                    auto &visual = _cell_visuals[visual_index];
                    visual.rect =
                        detail::make_rect_data(id(), AUIK_TAG_TABLE_CELL, {{cursor_x, cursor_y}, {column_w, row_h}},
                                               clip_id(), next_depth(detail::depth_work_range(depth_range())), 0u,
                                               cell_element_id(row, column));
                }
                cursor_x += column_w;
                ++visual_index;
            }
            cursor_y += row_h;
        }

        auto *theme = get_theme();
        const StyleID resize_style_id =
            _resize_border_style.id != Theme::STYLE_ID_INVALID
                ? _resize_border_style.id
                : theme->get_resolved_style(_resize_border_style.tag_id, _resize_border_style.tag_id, id(),
                                            StyleState::normal);
        const auto &resize_style = theme->get_style(resize_style_id);
        const amal::vec4 resize_margin = resize_style.margin();
        const amal::vec4 resize_padding = resize_style.padding();
        const f32 resize_w =
            amal::max(resize_margin.x + resize_margin.z + resize_padding.x + resize_padding.z,
                      resize_style.border_thickness());
        const f32 resize_hit_w = amal::max(resize_w, 1.0f);
        if (column_resizable() && resize_hit_w > 0.0f)
        {
            f32 cursor_x = inner_pos.x;
            for (size_t column = 0; column + 1 < _column_count; ++column)
            {
                cursor_x += _layout_metrics[column].x.value;
                if (column >= _resize_border_hit_visuals.size()) continue;
                auto &visual = _resize_border_hit_visuals[column].x;
                visual.rect = detail::make_rect_data(
                    id(), AUIK_TAG_TABLE_RESIZE_BORDER_V,
                    {{cursor_x - resize_hit_w * 0.5f, inner_pos.y}, {resize_hit_w, cursor_y - inner_pos.y}}, clip_id(),
                    depth_range().y, 0u, static_cast<u32>(column));
            }
        }

        const f32 resize_h =
            amal::max(resize_margin.y + resize_margin.w + resize_padding.y + resize_padding.w,
                      resize_style.border_thickness());
        const f32 resize_hit_h = amal::max(resize_h, 1.0f);
        if (row_resizable() && resize_hit_h > 0.0f)
        {
            f32 row_cursor_y = inner_pos.y;
            if (!_header_visuals.empty()) row_cursor_y += _header_visuals[0].rect.bounds.size.y;
            for (size_t row = 0; row + 1 < _rows.size(); ++row)
            {
                row_cursor_y += _layout_metrics[row].y.value;
                if (row >= _resize_border_hit_visuals.size()) continue;
                auto &visual = _resize_border_hit_visuals[row].y;
                visual.rect = detail::make_rect_data(
                    id(), AUIK_TAG_TABLE_RESIZE_BORDER_H,
                    {{inner_pos.x, row_cursor_y - resize_hit_h * 0.5f}, {inner_size.x, resize_hit_h}}, clip_id(),
                    depth_range().y, 0u, static_cast<u32>(row));
            }
        }
        update_resize_indicator();
        update_cell_clip_rects();
    }

    void Table::translate(const amal::vec2 &delta)
    {
        if (delta.x == 0.0f && delta.y == 0.0f) return;
        Widget::translate(delta);
        for (auto &visual : _header_visuals) visual.rect.bounds.offset += delta;
        for (auto &visual : _cell_visuals) visual.rect.bounds.offset += delta;
        for (auto &visual : _alt_row_visuals) visual.rect.bounds.offset += delta;
        for (auto &visuals : _resize_border_hit_visuals)
        {
            visuals.x.rect.bounds.offset += delta;
            visuals.y.rect.bounds.offset += delta;
        }
        _resize_indicator_visual.rect.bounds.offset += delta;
        for (auto *cell : _header)
            if (cell) cell->translate(delta);
        for (auto &row : _rows)
            for (auto *cell : row)
                if (cell) cell->translate(delta);
        const amal::vec4 parent_clip = parent() ? parent()->get_content_clip_rect() : get_main_viewport_rect();
        if (parent() && clip_id() == parent()->content_clip_id()) set_clip_id(0xFFFFu);
        ensure_own_clip_rect(detail::intersect_rects(parent_clip, {position().x, position().y, size().x, size().y}));
        update_cell_clip_rects();
        update_draw_commands(DrawReasonBits::external);
        detail::get_context().dirty_flags |= DirtyFlagBits::redraw;
    }

    void Table::rebuild_clip_rects()
    {
        const amal::vec4 parent_clip = parent() ? parent()->get_content_clip_rect() : get_main_viewport_rect();
        if (parent() && clip_id() == parent()->content_clip_id()) set_clip_id(0xFFFFu);
        ensure_own_clip_rect(detail::intersect_rects(parent_clip, {position().x, position().y, size().x, size().y}));
        update_cell_clip_rects();
    }

    void Table::reset_draw_records()
    {
        _bg = {};
        for (auto &visual : _header_visuals) visual.draw = {};
        for (auto &visual : _cell_visuals) visual.draw = {};
        for (auto &visual : _alt_row_visuals) visual.draw = {};
        for (auto &visuals : _resize_border_hit_visuals)
        {
            visuals.x.draw = {};
            visuals.y.draw = {};
        }
        _resize_indicator_visual.draw = {};
        for (auto *cell : _header)
            if (cell) cell->reset_draw_records();
        for (auto &row : _rows)
            for (auto *cell : row)
                if (cell) cell->reset_draw_records();
    }

    void Table::update_depth(const amal::vec2 &depth_range)
    {
        Widget::update_depth(depth_range);
        const amal::vec2 content_range = detail::depth_foreground_range(this->depth_range());
        for (auto *cell : _header)
        {
            if (!cell) continue;
            cell->update_depth(content_range);
        }
        for (auto &row : _rows)
        {
            for (auto *cell : row)
            {
                if (!cell) continue;
                cell->update_depth(content_range);
            }
        }
    }

    void Table::back_hit_depth()
    {
        Widget::back_hit_depth();
        for (auto *cell : _header)
            if (cell) cell->back_hit_depth();
        for (auto &row : _rows)
            for (auto *cell : row)
                if (cell) cell->back_hit_depth();
        auto lower = [&](CellVisual &visual) { visual.rect.hit_depth = get_rect().hit_depth; };
        for (auto &visual : _header_visuals) lower(visual);
        for (auto &visual : _cell_visuals) lower(visual);
        for (auto &visual : _alt_row_visuals) lower(visual);
        for (auto &visuals : _resize_border_hit_visuals)
        {
            lower(visuals.x);
            lower(visuals.y);
        }
        lower(_resize_indicator_visual);
        detail::get_context().dirty_flags |= DirtyFlagBits::hit_rect_update;
    }

    void Table::restore_hit_depth()
    {
        Widget::restore_hit_depth();
        for (auto *cell : _header)
            if (cell) cell->restore_hit_depth();
        for (auto &row : _rows)
            for (auto *cell : row)
                if (cell) cell->restore_hit_depth();
        auto restore = [](CellVisual &visual) { visual.rect.hit_depth = visual.rect.depth; };
        for (auto &visual : _header_visuals) restore(visual);
        for (auto &visual : _cell_visuals) restore(visual);
        for (auto &visual : _alt_row_visuals) restore(visual);
        for (auto &visuals : _resize_border_hit_visuals)
        {
            restore(visuals.x);
            restore(visuals.y);
        }
        restore(_resize_indicator_visual);
        detail::get_context().dirty_flags |= DirtyFlagBits::hit_rect_update;
    }

    void Table::draw(DrawCtx &ctx)
    {
        if (!(widget_flags & WidgetFlagBits::visible)) return;
        auto *quads_stream = get_primary_quads_stream();
        auto *theme = get_theme();
        const u32 parent_id = parent() ? parent()->id() : 0u;

        if (quads_stream)
        {
            QuadsInstanceData bg{};
            bg.rect = bounds();
            bg.z_order = get_z_order();
            const bool bg_visible = fill_quads_instance_by_style(theme->get_style(_style.id), clip_id(), bg);
            emit_quads_instance(ctx, quads_stream, _bg, bg, get_rect(), bg_visible, false);

            for (size_t row = 0; row < _alt_row_visuals.size(); ++row)
            {
                if (!alternating_rows() || (row % 2u) == 0u) continue;
                draw_cell_visual(ctx, quads_stream, _alt_row_visuals[row], theme->get_style(_alternating_row_style.id),
                                 clip_id(), false);
            }

            for (auto &visual : _header_visuals)
            {
                const StyleState state = resolve_element_state(visual.rect);
                const StyleID style_id =
                    theme->get_resolved_style(_header_cell_style.tag_id, AUIK_TAG_TABLE_HEADER_CELL, id(), state);
                draw_cell_visual(ctx, quads_stream, visual, theme->get_style(style_id), clip_id(), can_emit_hit(ctx));
            }

            for (auto &visual : _cell_visuals)
            {
                const StyleState state = resolve_element_state(visual.rect);
                const StyleID style_id =
                    theme->get_resolved_style(_cell_style.tag_id, AUIK_TAG_TABLE_CELL, parent_id, state);
                draw_cell_visual(ctx, quads_stream, visual, theme->get_style(style_id), clip_id(), can_emit_hit(ctx));
            }

            if (column_resizable() || row_resizable())
            {
                const size_t column_resize_count = _column_count > 0u ? _column_count - 1u : 0u;
                const size_t row_resize_count = _rows.size() > 0u ? _rows.size() - 1u : 0u;
                for (size_t index = 0; index < _resize_border_hit_visuals.size(); ++index)
                {
                    auto &visuals = _resize_border_hit_visuals[index];
                    if (column_resizable() && index < column_resize_count)
                        detail::draw_table_resize_border_visual(ctx, quads_stream, visuals.x, theme,
                                                                _resize_border_style.tag_id, id(), clip_id(),
                                                                can_emit_hit(ctx));
                    if (row_resizable() && index < row_resize_count)
                        detail::draw_table_resize_border_visual(ctx, quads_stream, visuals.y, theme,
                                                                _resize_border_style.tag_id, id(), clip_id(),
                                                                can_emit_hit(ctx));
                }
            }
        }

        const amal::vec4 content_clip = get_content_clip_rect();
        for (auto *cell : _header)
        {
            if (!cell) continue;
            DrawCtx cell_ctx = ctx;
            cell_ctx.is_hit_allowed = false;
            detail::draw_child_in_clip(cell, cell_ctx, content_clip);
        }
        for (auto &row : _rows)
        {
            for (auto *cell : row)
            {
                if (!cell) continue;
                DrawCtx cell_ctx = ctx;
                cell_ctx.is_hit_allowed = false;
                detail::draw_child_in_clip(cell, cell_ctx, content_clip);
            }
        }
    }

    void Table::on_hover(HoverState state)
    {
        auto &ctx = detail::get_context();
        detail::CursorID::enum_type cursor = detail::CursorID::arrow;
        bool resize_border_state = state == HoverState::leave;
        if (state != HoverState::leave && ctx.hover_id.widget_id == id())
        {
            if (column_resizable() && ctx.hover_id.tag_id == AUIK_TAG_TABLE_RESIZE_BORDER_V)
            {
                cursor = detail::CursorID::resize_ew;
                resize_border_state = true;
            }
            else if (row_resizable() && ctx.hover_id.tag_id == AUIK_TAG_TABLE_RESIZE_BORDER_H)
            {
                cursor = detail::CursorID::resize_ns;
                resize_border_state = true;
            }
        }
        detail::set_window_cursor(cursor, ctx.window_ctx);
        if (resize_border_state)
        {
            update_resize_indicator();
            ctx.dirty_flags |= DirtyFlagBits::redraw;
            detail::mark_host_refresh_request();
        }
    }

    void Table::on_drag(const amal::vec2 &delta, KeyPressState state)
    {
        const auto drag_id = detail::get_context().io.drag_id;
        if (state == KeyPressState::press)
        {
            _resizing_column = static_cast<size_t>(-1);
            _resizing_row = static_cast<size_t>(-1);
            _resize_drag_accum = {0.0f, 0.0f};
            _resize_size_basis.clear();
            if (drag_id.widget_id != id()) return;
            if (column_resizable() && drag_id.tag_id == AUIK_TAG_TABLE_RESIZE_BORDER_V)
            {
                _resizing_column = drag_id.element_id;
                resize_size_points(_resize_size_basis, _column_count);
                if (has_table_flag(_table_flags, AUIK_TABLE_FLAG_COLUMN_SIZE_OVERRIDES))
                    for (size_t column = 0; column < _column_count; ++column)
                        _resize_size_basis[column].x =
                            column < _size_overrides.size() ? _size_overrides[column].x : 0.0f;
                else
                    for (size_t column = 0; column < _column_count; ++column)
                        _resize_size_basis[column].x = _layout_metrics[column].x.value;
                detail::set_window_cursor(detail::CursorID::resize_ew, detail::get_context().window_ctx);
                return;
            }
            if (row_resizable() && drag_id.tag_id == AUIK_TAG_TABLE_RESIZE_BORDER_H)
            {
                _resizing_row = drag_id.element_id;
                resize_size_points(_resize_size_basis, _rows.size());
                if (has_table_flag(_table_flags, AUIK_TABLE_FLAG_ROW_SIZE_OVERRIDES))
                    for (size_t row = 0; row < _rows.size(); ++row)
                        _resize_size_basis[row].y = row < _size_overrides.size() ? _size_overrides[row].y : 0.0f;
                else
                    for (size_t row = 0; row < _rows.size(); ++row)
                        _resize_size_basis[row].y = _layout_metrics[row].y.value;
                detail::set_window_cursor(detail::CursorID::resize_ns, detail::get_context().window_ctx);
                return;
            }
            return;
        }

        if (state == KeyPressState::release)
        {
            _resizing_column = static_cast<size_t>(-1);
            _resizing_row = static_cast<size_t>(-1);
            _resize_drag_accum = {0.0f, 0.0f};
            _resize_size_basis.clear();
            detail::set_window_cursor(detail::CursorID::arrow, detail::get_context().window_ctx);
            return;
        }

        _resize_drag_accum += delta;
        bool changed = false;
        if (column_resizable() && _resizing_column != static_cast<size_t>(-1) && _resizing_column < _column_count)
        {
            changed = detail::apply_table_column_resize(
                _layout_metrics, _size_overrides, _resize_size_basis, _column_count, _resizing_column,
                _resize_drag_accum.x, has_table_flag(_table_flags, AUIK_TABLE_FLAG_COLUMN_SIZE_OVERRIDES),
                [this](size_t column) { return to_layout_settings(settings_for_column(column)); });
            if (changed) set_table_flag(_table_flags, AUIK_TABLE_FLAG_COLUMN_SIZE_OVERRIDES, true);
        }
        if (row_resizable() && _resizing_row != static_cast<size_t>(-1) && _resizing_row < _rows.size())
        {
            if (_resize_size_basis.size() < _rows.size())
            {
                resize_size_points(_resize_size_basis, _rows.size());
                if (has_table_flag(_table_flags, AUIK_TABLE_FLAG_ROW_SIZE_OVERRIDES))
                    for (size_t row = 0; row < _rows.size(); ++row)
                        _resize_size_basis[row].y = row < _size_overrides.size() ? _size_overrides[row].y : 0.0f;
                else
                    for (size_t row = 0; row < _rows.size(); ++row)
                        _resize_size_basis[row].y = _layout_metrics[row].y.value;
            }
            if (_size_overrides.size() < _rows.size()) resize_size_points(_size_overrides, _rows.size());
            for (size_t row = 0; row < _rows.size(); ++row) _size_overrides[row].y = _resize_size_basis[row].y;
            set_table_flag(_table_flags, AUIK_TABLE_FLAG_ROW_SIZE_OVERRIDES, true);
            const f32 min_height = _resizing_row < _rows.size() ? _layout_metrics[_resizing_row].y.min_value : 1.0f;
            const f32 next_height = amal::max(_resize_size_basis[_resizing_row].y + _resize_drag_accum.y, min_height);
            changed = changed || _size_overrides[_resizing_row].y != next_height;
            _size_overrides[_resizing_row].y = next_height;
        }
        if (changed) update_own_layout();
    }

    amal::vec4 Table::get_content_clip_rect() const
    {
        if (clip_id() == 0xFFFFu) return parent() ? parent()->get_content_clip_rect() : get_main_viewport_rect();
        return get_clip_rect(content_clip_id());
    }

    void Table::on_attach()
    {
        Widget::on_attach();
        sync_cell_parents();
    }

    void Table::on_detach() { Widget::on_detach(); }

    void Table::rebuild_cells()
    {
        _column_count = resolve_column_count();
        sync_cell_parents();
        update_depth(depth_range());
    }

    void Table::clear_cells(bool invalidate_draw)
    {
        for (auto *cell : _header)
            if (cell)
            {
                if (invalidate_draw) cell->reset_draw_records();
                acul::release(cell);
            }
        _header.clear();

        for (auto &row : _rows)
            for (auto *cell : row)
                if (cell)
                {
                    if (invalidate_draw) cell->reset_draw_records();
                    acul::release(cell);
                }
        _rows.clear();
        _header_visuals.clear();
        _cell_visuals.clear();
        _alt_row_visuals.clear();
    }

    size_t Table::resolve_column_count() const
    {
        size_t count = _header.size();
        for (const auto &row : _rows) count = amal::max(count, row.size());
        return count;
    }

    u32 Table::cell_element_id(size_t row, size_t column) const
    {
        return static_cast<u32>(row * _column_count + column);
    }

    Widget *Table::header_widget(size_t column) const
    {
        return column < _header.size() ? _header[column] : nullptr;
    }

    Widget *Table::cell_widget(size_t row, size_t column) const
    {
        if (row >= _rows.size()) return nullptr;
        return column < _rows[row].size() ? _rows[row][column] : nullptr;
    }

    const TableColumnSettings &Table::settings_for_column(size_t column) const
    {
        return column < _column_settings.size() ? _column_settings[column] : _default_column_settings;
    }

    void Table::update_column_widths(f32 inner_width)
    {
        detail::update_table_column_widths(
            _layout_metrics, _column_count, inner_width, _size_overrides,
            has_table_flag(_table_flags, AUIK_TABLE_FLAG_COLUMN_SIZE_OVERRIDES),
            [this](size_t column) { return to_layout_settings(settings_for_column(column)); });
    }

    void Table::invalidate_layout()
    {
        auto &ctx = detail::get_context();
        ctx.dirty_flags |= DirtyFlagBits::redraw | DirtyFlagBits::hit_rect_update;
        auto *layout_parent = parent();
        if (!layout_parent) return;
        layout_parent->update_layout(false);
        layout_parent->update_draw_commands(DrawReasonBits::layout);
        detail::mark_host_refresh_request();
    }

    void Table::update_own_layout()
    {
        auto &ctx = detail::get_context();
        ctx.dirty_flags |= DirtyFlagBits::redraw | DirtyFlagBits::hit_rect_update;
        const auto &style = get_theme()->get_style(_style.id);
        const amal::vec4 margin = style.margin();
        set_position({position().x - margin.x, position().y - margin.y});
        update_layout(false);
        update_draw_commands(DrawReasonBits::layout);
        detail::mark_host_refresh_request();
    }

    void Table::update_cell_clip_rects()
    {
        for (auto *cell : _header)
        {
            if (!cell) continue;
            cell->set_clip_id(content_clip_id());
            cell->rebuild_clip_rects();
        }
        for (auto &row : _rows)
        {
            for (auto *cell : row)
            {
                if (!cell) continue;
                cell->set_clip_id(content_clip_id());
                cell->rebuild_clip_rects();
            }
        }
        for (auto &visual : _header_visuals) visual.rect.clip_id = clip_id();
        for (auto &visual : _cell_visuals) visual.rect.clip_id = clip_id();
        for (auto &visual : _alt_row_visuals) visual.rect.clip_id = clip_id();
        for (auto &visuals : _resize_border_hit_visuals)
        {
            visuals.x.rect.clip_id = clip_id();
            visuals.y.rect.clip_id = clip_id();
        }
        _resize_indicator_visual.rect.clip_id = clip_id();
    }

    void Table::resize_visuals()
    {
        _header_visuals.resize(has_header() ? _column_count : 0u);
        _cell_visuals.resize(_rows.size() * _column_count);
        _alt_row_visuals.resize(_rows.size());
        const size_t column_resize_count = _column_count > 0u ? _column_count - 1u : 0u;
        const size_t row_resize_count = _rows.size() > 0u ? _rows.size() - 1u : 0u;
        _resize_border_hit_visuals.resize(amal::max(column_resize_count, row_resize_count));
    }

    StyleUpdateFlags Table::update_resize_indicator()
    {
        const detail::RectData prev_rect = _resize_indicator_visual.rect;
        const bool prev_active = has_table_flag(_table_flags, AUIK_TABLE_FLAG_RESIZE_INDICATOR_ACTIVE);
        const auto &ctx = detail::get_context();
        const auto transition = detail::get_widget_style_selector_transition(id());
        auto target = ctx.io.mouse_down ? ctx.io.drag_id : ElementID{};
        if (target.widget_id != id() || !is_resize_border_tag(target.tag_id)) target = transition.current_id;
        const bool resize_indicator_active =
            target.widget_id == id() && ((column_resizable() && target.tag_id == AUIK_TAG_TABLE_RESIZE_BORDER_V) ||
                                         (row_resizable() && target.tag_id == AUIK_TAG_TABLE_RESIZE_BORDER_H));
        set_table_flag(_table_flags, AUIK_TABLE_FLAG_RESIZE_INDICATOR_ACTIVE, resize_indicator_active);

        if (resize_indicator_active && target.tag_id == AUIK_TAG_TABLE_RESIZE_BORDER_V &&
            target.element_id < _resize_border_hit_visuals.size())
        {
            _resize_indicator_visual.rect = _resize_border_hit_visuals[target.element_id].x.rect;
            auto *theme = get_theme();
            const StyleID style_id =
                _resize_border_style.id != Theme::STYLE_ID_INVALID
                    ? _resize_border_style.id
                    : theme->get_resolved_style(_resize_border_style.tag_id, _resize_border_style.tag_id, id(),
                                                StyleState::normal);
            const auto &style = theme->get_style(style_id);
            const f32 visual_w = amal::max(style.border_thickness(), 1.0f);
            _resize_indicator_visual.rect.bounds.offset.x +=
                _resize_indicator_visual.rect.bounds.size.x * 0.5f - visual_w * 0.5f;
            _resize_indicator_visual.rect.bounds.size.x = visual_w;
        }
        else if (resize_indicator_active && target.tag_id == AUIK_TAG_TABLE_RESIZE_BORDER_H &&
                 target.element_id < _resize_border_hit_visuals.size())
        {
            _resize_indicator_visual.rect = _resize_border_hit_visuals[target.element_id].y.rect;
            auto *theme = get_theme();
            const StyleID style_id =
                _resize_border_style.id != Theme::STYLE_ID_INVALID
                    ? _resize_border_style.id
                    : theme->get_resolved_style(_resize_border_style.tag_id, _resize_border_style.tag_id, id(),
                                                StyleState::normal);
            const auto &style = theme->get_style(style_id);
            const f32 visual_h = amal::max(style.border_thickness(), 1.0f);
            _resize_indicator_visual.rect.bounds.offset.y +=
                _resize_indicator_visual.rect.bounds.size.y * 0.5f - visual_h * 0.5f;
            _resize_indicator_visual.rect.bounds.size.y = visual_h;
        }
        else
        {
            _resize_indicator_visual.rect =
                detail::make_rect_data(id(), target.tag_id, {{0.0f, 0.0f}, {0.0f, 0.0f}}, clip_id());
        }

        const auto &next_rect = _resize_indicator_visual.rect;
        const bool rect_changed =
            prev_rect.id != next_rect.id || prev_rect.bounds.offset.x != next_rect.bounds.offset.x ||
            prev_rect.bounds.offset.y != next_rect.bounds.offset.y ||
            prev_rect.bounds.size.x != next_rect.bounds.size.x || prev_rect.bounds.size.y != next_rect.bounds.size.y ||
            prev_rect.clip_id != next_rect.clip_id;
        if (prev_active != resize_indicator_active || rect_changed) return StyleUpdateFlagBits::redraw;
        return StyleUpdateFlagBits::none;
    }

    bool Table::is_resize_border_tag(u32 tag_id) const
    {
        return tag_id == AUIK_TAG_TABLE_RESIZE_BORDER_V || tag_id == AUIK_TAG_TABLE_RESIZE_BORDER_H;
    }

    void Table::sync_cell_parents()
    {
        for (auto *cell : _header)
            if (cell) cell->set_parent(this);
        for (auto &row : _rows)
            for (auto *cell : row)
                if (cell) cell->set_parent(this);
    }

    namespace
    {
        constexpr u32 g_persistent_table_flags = AUIK_TABLE_FLAG_ALTERNATING_ROWS | AUIK_TABLE_FLAG_COLUMN_RESIZABLE |
                                                AUIK_TABLE_FLAG_ROW_RESIZABLE |
                                                AUIK_TABLE_FLAG_COLUMN_SIZE_OVERRIDES |
                                                AUIK_TABLE_FLAG_ROW_SIZE_OVERRIDES;

        void write_column_settings(acul::bin_stream &stream, const TableColumnSettings &settings)
        {
            stream.write(static_cast<u8>(settings.sizing))
                .write(settings.value)
                .write(settings.min_width)
                .write(static_cast<u8>(settings.halign))
                .write(static_cast<u8>(settings.valign));
        }

        TableColumnSettings read_column_settings(acul::bin_stream &stream)
        {
            TableColumnSettings settings{};
            u8 sizing = static_cast<u8>(TableColumnSizing::stretch);
            u8 halign = static_cast<u8>(HAlign::left);
            u8 valign = static_cast<u8>(VAlign::none);
            stream.read(sizing).read(settings.value).read(settings.min_width).read(halign).read(valign);
            settings.sizing = static_cast<TableColumnSizing>(sizing);
            settings.halign = static_cast<HAlign>(halign);
            settings.valign = static_cast<VAlign>(valign);
            return settings;
        }

        void write_widget_row(acul::bin_stream &stream, const Table::Row &row)
        {
            acul::vector<u32> columns;
            acul::vector<umbf::Block *> blocks;
            for (size_t column = 0u; column < row.size(); ++column)
            {
                auto *cell = row[column];
                if (!cell || !(cell->widget_flags & WidgetFlagBits::configurable)) continue;
                columns.push_back(static_cast<u32>(column));
                blocks.push_back(cell);
            }

            stream.write(static_cast<u32>(columns.size()));
            if (!columns.empty()) stream.write(columns.data(), columns.size());
            stream.write(blocks);
        }

        Table::Row read_widget_row(acul::bin_stream &stream)
        {
            u32 cell_count = 0u;
            stream.read(cell_count);

            acul::vector<u32> columns;
            columns.resize(cell_count);
            if (!columns.empty()) stream.read(columns.data(), columns.size());

            acul::vector<umbf::Block *> blocks;
            stream.read(blocks);

            size_t column_count = 0u;
            for (u32 column : columns) column_count = amal::max(column_count, static_cast<size_t>(column + 1u));

            Table::Row row;
            row.resize(column_count);
            for (u32 cell_i = 0u; cell_i < cell_count; ++cell_i)
                row[columns[cell_i]] = static_cast<Widget *>(blocks[cell_i]);
            return row;
        }

        void write_table(acul::bin_stream &stream, umbf::Block *block)
        {
            auto *table = static_cast<Table *>(block);
            detail::write_widget_common_data(stream, *table);
            stream.write(table->style_tag())
                .write(table->header_cell_style_tag())
                .write(table->cell_style_tag())
                .write(table->alternating_row_style_tag())
                .write(table->resize_border_style_tag())
                .write(table->table_flags() & g_persistent_table_flags);

            write_column_settings(stream, table->default_column_settings());

            const auto &column_settings = table->column_settings();
            stream.write(static_cast<u32>(column_settings.size()));
            for (const auto &settings : column_settings) write_column_settings(stream, settings);

            const auto &size_overrides = table->size_overrides();
            stream.write(static_cast<u32>(size_overrides.size()));
            for (const auto &value : size_overrides) stream.write(value);

            write_widget_row(stream, table->header());

            u32 row_count = 0u;
            for (const auto &row : table->rows())
            {
                bool has_configurable = false;
                for (auto *cell : row)
                {
                    if (cell && (cell->widget_flags & WidgetFlagBits::configurable))
                    {
                        has_configurable = true;
                        break;
                    }
                }
                if (has_configurable) ++row_count;
            }

            stream.write(row_count);
            for (size_t row_i = 0u; row_i < table->rows().size(); ++row_i)
            {
                const auto &row = table->rows()[row_i];
                bool has_configurable = false;
                for (auto *cell : row)
                {
                    if (cell && (cell->widget_flags & WidgetFlagBits::configurable))
                    {
                        has_configurable = true;
                        break;
                    }
                }
                if (!has_configurable) continue;
                stream.write(static_cast<u32>(row_i));
                write_widget_row(stream, row);
            }
        }

        umbf::Block *read_table(acul::bin_stream &stream)
        {
            const auto common = detail::read_widget_common_data(stream);
            u32 style_tag = AUIK_STYLE_TAG_TABLE;
            u32 header_cell_style_tag = AUIK_STYLE_TAG_TABLE_HEADER_CELL;
            u32 cell_style_tag = AUIK_STYLE_TAG_TABLE_CELL;
            u32 alternating_row_style_tag = AUIK_STYLE_TAG_TABLE_ROW_ALT;
            u32 resize_border_style_tag = AUIK_STYLE_TAG_TABLE_RESIZE_BORDER;
            u32 table_flags = 0u;
            stream.read(style_tag)
                .read(header_cell_style_tag)
                .read(cell_style_tag)
                .read(alternating_row_style_tag)
                .read(resize_border_style_tag)
                .read(table_flags);

            auto default_settings = read_column_settings(stream);

            u32 column_settings_count = 0u;
            stream.read(column_settings_count);
            acul::vector<TableColumnSettings> column_settings;
            column_settings.reserve(column_settings_count);
            for (u32 column_i = 0u; column_i < column_settings_count; ++column_i)
                column_settings.push_back(read_column_settings(stream));

            u32 override_count = 0u;
            stream.read(override_count);
            acul::vector<acul::point2D<f32>> size_overrides;
            size_overrides.resize(override_count);
            if (!size_overrides.empty()) stream.read(size_overrides.data(), size_overrides.size());

            auto *table = acul::alloc<Table>(common.id, Table::Rows{}, common.requested_size,
                                             WidgetFlags(common.widget_flags), nullptr, style_tag);
            table->set_header_cell_style_tag(header_cell_style_tag);
            table->set_cell_style_tag(cell_style_tag);
            table->set_alternating_row_style_tag(alternating_row_style_tag);
            table->set_resize_border_style_tag(resize_border_style_tag);
            table->set_alternating_rows((table_flags & AUIK_TABLE_FLAG_ALTERNATING_ROWS) != 0u);
            table->set_column_resizable((table_flags & AUIK_TABLE_FLAG_COLUMN_RESIZABLE) != 0u);
            table->set_row_resizable((table_flags & AUIK_TABLE_FLAG_ROW_RESIZABLE) != 0u);
            table->set_default_column_settings(default_settings);
            table->set_column_settings(std::move(column_settings));
            table->set_size_overrides(std::move(size_overrides),
                                      (table_flags & AUIK_TABLE_FLAG_COLUMN_SIZE_OVERRIDES) != 0u,
                                      (table_flags & AUIK_TABLE_FLAG_ROW_SIZE_OVERRIDES) != 0u);

            auto header = read_widget_row(stream);
            if (!header.empty()) table->set_header(std::move(header));

            u32 row_count = 0u;
            stream.read(row_count);
            for (u32 row_i = 0u; row_i < row_count; ++row_i)
            {
                u32 row_index = 0u;
                stream.read(row_index);
                auto row = read_widget_row(stream);
                for (size_t column_i = 0u; column_i < row.size(); ++column_i)
                {
                    if (!row[column_i]) continue;
                    auto *cell = row[column_i];
                    row[column_i] = nullptr;
                    table->set_cell(row_index, column_i, cell);
                }
            }

            detail::apply_widget_common_data(table, common);
            return table;
        }
    } // namespace

    namespace streams
    {
        AUIK_EXPORT const umbf::streams::Stream table{read_table, write_table};
    } // namespace streams

} // namespace auik
