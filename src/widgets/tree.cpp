#include <amal/trigonometric.hpp>
#include <auik/auik.hpp>
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
    static inline bool valid_tree_rect_size(const amal::vec2 &size) { return size.x > 0.0f && size.y > 0.0f; }

    static inline detail::TableColumnLayoutSettings to_layout_settings(const TableColumnSettings &settings)
    { return {static_cast<u8>(settings.sizing), settings.value, settings.min_width}; }

    static inline const Style &tree_resolved_style(const StyleSelector &selector)
    {
        auto *theme = get_theme();
        const StyleID style_id =
            selector.id != Theme::STYLE_ID_INVALID
                ? selector.id
                : theme->get_resolved_style(selector.tag_id, selector.tag_id, 0u, StyleState::normal);
        return theme->get_style(style_id);
    }

    static inline amal::vec2 tree_icon_outer_size(const Style &style, amal::vec2 icon_size)
    {
        const amal::vec4 margin = style.margin();
        const amal::vec4 padding = style.padding();
        return {icon_size.x + margin.x + margin.z + padding.x + padding.z,
                icon_size.y + margin.y + margin.w + padding.y + padding.w};
    }

    static inline f32 tree_arrow_slot_width(const Style &style, amal::vec2 icon_size, f32 fallback)
    { return amal::max(fallback, tree_icon_outer_size(style, icon_size).x); }

    static inline f32 tree_icon_center_x(f32 slot_x, f32 slot_width, const Style &style)
    {
        const amal::vec4 margin = style.margin();
        const amal::vec4 padding = style.padding();
        const f32 content_x = slot_x + margin.x + padding.x;
        const f32 content_w = amal::max(slot_width - margin.x - margin.z - padding.x - padding.z, 0.0f);
        return content_x + content_w * 0.5f;
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

    Tree::Tree(u32 id, WidgetFlags flags, Widget *parent, u32 style_tag_id)
        : Tree(id, AUIK_SIZE_FIT, flags, parent, style_tag_id)
    {
    }

    Tree::Tree(u32 id, amal::vec2 size, WidgetFlags flags, Widget *parent, u32 style_tag_id)
        : Widget(id, flags, EventFlagBits::click | EventFlagBits::hover | EventFlagBits::drag, parent,
                 {{0.0f, 0.0f}, size}, style_tag_id),
          _style({Theme::STYLE_ID_INVALID, style_tag_id})
    { _default_column_settings.valign = VAlign::center; }

    Tree::~Tree()
    {
        if (_model_binding)
        {
            _model_binding->on_records = nullptr;
            detach_model_binding(*_model_binding);
        }
        release_arrow_animations();
        clear_cells(false);
        clear_nodes();
    }

    void Tree::clear()
    {
        clear_nodes();
        release_arrow_animations();
        rebuild_visible_nodes();
        rebuild_cells();
        invalidate_layout();
    }

    void Tree::set_model_binding(ModelBinding *binding)
    {
        acul::vector<ModelFieldID> fields;
        fields.push_back(1u);
        set_model_binding(binding, std::move(fields));
    }

    void Tree::set_model_binding(ModelBinding *binding, acul::vector<ModelFieldID> field_ids)
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

    size_t Tree::add_node(Widget *label, Row cells, size_t parent)
    {
        if (parent >= _nodes.size()) parent = invalid_node;
        const size_t out = _nodes.size();
        _nodes.push_back({std::move(label), parent, true});
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

    void Tree::set_resize_border_style_tag(u32 tag_id)
    {
        if (_resize_border_style.tag_id == tag_id) return;
        _resize_border_style = {Theme::STYLE_ID_INVALID, tag_id};
        invalidate_layout();
    }

    void Tree::set_size_overrides(acul::vector<acul::point2D<f32>> values, bool column_overrides)
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
        return hover.widget_id == id() && hover.tag_id == signature() && hover.element_id == visible_row;
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
        out |= resolve_style_selector(_cell_style, AUIK_TAG_TABLE_CELL, id(), StyleState::normal);
        out |= resolve_style_selector(_alternating_row_style, _alternating_row_style.tag_id, id(), StyleState::normal);
        out |= resolve_style_selector(_line_style, _line_style.tag_id, id(), StyleState::normal);
        out |= resolve_style_selector(_collapse_icon_style, _collapse_icon_style.tag_id, id(), StyleState::normal);
        if (supports_columns())
        {
            out |= resolve_style_selector(_resize_border_style, _resize_border_style.tag_id, id(), StyleState::normal);
            if (transition.prev_id.tag_id == AUIK_TAG_TABLE_RESIZE_BORDER_V ||
                transition.current_id.tag_id == AUIK_TAG_TABLE_RESIZE_BORDER_V)
                out |= StyleUpdateFlagBits::redraw;
            out |= update_resize_indicator();
        }
        for (auto &row : _cells)
            for (auto *cell : row)
                if (cell) out |= cell->update_style();
        ensure_arrow_resources();
        return out;
    }

    void Tree::update_layout_min_size()
    {
        rebuild_visible_nodes();
        ensure_arrow_resources();
        _column_count = resolve_column_count();
        _layout_metrics.assign(amal::max(_column_count, _visible_nodes.size()), {});

        const amal::vec4 cell_padding = tree_resolved_style(_cell_style).padding();
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
                cell->update_layout(false);
                f32 min_width = cell->required_size().x + cell_padding.x + cell_padding.z;
                if (column == 0u)
                {
                    const auto &icon_style = get_theme()->get_style(_collapse_icon_style.id);
                    min_width += static_cast<f32>(node_depth(node) + 1u) *
                                 tree_arrow_slot_width(icon_style, _arrow_size, _indent_width);
                }
                _layout_metrics[column].x.min_value = amal::max(_layout_metrics[column].x.min_value, min_width);
                _layout_metrics[row].y.min_value = amal::max(_layout_metrics[row].y.min_value,
                                                             cell->required_size().y + cell_padding.y + cell_padding.w);
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
            supports_columns() ? detail::resolve_table_required_axis(requested_size().x, fill_width(), natural_width)
                               : natural_width;
        const f32 required_height =
            supports_columns() ? detail::resolve_table_required_axis(requested_size().y, fill_height(), natural_height)
                               : natural_height;

        set_required_size({required_width + margin.x + margin.z, required_height + margin.y + margin.w});
    }

    void Tree::update_layout(bool min_size_known)
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
        if (!supports_columns()) { outer_size = required_inner; }
        else
        {
            if (!is_width_fixed()) outer_size.x = amal::max(outer_size.x - margin.x - margin.z, required_inner.x);
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
            _resize_border_hit_visuals[index].rect =
                detail::make_rect_data(id(), AUIK_TAG_TABLE_RESIZE_BORDER_V, {{0.0f, 0.0f}, {0.0f, 0.0f}}, clip_id(),
                                       next_depth(depth_range()), 0u, static_cast<u32>(index));
        }
        _tree_line_data.clear();

        f32 cursor_y = inner_pos.y;
        size_t visual_index = 0u;
        for (size_t row = 0; row < _visible_nodes.size(); ++row)
        {
            const size_t node = _visible_nodes[row];
            const f32 row_h = _layout_metrics[row].y.value;
            _row_visuals[row].rect =
                detail::make_rect_data(id(), signature(), {{inner_pos.x, cursor_y}, {inner_size.x, row_h}}, clip_id(),
                                       next_depth(depth_range()), 0u, static_cast<u32>(row));
            _alt_row_visuals[row].rect = detail::make_rect_data(id(), AUIK_STYLE_TAG_TREE_ROW_ALT,
                                                                {{inner_pos.x, cursor_y}, {inner_size.x, row_h}},
                                                                clip_id(), next_depth(depth_range()));

            amal::vec2 tree_icon_size = _arrow_size;
            if (tree_icon_size.y <= 0.0f)
            {
                const auto &cell_style = get_theme()->get_style(_cell_style.id);
                tree_icon_size = {cell_style.text_size(), cell_style.text_size()};
            }
            const auto &icon_style = get_theme()->get_style(_collapse_icon_style.id);
            const f32 arrow_slot_w = tree_arrow_slot_width(icon_style, tree_icon_size, _indent_width);
            const f32 indent = static_cast<f32>(node_depth(node)) * arrow_slot_w;
            const f32 arrow_slot_x = inner_pos.x + indent;
            _arrow_visuals[row].node = node;
            _arrow_visuals[row].rect = detail::make_rect_data(
                id(), AUIK_TAG_TABLE_TREE_ARROW, {{arrow_slot_x, cursor_y}, {arrow_slot_w, row_h}}, clip_id(),
                next_depth(depth_range()), 0u, static_cast<u32>(node));
            _arrow_visuals[row].icon_center_x = tree_icon_center_x(arrow_slot_x, arrow_slot_w, icon_style);
            _arrow_visuals[row].icon_center_y = cursor_y + row_h * 0.5f;

            f32 cursor_x = inner_pos.x;
            for (size_t column = 0; column < _column_count; ++column)
            {
                const f32 column_w = _layout_metrics[column].x.value;
                auto *cell = cell_widget(row, column);
                if (cell)
                {
                    const auto &settings = settings_for_column(column);
                    const amal::vec4 cell_padding = tree_resolved_style(_cell_style).padding();
                    amal::vec2 cell_pos{cursor_x, cursor_y};
                    amal::vec2 cell_size{column_w, row_h};
                    if (column == 0u)
                    {
                        const f32 left_offset = indent + arrow_slot_w;
                        cell_pos.x += left_offset;
                        cell_size.x = amal::max(cell_size.x - left_offset, 0.0f);
                    }
                    detail::apply_table_cell_alignment(cell, settings.halign, settings.valign);
                    cell->set_position(cell_pos + amal::vec2{cell_padding.x, cell_padding.y});
                    cell->set_layout_size({amal::max(cell_size.x - cell_padding.x - cell_padding.z, 0.0f),
                                           amal::max(cell_size.y - cell_padding.y - cell_padding.w, 0.0f)});
                    cell->update_layout(true);
                    if (column == 0u)
                        _arrow_visuals[row].icon_center_y = cell->bounds().offset.y + cell->bounds().size.y * 0.5f;
                }
                if (visual_index < _cell_visuals.size())
                {
                    _cell_visuals[visual_index].rect =
                        detail::make_rect_data(id(), AUIK_TAG_TABLE_CELL, {{cursor_x, cursor_y}, {column_w, row_h}},
                                               clip_id(), next_depth(depth_range()), 0u, cell_element_id(row, column));
                }
                cursor_x += column_w;
                ++visual_index;
            }

            const auto &line_style = get_theme()->get_style(_line_style.id);
            const f32 line_thickness = amal::max(amal::round(line_style.border_thickness()), 1.0f);
            const f32 half_line = line_thickness * 0.5f;
            const f32 row_mid_y = amal::round(_arrow_visuals[row].icon_center_y);
            const f32 icon_gap_h = amal::min(tree_icon_outer_size(icon_style, tree_icon_size).y, row_h);
            const f32 icon_top = amal::round(_arrow_visuals[row].icon_center_y - icon_gap_h * 0.5f);
            const f32 icon_bottom = amal::round(icon_top + icon_gap_h);
            auto add_line = [&](amal::rect rect) {
                rect.offset.x = amal::round(rect.offset.x);
                rect.offset.y = amal::round(rect.offset.y);
                rect.size.x = amal::round(rect.size.x);
                rect.size.y = amal::round(rect.size.y);
                if (!valid_tree_rect_size(rect.size)) return;
                QuadsInstanceData line{};
                line.rect = rect;
                line.z_order = next_depth(depth_range());
                _tree_line_data.push_back(line);
            };
            const size_t depth = node_depth(node);
            for (size_t level = 0; level <= depth; ++level)
            {
                const size_t level_node = node_ancestor_at_depth(node, level);
                if (level_node >= _nodes.size()) continue;
                const bool own_level = level == depth;
                const bool has_children = own_level && node_has_children(node);
                if (!own_level && node_is_last_sibling(level_node)) continue;

                const f32 level_indent = static_cast<f32>(level) * arrow_slot_w;
                const f32 level_slot_x = inner_pos.x + level_indent;
                const f32 line_center_x = tree_icon_center_x(level_slot_x, arrow_slot_w, icon_style);
                const f32 line_x = amal::round(line_center_x - half_line);
                const f32 top_y = amal::round(cursor_y);
                const f32 bottom_y =
                    own_level && node_is_last_sibling(node) ? row_mid_y : amal::round(cursor_y + row_h);

                if (has_children)
                {
                    add_line({{line_x, top_y}, {line_thickness, amal::max(icon_top - top_y, 0.0f)}});
                    add_line({{line_x, icon_bottom}, {line_thickness, amal::max(bottom_y - icon_bottom, 0.0f)}});
                }
                else
                {
                    add_line({{line_x, top_y}, {line_thickness, amal::max(bottom_y - top_y, 0.0f)}});
                    if (own_level)
                    {
                        const f32 branch_x = amal::round(line_center_x);
                        add_line(
                            {{branch_x, row_mid_y - half_line},
                             {amal::max(inner_pos.x + level_indent + arrow_slot_w - branch_x, 0.0f), line_thickness}});
                    }
                }
            }
            cursor_y += row_h;
        }

        auto *theme = get_theme();
        f32 resize_hit_w = 0.0f;
        if (supports_columns())
        {
            const StyleID resize_style_id =
                _resize_border_style.id != Theme::STYLE_ID_INVALID
                    ? _resize_border_style.id
                    : theme->get_resolved_style(_resize_border_style.tag_id, _resize_border_style.tag_id, id(),
                                                StyleState::normal);
            const auto &resize_style = theme->get_style(resize_style_id);
            const amal::vec4 resize_margin = resize_style.margin();
            const amal::vec4 resize_padding = resize_style.padding();
            const f32 resize_w = amal::max(resize_margin.x + resize_margin.z + resize_padding.x + resize_padding.z,
                                           resize_style.border_thickness());
            resize_hit_w = amal::max(resize_w, 1.0f);
        }
        if (supports_columns() && column_resizable() && resize_hit_w > 0.0f)
        {
            f32 cursor_x = inner_pos.x;
            for (size_t column = 0; column + 1 < _column_count; ++column)
            {
                cursor_x += _layout_metrics[column].x.value;
                if (column >= _resize_border_hit_visuals.size()) continue;
                auto &visual = _resize_border_hit_visuals[column];
                visual.rect = detail::make_rect_data(
                    id(), AUIK_TAG_TABLE_RESIZE_BORDER_V,
                    {{cursor_x - resize_hit_w * 0.5f, inner_pos.y}, {resize_hit_w, cursor_y - inner_pos.y}}, clip_id(),
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
        for (auto &visual : _row_visuals) visual.rect.bounds.offset += delta;
        for (auto &visual : _cell_visuals) visual.rect.bounds.offset += delta;
        for (auto &visual : _alt_row_visuals) visual.rect.bounds.offset += delta;
        for (auto &visual : _resize_border_hit_visuals) visual.rect.bounds.offset += delta;
        _resize_indicator_visual.rect.bounds.offset += delta;
        for (auto &line : _tree_line_data) line.rect.offset += delta;
        for (auto &visual : _arrow_visuals) visual.rect.bounds.offset += delta;
        for (auto &row : _cells)
            for (auto *cell : row)
                if (cell) cell->translate(delta);
        const amal::vec4 parent_clip = parent() ? parent()->get_content_clip_rect() : get_main_viewport_rect();
        if (parent() && clip_id() == parent()->content_clip_id()) set_clip_id(0xFFFFu);
        ensure_own_clip_rect(detail::intersect_rects(parent_clip, {position().x, position().y, size().x, size().y}));
        update_cell_clip_rects();
        update_draw_commands(DrawReasonBits::external);
        detail::get_context().dirty_flags |= DirtyFlagBits::redraw;
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
        for (auto &visual : _row_visuals) visual.draw = {};
        for (auto &visual : _cell_visuals) visual.draw = {};
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

    void Tree::update_depth(const amal::vec2 &depth_range)
    {
        Widget::update_depth(depth_range);
        for (auto &row : _cells)
        {
            for (auto *cell : row)
            {
                if (!cell) continue;
                cell->update_depth(this->depth_range());
            }
        }
    }

    void Tree::back_hit_depth()
    {
        Widget::back_hit_depth();
        for (auto &row : _cells)
            for (auto *cell : row)
                if (cell) cell->back_hit_depth();
        auto lower = [&](detail::TableCellVisual &visual) { visual.rect.hit_depth = get_rect().hit_depth; };
        for (auto &visual : _row_visuals) lower(visual);
        for (auto &visual : _cell_visuals) lower(visual);
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
        for (auto &visual : _row_visuals) restore(visual);
        for (auto &visual : _cell_visuals) restore(visual);
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
                detail::draw_table_cell_visual(ctx, quads_stream, _alt_row_visuals[row],
                                               theme->get_style(_alternating_row_style.id), clip_id(), false);
            }

            for (auto &visual : _row_visuals)
            {
                const StyleState state = detail::resolve_table_element_state(visual.rect);
                const StyleID style_id =
                    theme->get_resolved_style(_cell_style.tag_id, AUIK_TAG_TABLE_CELL, parent_id, state);
                detail::draw_table_cell_visual(ctx, quads_stream, visual, theme->get_style(style_id), clip_id(),
                                               can_emit_hit(ctx));
            }

            for (auto &visual : _cell_visuals)
            {
                const StyleState state = detail::resolve_table_element_state(visual.rect);
                const StyleID style_id =
                    theme->get_resolved_style(_cell_style.tag_id, AUIK_TAG_TABLE_CELL, parent_id, state);
                detail::draw_table_cell_visual(ctx, quads_stream, visual, theme->get_style(style_id), clip_id(), false);
            }

            if (supports_columns() && column_resizable())
            {
                const size_t resize_count = _column_count > 0u ? _column_count - 1u : 0u;
                for (size_t index = 0; index < _resize_border_hit_visuals.size() && index < resize_count; ++index)
                    detail::draw_table_resize_border_visual(ctx, quads_stream, _resize_border_hit_visuals[index], theme,
                                                            _resize_border_style.tag_id, id(), clip_id(),
                                                            can_emit_hit(ctx));
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
                cell_ctx.is_hit_allowed = false;
                detail::draw_child_in_clip(cell, cell_ctx, content_clip);
            }
        }
    }

    void Tree::on_click(MouseKey key, KeyPressState state, u32 click_count)
    {
        (void)click_count;
        if (key != MouseKey::left || state != KeyPressState::press) return;
        const auto hover_id = detail::get_context().hover_id;
        if (hover_id.widget_id != id() || hover_id.tag_id != AUIK_TAG_TABLE_TREE_ARROW) return;
        add_render_command<detail::ClickEventTraits>(this, [this, node = static_cast<size_t>(hover_id.element_id)]() {
            if (node >= _nodes.size() || !node_has_children(node)) return;
            set_node_expanded(node, !_nodes[node].expanded);
        });
        detail::mark_host_refresh_request();
    }

    void Tree::on_hover(HoverState state)
    {
        auto &ctx = detail::get_context();
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
            detail::mark_host_refresh_request();
        }
    }

    void Tree::on_drag(const amal::vec2 &delta, KeyPressState state)
    {
        const auto drag_id = detail::get_context().io.drag_id;
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
            {
                for (size_t column = 0; column < _column_count; ++column)
                    _resize_size_basis[column].x = column < _size_overrides.size() ? _size_overrides[column].x : 0.0f;
            }
            else
            {
                for (size_t column = 0; column < _column_count; ++column)
                    _resize_size_basis[column].x = _layout_metrics[column].x.value;
            }
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

    void Tree::on_detach() { Widget::on_detach(); }

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
        clear_cells();
        _column_count = resolve_column_count();
        _cells.reserve(_visible_nodes.size());
        for (size_t visible_row = 0; visible_row < _visible_nodes.size(); ++visible_row)
        {
            const auto &node = _nodes[_visible_nodes[visible_row]];
            _cells.emplace_back();
            auto &row = _cells.back();
            row.reserve(_column_count);
            row.push_back(node.label);
            if (supports_columns())
                for (auto *cell : _node_cells[_visible_nodes[visible_row]]) row.push_back(cell);
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
        _cells.clear();
        _row_visuals.clear();
        _cell_visuals.clear();
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
        for (auto &visual : _row_visuals) invalidate_quad(visual.draw);
        for (auto &visual : _cell_visuals) invalidate_quad(visual.draw);
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
    }

    void Tree::rebuild_from_model_binding()
    {
        if (!_model_binding || !is_model_binding_valid(*_model_binding))
        {
            clear();
            return;
        }

        auto *model = find_model(_model_binding->db, _model_binding->model_id);
        clear();
        acul::vector<ModelRecordID> added_records;
        acul::vector<size_t> added_nodes;
        if (!model) return;
        added_records.reserve(_model_binding->records.size());
        added_nodes.reserve(_model_binding->records.size());

        for (ModelRecordID record_id : _model_binding->records)
        {
            auto *record = model->find_record(record_id);
            if (!record) continue;

            size_t parent_node = invalid_node;
            ModelRecordID parent_record_id = AUIK_MODEL_RECORD_ID_INVALID;
            if (read_model_binding_value(*_model_binding, record_id, AUIK_TREE_PARENT_FIELD, parent_record_id) &&
                parent_record_id != AUIK_MODEL_RECORD_ID_INVALID)
            {
                for (size_t i = 0; i < added_records.size(); ++i)
                {
                    if (added_records[i] != parent_record_id) continue;
                    parent_node = added_nodes[i];
                    break;
                }
            }

            const auto &field_ids = _model_binding->presenter.field_ids;
            Widget *label = present_model_field(*_model_binding, *record, field_ids[0]);
            Row cells;
            if (supports_columns() && field_ids.size() > 1u)
            {
                cells.reserve(field_ids.size() - 1u);
                for (size_t i = 1u; i < field_ids.size(); ++i)
                    if (auto *widget = present_model_field(*_model_binding, *record, field_ids[i]))
                        cells.push_back(widget);
            }

            const size_t node = add_node(label, std::move(cells), parent_node);
            added_records.push_back(record_id);
            added_nodes.push_back(node);
        }
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
    { return static_cast<u32>(visible_row * _column_count + column); }

    Widget *Tree::cell_widget(size_t visible_row, size_t column) const
    {
        if (visible_row >= _cells.size()) return nullptr;
        return column < _cells[visible_row].size() ? _cells[visible_row][column] : nullptr;
    }

    const TableColumnSettings &Tree::settings_for_column(size_t column) const
    { return column < _column_settings.size() ? _column_settings[column] : _default_column_settings; }

    void Tree::update_column_widths(f32 inner_width)
    {
        detail::update_table_column_widths(
            _layout_metrics, _column_count, inner_width, _size_overrides,
            detail::has_table_flag(_tree_flags, AUIK_TABLE_TREE_FLAG_COLUMN_SIZE_OVERRIDES),
            [this](size_t column) { return to_layout_settings(settings_for_column(column)); });
    }

    void Tree::resize_visuals()
    {
        _row_visuals.resize(_visible_nodes.size());
        _cell_visuals.resize(_visible_nodes.size() * _column_count);
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
        for (auto &visual : _row_visuals) visual.rect.clip_id = clip_id();
        for (auto &visual : _cell_visuals) visual.rect.clip_id = clip_id();
        for (auto &visual : _alt_row_visuals) visual.rect.clip_id = clip_id();
        for (auto &visual : _resize_border_hit_visuals) visual.rect.clip_id = clip_id();
        _resize_indicator_visual.rect.clip_id = clip_id();
        for (auto &visual : _arrow_visuals) visual.rect.clip_id = clip_id();
    }

    void Tree::sync_cell_parents()
    {
        for (auto &row : _cells)
            for (auto *cell : row)
                if (cell) cell->set_parent(this);
    }

    void Tree::invalidate_layout()
    {
        auto &ctx = detail::get_context();
        ctx.dirty_flags |= DirtyFlagBits::redraw | DirtyFlagBits::hit_rect_update;
        auto *layout_parent = parent();
        if (!layout_parent) return;
        layout_parent->update_layout(false);
        layout_parent->update_draw_commands(DrawReasonBits::layout);
        detail::mark_host_refresh_request();
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
        detail::mark_host_refresh_request();
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
            const f32 visual_w = amal::max(style.border_thickness(), 1.0f);
            _resize_indicator_visual.rect.bounds.offset.x +=
                _resize_indicator_visual.rect.bounds.size.x * 0.5f - visual_w * 0.5f;
            _resize_indicator_visual.rect.bounds.size.x = visual_w;
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
        detail::mark_host_refresh_request();
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
        detail::emit_table_service_hit_rect(ctx, visual.hit_draw, visual.rect, can_emit_hit(ctx));
        if (!node_has_children(visual.node)) return;
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
        icon.flags = AUIK_TEXTURE_INSTANCE_TEXT_BIT;
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
            batch.flags = AUIK_TEXTURE_INSTANCE_TEXT_BIT;
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

        bool is_configurable_cell(Widget *cell) { return cell && (cell->widget_flags & WidgetFlagBits::configurable); }

        bool node_has_configurable_widgets(const TableTree &tree, size_t node_i)
        {
            const auto &node = tree.nodes()[node_i];
            if (is_configurable_cell(node.label)) return true;
            for (auto *cell : tree.node_cells(node_i))
                if (is_configurable_cell(cell)) return true;
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

        void write_node_row(acul::bin_stream &stream, Widget *label, const acul::vector<Widget *> &cells)
        {
            acul::vector<u32> columns;
            acul::vector<umbf::Block *> blocks;
            if (is_configurable_cell(label))
            {
                columns.push_back(0u);
                blocks.push_back(label);
            }
            for (size_t column = 0u; column < cells.size(); ++column)
            {
                auto *cell = cells[column];
                if (!is_configurable_cell(cell)) continue;
                columns.push_back(static_cast<u32>(column + 1u));
                blocks.push_back(cell);
            }

            stream.write(static_cast<u32>(columns.size()));
            if (!columns.empty()) stream.write(columns.data(), columns.size());
            stream.write(blocks);
        }

        void read_node_row(acul::bin_stream &stream, Widget *&label, acul::vector<Widget *> &cells)
        {
            u32 cell_count = 0u;
            stream.read(cell_count);

            acul::vector<u32> columns;
            columns.resize(cell_count);
            if (!columns.empty()) stream.read(columns.data(), columns.size());

            acul::vector<umbf::Block *> blocks;
            stream.read(blocks);

            size_t column_count = 0u;
            for (u32 column : columns) column_count = amal::max(column_count, static_cast<size_t>(column));
            cells.resize(column_count);

            for (u32 cell_i = 0u; cell_i < cell_count; ++cell_i)
            {
                auto *cell = static_cast<Widget *>(blocks[cell_i]);
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
                .write(tree->resize_border_style_tag())
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
                keep[node_i] = node_has_configurable_widgets(*tree, node_i);
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
            u32 resize_border_style_tag = AUIK_STYLE_TAG_TABLE_TREE_RESIZE_BORDER;
            u32 line_style_tag = AUIK_STYLE_TAG_TREE_LINE;
            u32 collapse_icon_style_tag = AUIK_STYLE_TAG_TREE_COLLAPSE_ICON;
            f32 indent_width = 16.0f;
            u32 tree_flags = 0u;
            stream.read(style_tag)
                .read(cell_style_tag)
                .read(alternating_row_style_tag)
                .read(resize_border_style_tag)
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
            acul::vector<acul::point2D<f32>> size_overrides;
            size_overrides.resize(override_count);
            if (!size_overrides.empty()) stream.read(size_overrides.data(), size_overrides.size());

            auto *tree = acul::alloc<TableTree>(common.id, common.requested_size, WidgetFlags(common.widget_flags),
                                                nullptr, style_tag);
            tree->set_cell_style_tag(cell_style_tag);
            tree->set_alternating_row_style_tag(alternating_row_style_tag);
            tree->set_resize_border_style_tag(resize_border_style_tag);
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

                Widget *label = nullptr;
                acul::vector<Widget *> cells;
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

        bool tree_node_has_configurable_widget(const Tree::Node &node) { return is_configurable_cell(node.label); }

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
                keep[node_i] = tree_node_has_configurable_widget(nodes[node_i]);
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

            auto *tree = acul::alloc<Tree>(common.id, WidgetFlags(common.widget_flags), nullptr, style_tag);
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

                Widget *label = nullptr;
                acul::vector<Widget *> cells;
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
        AUIK_EXPORT const umbf::streams::Stream tree{read_tree, write_tree};
        AUIK_EXPORT const umbf::streams::Stream table_tree{read_table_tree, write_table_tree};
    } // namespace streams
} // namespace auik
