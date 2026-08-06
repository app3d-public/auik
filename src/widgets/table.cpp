#include <auik/auik.hpp>
#include <auik/detail/depth.hpp>
#include <auik/detail/rect.hpp>
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
            case TableColumnSizing::shrink:
                sizing = 0u;
                break;
            case TableColumnSizing::fixed:
                sizing = 1u;
                break;
            case TableColumnSizing::stretch:
                sizing = 2u;
                break;
        }
        return {sizing, settings.value, settings.min_width};
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

    static void release_table_row(Table::Row &row, bool invalidate_draw = true)
    {
        for (auto *cell : row)
        {
            if (!cell) continue;
            if (invalidate_draw) cell->reset_draw_records();
            acul::release(cell);
        }
        row.clear();
    }

    static void attach_table_cell(Table::Cell cell, Widget *focus_parent)
    {
        if (!cell) return;
        auto &id_map = detail::get_context().id_map;
        if (cell->widget_flags & WidgetFlagBits::attachable)
        {
            const auto current = id_map.find(cell->id());
            if (current == id_map.end() || current->second != cell) cell->on_attach();
            return;
        }
        for (auto *child : cell->children)
        {
            if (!child || !(child->widget_flags & WidgetFlagBits::attachable)) continue;
            child->set_focus_parent(focus_parent);
            const auto current = id_map.find(child->id());
            if (current == id_map.end() || current->second != child) child->on_attach();
        }
    }

    static void detach_table_cell(Table::Cell cell)
    {
        if (!cell) return;
        if (cell->widget_flags & WidgetFlagBits::attachable)
        {
            cell->on_detach();
            return;
        }
        for (auto *child : cell->children)
            if (child && (child->widget_flags & WidgetFlagBits::attachable)) child->on_detach();
    }

    Table::Table(u32 id, Rows rows, amal::vec2 inline_size, WidgetFlags flags, u32 style_tag_id)
        : Widget(id, flags, EventFlagBits::click | EventFlagBits::hover | EventFlagBits::drag,
                 {{0.0f, 0.0f}, inline_size}, style_tag_id),
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
        clear_cells();
        _cell_style_tags.clear();
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
        if (!_model_binding->presenter.present_record)
        {
            _model_binding->presenter.data = nullptr;
            _model_binding->presenter.present_record = present_model_text_record;
        }
        _model_binding->on_records = [this](const ModelRecordsEvent &) { rebuild_from_model_binding(); };
        _model_binding->on_field_change = [this](ModelRecordID, ModelFieldID) { rebuild_from_model_binding(); };
        attach_model_binding(*_model_binding);
        rebuild_model_binding_records(*_model_binding);
        rebuild_from_model_binding();
    }

    void Table::set_rows(Rows rows)
    {
        for (auto &row : _rows) release_table_row(row);
        _rows.clear();
        _rows = std::move(rows);
        _cell_style_tags.clear();
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
            u32 row_index = 0u;
            for (ModelRecordID record_id : _model_binding->records)
            {
                auto *record = model->find_record(record_id);
                Row row;
                if (record)
                {
                    const auto &field_ids = _model_binding->presenter.field_ids;
                    acul::vector<Widget *> widgets(field_ids.size(), nullptr);
                    _model_binding->presenter.present_record(_model_binding, *record, row_index, widgets.data(),
                                                             static_cast<u32>(widgets.size()),
                                                             _model_binding->presenter.data);
                    row.reserve(widgets.size());
                    for (auto *widget : widgets) row.push_back(make_table_cell(widget));
                }
                add_row(std::move(row));
                ++row_index;
            }
        }
    }

    void Table::add_row(Row row)
    {
        _rows.push_back(std::move(row));
        rebuild_cells();
        invalidate_layout();
    }

    void Table::set_cell(size_t row, size_t column, Cell value)
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
        release_table_row(_header);
        _header = std::move(header);
        rebuild_cells();
        invalidate_layout();
    }

    void Table::clear_header()
    {
        release_table_row(_header);
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
        sync_cell_parents();
        invalidate_layout();
    }

    void Table::set_cell_style_tag(u32 tag_id)
    {
        if (_cell_style.tag_id == tag_id) return;
        _cell_style = {Theme::STYLE_ID_INVALID, tag_id};
        sync_cell_parents();
        invalidate_layout();
    }

    void Table::set_cell_style_tag(size_t row, size_t column, u32 tag_id)
    {
        if (row >= _rows.size() || column >= _column_count) return;
        const size_t index = cell_element_id(row, column);
        const size_t prev_size = _cell_style_tags.size();
        if (index >= prev_size)
        {
            _cell_style_tags.resize(index + 1u);
            for (size_t i = prev_size; i < _cell_style_tags.size(); ++i) _cell_style_tags[i] = 0u;
        }
        if (_cell_style_tags[index] == tag_id) return;
        _cell_style_tags[index] = tag_id;
        sync_cell_parents();
        invalidate_layout();
    }

    void Table::clear_cell_style_tag(size_t row, size_t column)
    {
        if (row >= _rows.size() || column >= _column_count) return;
        const size_t index = cell_element_id(row, column);
        if (index >= _cell_style_tags.size() || _cell_style_tags[index] == 0u) return;
        _cell_style_tags[index] = 0u;
        sync_cell_parents();
        invalidate_layout();
    }

    u32 Table::cell_style_tag(size_t row, size_t column) const
    {
        if (row >= _rows.size() || column >= _column_count) return 0u;
        const size_t index = cell_element_id(row, column);
        return index < _cell_style_tags.size() ? _cell_style_tags[index] : 0u;
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

    Table::Cell Table::CellRef::value() const { return valid() ? table->_rows[row_index][column_index] : nullptr; }

    bool Table::ConstCellRef::valid() const
    {
        return table && row_index < table->_rows.size() && column_index < table->_rows[row_index].size();
    }

    Table::ConstCell Table::ConstCellRef::value() const
    {
        return valid() ? table->_rows[row_index][column_index] : nullptr;
    }

    Table::Cell Table::Column::header() const
    {
        return table && column_index < table->_header.size() ? table->_header[column_index] : nullptr;
    }

    Table::Cell Table::Column::cell(size_t row) const
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

    Table::ConstCell Table::ConstColumn::header() const
    {
        return table && column_index < table->_header.size() ? table->_header[column_index] : nullptr;
    }

    Table::ConstCell Table::ConstColumn::cell(size_t row) const
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
        _cell_style_tags.clear();
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
        _cell_style_tags.clear();
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
        _cell_style_tags.clear();
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
        _cell_style_tags.clear();
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
        const auto is_local_element_tag = [this](u32 tag_id) {
            return tag_id == AUIK_TAG_TABLE_HEADER_CELL || tag_id == AUIK_TAG_TABLE_CELL ||
                   is_resize_border_tag(tag_id);
        };
        if (is_local_element_tag(transition.prev_id.tag_id) || is_local_element_tag(transition.current_id.tag_id))
            out |= StyleUpdateFlagBits::redraw;
        out |= update_resize_indicator();

        auto update_cell = [&](DrawBlock *cell) {
            if (!cell || !cell->is_visible()) return;
            const auto &element = cell->get_rect().id;
            cell->set_style_state(transition.current_id == element ? transition.current_state : StyleState::normal);
            out |= cell->update_style();
        };
        for (auto *cell : _header) update_cell(cell);
        for (auto &row : _rows)
            for (auto *cell : row) update_cell(cell);
        return out;
    }

    void Table::update_layout_min_size()
    {
        _column_count = resolve_column_count();
        _layout_metrics.assign(amal::max(_column_count, _rows.size()), {});

        f32 header_height = 0.0f;
        for (size_t column = 0; column < _column_count; ++column)
        {
            auto *cell = header_block(column);
            if (!cell || !cell->is_visible()) continue;
            cell->update_layout_min_size();
            _layout_metrics[column].x.min_value =
                amal::max(_layout_metrics[column].x.min_value, cell->required_size().x);
            header_height = amal::max(header_height, cell->required_size().y);
        }

        for (size_t row = 0; row < _rows.size(); ++row)
        {
            for (size_t column = 0; column < _column_count; ++column)
            {
                auto *cell = cell_block(row, column);
                if (!cell || !cell->is_visible()) continue;
                cell->update_layout_min_size();
                _layout_metrics[column].x.min_value =
                    amal::max(_layout_metrics[column].x.min_value, cell->required_size().x);
                _layout_metrics[row].y.min_value = amal::max(_layout_metrics[row].y.min_value, cell->required_size().y);
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
            detail::resolve_table_required_axis(style_size().x, fill_width(), content_w + padding.x + padding.z);
        const f32 required_height =
            detail::resolve_table_required_axis(style_size().y, fill_height(), content_h + padding.y + padding.w);

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

        amal::vec2 outer_size = {amal::max(size().x - margin.x - margin.z, 0.0f),
                                 amal::max(size().y - margin.y - margin.w, 0.0f)};
        if (!is_width_fixed()) outer_size.x = amal::max(outer_size.x, required_inner.x);
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
            for (auto *cell : _header)
                if (cell && cell->is_visible()) header_h = amal::max(header_h, cell->required_size().y);

            f32 cursor_x = inner_pos.x;
            for (size_t column = 0; column < _column_count; ++column)
            {
                const f32 column_w = _layout_metrics[column].x.value;
                auto *cell = header_block(column);
                if (cell && cell->is_visible())
                {
                    cell->set_position({cursor_x, cursor_y});
                    cell->set_layout_size({column_w, header_h});
                    cell->update_layout(true);
                }
                cursor_x += column_w;
            }
            cursor_y += header_h;
        }

        for (size_t row = 0; row < _rows.size(); ++row)
        {
            const f32 row_h = _layout_metrics[row].y.value;
            if (row < _alt_row_visuals.size())
            {
                _alt_row_visuals[row].rect = detail::make_rect_data(
                    id(), AUIK_STYLE_TAG_TABLE_ROW_ALT, {{inner_pos.x, cursor_y}, {inner_size.x, row_h}}, clip_id(),
                    next_depth(detail::depth_work_range(depth_range())));
            }

            f32 cursor_x = inner_pos.x;
            for (size_t column = 0; column < _column_count; ++column)
            {
                const f32 column_w = _layout_metrics[column].x.value;
                auto *cell = cell_block(row, column);
                if (cell && cell->is_visible())
                {
                    cell->set_position({cursor_x, cursor_y});
                    cell->set_layout_size({column_w, row_h});
                    cell->update_layout(true);
                }
                cursor_x += column_w;
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
        const bool resize_border_visible = (resize_style.mask() & detail::g_style_visible_draw_mask) != 0u;
        const amal::vec4 resize_margin = resize_style.margin();
        const amal::vec4 resize_padding = resize_style.padding();
        const f32 resize_w = resize_border_visible
                                 ? amal::max(resize_margin.x + resize_margin.z + resize_padding.x + resize_padding.z,
                                             resize_style.border_thickness())
                                 : 0.0f;
        if (resize_w > 0.0f)
        {
            f32 cursor_x = inner_pos.x;
            for (size_t column = 0; column + 1 < _column_count; ++column)
            {
                cursor_x += _layout_metrics[column].x.value;
                if (column >= _resize_border_hit_visuals.size()) continue;
                auto &visual = _resize_border_hit_visuals[column].x;
                visual.rect = detail::make_rect_data(
                    id(), AUIK_TAG_TABLE_RESIZE_BORDER_V,
                    {{cursor_x - resize_w * 0.5f, inner_pos.y}, {resize_w, cursor_y - inner_pos.y}}, clip_id(),
                    depth_range().y, 0u, static_cast<u32>(column));
            }
        }

        const f32 resize_h = resize_border_visible
                                 ? amal::max(resize_margin.y + resize_margin.w + resize_padding.y + resize_padding.w,
                                             resize_style.border_thickness())
                                 : 0.0f;
        if (resize_h > 0.0f)
        {
            f32 row_cursor_y = inner_pos.y;
            if (has_header())
            {
                f32 header_h = 0.0f;
                for (auto *cell : _header)
                    if (cell) header_h = amal::max(header_h, cell->size().y);
                row_cursor_y += header_h;
            }
            for (size_t row = 0; row + 1 < _rows.size(); ++row)
            {
                row_cursor_y += _layout_metrics[row].y.value;
                if (row >= _resize_border_hit_visuals.size()) continue;
                auto &visual = _resize_border_hit_visuals[row].y;
                visual.rect =
                    detail::make_rect_data(id(), AUIK_TAG_TABLE_RESIZE_BORDER_H,
                                           {{inner_pos.x, row_cursor_y - resize_h * 0.5f}, {inner_size.x, resize_h}},
                                           clip_id(), depth_range().y, 0u, static_cast<u32>(row));
            }
        }
        update_resize_indicator();
        update_cell_clip_rects();
    }

    void Table::translate(const amal::vec2 &delta)
    {
        if (delta.x == 0.0f && delta.y == 0.0f) return;
        Widget::translate(delta);
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
            if (!cell || !cell->is_visible()) continue;
            cell->update_depth(content_range);
        }
        for (auto &row : _rows)
            for (auto *cell : row)
                if (cell && cell->is_visible()) cell->update_depth(content_range);
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
        if (!is_visible() && !(ctx.reason & DrawReasonBits::invalidate)) return;
        auto *quads_stream = get_primary_quads_stream();
        auto *theme = get_theme();

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

            if (!_resize_border_hit_visuals.empty())
            {
                const size_t column_resize_count = _column_count > 0u ? _column_count - 1u : 0u;
                const size_t row_resize_count = _rows.size() > 0u ? _rows.size() - 1u : 0u;
                const bool column_resize_hit = column_resizable() && can_emit_hit(ctx);
                const bool row_resize_hit = row_resizable() && can_emit_hit(ctx);
                for (size_t index = 0; index < _resize_border_hit_visuals.size(); ++index)
                {
                    auto &visuals = _resize_border_hit_visuals[index];
                    if (index < column_resize_count)
                        detail::draw_table_resize_border_visual(ctx, quads_stream, visuals.x, theme,
                                                                _resize_border_style.tag_id, id(), clip_id(),
                                                                column_resize_hit);
                    if (index < row_resize_count)
                        detail::draw_table_resize_border_visual(ctx, quads_stream, visuals.y, theme,
                                                                _resize_border_style.tag_id, id(), clip_id(),
                                                                row_resize_hit);
                }
            }
        }

        const amal::vec4 content_clip = get_content_clip_rect();
        for (auto *cell : _header)
        {
            if (!cell) continue;
            DrawCtx cell_ctx = ctx;
            detail::draw_child_in_clip(cell, cell_ctx, content_clip);
        }
        for (auto &row : _rows)
            for (auto *cell : row)
            {
                if (!cell) continue;
                DrawCtx cell_ctx = ctx;
                detail::draw_child_in_clip(cell, cell_ctx, content_clip);
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
            mark_host_refresh_request();
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

    void Table::on_detach()
    {
        for (auto *cell : _header) detach_table_cell(cell);
        for (auto &row : _rows)
            for (auto *cell : row) detach_table_cell(cell);
        Widget::on_detach();
    }

    void Table::rebuild_cells()
    {
        _column_count = resolve_column_count();
        sync_cell_parents();
        update_depth(depth_range());
    }

    void Table::clear_cells(bool invalidate_draw)
    {
        release_table_row(_header, invalidate_draw);
        for (auto &row : _rows) release_table_row(row, invalidate_draw);
        _rows.clear();
        _alt_row_visuals.clear();
        _cell_style_tags.clear();
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

    Table::Cell Table::header_block(size_t column) const { return column < _header.size() ? _header[column] : nullptr; }

    Table::Cell Table::cell_block(size_t row, size_t column) const
    {
        if (row >= _rows.size()) return nullptr;
        return column < _rows[row].size() ? _rows[row][column] : nullptr;
    }

    u32 Table::resolved_cell_style_tag(size_t row, size_t column) const
    {
        const u32 explicit_tag = cell_style_tag(row, column);
        if (explicit_tag != 0u) return explicit_tag;
        return _cell_style.tag_id;
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
        mark_host_refresh_request();
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
        mark_host_refresh_request();
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
            for (auto *cell : row)
            {
                if (!cell) continue;
                cell->set_clip_id(content_clip_id());
                cell->rebuild_clip_rects();
            }
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
            QuadsInstanceData indicator{};
            indicator.rect = _resize_indicator_visual.rect.bounds;
            detail::apply_table_resize_border_visual_box(indicator, style);
            _resize_indicator_visual.rect.bounds = indicator.rect;
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
            QuadsInstanceData indicator{};
            indicator.rect = _resize_indicator_visual.rect.bounds;
            detail::apply_table_resize_border_visual_box(indicator, style);
            _resize_indicator_visual.rect.bounds = indicator.rect;
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
        const bool attached =
            detail::g_context && detail::get_context().id_map.find(id()) != detail::get_context().id_map.end();
        for (size_t column = 0u; column < _header.size(); ++column)
        {
            auto *cell = _header[column];
            if (cell)
            {
                cell->set_parent(this);
                cell->set_focus_parent(this);
                if (cell->id() == AUIK_TAG_TABLE_CELL) cell->set_style_tag(_header_cell_style.tag_id);
                if (attached) attach_table_cell(cell, this);
            }
        }
        for (size_t row = 0u; row < _rows.size(); ++row)
            for (size_t column = 0u; column < _rows[row].size(); ++column)
            {
                auto *cell = _rows[row][column];
                if (cell)
                {
                    cell->set_parent(this);
                    cell->set_focus_parent(this);
                    if (cell->id() == AUIK_TAG_TABLE_CELL) cell->set_style_tag(resolved_cell_style_tag(row, column));
                    if (attached) attach_table_cell(cell, this);
                }
            }
    }

    namespace
    {
        constexpr u32 g_persistent_table_flags = AUIK_TABLE_FLAG_ALTERNATING_ROWS | AUIK_TABLE_FLAG_COLUMN_RESIZABLE |
                                                 AUIK_TABLE_FLAG_ROW_RESIZABLE | AUIK_TABLE_FLAG_COLUMN_SIZE_OVERRIDES |
                                                 AUIK_TABLE_FLAG_ROW_SIZE_OVERRIDES;

        void write_column_settings(acul::bin_stream &stream, const TableColumnSettings &settings)
        {
            stream.write(static_cast<u8>(settings.sizing))
                .write(settings.value)
                .write(settings.min_width)
                // Keep the two retired alignment bytes for stream compatibility. Cell contents
                // are now laid out entirely by the DrawBlock stored in the cell.
                .write(static_cast<u8>(HAlign::left))
                .write(static_cast<u8>(VAlign::none));
        }

        TableColumnSettings read_column_settings(acul::bin_stream &stream)
        {
            TableColumnSettings settings{};
            u8 sizing = static_cast<u8>(TableColumnSizing::stretch);
            u8 halign = static_cast<u8>(HAlign::left);
            u8 valign = static_cast<u8>(VAlign::none);
            stream.read(sizing).read(settings.value).read(settings.min_width).read(halign).read(valign);
            settings.sizing = static_cast<TableColumnSizing>(sizing);
            return settings;
        }

        struct CellStyleEntry
        {
            u32 row = 0u;
            u32 column = 0u;
            u32 tag_id = 0u;
        };

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
            {
                auto *widget = static_cast<Widget *>(blocks[cell_i]);
                row[columns[cell_i]] = widget && widget->signature() == AUIK_TAG_DRAW_BLOCK
                                           ? static_cast<DrawBlock *>(widget)
                                           : make_table_cell(widget);
            }
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
                .write(table->table_flags() & g_persistent_table_flags);

            write_column_settings(stream, table->default_column_settings());

            const auto &column_settings = table->column_settings();
            stream.write(static_cast<u32>(column_settings.size()));
            for (const auto &settings : column_settings) write_column_settings(stream, settings);

            const auto &size_overrides = table->size_overrides();
            stream.write(static_cast<u32>(size_overrides.size()));
            for (const auto &value : size_overrides) stream.write(value);

            u32 cell_style_count = 0u;
            for (size_t row = 0u; row < table->row_count(); ++row)
                for (size_t column = 0u; column < table->column_count(); ++column)
                    if (table->cell_style_tag(row, column) != 0u) ++cell_style_count;
            stream.write(cell_style_count);
            for (size_t row = 0u; row < table->row_count(); ++row)
            {
                for (size_t column = 0u; column < table->column_count(); ++column)
                {
                    const u32 tag_id = table->cell_style_tag(row, column);
                    if (tag_id == 0u) continue;
                    stream.write(static_cast<u32>(row)).write(static_cast<u32>(column)).write(tag_id);
                }
            }

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
            u32 table_flags = 0u;
            stream.read(style_tag)
                .read(header_cell_style_tag)
                .read(cell_style_tag)
                .read(alternating_row_style_tag)
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

            u32 cell_style_count = 0u;
            stream.read(cell_style_count);
            acul::vector<CellStyleEntry> cell_styles;
            cell_styles.reserve(cell_style_count);
            for (u32 i = 0u; i < cell_style_count; ++i)
            {
                CellStyleEntry entry{};
                stream.read(entry.row).read(entry.column).read(entry.tag_id);
                cell_styles.push_back(entry);
            }

            auto *table = acul::alloc<Table>(common.id, Table::Rows{}, common.inline_size,
                                             WidgetFlags(common.widget_flags), style_tag);
            table->set_header_cell_style_tag(header_cell_style_tag);
            table->set_cell_style_tag(cell_style_tag);
            table->set_alternating_row_style_tag(alternating_row_style_tag);
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

            for (const auto &entry : cell_styles)
                if (entry.tag_id != 0u) table->set_cell_style_tag(entry.row, entry.column, entry.tag_id);

            detail::apply_widget_common_data(table, common);
            return table;
        }
    } // namespace

    namespace streams
    {
        AUIK_EXPORT const umbf::streams::Stream table{read_table, write_table};
    } // namespace streams

} // namespace auik
