#include <amal/trigonometric.hpp>
#include <auik/auik.hpp>
#include <auik/detail/depth.hpp>
#include <auik/detail/rect.hpp>
#include <auik/pipelines.hpp>
#include <auik/post_effects.hpp>
#include <auik/widgets/detail/draw_cull.hpp>
#include <auik/widgets/image.hpp>
#include <auik/widgets/text.hpp>
#include <auik/widgets/tree.hpp>
#include "../core/session_stream_utils.hpp"

#define AUIK_TABLE_TREE_ARROW_ROTATE_DURATION 0.16

namespace auik
{
    struct Tree::ModelData
    {
        ModelBinding *binding = nullptr;
        acul::vector<ModelRecordID> record_ids;
        acul::hashmap<ModelRecordID, size_t> record_nodes;
        ModelFieldID parent_field_id = AUIK_TREE_PARENT_FIELD;
    };

    static inline bool valid_tree_rect_size(const amal::vec2 &size) { return size.x > 0.0f && size.y > 0.0f; }

    static inline detail::TableColumnLayoutSettings to_layout_settings(const TableColumnSettings &settings)
    {
        return {static_cast<u8>(settings.sizing), settings.value, settings.min_width};
    }

    static inline amal::vec2 tree_icon_outer_size(const Style &style, amal::vec2 icon_size)
    {
        const amal::vec4 margin = style.margin();
        const amal::vec4 padding = style.padding();
        return {icon_size.x + margin.x + margin.z + padding.x + padding.z,
                icon_size.y + margin.y + margin.w + padding.y + padding.w};
    }

    static inline f32 tree_arrow_slot_width(const Style &style, amal::vec2 icon_size, f32 fallback)
    {
        return amal::max(fallback, tree_icon_outer_size(style, icon_size).x);
    }

    static inline f32 tree_icon_center_x(f32 slot_x, f32 slot_width, const Style &style)
    {
        const amal::vec4 margin = style.margin();
        const amal::vec4 padding = style.padding();
        const f32 content_x = slot_x + margin.x + padding.x;
        const f32 content_w = amal::max(slot_width - margin.x - margin.z - padding.x - padding.z, 0.0f);
        return content_x + content_w * 0.5f;
    }

    static DrawBlock *wrap_tree_cell(Widget *child)
    {
        auto *cell = acul::alloc<DrawBlock>(AUIK_TAG_TABLE_CELL, WidgetFlagBits::visible, AUIK_STYLE_TAG_TREE_CELL);
        if (child) cell->add_child(child);
        return cell;
    }

    static void attach_tree_cell(DrawBlock *cell, Widget *focus_parent)
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

    static void detach_tree_cell(DrawBlock *cell)
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

    static inline void build_tree_arrow_vertices(TexturedVertexStreamVertex (&vertices)[4], const amal::rect &icon_rect,
                                                 const amal::rect &uv_rect, f32 z, u32 clip_id)
    {
        const amal::vec2 min = icon_rect.offset;
        const amal::vec2 max = icon_rect.offset + icon_rect.size;
        const amal::vec2 uv_min = uv_rect.offset;
        const amal::vec2 uv_max = uv_rect.offset + uv_rect.size;
        vertices[0] = {{min.x, min.y}, z, 0.0f, {uv_min.x, uv_min.y}, clip_id};
        vertices[1] = {{max.x, min.y}, z, 0.0f, {uv_max.x, uv_min.y}, clip_id};
        vertices[2] = {{max.x, max.y}, z, 0.0f, {uv_max.x, uv_max.y}, clip_id};
        vertices[3] = {{min.x, max.y}, z, 0.0f, {uv_min.x, uv_max.y}, clip_id};
    }

    static inline bool tree_contains_child(const acul::vector<Tree::Node> &nodes, size_t parent)
    {
        for (const auto &node : nodes)
            if (node.parent == parent) return true;
        return false;
    }

    static inline bool resolve_tree_icon(u32 icon_id, TextureID &texture, amal::rect &uv_rect, amal::vec2 &size)
    {
        if (auto *cached = get_cached_image(icon_id))
        {
            texture = cached->texture_id();
            uv_rect = {cached->uv_offset(), cached->uv_size()};
            size = cached->size();
            return texture.handle != 0;
        }
        texture = {};
        uv_rect = {{0.0f, 0.0f}, {1.0f, 1.0f}};
        size = {0.0f, 0.0f};
        return false;
    }

    Tree::Tree(u32 id, amal::vec2 inline_size, WidgetFlags flags, u32 style_tag_id)
        : Widget(id, flags, EventFlagBits::click | EventFlagBits::hover | EventFlagBits::drag,
                 {{0.0f, 0.0f}, inline_size}, style_tag_id),
          _style({Theme::STYLE_ID_INVALID, style_tag_id})
    {
    }

    Tree::~Tree()
    {
        if (_model_data)
        {
            _model_data->binding->on_records = nullptr;
            _model_data->binding->on_field_change = nullptr;
            _model_data->binding->on_field_change_batch = nullptr;
            detach_model_binding(*_model_data->binding);
            acul::release(_model_data);
        }
        release_arrow_animations();
        clear_cells(false);
        clear_nodes();
    }

    void Tree::clear()
    {
        if (_model_data)
        {
            _model_data->record_ids.clear();
            _model_data->record_nodes.clear();
        }
        clear_cells();
        clear_nodes();
        release_arrow_animations();
        rebuild_visible_nodes();
        invalidate_layout();
    }

    void Tree::set_model_binding(ModelBinding *binding)
    {
        acul::vector<ModelFieldID> fields;
        fields.push_back(1u);
        set_model_binding(binding, std::move(fields), AUIK_TREE_PARENT_FIELD);
    }

    void Tree::set_model_binding(ModelBinding *binding, acul::vector<ModelFieldID> field_ids,
                                 ModelFieldID parent_field_id)
    {
        if (_model_data)
        {
            _model_data->binding->on_records = nullptr;
            _model_data->binding->on_field_change = nullptr;
            _model_data->binding->on_field_change_batch = nullptr;
            detach_model_binding(*_model_data->binding);
            acul::release(_model_data);
            _model_data = nullptr;
        }
        if (!binding) return;
        _model_data = acul::alloc<ModelData>();
        _model_data->binding = binding;
        _model_data->parent_field_id = parent_field_id;

        binding->presenter.field_ids = std::move(field_ids);
        if (binding->presenter.field_ids.empty()) binding->presenter.field_ids.push_back(1u);
        if (!binding->presenter.present_record)
        {
            binding->presenter.data = nullptr;
            binding->presenter.present_record = present_model_text_record;
        }
        binding->on_records = [this](const ModelRecordsEvent &event) { defer_model_records(event); };
        binding->on_field_change = [this](ModelRecordID record_id, ModelFieldID field_id) {
            if (_model_data && field_id == _model_data->parent_field_id)
                defer_model_hierarchy_refresh();
            else defer_model_record_refresh(record_id);
        };
        binding->on_field_change_batch = [this](const ModelFieldChangeBatch &batch) {
            if (!_model_data) return;
            bool hierarchy_changed = false;
            for (const auto &change : batch)
                if (change.field_id == _model_data->parent_field_id)
                    hierarchy_changed = true;
            if (hierarchy_changed) defer_model_hierarchy_refresh();
            for (const auto &change : batch)
                if (change.field_id != _model_data->parent_field_id)
                    defer_model_record_refresh(change.record_id);
        };
        attach_model_binding(*binding);
        rebuild_model_binding_records(*binding);
        rebuild_from_model_binding();
    }

    void Tree::defer_model_records(ModelRecordsEvent event)
    {
        if (!_model_data) return;
        auto *expected = _model_data;
        const u32 widget_id = id();
        add_render_command([this, widget_id, expected, event]() {
            if (get_widget_by_id(widget_id) != this || _model_data != expected) return;
            apply_model_records(event);
        });
    }

    void Tree::defer_model_record_refresh(ModelRecordID record_id)
    {
        if (!_model_data || record_id == AUIK_MODEL_RECORD_ID_INVALID) return;
        auto *expected = _model_data;
        const u32 widget_id = id();
        add_render_command([this, widget_id, expected, record_id]() {
            if (get_widget_by_id(widget_id) != this || _model_data != expected) return;
            refresh_model_record(record_id);
        });
    }

    void Tree::defer_model_hierarchy_refresh()
    {
        if (!_model_data) return;
        auto *expected = _model_data;
        const u32 widget_id = id();
        add_render_command([this, widget_id, expected]() {
            if (get_widget_by_id(widget_id) != this || _model_data != expected) return;
            refresh_model_hierarchy();
        });
    }

    size_t Tree::node_from_widget(const Widget *widget) const
    {
        while (widget && widget != this)
        {
            const auto it = _widget_nodes.find(widget);
            if (it != _widget_nodes.end()) return it->second;
            widget = widget->parent();
        }
        return invalid_node;
    }

    size_t Tree::node_from_element(ElementID element) const { return node_from_widget(element_widget(element)); }

    size_t Tree::add_node(DrawBlock *label, Row cells, size_t parent)
    {
        if (parent >= _nodes.size()) parent = invalid_node;
        const size_t out = _nodes.size();
        _nodes.push_back({label, parent, true});
        _node_cells.push_back(std::move(cells));
        rebuild_visible_nodes();
        rebuild_cells();
        invalidate_layout();
        return out;
    }

    void Tree::set_node_expanded(size_t node, bool expanded, bool animate)
    {
        if (node >= _nodes.size() || _nodes[node].expanded == expanded || !node_has_children(node)) return;
        _nodes[node].expanded = expanded;
        if (animate) start_arrow_animation(node, expanded);
        rebuild_visible_nodes();
        rebuild_cells();
        invalidate_layout();
    }

    bool Tree::node_expanded(size_t node) const { return node < _nodes.size() && _nodes[node].expanded; }

    bool Tree::node_has_children(size_t node) const
    {
        if (node >= _nodes.size()) return false;
        return tree_contains_child(_nodes, node);
    }

    void Tree::set_alternating_rows(bool value)
    {
        if (alternating_rows() == value) return;
        detail::set_table_flag(_tree_flags, AUIK_TABLE_TREE_FLAG_ALTERNATING_ROWS, value);
        invalidate_layout();
    }

    void Tree::set_alternating_row_style_tag(u32 tag_id)
    {
        if (_alternating_row_style.tag_id == tag_id) return;
        _alternating_row_style = {Theme::STYLE_ID_INVALID, tag_id};
        invalidate_layout();
    }

    void Tree::set_default_column_settings(TableColumnSettings settings)
    {
        _default_column_settings = settings;
        invalidate_layout();
    }

    void Tree::set_column_settings(acul::vector<TableColumnSettings> settings)
    {
        _column_settings = std::move(settings);
        invalidate_layout();
    }

    void Tree::set_column_settings(size_t column, TableColumnSettings settings)
    {
        if (column >= _column_settings.size()) _column_settings.resize(column + 1u);
        _column_settings[column] = settings;
        invalidate_layout();
    }

    void Tree::clear_column_settings()
    {
        _column_settings.clear();
        detail::set_table_flag(_tree_flags, AUIK_TABLE_TREE_FLAG_COLUMN_SIZE_OVERRIDES, false);
        _size_overrides.clear();
        invalidate_layout();
    }

    void Tree::set_column_resizable(bool value)
    {
        if (column_resizable() == value) return;
        detail::set_table_flag(_tree_flags, AUIK_TABLE_TREE_FLAG_COLUMN_RESIZABLE, value);
        invalidate_layout();
    }

    void Tree::set_size_overrides(acul::vector<amal::vec2> values, bool column_overrides)
    {
        _size_overrides = std::move(values);
        detail::set_table_flag(_tree_flags, AUIK_TABLE_TREE_FLAG_COLUMN_SIZE_OVERRIDES, column_overrides);
        invalidate_layout();
    }

    void Tree::set_style_tag(u32 tag_id)
    {
        if (_style.tag_id == tag_id) return;
        _style = {Theme::STYLE_ID_INVALID, tag_id};
        set_rect_tag_id(tag_id);
        invalidate_layout();
    }

    void Tree::set_cell_style_tag(u32 tag_id)
    {
        if (_cell_style.tag_id == tag_id) return;
        _cell_style = {Theme::STYLE_ID_INVALID, tag_id};
        sync_cell_parents();
        invalidate_layout();
    }

    void Tree::set_line_style_tag(u32 tag_id)
    {
        if (_line_style.tag_id == tag_id) return;
        _line_style = {Theme::STYLE_ID_INVALID, tag_id};
        invalidate_layout();
    }

    void Tree::set_collapse_icon_style_tag(u32 tag_id)
    {
        if (_collapse_icon_style.tag_id == tag_id) return;
        _collapse_icon_style = {Theme::STYLE_ID_INVALID, tag_id};
        invalidate_layout();
    }

    bool Tree::is_row_hovered(size_t visible_row) const
    {
        const auto hover = detail::get_context().hover_id;
        return hover.widget_id == id() && hover.tag_id == AUIK_TAG_TABLE_CELL && _column_count > 0u &&
               hover.element_id / _column_count == visible_row;
    }

    bool Tree::is_arrow_hovered(size_t node) const
    {
        const auto hover = detail::get_context().hover_id;
        return hover.widget_id == id() && hover.tag_id == AUIK_TAG_TABLE_TREE_ARROW && hover.element_id == node;
    }

    bool Tree::is_resize_border_hovered(size_t element_id) const
    {
        const auto hover = detail::get_context().hover_id;
        return hover.widget_id == id() && hover.tag_id == AUIK_TAG_TABLE_RESIZE_BORDER_V &&
               hover.element_id == element_id;
    }

    StyleUpdateFlags Tree::update_style()
    {
        const u32 parent_id = parent() ? parent()->id() : 0u;
        const auto transition = detail::get_widget_style_selector_transition(id());
        StyleUpdateFlags out = resolve_style_selector(_style, id(), parent_id, style_state());
        out |= resolve_style_selector(_cell_style, _cell_style.tag_id, id(), StyleState::normal);
        out |= resolve_style_selector(_alternating_row_style, _alternating_row_style.tag_id, id(), StyleState::normal);
        out |= resolve_style_selector(_line_style, _line_style.tag_id, id(), StyleState::normal);
        out |= resolve_style_selector(_collapse_icon_style, _collapse_icon_style.tag_id, id(), StyleState::normal);
        const auto is_local_element_tag = [this](u32 tag_id) {
            return tag_id == signature() || tag_id == AUIK_TAG_TABLE_CELL || tag_id == AUIK_TAG_TABLE_TREE_ARROW ||
                   tag_id == AUIK_TAG_TABLE_RESIZE_BORDER_V;
        };
        if (is_local_element_tag(transition.prev_id.tag_id) || is_local_element_tag(transition.current_id.tag_id))
            out |= StyleUpdateFlagBits::redraw;
        if (supports_columns())
        {
            out |= resolve_style_selector(_resize_border_style, _resize_border_style.tag_id, id(), StyleState::normal);
            out |= update_resize_indicator();
        }
        for (auto &row : _cells)
            for (auto *cell : row)
            {
                if (!cell) continue;
                cell->set_style_state(transition.current_id == cell->get_rect().id ? transition.current_state
                                                                                   : StyleState::normal);
                for (auto *child : cell->children)
                    if (child)
                        child->set_style_state(transition.current_id == child->get_rect().id ? transition.current_state
                                                                                             : StyleState::normal);
                out |= cell->update_style_invalidated();
            }
        ensure_arrow_resources();
        return out;
    }

    void Tree::update_layout_min_size_force()
    {
        rebuild_visible_nodes();
        ensure_arrow_resources();
        _column_count = resolve_column_count();
        _layout_metrics.assign(amal::max(_column_count, _visible_nodes.size()), {});

        for (size_t row = 0; row < _visible_nodes.size(); ++row)
        {
            const size_t node = _visible_nodes[row];
            if (node_has_children(node))
            {
                amal::vec2 arrow_size = _arrow_size;
                if (arrow_size.y <= 0.0f)
                {
                    const auto &cell_style = get_theme()->get_style(_cell_style.id);
                    arrow_size = {cell_style.text_size(), cell_style.text_size()};
                }
                const auto &icon_style = get_theme()->get_style(_collapse_icon_style.id);
                _layout_metrics[row].y.min_value =
                    amal::max(_layout_metrics[row].y.min_value, tree_icon_outer_size(icon_style, arrow_size).y);
            }
            for (size_t column = 0; column < _column_count; ++column)
            {
                auto *cell = cell_widget(row, column);
                if (!cell) continue;
                // Measuring a cell must not perform its positioned layout. In particular,
                // container cells need an assigned clip rect before update_layout().
                cell->update_layout_min_size();
                f32 min_width = cell->required_size().x;
                if (column == 0u)
                {
                    const auto &icon_style = get_theme()->get_style(_collapse_icon_style.id);
                    const size_t depth = node_depth(node);
                    // Every level owns one disclosure slot. Expandable nodes draw an arrow in it; leaves draw a
                    // branch. Keeping the slot for root leaves aligns their content with expandable root nodes.
                    const size_t slot_count = depth + 1u;
                    min_width +=
                        static_cast<f32>(slot_count) * tree_arrow_slot_width(icon_style, _arrow_size, _indent_width);
                }
                _layout_metrics[column].x.min_value = amal::max(_layout_metrics[column].x.min_value, min_width);
                _layout_metrics[row].y.min_value = amal::max(_layout_metrics[row].y.min_value, cell->required_size().y);
            }
            _layout_metrics[row].y.value = _layout_metrics[row].y.min_value;
        }

        const auto &style = get_theme()->get_style(_style.id);
        const amal::vec4 margin = style.margin();
        const amal::vec4 padding = style.padding();

        update_column_widths(0.0f);

        f32 content_w = 0.0f;
        for (size_t column = 0; column < _column_count; ++column)
            content_w += amal::max(_layout_metrics[column].x.value, _layout_metrics[column].x.min_value);
        f32 content_h = 0.0f;
        for (size_t row = 0; row < _visible_nodes.size(); ++row) content_h += _layout_metrics[row].y.value;

        const f32 natural_width = content_w + padding.x + padding.z;
        const f32 natural_height = content_h + padding.y + padding.w;
        const f32 required_width =
            supports_columns() ? detail::resolve_table_required_axis(style_size().x, fill_width(), natural_width)
                               : natural_width;
        const f32 required_height =
            supports_columns() ? detail::resolve_table_required_axis(style_size().y, fill_height(), natural_height)
                               : natural_height;

        set_required_size({required_width + margin.x + margin.z, required_height + margin.y + margin.w});
    }

    void Tree::update_layout(bool min_size_known)
    {
        if (layout_measure_required(min_size_known)) update_layout_min_size_force();

        const auto &style = get_theme()->get_style(_style.id);
        const amal::vec4 margin = style.margin();
        const amal::vec4 padding = style.padding();
        const amal::vec2 layout_origin = position();
        const amal::vec2 outer_pos = {layout_origin.x + margin.x, layout_origin.y + margin.y};
        const amal::vec2 required_inner = {amal::max(required_size().x - margin.x - margin.z, 0.0f),
                                           amal::max(required_size().y - margin.y - margin.w, 0.0f)};

        amal::vec2 outer_size = {amal::max(size().x - margin.x - margin.z, 0.0f),
                                 amal::max(size().y - margin.y - margin.w, 0.0f)};
        if (!supports_columns())
        {
            outer_size = required_inner;
        }
        else
        {
            if (!is_width_fixed()) outer_size.x = amal::max(outer_size.x, required_inner.x);
            else if (outer_size.x <= 0.0f) outer_size.x = required_inner.x;
            if (outer_size.y <= 0.0f) outer_size.y = required_inner.y;
            if (!is_height_fixed()) outer_size.y = amal::max(outer_size.y, required_inner.y);
        }

        set_position(outer_pos);
        set_layout_size(outer_size);
        Widget::update_layout(true);
        rebuild_clip_rects();

        const amal::vec2 inner_pos = outer_pos + amal::vec2{padding.x, padding.y};
        const amal::vec2 inner_size = {amal::max(outer_size.x - padding.x - padding.z, 0.0f),
                                       amal::max(outer_size.y - padding.y - padding.w, 0.0f)};
        update_column_widths(inner_size.x);
        resize_visuals();
        for (size_t index = 0; supports_columns() && index < _resize_border_hit_visuals.size(); ++index)
        {
            _resize_border_hit_visuals[index].rect = detail::make_rect_data(
                id(), AUIK_TAG_TABLE_RESIZE_BORDER_V, {{0.0f, 0.0f}, {0.0f, 0.0f}}, clip_id(),
                next_depth(detail::depth_work_range(depth_range())), 0u, static_cast<u32>(index));
        }
        _tree_line_data.clear();

        acul::vector<f32> node_axis_centers(_nodes.size(), 0.0f);
        acul::vector<f32> node_branch_ends(_nodes.size(), 0.0f);
        acul::vector<bool> node_branch_ends_valid(_nodes.size(), false);
        const auto &line_style = get_theme()->get_style(_line_style.id);
        const amal::vec4 line_margin = line_style.margin();
        const amal::vec4 line_padding = line_style.padding();

        f32 cursor_y = inner_pos.y;
        for (size_t row = 0; row < _visible_nodes.size(); ++row)
        {
            const size_t node = _visible_nodes[row];
            const f32 row_h = _layout_metrics[row].y.value;
            // Tree padding is a content inset. Alternating rows still span the complete
            // widget width so root insets do not shorten row backgrounds.
            _alt_row_visuals[row].rect = detail::make_rect_data(
                id(), AUIK_STYLE_TAG_TREE_ROW_ALT, {{outer_pos.x, cursor_y}, {outer_size.x, row_h}}, clip_id(),
                next_depth(detail::depth_work_range(depth_range())));

            amal::vec2 tree_icon_size = _arrow_size;
            if (tree_icon_size.y <= 0.0f)
            {
                const auto &cell_style = get_theme()->get_style(_cell_style.id);
                tree_icon_size = {cell_style.text_size(), cell_style.text_size()};
            }
            const auto &icon_style = get_theme()->get_style(_collapse_icon_style.id);
            const f32 arrow_slot_w = tree_arrow_slot_width(icon_style, tree_icon_size, _indent_width);
            const size_t depth = node_depth(node);
            const f32 indent = static_cast<f32>(depth) * arrow_slot_w;
            const f32 nominal_arrow_slot_x = inner_pos.x + indent;
            f32 arrow_center = tree_icon_center_x(nominal_arrow_slot_x, arrow_slot_w, icon_style);
            if (depth > 0u)
            {
                const size_t parent = _nodes[node].parent;
                if (parent < node_branch_ends_valid.size() && node_branch_ends_valid[parent])
                    arrow_center = node_branch_ends[parent];
            }
            const f32 arrow_slot_x = arrow_center - tree_icon_center_x(0.0f, arrow_slot_w, icon_style);
            const f32 label_start_x = arrow_slot_x + arrow_slot_w;
            node_axis_centers[node] = arrow_center;
            _arrow_visuals[row].node = node;
            _arrow_visuals[row].rect = detail::make_rect_data(
                id(), AUIK_TAG_TABLE_TREE_ARROW, {{arrow_slot_x, cursor_y}, {arrow_slot_w, row_h}}, clip_id(),
                next_depth(detail::depth_foreground_range(depth_range())), 0u, static_cast<u32>(node));
            _arrow_visuals[row].icon_center_x = arrow_center;
            _arrow_visuals[row].icon_center_y = cursor_y + row_h * 0.5f;

            f32 cursor_x = inner_pos.x;
            for (size_t column = 0; column < _column_count; ++column)
            {
                const f32 column_w = _layout_metrics[column].x.value;
                auto *cell = cell_widget(row, column);
                if (cell)
                {
                    f32 left_offset = 0.0f;
                    if (column == 0u) left_offset = label_start_x - inner_pos.x;
                    cell->set_content_padding({left_offset, 0.0f, 0.0f, 0.0f});
                    cell->set_position({cursor_x, cursor_y});
                    cell->set_layout_size({column_w, row_h});
                    cell->update_layout(true);
                    if (column == 0u)
                        _arrow_visuals[row].icon_center_y = cell->bounds().offset.y + cell->bounds().size.y * 0.5f;
                }
                cursor_x += column_w;
            }

            // Left padding advances the child hierarchy axis into the first cell. Right margin only shortens the
            // visible horizontal connector, so its gap does not also change the indentation of descendants.
            node_branch_ends[node] = label_start_x + line_padding.x;
            node_branch_ends_valid[node] = true;
            const f32 line_thickness = amal::max(amal::round(line_style.border_thickness()), 1.0f);
            const f32 half_line = line_thickness * 0.5f;
            const f32 row_mid_y = amal::round(_arrow_visuals[row].icon_center_y);
            const f32 connector_gap = 2.0f;
            auto add_line = [&](amal::rect rect) {
                rect.offset.x = amal::round(rect.offset.x);
                rect.offset.y = amal::round(rect.offset.y);
                rect.size.x = amal::round(rect.size.x);
                rect.size.y = amal::round(rect.size.y);
                if (!valid_tree_rect_size(rect.size)) return;
                QuadsInstanceData line{};
                line.rect = rect;
                // Guides sit above the tree background but below row contents, so opaque parts of labels and
                // icons naturally hide the connector passing underneath them.
                line.z_order = detail::depth_foreground_range(depth_range()).x;
                _tree_line_data.push_back(line);
            };
            // Continue the vertical guides of ancestor levels which still have following siblings.
            for (size_t level = 0u; level < depth; ++level)
            {
                const size_t level_node = node_ancestor_at_depth(node, level);
                if (level_node >= _nodes.size() || node_is_last_sibling(level_node)) continue;
                const f32 line_center_x = node_axis_centers[level_node];
                const f32 line_x = amal::round(line_center_x - half_line);
                add_line({{line_x, amal::round(cursor_y)}, {line_thickness, amal::round(row_h)}});
            }

            // The current level enters the row through the disclosure slot. The horizontal connector is independent
            // of the label contents and is therefore emitted for both leaves and expandable nodes.
            const f32 own_axis = node_axis_centers[node];
            const f32 own_line_x = amal::round(own_axis - half_line);
            const f32 own_bottom = node_is_last_sibling(node) ? row_mid_y : amal::round(cursor_y + row_h);
            if (node_has_children(node))
            {
                const f32 arrow_h = amal::min(tree_icon_outer_size(icon_style, tree_icon_size).y, row_h);
                const f32 arrow_top = amal::round(row_mid_y - arrow_h * 0.5f - connector_gap);
                const f32 arrow_bottom = amal::round(row_mid_y + arrow_h * 0.5f + connector_gap);
                const f32 row_top = amal::round(cursor_y);
                add_line({{own_line_x, row_top},
                          {line_thickness, amal::max(amal::min(arrow_top, own_bottom) - row_top, 0.0f)}});
                if (!node_is_last_sibling(node))
                    add_line(
                        {{own_line_x, arrow_bottom}, {line_thickness, amal::max(own_bottom - arrow_bottom, 0.0f)}});
            }
            else
            {
                add_line({{own_line_x, amal::round(cursor_y)},
                          {line_thickness, amal::max(own_bottom - amal::round(cursor_y), 0.0f)}});
            }

            const f32 branch_start = amal::round(own_axis + line_margin.x);
            // Snap the connector length independently of its position. Rounding both endpoints made an otherwise
            // identical branch alternate by one pixel between hierarchy levels at fractional DPI scales.
            const f32 branch_width =
                amal::round(node_branch_ends[node] - own_axis - line_margin.x - line_margin.z);
            add_line({{branch_start, row_mid_y - half_line}, {amal::max(branch_width, 0.0f), line_thickness}});

            cursor_y += row_h;
        }

        auto *theme = get_theme();
        f32 resize_w = 0.0f;
        if (supports_columns())
        {
            const StyleID resize_style_id =
                _resize_border_style.id != Theme::STYLE_ID_INVALID
                    ? _resize_border_style.id
                    : theme->get_resolved_style(_resize_border_style.tag_id, _resize_border_style.tag_id, id(),
                                                StyleState::normal);
            const auto &resize_style = theme->get_style(resize_style_id);
            if ((resize_style.mask() & detail::g_style_visible_draw_mask) != 0u)
            {
                const amal::vec4 resize_margin = resize_style.margin();
                const amal::vec4 resize_padding = resize_style.padding();
                resize_w = amal::max(resize_margin.x + resize_margin.z + resize_padding.x + resize_padding.z,
                                     resize_style.border_thickness());
            }
        }
        if (supports_columns() && resize_w > 0.0f)
        {
            f32 cursor_x = inner_pos.x;
            for (size_t column = 0; column + 1 < _column_count; ++column)
            {
                cursor_x += _layout_metrics[column].x.value;
                if (column >= _resize_border_hit_visuals.size()) continue;
                auto &visual = _resize_border_hit_visuals[column];
                visual.rect = detail::make_rect_data(
                    id(), AUIK_TAG_TABLE_RESIZE_BORDER_V,
                    {{cursor_x - resize_w * 0.5f, inner_pos.y}, {resize_w, cursor_y - inner_pos.y}}, clip_id(),
                    depth_range().y, 0u, static_cast<u32>(column));
            }
        }

        update_resize_indicator();
        update_cell_clip_rects();
    }

    void Tree::translate(const amal::vec2 &delta)
    {
        if (delta.x == 0.0f && delta.y == 0.0f) return;
        Widget::translate(delta);
        for (auto &visual : _alt_row_visuals) visual.rect.bounds.offset += delta;
        for (auto &visual : _resize_border_hit_visuals) visual.rect.bounds.offset += delta;
        _resize_indicator_visual.rect.bounds.offset += delta;
        for (auto &line : _tree_line_data) line.rect.offset += delta;
        for (auto &visual : _arrow_visuals) visual.rect.bounds.offset += delta;
        for (auto &row : _cells)
            for (auto *cell : row)
                if (cell)
                {
                    cell->translate(delta);
                }
        const amal::vec4 parent_clip = parent() ? parent()->get_content_clip_rect() : get_main_viewport_rect();
        if (parent() && clip_id() == parent()->content_clip_id()) set_clip_id(0xFFFFu);
        ensure_own_clip_rect(detail::intersect_rects(parent_clip, {position().x, position().y, size().x, size().y}));
        update_cell_clip_rects();
    }

    void Tree::reset_clip_rect_records()
    {
        Widget::reset_clip_rect_records();
        for (auto &row : _cells)
            for (auto *cell : row)
                if (cell) cell->reset_clip_rect_records();
        for (auto &visual : _alt_row_visuals) visual.rect.clip_id = 0xFFFFu;
        for (auto &visual : _resize_border_hit_visuals) visual.rect.clip_id = 0xFFFFu;
        _resize_indicator_visual.rect.clip_id = 0xFFFFu;
        for (auto &visual : _arrow_visuals) visual.rect.clip_id = 0xFFFFu;
    }

    void Tree::rebuild_clip_rects()
    {
        const amal::vec4 parent_clip = parent() ? parent()->get_content_clip_rect() : get_main_viewport_rect();
        if (parent() && clip_id() == parent()->content_clip_id()) set_clip_id(0xFFFFu);
        ensure_own_clip_rect(detail::intersect_rects(parent_clip, {position().x, position().y, size().x, size().y}));
        update_cell_clip_rects();
    }

    void Tree::reset_draw_records()
    {
        Widget::reset_draw_records();
        _bg = {};
        for (auto &visual : _alt_row_visuals) visual.draw = {};
        for (auto &visual : _resize_border_hit_visuals) visual.draw = {};
        _resize_indicator_visual.draw = {};
        _tree_line_draws.clear();
        for (auto &visual : _arrow_visuals)
        {
            visual.hit_draw = {};
            visual.icon_draw = {};
        }
        for (auto &animation : _arrow_animations) animation.draw = {};
        for (auto &row : _cells)
            for (auto *cell : row)
                if (cell) cell->reset_draw_records();
    }

    void Tree::invalidate_style()
    {
        Widget::invalidate_style();
        for (auto &row : _cells)
            for (auto *cell : row)
                if (cell) cell->invalidate_style();
    }

    void Tree::add_state_flags_inherit(WidgetStateFlags flags)
    {
        Widget::add_state_flags_inherit(flags);
        if ((flags & WidgetStateFlagBits::visible) && !is_visible()) flags &= ~WidgetStateFlagBits::visible;
        for (auto &node : _nodes)
            if (node.label) node.label->add_state_flags_inherit(flags);
        for (auto &row : _node_cells)
            for (auto *cell : row)
                if (cell) cell->add_state_flags_inherit(flags);
    }

    void Tree::remove_state_flags_inherit(WidgetStateFlags flags)
    {
        Widget::remove_state_flags_inherit(flags);
        for (auto &node : _nodes)
            if (node.label) node.label->remove_state_flags_inherit(flags);
        for (auto &row : _node_cells)
            for (auto *cell : row)
                if (cell) cell->remove_state_flags_inherit(flags);
    }

    void Tree::update_depth(const amal::vec2 &depth_range)
    {
        Widget::update_depth(depth_range);
        const amal::vec2 content_range = detail::depth_foreground_range(this->depth_range());
        for (auto &row : _cells)
            for (auto *cell : row)
                if (cell) cell->update_depth(content_range);
    }

    void Tree::back_hit_depth()
    {
        Widget::back_hit_depth();
        for (auto &row : _cells)
            for (auto *cell : row)
                if (cell) cell->back_hit_depth();
        auto lower = [&](detail::TableCellVisual &visual) { visual.rect.hit_depth = get_rect().hit_depth; };
        for (auto &visual : _alt_row_visuals) lower(visual);
        for (auto &visual : _resize_border_hit_visuals) lower(visual);
        lower(_resize_indicator_visual);
        for (auto &visual : _arrow_visuals) visual.rect.hit_depth = get_rect().hit_depth;
        detail::get_context().dirty_flags |= DirtyFlagBits::hit_rect_update;
    }

    void Tree::restore_hit_depth()
    {
        Widget::restore_hit_depth();
        for (auto &row : _cells)
            for (auto *cell : row)
                if (cell) cell->restore_hit_depth();
        auto restore = [](detail::TableCellVisual &visual) { visual.rect.hit_depth = visual.rect.depth; };
        for (auto &visual : _alt_row_visuals) restore(visual);
        for (auto &visual : _resize_border_hit_visuals) restore(visual);
        restore(_resize_indicator_visual);
        for (auto &visual : _arrow_visuals) visual.rect.hit_depth = visual.rect.depth;
        detail::get_context().dirty_flags |= DirtyFlagBits::hit_rect_update;
    }

    void Tree::draw(DrawCtx &ctx)
    {
        if (!(widget_flags & WidgetFlagBits::visible)) return;
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
                detail::draw_table_cell_visual(ctx, quads_stream, _alt_row_visuals[row],
                                               theme->get_style(_alternating_row_style.id), clip_id(), false);
            }

            if (supports_columns() && !_resize_border_hit_visuals.empty())
            {
                const size_t resize_count = _column_count > 0u ? _column_count - 1u : 0u;
                const bool resize_hit = column_resizable() && can_emit_hit(ctx);
                for (size_t index = 0; index < _resize_border_hit_visuals.size() && index < resize_count; ++index)
                    detail::draw_table_resize_border_visual(ctx, quads_stream, _resize_border_hit_visuals[index], theme,
                                                            _resize_border_style.tag_id, id(), clip_id(), resize_hit);
            }

            draw_tree_lines(ctx, quads_stream);
        }

        for (auto &visual : _arrow_visuals) draw_arrow(ctx, visual);

        const amal::vec4 content_clip = get_content_clip_rect();
        for (auto &row : _cells)
        {
            for (auto *cell : row)
            {
                if (!cell) continue;
                DrawCtx cell_ctx = ctx;
                detail::draw_child_in_clip(cell, cell_ctx, content_clip);
            }
        }
    }

    bool Tree::update_locale()
    {
        bool changed = Widget::update_locale();
        for (auto &row : _cells)
            for (auto *cell : row)
                if (cell) changed |= cell->update_locale();
        return changed;
    }

    void Tree::on_click(MouseKey key, KeyPressState state, u32 click_count)
    {
        auto &ctx = detail::get_context();
        ClickEvent event{};
        event.key = key;
        event.state = state;
        event.click_count = click_count;
        event.target = ctx.io.clicked_id;
        event.drag_id = ctx.io.drag_id;
        event.mods = ctx.io.active_mods;

        const auto hover_id = ctx.hover_id;
        if (key == MouseKey::left && state == KeyPressState::press && hover_id.widget_id == id() &&
            hover_id.tag_id == AUIK_TAG_TABLE_TREE_ARROW)
        {
            add_render_command<detail::ClickEventTraits>(this,
                                                         [this, node = static_cast<size_t>(hover_id.element_id)]() {
                                                             if (node >= _nodes.size() || !node_has_children(node))
                                                                 return;
                                                             set_node_expanded(node, !_nodes[node].expanded);
                                                         });
            mark_host_refresh_request();
            return;
        }
        if (event.target.widget_id == id())
            if (auto *target = element_widget(event.target))
            {
                target->dispatch_click(key, state, click_count);
                return;
            }
        bool row_hit = false;
        if (event.target.element_id < _nodes.size())
        {
            const auto *label = _nodes[event.target.element_id].label;
            if (label)
            {
                const auto bounds = label->bounds();
                const auto mouse = get_mouse_pos();
                row_hit = mouse.x >= bounds.offset.x && mouse.x < bounds.offset.x + bounds.size.x &&
                          mouse.y >= bounds.offset.y && mouse.y < bounds.offset.y + bounds.size.y;
            }
        }
        if (_on_background_click && !row_hit && event.target.widget_id == id() &&
            event.target.tag_id != AUIK_TAG_TABLE_TREE_ARROW && event.target.tag_id != AUIK_TAG_TABLE_RESIZE_BORDER_V)
            _on_background_click(event);
    }

    void Tree::on_hover(HoverState state)
    {
        auto &ctx = detail::get_context();
        if (state == HoverState::leave)
        {
            if (auto *target = element_widget(ctx.last_hover_id)) target->dispatch_hover(HoverState::leave);
        }
        else if (auto *target = element_widget(ctx.hover_id)) target->dispatch_hover(HoverState::enter);

        detail::CursorID::enum_type cursor = detail::CursorID::arrow;
        bool resize_border_state = state == HoverState::leave;
        if (state != HoverState::leave && supports_columns() && column_resizable() && ctx.hover_id.widget_id == id() &&
            ctx.hover_id.tag_id == AUIK_TAG_TABLE_RESIZE_BORDER_V)
        {
            cursor = detail::CursorID::resize_ew;
            resize_border_state = true;
        }
        detail::set_window_cursor(cursor, ctx.window_ctx);
        if (resize_border_state)
        {
            if (supports_columns()) update_resize_indicator();
            ctx.dirty_flags |= DirtyFlagBits::redraw;
            mark_host_refresh_request();
        }
    }

    void Tree::on_drag(const amal::vec2 &delta, KeyPressState state)
    {
        auto &ctx = detail::get_context();
        const auto drag_id = ctx.io.drag_id;
        const bool bubble_drag = drag_id.widget_id == id() && drag_id.tag_id != AUIK_TAG_TABLE_TREE_ARROW &&
                                 drag_id.tag_id != AUIK_TAG_TABLE_RESIZE_BORDER_V;
        if (bubble_drag)
        {
            auto *target = focus_parent();
            if (target && target != this && target->has_event_handler(EventFlagBits::drag))
            {
                target->dispatch_drag(delta, state);
                return;
            }
        }

        if (state == KeyPressState::press)
        {
            _resizing_column = static_cast<size_t>(-1);
            _resize_drag_accum = {0.0f, 0.0f};
            _resize_size_basis.clear();
            if (!supports_columns() || !column_resizable() || drag_id.widget_id != id() ||
                drag_id.tag_id != AUIK_TAG_TABLE_RESIZE_BORDER_V)
                return;

            _resizing_column = drag_id.element_id;
            detail::resize_table_size_points(_resize_size_basis, _column_count);
            if (detail::has_table_flag(_tree_flags, AUIK_TABLE_TREE_FLAG_COLUMN_SIZE_OVERRIDES))
                for (size_t column = 0; column < _column_count; ++column)
                    _resize_size_basis[column].x = column < _size_overrides.size() ? _size_overrides[column].x : 0.0f;
            else
                for (size_t column = 0; column < _column_count; ++column)
                    _resize_size_basis[column].x = _layout_metrics[column].x.value;
            detail::set_window_cursor(detail::CursorID::resize_ew, detail::get_context().window_ctx);
            return;
        }

        if (state == KeyPressState::release)
        {
            _resizing_column = static_cast<size_t>(-1);
            _resize_drag_accum = {0.0f, 0.0f};
            _resize_size_basis.clear();
            detail::set_window_cursor(detail::CursorID::arrow, detail::get_context().window_ctx);
            return;
        }

        _resize_drag_accum += delta;
        const bool changed =
            supports_columns() && column_resizable() &&
            detail::apply_table_column_resize(
                _layout_metrics, _size_overrides, _resize_size_basis, _column_count, _resizing_column,
                _resize_drag_accum.x, detail::has_table_flag(_tree_flags, AUIK_TABLE_TREE_FLAG_COLUMN_SIZE_OVERRIDES),
                [this](size_t column) { return to_layout_settings(settings_for_column(column)); });
        if (!changed) return;
        detail::set_table_flag(_tree_flags, AUIK_TABLE_TREE_FLAG_COLUMN_SIZE_OVERRIDES, true);
        update_own_layout();
    }

    amal::vec4 Tree::get_content_clip_rect() const
    {
        if (clip_id() == 0xFFFFu) return parent() ? parent()->get_content_clip_rect() : get_main_viewport_rect();
        return get_clip_rect(content_clip_id());
    }

    void Tree::on_attach()
    {
        Widget::on_attach();
        sync_cell_parents();
    }

    void Tree::on_detach()
    {
        for (auto &row : _cells)
            for (auto *cell : row) detach_tree_cell(cell);
        Widget::on_detach();
    }

    void Tree::rebuild_visible_nodes()
    {
        _visible_nodes.clear();
        auto append_children = [this](auto &&self, size_t parent) -> void {
            for (size_t index = 0; index < _nodes.size(); ++index)
            {
                if (_nodes[index].parent != parent) continue;
                _visible_nodes.push_back(index);
                if (_nodes[index].expanded) self(self, index);
            }
        };
        append_children(append_children, invalid_node);
    }

    void Tree::rebuild_cells()
    {
        auto previous_cells = std::move(_cells);
        _cells.clear();
        _widget_nodes.clear();
        for (size_t node = 0u; node < _nodes.size(); ++node)
        {
            if (_nodes[node].label) _widget_nodes[_nodes[node].label] = node;
            if (node < _node_cells.size())
                for (auto *cell : _node_cells[node])
                    if (cell) _widget_nodes[cell] = node;
        }
        _column_count = resolve_column_count();
        _cells.reserve(_visible_nodes.size());
        for (size_t visible_row = 0; visible_row < _visible_nodes.size(); ++visible_row)
        {
            const auto &node = _nodes[_visible_nodes[visible_row]];
            _cells.emplace_back();
            auto &row = _cells.back();
            row.reserve(_column_count);
            row.push_back(static_cast<DrawBlock *>(node.label));
            if (supports_columns())
                for (auto *cell : _node_cells[_visible_nodes[visible_row]])
                    row.push_back(static_cast<DrawBlock *>(cell));
        }
        // Rows are a view of owned nodes. Moving a row must not discard the
        // draw IDs of its text, icons or controls.
        acul::hashmap<DrawBlock *, bool> retained;
        for (auto &row : _cells)
            for (auto *cell : row)
                if (cell) retained[cell] = true;
        for (auto &row : previous_cells)
            for (auto *cell : row)
            {
                if (!cell || retained.find(cell) != retained.end()) continue;
                cell->invalidate_draw_commands(DrawReasonBits::layout);
                detach_tree_cell(cell);
                cell->set_parent(nullptr);
                cell->set_focus_parent(nullptr);
            }
        sync_cell_parents();
        update_depth(depth_range());
    }

    void Tree::clear_cells(bool invalidate_draw)
    {
        if (invalidate_draw) invalidate_visual_draw_records();
        if (invalidate_draw)
            for (auto &row : _cells)
                for (auto *cell : row)
                    if (cell) cell->invalidate_draw_commands(DrawReasonBits::layout);
        if (detail::g_context)
        {
            for (auto &row : _cells)
                for (auto *cell : row)
                {
                    if (!cell) continue;
                    detach_tree_cell(cell);
                    cell->set_parent(nullptr);
                    cell->set_focus_parent(nullptr);
                }
        }
        _cells.clear();
        _alt_row_visuals.clear();
        _resize_border_hit_visuals.clear();
        _resize_indicator_visual = {};
        _tree_line_data.clear();
        _tree_line_draws.clear();
        _arrow_visuals.clear();
    }

    void Tree::invalidate_visual_draw_records()
    {
        if (!detail::g_context || !detail::get_context().streams.default_streams) return;
        auto *quads_stream = get_primary_quads_stream();
        auto invalidate_quad = [quads_stream](DrawDataID &draw) {
            if (draw.render_id == AUIK_INVALID_DRAW_DATA_ID) return;
            if (quads_stream && quads_stream->invalidate_data_in_stream)
                quads_stream->invalidate_data_in_stream(quads_stream, draw);
            draw = {};
        };
        for (auto &visual : _alt_row_visuals) invalidate_quad(visual.draw);
        invalidate_quad(_resize_indicator_visual.draw);
        for (auto &visual : _resize_border_hit_visuals) visual.draw = {};
        clear_tree_line_draw_records();

        auto *textured_quads_stream = get_primary_textured_quads_stream();
        for (auto &visual : _arrow_visuals)
        {
            if (visual.icon_draw.render_id != AUIK_INVALID_DRAW_DATA_ID && textured_quads_stream &&
                textured_quads_stream->invalidate_data_in_stream)
                textured_quads_stream->invalidate_data_in_stream(textured_quads_stream, visual.icon_draw);
            visual.hit_draw = {};
            visual.icon_draw = {};
        }
    }

    void Tree::clear_tree_line_draw_records()
    {
        if (_tree_line_draws.empty()) return;
        auto *quads_stream = get_primary_quads_stream();
        if (quads_stream && quads_stream->invalidate_data_batch_in_stream)
            invalidate_data_batch_in_stream(quads_stream, _tree_line_draws.data(),
                                            static_cast<u32>(_tree_line_draws.size()));
        _tree_line_draws.clear();
    }

    void Tree::draw_tree_lines(DrawCtx &ctx, DrawStream *stream)
    {
        if (!stream) return;
        if ((ctx.reason & DrawReasonBits::invalidate))
        {
            clear_tree_line_draw_records();
            return;
        }

        auto &style = get_theme()->get_style(_line_style.id);
        const bool visible = (style.mask() & detail::g_style_visible_draw_mask) != 0u;
        if (!visible || _tree_line_data.empty())
        {
            clear_tree_line_draw_records();
            return;
        }

        for (auto &line : _tree_line_data)
        {
            line.mask = static_cast<u32>(clip_id());
            line.background_color = style.background_color();
            line.border_color = 0u;
            line.border_radius = 0.0f;
            line.border_thickness = 0.0f;
        }

        if ((ctx.reason & DrawReasonBits::record) || _tree_line_draws.empty())
        {
            _tree_line_draws.resize(_tree_line_data.size());
            push_quads_batch_to_stream(stream, _tree_line_data.data(), static_cast<u32>(_tree_line_data.size()),
                                       _tree_line_draws.data());
            return;
        }

        if (_tree_line_draws.size() != _tree_line_data.size())
        {
            clear_tree_line_draw_records();
            _tree_line_draws.resize(_tree_line_data.size());
            push_quads_batch_to_stream(stream, _tree_line_data.data(), static_cast<u32>(_tree_line_data.size()),
                                       _tree_line_draws.data());
            return;
        }

        update_quads_batch_in_stream(stream, _tree_line_draws.data(), _tree_line_data.data(),
                                     static_cast<u32>(_tree_line_data.size()));
    }

    void Tree::clear_nodes()
    {
        for (auto &node : _nodes)
        {
            if (node.label) acul::release(node.label);
        }
        for (auto &row : _node_cells)
            for (auto *cell : row)
                if (cell) acul::release(cell);
        _nodes.clear();
        _node_cells.clear();
        _widget_nodes.clear();
    }

    bool Tree::present_model_record(size_t record_index, DrawBlock *&label, Row &cells, ModelRecordID *parent_record_id)
    {
        label = nullptr;
        cells.clear();
        if (!_model_data) return false;
        auto *binding = _model_data->binding;
        if (record_index >= binding->records.size()) return false;
        auto *model = find_model(binding->db, binding->model_id);
        if (!model) return false;
        const ModelRecordID record_id = binding->records[record_index];
        auto *record = model->find_record(record_id);
        if (!record) return false;

        if (parent_record_id)
        {
            *parent_record_id = AUIK_MODEL_RECORD_ID_INVALID;
            read_model_binding_value(*binding, record_id, _model_data->parent_field_id, *parent_record_id);
        }

        const auto &field_ids = binding->presenter.field_ids;
        acul::vector<Widget *> widgets(field_ids.size(), nullptr);
        binding->presenter.present_record(binding, *record, static_cast<u32>(record_index), widgets.data(),
                                          static_cast<u32>(widgets.size()), binding->presenter.data);
        label = widgets.empty() ? nullptr : dynamic_cast<DrawBlock *>(widgets[0]);
        if (!label) label = wrap_tree_cell(widgets.empty() ? nullptr : widgets[0]);
        if (supports_columns() && widgets.size() > 1u)
        {
            cells.reserve(widgets.size() - 1u);
            for (size_t index = 1u; index < widgets.size(); ++index) cells.push_back(wrap_tree_cell(widgets[index]));
        }
        return true;
    }

    void Tree::apply_model_records(const ModelRecordsEvent &event)
    {
        if (!_model_data || !is_model_binding_valid(*_model_data->binding))
        {
            clear();
            return;
        }

        if (event.op == ModelRecordsOp::create &&
            _model_data->record_nodes.find(event.record_id) != _model_data->record_nodes.end())
            return;
        if ((event.op == ModelRecordsOp::destroy || event.op == ModelRecordsOp::move) &&
            _model_data->record_nodes.find(event.record_id) == _model_data->record_nodes.end())
            return;

        // ModelBinding applies the payload before notifying the widget. Keep matching nodes alive and reconcile their
        // order against the resulting binding; only genuinely created or destroyed records change widget ownership.
        auto *binding = _model_data->binding;
        release_arrow_animations();
        auto old_nodes = std::move(_nodes);
        auto old_cells = std::move(_node_cells);
        auto old_ids = std::move(_model_data->record_ids);

        acul::hashmap<ModelRecordID, size_t> old_indices;
        old_indices.reserve(old_ids.size());
        for (size_t index = 0u; index < old_ids.size(); ++index) old_indices[old_ids[index]] = index;
        acul::vector<bool> reused(old_nodes.size(), false);

        _nodes.clear();
        _node_cells.clear();
        _nodes.reserve(binding->records.size());
        _node_cells.reserve(binding->records.size());
        for (size_t index = 0u; index < binding->records.size(); ++index)
        {
            const ModelRecordID record_id = binding->records[index];
            const auto old = old_indices.find(record_id);
            if (old != old_indices.end() && old->second < old_nodes.size() && old->second < old_cells.size())
            {
                reused[old->second] = true;
                auto node = std::move(old_nodes[old->second]);
                node.parent = invalid_node;
                _nodes.push_back(std::move(node));
                _node_cells.push_back(std::move(old_cells[old->second]));
                continue;
            }

            DrawBlock *label = nullptr;
            Row cells;
            if (!present_model_record(index, label, cells)) label = wrap_tree_cell(nullptr);
            Node node{};
            node.label = label;
            _nodes.push_back(node);
            _node_cells.push_back(std::move(cells));
        }

        for (size_t index = 0u; index < old_nodes.size(); ++index)
        {
            if (reused[index]) continue;
            auto release_cell = [this](DrawBlock *cell) {
                if (!cell) return;
                cell->invalidate_draw_commands(DrawReasonBits::layout);
                detach_tree_cell(cell);
                // rebuild_cells() must not visit a deleted row widget.
                for (auto &row : _cells)
                    for (auto *&entry : row)
                        if (entry == cell) entry = nullptr;
                acul::release(cell);
            };
            release_cell(old_nodes[index].label);
            if (index < old_cells.size())
                for (auto *cell : old_cells[index])
                    release_cell(cell);
        }

        _model_data->record_ids = binding->records;
        _model_data->record_nodes.clear();
        _model_data->record_nodes.reserve(binding->records.size());
        _widget_nodes.clear();
        for (size_t index = 0u; index < _nodes.size(); ++index)
        {
            _model_data->record_nodes[binding->records[index]] = index;
            auto &node = _nodes[index];
            if (node.label)
            {
                _widget_nodes[node.label] = index;
            }
            for (auto *cell : _node_cells[index])
                if (cell) _widget_nodes[cell] = index;
        }

        apply_model_hierarchy();
    }

    void Tree::refresh_model_hierarchy()
    {
        if (!_model_data || !is_model_binding_valid(*_model_data->binding))
        {
            clear();
            return;
        }

        auto *binding = _model_data->binding;
        if (binding->records.size() != _nodes.size() || binding->records.size() != _model_data->record_ids.size() ||
            !std::equal(binding->records.begin(), binding->records.end(), _model_data->record_ids.begin()))
        {
            apply_model_records(ModelRecordsEvent{ModelRecordsOp::reset});
            return;
        }

        release_arrow_animations();
        apply_model_hierarchy();
    }

    void Tree::apply_model_hierarchy()
    {
        auto *binding = _model_data->binding;
        for (auto &node : _nodes) node.parent = invalid_node;
        for (size_t index = 0u; index < _nodes.size(); ++index)
        {
            ModelRecordID parent_id = AUIK_MODEL_RECORD_ID_INVALID;
            read_model_binding_value(*binding, binding->records[index], _model_data->parent_field_id, parent_id);
            const auto parent = _model_data->record_nodes.find(parent_id);
            if (parent != _model_data->record_nodes.end() && parent->second != index)
                _nodes[index].parent = parent->second;
        }

        // Ignore cyclic parent chains just as the initial model build does.
        for (size_t index = 0u; index < _nodes.size(); ++index)
        {
            size_t parent = _nodes[index].parent;
            for (size_t steps = 0u; parent != invalid_node && steps < _nodes.size(); ++steps)
            {
                if (parent == index)
                {
                    _nodes[index].parent = invalid_node;
                    break;
                }
                parent = parent < _nodes.size() ? _nodes[parent].parent : invalid_node;
            }
        }

        rebuild_visible_nodes();
        rebuild_cells();
        invalidate_layout();
    }

    void Tree::refresh_model_record(ModelRecordID record_id)
    {
        if (!_model_data) return;
        auto *binding = _model_data->binding;
        if (!is_model_binding_valid(*binding) || record_id == AUIK_MODEL_RECORD_ID_INVALID) return;

        acul::vector<size_t> visible_rows(_nodes.size(), invalid_node);
        for (size_t row = 0u; row < _visible_nodes.size(); ++row)
            if (_visible_nodes[row] < visible_rows.size()) visible_rows[_visible_nodes[row]] = row;

        const auto record_it = _model_data->record_nodes.find(record_id);
        if (record_it == _model_data->record_nodes.end()) return;
        const size_t node = record_it->second;
        if (node >= _nodes.size() || node >= _node_cells.size() || node >= binding->records.size() ||
            binding->records[node] != record_id)
        {
            if (std::find(binding->records.begin(), binding->records.end(), record_id) != binding->records.end())
                defer_model_record_refresh(record_id);
            return;
        }

        auto *old_label = _nodes[node].label;
        Row old_cells = std::move(_node_cells[node]);
        auto detach_old = [this](DrawBlock *cell) {
            if (!cell) return;
            cell->invalidate_draw_commands(DrawReasonBits::layout);
            detach_tree_cell(cell);
            cell->set_parent(nullptr);
            cell->set_focus_parent(nullptr);
            _widget_nodes.erase(cell);
        };
        detach_old(old_label);
        for (auto *cell : old_cells) detach_old(cell);

        if (old_label) acul::release(old_label);
        for (auto *cell : old_cells)
            if (cell) acul::release(cell);

        // Presenter-created widgets use stable IDs. The previous row must be fully detached and destroyed before the
        // replacement is constructed; otherwise both widget trees temporarily own the same IDs and destruction of
        // the old Textbox can clear state belonging to its replacement.
        DrawBlock *label = nullptr;
        Row cells;
        if (!present_model_record(node, label, cells) || !label) label = wrap_tree_cell(nullptr);

        _nodes[node].label = label;
        _node_cells[node] = std::move(cells);
        _widget_nodes[label] = node;
        for (auto *cell : _node_cells[node])
            if (cell) _widget_nodes[cell] = node;

        const size_t visible_row = node < visible_rows.size() ? visible_rows[node] : invalid_node;
        if (visible_row != invalid_node)
        {
            if (visible_row < _cells.size())
            {
                auto &visible_cells = _cells[visible_row];
                if (!visible_cells.empty()) visible_cells[0] = label;
                for (size_t column = 1u; column < visible_cells.size(); ++column)
                    visible_cells[column] =
                        column - 1u < _node_cells[node].size() ? _node_cells[node][column - 1u] : nullptr;
            }
        }

        sync_cell_parents();
        update_depth(depth_range());
        invalidate_layout();
    }

    void Tree::rebuild_from_model_binding()
    {
        if (!_model_data || !is_model_binding_valid(*_model_data->binding))
        {
            if (_model_data) _model_data->record_nodes.clear();
            clear();
            return;
        }

        auto *binding = _model_data->binding;
        auto *model = find_model(binding->db, binding->model_id);
        clear();
        acul::vector<ModelRecordID> parent_records;
        if (!model) return;
        parent_records.reserve(binding->records.size());
        _model_data->record_ids = binding->records;
        _model_data->record_nodes.reserve(binding->records.size());

        for (size_t record_index = 0u; record_index < binding->records.size(); ++record_index)
        {
            const ModelRecordID record_id = binding->records[record_index];
            auto *record = model->find_record(record_id);
            if (!record)
            {
                add_node(wrap_tree_cell(nullptr), {}, invalid_node);
                parent_records.push_back(AUIK_MODEL_RECORD_ID_INVALID);
                continue;
            }

            ModelRecordID parent_record_id = AUIK_MODEL_RECORD_ID_INVALID;
            DrawBlock *label = nullptr;
            Row cells;
            if (!present_model_record(record_index, label, cells, &parent_record_id)) label = wrap_tree_cell(nullptr);

            const size_t node = add_node(label, std::move(cells), invalid_node);
            _model_data->record_nodes[record_id] = node;
            _widget_nodes[label] = node;
            for (auto *cell : _node_cells[node])
                if (cell) _widget_nodes[cell] = node;
            parent_records.push_back(parent_record_id);
        }

        acul::vector<size_t> parent_nodes(_nodes.size(), invalid_node);
        for (size_t index = 0u; index < _nodes.size(); ++index)
        {
            if (parent_records[index] == AUIK_MODEL_RECORD_ID_INVALID) continue;
            const auto parent = _model_data->record_nodes.find(parent_records[index]);
            if (parent != _model_data->record_nodes.end()) parent_nodes[index] = parent->second;
        }
        for (size_t index = 0u; index < _nodes.size(); ++index)
        {
            size_t parent = parent_nodes[index];
            bool cyclic = false;
            for (size_t steps = 0u;
                 parent != invalid_node && parent < parent_nodes.size() && steps < parent_nodes.size(); ++steps)
            {
                if (parent == index)
                {
                    cyclic = true;
                    break;
                }
                parent = parent_nodes[parent];
            }
            if (!cyclic) _nodes[index].parent = parent_nodes[index];
        }
        rebuild_visible_nodes();
        rebuild_cells();
        invalidate_layout();
    }

    size_t Tree::node_depth(size_t node) const
    {
        size_t depth = 0u;
        while (node < _nodes.size() && _nodes[node].parent != invalid_node)
        {
            node = _nodes[node].parent;
            ++depth;
        }
        return depth;
    }

    bool Tree::node_is_last_sibling(size_t node) const
    {
        if (node >= _nodes.size()) return true;
        const size_t parent = _nodes[node].parent;
        for (size_t index = node + 1u; index < _nodes.size(); ++index)
            if (_nodes[index].parent == parent) return false;
        return true;
    }

    size_t Tree::node_ancestor_at_depth(size_t node, size_t depth) const
    {
        if (node >= _nodes.size()) return invalid_node;
        size_t current_depth = node_depth(node);
        while (node < _nodes.size() && current_depth > depth)
        {
            node = _nodes[node].parent;
            --current_depth;
        }
        return current_depth == depth ? node : invalid_node;
    }

    size_t Tree::resolve_column_count() const
    {
        size_t count = 1u;
        if (!supports_columns()) return count;
        for (const auto &cells : _node_cells) count = amal::max(count, cells.size() + 1u);
        return count;
    }

    u32 Tree::cell_element_id(size_t visible_row, size_t column) const
    {
        return static_cast<u32>(visible_row * _column_count + column);
    }

    DrawBlock *Tree::cell_widget(size_t visible_row, size_t column) const
    {
        if (visible_row >= _cells.size()) return nullptr;
        return column < _cells[visible_row].size() ? _cells[visible_row][column] : nullptr;
    }

    Widget *Tree::element_widget(ElementID element) const
    {
        if (!element || element.widget_id != id()) return nullptr;
        for (const auto &row : _cells)
            for (auto *cell : row)
            {
                if (!cell) continue;
                if (cell->get_rect().id == element) return cell;
                for (auto *child : cell->children)
                    if (child && child->get_rect().id == element) return child;
            }
        return nullptr;
    }

    const TableColumnSettings &Tree::settings_for_column(size_t column) const
    {
        return column < _column_settings.size() ? _column_settings[column] : _default_column_settings;
    }

    void Tree::update_column_widths(f32 inner_width)
    {
        detail::update_table_column_widths(
            _layout_metrics, _column_count, inner_width, _size_overrides,
            detail::has_table_flag(_tree_flags, AUIK_TABLE_TREE_FLAG_COLUMN_SIZE_OVERRIDES),
            [this](size_t column) { return to_layout_settings(settings_for_column(column)); });
    }

    void Tree::resize_visuals()
    {
        if (_visible_nodes.size() < _alt_row_visuals.size() ||
            _visible_nodes.size() < _arrow_visuals.size())
            invalidate_visual_draw_records();
        _alt_row_visuals.resize(_visible_nodes.size());
        _arrow_visuals.resize(_visible_nodes.size());
        _resize_border_hit_visuals.resize(supports_columns() && _column_count > 0u ? _column_count - 1u : 0u);
    }

    void Tree::update_cell_clip_rects()
    {
        for (auto &row : _cells)
        {
            for (auto *cell : row)
            {
                if (!cell) continue;
                cell->set_clip_id(content_clip_id());
                cell->rebuild_clip_rects();
            }
        }
        for (auto &visual : _alt_row_visuals) visual.rect.clip_id = clip_id();
        for (auto &visual : _resize_border_hit_visuals) visual.rect.clip_id = clip_id();
        _resize_indicator_visual.rect.clip_id = clip_id();
        for (auto &visual : _arrow_visuals) visual.rect.clip_id = clip_id();
    }

    void Tree::sync_cell_parents()
    {
        const bool attached =
            detail::g_context && detail::get_context().id_map.find(id()) != detail::get_context().id_map.end();
        for (size_t row_i = 0u; row_i < _cells.size(); ++row_i)
            for (size_t column = 0u; column < _cells[row_i].size(); ++column)
            {
                auto *cell = _cells[row_i][column];
                if (cell)
                {
                    cell->set_parent(this);
                    cell->set_focus_parent(this);
                    if (cell->id() == AUIK_TAG_TABLE_CELL) cell->set_style_tag(_cell_style.tag_id);
                    cell->update_style_invalidated();
                    if (attached) attach_tree_cell(cell, this);
                }
            }
    }

    void Tree::invalidate_layout()
    {
        invalidate_layout_measure();
        auto &ctx = detail::get_context();
        ctx.dirty_flags |= DirtyFlagBits::redraw | DirtyFlagBits::hit_rect_update;
        auto *layout_parent = parent();
        if (!layout_parent || clip_id() == 0xFFFFu || layout_parent->clip_id() == 0xFFFFu) return;
        layout_parent->update_layout(false);
        layout_parent->update_draw_commands(DrawReasonBits::layout);
        mark_host_refresh_request();
    }

    void Tree::update_own_layout()
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

    StyleUpdateFlags Tree::update_resize_indicator()
    {
        const detail::RectData prev_rect = _resize_indicator_visual.rect;
        const bool prev_active = detail::has_table_flag(_tree_flags, AUIK_TABLE_TREE_FLAG_RESIZE_INDICATOR_ACTIVE);
        const auto &ctx = detail::get_context();
        const auto transition = detail::get_widget_style_selector_transition(id());
        auto target = ctx.io.mouse_down ? ctx.io.drag_id : ElementID{};
        if (target.widget_id != id() || target.tag_id != AUIK_TAG_TABLE_RESIZE_BORDER_V) target = transition.current_id;
        const bool active = column_resizable() && target.widget_id == id() &&
                            target.tag_id == AUIK_TAG_TABLE_RESIZE_BORDER_V &&
                            target.element_id < _resize_border_hit_visuals.size();
        detail::set_table_flag(_tree_flags, AUIK_TABLE_TREE_FLAG_RESIZE_INDICATOR_ACTIVE, active);

        if (active)
        {
            _resize_indicator_visual.rect = _resize_border_hit_visuals[target.element_id].rect;
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
                detail::make_rect_data(id(), AUIK_TAG_TABLE_RESIZE_BORDER_V, {{0.0f, 0.0f}, {0.0f, 0.0f}}, clip_id());
        }

        const auto &next_rect = _resize_indicator_visual.rect;
        const bool rect_changed =
            prev_rect.id != next_rect.id || prev_rect.bounds.offset.x != next_rect.bounds.offset.x ||
            prev_rect.bounds.offset.y != next_rect.bounds.offset.y ||
            prev_rect.bounds.size.x != next_rect.bounds.size.x || prev_rect.bounds.size.y != next_rect.bounds.size.y ||
            prev_rect.clip_id != next_rect.clip_id;
        if (prev_active != active || rect_changed) return StyleUpdateFlagBits::redraw;
        return StyleUpdateFlagBits::none;
    }

    void Tree::ensure_arrow_resources()
    {
        if (auto *cached = get_cached_image(AUIK_ICON_CHEVRON_RIGHT))
        {
            _arrow_texture = cached->texture_id();
            _arrow_size = cached->size();
            _arrow_uv_rect = {cached->uv_offset(), cached->uv_size()};
            return;
        }
        _arrow_texture = {};
        _arrow_size = {0.0f, 0.0f};
        _arrow_uv_rect = {{0.0f, 0.0f}, {1.0f, 1.0f}};
    }

    Tree::ArrowAnimation *Tree::find_arrow_animation(size_t node)
    {
        for (auto &animation : _arrow_animations)
            if (animation.node == node) return &animation;
        return nullptr;
    }

    const Tree::ArrowAnimation *Tree::find_arrow_animation(size_t node) const
    {
        for (const auto &animation : _arrow_animations)
            if (animation.node == node) return &animation;
        return nullptr;
    }

    void Tree::start_arrow_animation(size_t node, bool opening)
    {
        if (!detail::g_context) return;
        auto *animation = find_arrow_animation(node);
        bool added_animation = false;
        if (!animation)
        {
            _arrow_animations.push_back({});
            animation = &_arrow_animations.back();
            animation->node = node;
            added_animation = true;
        }

        const u32 rotate_post_id = animation->state.post_data_id;
        auto *rotate_data = get_rotate_post_effect_data(get_rotate_post_effect(), rotate_post_id);
        const f32 current_angle = rotate_data && rotate_data->animating ? rotate_data->angle
                                  : opening                             ? 0.0f
                                                                        : amal::half_pi<f32>();
        const f32 target_angle = opening ? amal::half_pi<f32>() : 0.0f;
        configure_rotate_animation(animation->state, AUIK_TABLE_TREE_ARROW_ROTATE_DURATION, {0.0f, 0.0f}, current_angle,
                                   target_angle);
        if (!start_animation(animation->state, this))
        {
            if (added_animation) _arrow_animations.erase(_arrow_animations.end() - 1);
            return;
        }
        mark_host_refresh_request();
    }

    void Tree::clear_arrow_animation_draw(ArrowAnimation &animation)
    {
        if (animation.draw.render_id == AUIK_INVALID_DRAW_DATA_ID) return;
        auto *stream = get_primary_textured_vertex_stream();
        if (!stream || !stream->invalidate_data_in_stream) return;
        stream->invalidate_data_in_stream(stream, animation.draw);
        animation.draw = {};
    }

    void Tree::release_arrow_animations()
    {
        for (auto &animation : _arrow_animations)
        {
            animation.state.clear(this);
            clear_arrow_animation_draw(animation);
        }
        _arrow_animations.clear();
        erase_widget_from_transient_cache(this);
    }

    void Tree::draw_arrow(DrawCtx &ctx, ArrowVisual &visual)
    {
        if (visual.node >= _nodes.size()) return;
        if (!(ctx.reason & DrawReasonBits::invalidate) && visual.rect.clip_id != 0xFFFFu)
        {
            const auto clip = get_clip_rect(visual.rect.clip_id);
            const auto &rect = visual.rect.bounds;
            const bool culled = rect.offset.x + rect.size.x <= clip.x || rect.offset.y + rect.size.y <= clip.y ||
                                rect.offset.x >= clip.x + clip.z || rect.offset.y >= clip.y + clip.w;
            if (culled)
            {
                DrawCtx invalidate_ctx = ctx;
                invalidate_ctx.reason |= DrawReasonBits::invalidate;
                if (visual.hit_draw.hit_id != AUIK_INVALID_DRAW_DATA_ID)
                {
                    auto hidden_rect = visual.rect;
                    hidden_rect.bounds.size = {0.0f, 0.0f};
                    update_hit_rect(visual.hit_draw.hit_id, hidden_rect, true);
                }
                if (auto *stream = get_primary_textured_quads_stream())
                    emit_context_draw(invalidate_ctx, stream, visual.icon_draw, nullptr, visual.rect, false);
                return;
            }
        }
        if (!node_has_children(visual.node))
        {
            if (visual.hit_draw.hit_id != AUIK_INVALID_DRAW_DATA_ID)
            {
                auto hidden_rect = visual.rect;
                hidden_rect.bounds.size = {0.0f, 0.0f};
                update_hit_rect(visual.hit_draw.hit_id, hidden_rect, true);
            }
            if (visual.icon_draw.render_id != AUIK_INVALID_DRAW_DATA_ID)
                if (auto *stream = get_primary_textured_quads_stream())
                {
                    DrawCtx invalidate_ctx = ctx;
                    invalidate_ctx.reason |= DrawReasonBits::invalidate;
                    emit_context_draw(invalidate_ctx, stream, visual.icon_draw, nullptr, visual.rect, false);
                }
            return;
        }
        detail::emit_table_service_hit_rect(ctx, visual.hit_draw, visual.rect, can_emit_hit(ctx));
        ensure_arrow_resources();
        if (_arrow_texture.handle == 0) return;

        auto *animation = find_arrow_animation(visual.node);
        auto *rotate_effect = get_rotate_post_effect();
        const u32 rotate_post_id = animation ? animation->state.post_data_id : AUIK_INVALID_POST_EFFECT_DATA_ID;
        auto *rotate_data =
            animation && rotate_effect ? get_rotate_post_effect_data(rotate_effect, rotate_post_id) : nullptr;
        const bool animating = rotate_data && rotate_data->animating;
        if (animation && !animating) clear_arrow_animation_draw(*animation);

        TextureID closed_texture{};
        amal::rect closed_uv_rect{};
        amal::vec2 closed_size{};
        if (!resolve_tree_icon(AUIK_ICON_CHEVRON_RIGHT, closed_texture, closed_uv_rect, closed_size)) return;

        TextureID static_texture = closed_texture;
        amal::rect static_uv_rect = closed_uv_rect;
        amal::vec2 icon_size = closed_size;
        if (!animating && _nodes[visual.node].expanded)
            resolve_tree_icon(AUIK_ICON_CHEVRON_DOWN, static_texture, static_uv_rect, icon_size);

        if (icon_size.x <= 0.0f || icon_size.y <= 0.0f)
        {
            const auto &style = get_theme()->get_style(_cell_style.id);
            icon_size = {style.text_size(), style.text_size()};
        }
        const f32 icon_center_y = visual.icon_center_y > 0.0f
                                      ? visual.icon_center_y
                                      : visual.rect.bounds.offset.y + visual.rect.bounds.size.y * 0.5f;
        const auto &icon_style = get_theme()->get_style(_collapse_icon_style.id);
        const amal::vec4 icon_margin = icon_style.margin();
        const amal::vec4 icon_padding = icon_style.padding();
        const f32 icon_content_x = visual.rect.bounds.offset.x + icon_margin.x + icon_padding.x;
        const f32 icon_content_w = amal::max(
            visual.rect.bounds.size.x - icon_margin.x - icon_margin.z - icon_padding.x - icon_padding.z, 0.0f);
        const f32 icon_center_x =
            visual.icon_center_x > 0.0f ? visual.icon_center_x : icon_content_x + icon_content_w * 0.5f;
        amal::rect icon_rect{
            {amal::round(icon_center_x - icon_size.x * 0.5f), amal::round(icon_center_y - icon_size.y * 0.5f)},
            icon_size};

        auto *stream = get_primary_textured_quads_stream();
        if (!stream) return;
        if ((detail::get_context().dirty_flags & DirtyFlagBits::textures) ||
            static_texture.bind_slot == AUIK_INVALID_DRAW_DATA_ID)
            static_texture.bind_slot = get_texture_bind_slot(static_texture.handle);
        if (static_texture.bind_slot == AUIK_INVALID_DRAW_DATA_ID) return;

        TexturesInstanceData icon{};
        icon.rect = icon_rect;
        icon.uv_rect = static_uv_rect;
        icon.tint_color = animating ? 0u : icon_style.text_color();
        icon.z_order = visual.rect.depth;
        icon.texture_id = static_cast<u16>(static_texture.bind_slot);
        icon.clip_id = visual.rect.clip_id;
        icon.flags = AUIK_TEXTURE_INSTANCE_TINT_BIT;
        emit_context_draw(ctx, stream, visual.icon_draw, &icon, visual.rect, false);

        if (animating)
        {
            rotate_data->center = icon_rect.offset + icon_rect.size * 0.5f;
            auto *vertex_stream = get_primary_textured_vertex_stream();
            if (!vertex_stream || !rotate_effect) return;
            TextureID animated_texture = closed_texture;
            if ((detail::get_context().dirty_flags & DirtyFlagBits::textures) ||
                animated_texture.bind_slot == AUIK_INVALID_DRAW_DATA_ID)
                animated_texture.bind_slot = get_texture_bind_slot(animated_texture.handle);
            if (animated_texture.bind_slot == AUIK_INVALID_DRAW_DATA_ID) return;
            TexturedVertexStreamVertex vertices[4]{};
            const TexturedVertexStreamIndex indices[6]{0u, 1u, 2u, 0u, 2u, 3u};
            build_tree_arrow_vertices(vertices, icon_rect, closed_uv_rect, visual.rect.depth,
                                      static_cast<u32>(visual.rect.clip_id));
            TexturedVertexStreamBatchData batch{};
            batch.vertices = vertices;
            batch.indices = indices;
            batch.vertex_count = 4u;
            batch.index_count = 6u;
            batch.texture_id = animated_texture;
            batch.flags = AUIK_TEXTURE_INSTANCE_TINT_BIT;
            RotatePostData rotate_post{rotate_post_id};
            PostFxChain rotate_chain{rotate_effect, &rotate_post, rotate_post_id, ctx.post_fx_chain};
            DrawCtx rotated_ctx = ctx;
            rotated_ctx.post_fx_chain = &rotate_chain;
            emit_context_draw(rotated_ctx, vertex_stream, animation->draw, &batch, visual.rect, false);
        }
    }

    namespace
    {
        constexpr u32 g_persistent_tree_flags = AUIK_TABLE_TREE_FLAG_ALTERNATING_ROWS |
                                                AUIK_TABLE_TREE_FLAG_COLUMN_RESIZABLE |
                                                AUIK_TABLE_TREE_FLAG_COLUMN_SIZE_OVERRIDES;

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

        bool is_snapshot_cell(DrawBlock *cell) { return cell && (cell->widget_flags & WidgetFlagBits::cache_snapshot); }

        bool node_has_snapshot_widgets(const TableTree &tree, size_t node_i)
        {
            const auto &node = tree.nodes()[node_i];
            if (is_snapshot_cell(node.label)) return true;
            for (auto *cell : tree.node_cells(node_i))
                if (is_snapshot_cell(cell)) return true;
            return false;
        }

        bool node_has_kept_descendant(const acul::vector<Tree::Node> &nodes, const acul::vector<bool> &keep,
                                      size_t node)
        {
            for (size_t child_i = 0u; child_i < nodes.size(); ++child_i)
            {
                if (nodes[child_i].parent != node) continue;
                if (keep[child_i] || node_has_kept_descendant(nodes, keep, child_i)) return true;
            }
            return false;
        }

        void write_node_row(acul::bin_stream &stream, DrawBlock *label, const acul::vector<DrawBlock *> &cells)
        {
            acul::vector<u32> columns;
            acul::vector<Widget *> blocks;
            if (is_snapshot_cell(label))
            {
                columns.push_back(0u);
                blocks.push_back(label);
            }
            for (size_t column = 0u; column < cells.size(); ++column)
            {
                auto *cell = cells[column];
                if (!is_snapshot_cell(cell)) continue;
                columns.push_back(static_cast<u32>(column + 1u));
                blocks.push_back(cell);
            }

            stream.write(static_cast<u32>(columns.size()));
            if (!columns.empty()) stream.write(columns.data(), columns.size());
            stream.write(blocks);
        }

        void read_node_row(acul::bin_stream &stream, DrawBlock *&label, acul::vector<DrawBlock *> &cells)
        {
            u32 cell_count = 0u;
            stream.read(cell_count);

            acul::vector<u32> columns;
            columns.resize(cell_count);
            if (!columns.empty()) stream.read(columns.data(), columns.size());

            acul::vector<Widget *> blocks;
            stream.write(blocks);
            if (blocks.size() != columns.size())
            {
                for (auto *block : blocks)
                    if (block) acul::release(block);
                throw acul::runtime_error("invalid tree cell block count");
            }

            size_t column_count = 0u;
            for (u32 column : columns) column_count = amal::max(column_count, static_cast<size_t>(column));
            cells.resize(column_count);

            for (u32 cell_i = 0u; cell_i < cell_count; ++cell_i)
            {
                auto *cell = dynamic_cast<DrawBlock *>(blocks[cell_i]);
                if (!cell)
                {
                    if (blocks[cell_i]) acul::release(blocks[cell_i]);
                    continue;
                }
                if (columns[cell_i] == 0u) label = cell;
                else cells[columns[cell_i] - 1u] = cell;
            }
        }

        void write_table_tree(acul::bin_stream &stream, umbf::Block *block)
        {
            auto *tree = static_cast<TableTree *>(block);
            detail::write_widget_common_data(stream, *tree);
            stream.write(tree->style_tag())
                .write(tree->cell_style_tag())
                .write(tree->alternating_row_style_tag())
                .write(tree->line_style_tag())
                .write(tree->collapse_icon_style_tag())
                .write(tree->indent_width())
                .write(tree->tree_flags() & g_persistent_tree_flags);

            write_column_settings(stream, tree->default_column_settings());

            const auto &column_settings = tree->column_settings();
            stream.write(static_cast<u32>(column_settings.size()));
            for (const auto &settings : column_settings) write_column_settings(stream, settings);

            const auto &size_overrides = tree->size_overrides();
            stream.write(static_cast<u32>(size_overrides.size()));
            for (const auto &value : size_overrides) stream.write(value);

            const auto &nodes = tree->nodes();
            acul::vector<bool> keep;
            keep.resize(nodes.size());
            for (size_t node_i = 0u; node_i < nodes.size(); ++node_i)
                keep[node_i] = node_has_snapshot_widgets(*tree, node_i);
            for (size_t node_i = 0u; node_i < nodes.size(); ++node_i)
                if (!keep[node_i] && node_has_kept_descendant(nodes, keep, node_i)) keep[node_i] = true;

            u32 kept_count = 0u;
            for (bool value : keep)
                if (value) ++kept_count;
            stream.write(kept_count);

            for (size_t node_i = 0u; node_i < nodes.size(); ++node_i)
            {
                if (!keep[node_i]) continue;
                const auto &node = nodes[node_i];
                stream.write(static_cast<u32>(node_i))
                    .write(node.parent == Tree::invalid_node ? static_cast<u32>(Tree::invalid_node)
                                                             : static_cast<u32>(node.parent))
                    .write(node.expanded);
                write_node_row(stream, node.label, tree->node_cells(node_i));
            }
        }

        umbf::Block *read_table_tree(acul::bin_stream &stream)
        {
            const auto common = detail::read_widget_common_data(stream);
            u32 style_tag = AUIK_STYLE_TAG_TREE;
            u32 cell_style_tag = AUIK_STYLE_TAG_TREE_CELL;
            u32 alternating_row_style_tag = AUIK_STYLE_TAG_TREE_ROW_ALT;
            u32 line_style_tag = AUIK_STYLE_TAG_TREE_LINE;
            u32 collapse_icon_style_tag = AUIK_STYLE_TAG_TREE_COLLAPSE_ICON;
            f32 indent_width = 16.0f;
            u32 tree_flags = 0u;
            stream.read(style_tag)
                .read(cell_style_tag)
                .read(alternating_row_style_tag)
                .read(line_style_tag)
                .read(collapse_icon_style_tag)
                .read(indent_width)
                .read(tree_flags);

            auto default_settings = read_column_settings(stream);

            u32 column_settings_count = 0u;
            stream.read(column_settings_count);
            acul::vector<TableColumnSettings> column_settings;
            column_settings.reserve(column_settings_count);
            for (u32 column_i = 0u; column_i < column_settings_count; ++column_i)
                column_settings.push_back(read_column_settings(stream));

            u32 override_count = 0u;
            stream.read(override_count);
            acul::vector<amal::vec2> size_overrides;
            size_overrides.resize(override_count);
            if (!size_overrides.empty()) stream.read(size_overrides.data(), size_overrides.size());

            auto *tree =
                acul::alloc<TableTree>(common.id, common.inline_size, WidgetFlags(common.widget_flags), style_tag);
            tree->set_cell_style_tag(cell_style_tag);
            tree->set_alternating_row_style_tag(alternating_row_style_tag);
            tree->set_line_style_tag(line_style_tag);
            tree->set_collapse_icon_style_tag(collapse_icon_style_tag);
            tree->set_indent_width(indent_width);
            tree->set_alternating_rows((tree_flags & AUIK_TABLE_TREE_FLAG_ALTERNATING_ROWS) != 0u);
            tree->set_column_resizable((tree_flags & AUIK_TABLE_TREE_FLAG_COLUMN_RESIZABLE) != 0u);
            tree->set_default_column_settings(default_settings);
            tree->set_column_settings(std::move(column_settings));
            tree->set_size_overrides(std::move(size_overrides),
                                     (tree_flags & AUIK_TABLE_TREE_FLAG_COLUMN_SIZE_OVERRIDES) != 0u);

            u32 node_count = 0u;
            stream.read(node_count);
            acul::vector<acul::pair<u32, size_t>> remap;
            remap.reserve(node_count);
            acul::vector<acul::pair<size_t, bool>> expanded_states;
            expanded_states.reserve(node_count);

            auto find_new_parent = [&remap](u32 original_parent) {
                if (original_parent == static_cast<u32>(Tree::invalid_node)) return Tree::invalid_node;
                for (const auto &item : remap)
                    if (item.first == original_parent) return item.second;
                return Tree::invalid_node;
            };

            for (u32 node_i = 0u; node_i < node_count; ++node_i)
            {
                u32 original_index = 0u;
                u32 original_parent = static_cast<u32>(Tree::invalid_node);
                bool expanded = true;
                stream.read(original_index).read(original_parent).read(expanded);

                DrawBlock *label = nullptr;
                acul::vector<DrawBlock *> cells;
                read_node_row(stream, label, cells);

                const size_t new_parent = find_new_parent(original_parent);
                const size_t new_index = tree->add_node(label, std::move(cells), new_parent);
                remap.push_back({original_index, new_index});
                expanded_states.push_back({new_index, expanded});
            }

            for (const auto &state : expanded_states) tree->set_node_expanded(state.first, state.second, false);

            detail::apply_widget_common_data(tree, common);
            return tree;
        }

        bool tree_node_has_snapshot_widget(const Tree::Node &node) { return is_snapshot_cell(node.label); }

        void write_tree(acul::bin_stream &stream, umbf::Block *block)
        {
            auto *tree = static_cast<Tree *>(block);
            detail::write_widget_common_data(stream, *tree);
            stream.write(tree->style_tag())
                .write(tree->cell_style_tag())
                .write(tree->alternating_row_style_tag())
                .write(tree->line_style_tag())
                .write(tree->collapse_icon_style_tag())
                .write(tree->indent_width())
                .write(tree->tree_flags() & AUIK_TABLE_TREE_FLAG_ALTERNATING_ROWS);

            const auto &nodes = tree->nodes();
            acul::vector<bool> keep;
            keep.resize(nodes.size());
            for (size_t node_i = 0u; node_i < nodes.size(); ++node_i)
                keep[node_i] = tree_node_has_snapshot_widget(nodes[node_i]);
            for (size_t node_i = 0u; node_i < nodes.size(); ++node_i)
                if (!keep[node_i] && node_has_kept_descendant(nodes, keep, node_i)) keep[node_i] = true;

            u32 kept_count = 0u;
            for (bool value : keep)
                if (value) ++kept_count;
            stream.write(kept_count);

            for (size_t node_i = 0u; node_i < nodes.size(); ++node_i)
            {
                if (!keep[node_i]) continue;
                const auto &node = nodes[node_i];
                stream.write(static_cast<u32>(node_i))
                    .write(node.parent == Tree::invalid_node ? static_cast<u32>(Tree::invalid_node)
                                                             : static_cast<u32>(node.parent))
                    .write(node.expanded);
                write_node_row(stream, node.label, {});
            }
        }

        umbf::Block *read_tree(acul::bin_stream &stream)
        {
            const auto common = detail::read_widget_common_data(stream);
            u32 style_tag = AUIK_STYLE_TAG_TREE;
            u32 cell_style_tag = AUIK_STYLE_TAG_TREE_CELL;
            u32 alternating_row_style_tag = AUIK_STYLE_TAG_TREE_ROW_ALT;
            u32 line_style_tag = AUIK_STYLE_TAG_TREE_LINE;
            u32 collapse_icon_style_tag = AUIK_STYLE_TAG_TREE_COLLAPSE_ICON;
            f32 indent_width = 16.0f;
            u32 tree_flags = 0u;
            stream.read(style_tag)
                .read(cell_style_tag)
                .read(alternating_row_style_tag)
                .read(line_style_tag)
                .read(collapse_icon_style_tag)
                .read(indent_width)
                .read(tree_flags);

            auto *tree = acul::alloc<Tree>(common.id, common.inline_size, WidgetFlags(common.widget_flags), style_tag);
            tree->set_cell_style_tag(cell_style_tag);
            tree->set_alternating_row_style_tag(alternating_row_style_tag);
            tree->set_line_style_tag(line_style_tag);
            tree->set_collapse_icon_style_tag(collapse_icon_style_tag);
            tree->set_indent_width(indent_width);
            tree->set_alternating_rows((tree_flags & AUIK_TABLE_TREE_FLAG_ALTERNATING_ROWS) != 0u);

            u32 node_count = 0u;
            stream.read(node_count);
            acul::vector<acul::pair<u32, size_t>> remap;
            remap.reserve(node_count);
            acul::vector<acul::pair<size_t, bool>> expanded_states;
            expanded_states.reserve(node_count);

            auto find_new_parent = [&remap](u32 original_parent) {
                if (original_parent == static_cast<u32>(Tree::invalid_node)) return Tree::invalid_node;
                for (const auto &item : remap)
                    if (item.first == original_parent) return item.second;
                return Tree::invalid_node;
            };

            for (u32 node_i = 0u; node_i < node_count; ++node_i)
            {
                u32 original_index = 0u;
                u32 original_parent = static_cast<u32>(Tree::invalid_node);
                bool expanded = true;
                stream.read(original_index).read(original_parent).read(expanded);

                DrawBlock *label = nullptr;
                acul::vector<DrawBlock *> cells;
                read_node_row(stream, label, cells);

                const size_t new_parent = find_new_parent(original_parent);
                const size_t new_index = tree->add_node(label, new_parent);
                remap.push_back({original_index, new_index});
                expanded_states.push_back({new_index, expanded});
            }

            for (const auto &state : expanded_states) tree->set_node_expanded(state.first, state.second, false);

            detail::apply_widget_common_data(tree, common);
            tree->set_size(AUIK_SIZE_FIT);
            return tree;
        }
    } // namespace

    namespace streams
    {
        extern AUIK_EXPORT const umbf::registry::BlockStream tree{read_tree, write_tree};
        extern AUIK_EXPORT const umbf::registry::BlockStream table_tree{read_table_tree, write_table_tree};
    } // namespace streams
} // namespace auik
