#include <amal/trigonometric.hpp>
#include <auik/v2/auik.hpp>
#include <auik/v2/detail/rect.hpp>
#include <auik/v2/pipelines.hpp>
#include <auik/v2/post_effects.hpp>
#include <auik/v2/widgets/image.hpp>
#include <auik/v2/widgets/table_tree.hpp>

#define AUIK_TABLE_TREE_ARROW_ROTATE_DURATION 0.16

namespace auik::v2
{
    static inline bool valid_tree_rect_size(const amal::vec2 &size) { return size.x > 0.0f && size.y > 0.0f; }

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

    static inline void build_tree_arrow_vertices(TexturedVertexStreamVertex (&vertices)[4],
                                                 const amal::rect &icon_rect, const amal::rect &uv_rect, f32 z,
                                                 u32 clip_id)
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

    static inline bool table_tree_contains_child(const acul::vector<TableTree::Node> &nodes, size_t parent)
    {
        for (const auto &node : nodes)
            if (node.parent == parent) return true;
        return false;
    }

    static inline bool resolve_table_tree_icon(u32 icon_id, TextureID &texture, amal::rect &uv_rect, amal::vec2 &size)
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

    TableTree::TableTree(u32 id, amal::vec2 size, WidgetFlags flags, Widget *parent, u32 style_tag_id)
        : Widget(id, flags, EventFlagBits::click | EventFlagBits::hover | EventFlagBits::drag, parent,
                 {{0.0f, 0.0f}, size}, style_tag_id),
          _style({Theme::STYLE_ID_INVALID, style_tag_id})
    {
    }

    TableTree::~TableTree()
    {
        release_arrow_animations();
        clear_cells(false);
    }

    void TableTree::clear()
    {
        _nodes.clear();
        release_arrow_animations();
        rebuild_visible_nodes();
        rebuild_cells();
        invalidate_layout();
    }

    size_t TableTree::add_node(acul::string label, Row cells, size_t parent)
    {
        if (parent >= _nodes.size()) parent = invalid_node;
        const size_t out = _nodes.size();
        _nodes.push_back({std::move(label), std::move(cells), parent, true});
        rebuild_visible_nodes();
        rebuild_cells();
        invalidate_layout();
        return out;
    }

    void TableTree::set_node_expanded(size_t node, bool expanded)
    {
        if (node >= _nodes.size() || _nodes[node].expanded == expanded || !node_has_children(node)) return;
        _nodes[node].expanded = expanded;
        start_arrow_animation(node, expanded);
        rebuild_visible_nodes();
        rebuild_cells();
        invalidate_layout();
    }

    bool TableTree::node_expanded(size_t node) const { return node < _nodes.size() && _nodes[node].expanded; }

    bool TableTree::node_has_children(size_t node) const
    {
        if (node >= _nodes.size()) return false;
        return table_tree_contains_child(_nodes, node);
    }

    void TableTree::set_alternating_rows(bool value)
    {
        if (alternating_rows() == value) return;
        detail::set_table_flag(_tree_flags, AUIK_TABLE_TREE_FLAG_ALTERNATING_ROWS, value);
        invalidate_layout();
    }

    void TableTree::set_alternating_row_style_tag(u32 tag_id)
    {
        if (_alternating_row_style.tag_id == tag_id) return;
        _alternating_row_style = {Theme::STYLE_ID_INVALID, tag_id};
        invalidate_layout();
    }

    void TableTree::set_default_column_settings(TableColumnSettings settings)
    {
        _default_column_settings = settings;
        invalidate_layout();
    }

    void TableTree::set_column_settings(acul::vector<TableColumnSettings> settings)
    {
        _column_settings = std::move(settings);
        invalidate_layout();
    }

    void TableTree::set_column_settings(size_t column, TableColumnSettings settings)
    {
        if (column >= _column_settings.size()) _column_settings.resize(column + 1u);
        _column_settings[column] = settings;
        invalidate_layout();
    }

    void TableTree::clear_column_settings()
    {
        _column_settings.clear();
        detail::set_table_flag(_tree_flags, AUIK_TABLE_TREE_FLAG_COLUMN_SIZE_OVERRIDES, false);
        _size_overrides.clear();
        invalidate_layout();
    }

    void TableTree::set_column_resizable(bool value)
    {
        if (column_resizable() == value) return;
        detail::set_table_flag(_tree_flags, AUIK_TABLE_TREE_FLAG_COLUMN_RESIZABLE, value);
        invalidate_layout();
    }

    void TableTree::set_resize_border_style_tag(u32 tag_id)
    {
        if (_resize_border_style.tag_id == tag_id) return;
        _resize_border_style = {Theme::STYLE_ID_INVALID, tag_id};
        invalidate_layout();
    }

    void TableTree::set_style_tag(u32 tag_id)
    {
        if (_style.tag_id == tag_id) return;
        _style = {Theme::STYLE_ID_INVALID, tag_id};
        set_rect_tag_id(tag_id);
        invalidate_layout();
    }

    void TableTree::set_cell_style_tag(u32 tag_id)
    {
        if (_cell_style.tag_id == tag_id) return;
        _cell_style = {Theme::STYLE_ID_INVALID, tag_id};
        rebuild_cells();
        invalidate_layout();
    }

    bool TableTree::is_row_hovered(size_t visible_row) const
    {
        const auto hover = detail::get_context().hover_id;
        return hover.widget_id == id() && hover.tag_id == AUIK_TAG_TABLE_TREE && hover.element_id == visible_row;
    }

    bool TableTree::is_arrow_hovered(size_t node) const
    {
        const auto hover = detail::get_context().hover_id;
        return hover.widget_id == id() && hover.tag_id == AUIK_TAG_TABLE_TREE_ARROW && hover.element_id == node;
    }

    bool TableTree::is_resize_border_hovered(size_t element_id) const
    {
        const auto hover = detail::get_context().hover_id;
        return hover.widget_id == id() && hover.tag_id == AUIK_TAG_TABLE_RESIZE_BORDER_V &&
               hover.element_id == element_id;
    }

    StyleUpdateFlags TableTree::update_style()
    {
        const u32 parent_id = parent() ? parent()->id() : 0u;
        const auto transition = detail::get_widget_style_selector_transition(id());
        StyleUpdateFlags out = resolve_style_selector(_style, id(), parent_id, style_state());
        out |= resolve_style_selector(_cell_style, AUIK_TAG_TABLE_CELL, id(), StyleState::normal);
        out |= resolve_style_selector(_alternating_row_style, _alternating_row_style.tag_id, id(), StyleState::normal);
        out |= resolve_style_selector(_line_style, _line_style.tag_id, id(), StyleState::normal);
        out |= resolve_style_selector(_collapse_icon_style, _collapse_icon_style.tag_id, id(), StyleState::normal);
        out |= resolve_style_selector(_resize_border_style, _resize_border_style.tag_id, id(), StyleState::normal);
        if (transition.prev_id.tag_id == AUIK_TAG_TABLE_RESIZE_BORDER_V ||
            transition.current_id.tag_id == AUIK_TAG_TABLE_RESIZE_BORDER_V)
            out |= StyleUpdateFlagBits::redraw;
        out |= update_resize_indicator();
        for (auto &row : _cells)
            for (auto *cell : row)
                if (cell) out |= cell->update_style();
        ensure_arrow_resources();
        return out;
    }

    void TableTree::update_layout_min_size()
    {
        rebuild_visible_nodes();
        _column_count = resolve_column_count();
        _layout_metrics.assign(amal::max(_column_count, _visible_nodes.size()), {});

        for (size_t row = 0; row < _visible_nodes.size(); ++row)
        {
            const size_t node = _visible_nodes[row];
            for (size_t column = 0; column < _column_count; ++column)
            {
                auto *cell = cell_text(row, column);
                if (!cell) continue;
                detail::measure_table_cell(cell);
                f32 min_width = cell->required_size().x;
                if (column == 0u) min_width += static_cast<f32>(node_depth(node) + 1u) * _indent_width;
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
        for (size_t column = 0; column < _column_count; ++column) content_w += _layout_metrics[column].x.value;
        f32 content_h = 0.0f;
        for (size_t row = 0; row < _visible_nodes.size(); ++row) content_h += _layout_metrics[row].y.value;

        f32 required_width = size().x;
        if (!is_fixed()) required_width = 0.0f;
        else if (required_width <= 0.0f) required_width = content_w + padding.x + padding.z;

        f32 required_height = size().y;
        if (required_height <= 0.0f) required_height = content_h + padding.y + padding.w;
        if (!is_fixed()) required_height = content_h + padding.y + padding.w;

        set_required_size({required_width + margin.x + margin.z, required_height + margin.y + margin.w});
    }

    void TableTree::update_layout(bool min_size_known)
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
        if (!is_fixed()) outer_size.x = amal::max(outer_size.x - margin.x - margin.z, required_inner.x);
        else if (outer_size.x <= 0.0f) outer_size.x = required_inner.x;
        if (outer_size.y <= 0.0f) outer_size.y = required_inner.y;
        if (!is_fixed()) outer_size.y = amal::max(outer_size.y, required_inner.y);

        set_position(outer_pos);
        set_size(outer_size);
        Widget::update_layout(true);
        rebuild_clip_rects();

        const amal::vec2 inner_pos = outer_pos + amal::vec2{padding.x, padding.y};
        const amal::vec2 inner_size = {amal::max(outer_size.x - padding.x - padding.z, 0.0f),
                                       amal::max(outer_size.y - padding.y - padding.w, 0.0f)};
        update_column_widths(inner_size.x);
        resize_visuals();
        for (size_t index = 0; index < _resize_border_hit_visuals.size(); ++index)
        {
            _resize_border_hit_visuals[index].rect =
                detail::make_rect_data(id(), AUIK_TAG_TABLE_RESIZE_BORDER_V, {{0.0f, 0.0f}, {0.0f, 0.0f}},
                                       clip_id(), next_depth(depth_range()), 0u, static_cast<u32>(index));
        }
        _tree_line_data.clear();

        f32 cursor_y = inner_pos.y;
        size_t visual_index = 0u;
        for (size_t row = 0; row < _visible_nodes.size(); ++row)
        {
            const size_t node = _visible_nodes[row];
            const f32 row_h = _layout_metrics[row].y.value;
            _row_visuals[row].rect = detail::make_rect_data(id(), AUIK_TAG_TABLE_TREE,
                                                            {{inner_pos.x, cursor_y}, {inner_size.x, row_h}}, clip_id(),
                                                            next_depth(depth_range()), 0u, static_cast<u32>(row));
            _alt_row_visuals[row].rect = detail::make_rect_data(id(), AUIK_STYLE_TAG_TABLE_ROW_ALT,
                                                                {{inner_pos.x, cursor_y}, {inner_size.x, row_h}},
                                                                clip_id(), next_depth(depth_range()));

            const f32 indent = static_cast<f32>(node_depth(node)) * _indent_width;
            _arrow_visuals[row].node = node;
            _arrow_visuals[row].rect =
                detail::make_rect_data(id(), AUIK_TAG_TABLE_TREE_ARROW,
                                       {{inner_pos.x + indent, cursor_y}, {_indent_width, row_h}}, clip_id(),
                                       next_depth(depth_range()), 0u, static_cast<u32>(node));
            const auto &line_style = get_theme()->get_style(_line_style.id);
            const f32 line_thickness = amal::max(amal::round(line_style.border_thickness()), 1.0f);
            const f32 half_line = line_thickness * 0.5f;
            const f32 row_mid_y = amal::round(cursor_y + row_h * 0.5f);
            amal::vec2 tree_icon_size = _arrow_size;
            if (tree_icon_size.y <= 0.0f)
            {
                const auto &cell_style = get_theme()->get_style(_cell_style.id);
                tree_icon_size = {cell_style.text_size(), cell_style.text_size()};
            }
            const auto &icon_style = get_theme()->get_style(_collapse_icon_style.id);
            const amal::vec4 icon_margin = icon_style.margin();
            const f32 icon_gap_h = amal::min(tree_icon_size.y + icon_margin.y + icon_margin.w, row_h);
            const f32 icon_top = amal::round(cursor_y + amal::max((row_h - icon_gap_h) * 0.5f, 0.0f));
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

                const f32 level_indent = static_cast<f32>(level) * _indent_width;
                const f32 line_x = amal::round(inner_pos.x + level_indent + _indent_width * 0.5f - half_line);
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
                        const f32 branch_x = amal::round(inner_pos.x + level_indent + _indent_width * 0.5f);
                        add_line({{branch_x, row_mid_y - half_line}, {amal::max(_indent_width * 0.5f, 0.0f),
                                                                      line_thickness}});
                    }
                }
            }

            f32 cursor_x = inner_pos.x;
            for (size_t column = 0; column < _column_count; ++column)
            {
                const f32 column_w = _layout_metrics[column].x.value;
                auto *cell = cell_text(row, column);
                if (cell)
                {
                    amal::vec2 cell_pos{cursor_x, cursor_y};
                    amal::vec2 cell_size{column_w, row_h};
                    if (column == 0u)
                    {
                        const f32 left_offset = indent + _indent_width;
                        cell_pos.x += left_offset;
                        cell_size.x = amal::max(cell_size.x - left_offset, 0.0f);
                    }
                    cell->set_position(cell_pos);
                    cell->set_size(cell_size);
                    cell->update_layout(true);
                }
                if (visual_index < _cell_visuals.size())
                {
                    _cell_visuals[visual_index].rect = detail::make_rect_data(
                        id(), AUIK_TAG_TABLE_CELL, {{cursor_x, cursor_y}, {column_w, row_h}}, clip_id(),
                        next_depth(depth_range()), 0u, cell_element_id(row, column));
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
        const amal::vec4 resize_padding = resize_style.padding();
        const f32 resize_w = amal::max(resize_padding.x + resize_padding.z, resize_style.border_thickness());
        const f32 resize_hit_w = amal::max(resize_w, 1.0f);
        if (column_resizable() && resize_hit_w > 0.0f)
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

    void TableTree::translate(const amal::vec2 &delta)
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
        const amal::vec4 parent_clip = parent() ? parent()->get_content_clip_rect() : get_main_viewport();
        ensure_own_clip_rect(detail::intersect_rects(parent_clip, {position().x, position().y, size().x, size().y}));
        update_cell_clip_rects();
        update_draw_commands(DrawReasonBits::external);
        detail::get_context().dirty_flags |= DirtyFlagBits::redraw;
    }

    void TableTree::rebuild_clip_rects()
    {
        const amal::vec4 parent_clip = parent() ? parent()->get_content_clip_rect() : get_main_viewport();
        ensure_own_clip_rect(detail::intersect_rects(parent_clip, {position().x, position().y, size().x, size().y}));
        update_cell_clip_rects();
    }

    void TableTree::reset_draw_records()
    {
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

    void TableTree::update_depth(const amal::vec2 &depth_range)
    {
        Widget::update_depth(depth_range);
        amal::vec2 next_range = this->depth_range();
        for (auto &row : _cells)
        {
            for (auto *cell : row)
            {
                if (!cell) continue;
                amal::vec2 cell_range{};
                assign_next_depth(next_range, cell_range);
                cell->update_depth(cell_range);
                next_range = cell_range;
            }
        }
    }

    void TableTree::draw(DrawCtx &ctx)
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
                const StyleID style_id = theme->get_resolved_style(_cell_style.tag_id, AUIK_TAG_TABLE_CELL, parent_id,
                                                                   state);
                detail::draw_table_cell_visual(ctx, quads_stream, visual, theme->get_style(style_id), clip_id(),
                                               ctx.emit_hit_rect);
            }

            for (auto &visual : _cell_visuals)
            {
                const StyleState state = detail::resolve_table_element_state(visual.rect);
                const StyleID style_id = theme->get_resolved_style(_cell_style.tag_id, AUIK_TAG_TABLE_CELL, parent_id,
                                                                   state);
                detail::draw_table_cell_visual(ctx, quads_stream, visual, theme->get_style(style_id), clip_id(),
                                               false);
            }

            if (column_resizable())
            {
                const size_t resize_count = _column_count > 0u ? _column_count - 1u : 0u;
                for (size_t index = 0; index < _resize_border_hit_visuals.size() && index < resize_count; ++index)
                    detail::draw_table_resize_border_visual(ctx, quads_stream, _resize_border_hit_visuals[index], theme,
                                                            _resize_border_style.tag_id, id(), clip_id(),
                                                            ctx.emit_hit_rect);
            }

            draw_tree_lines(ctx, quads_stream);
        }

        for (auto &visual : _arrow_visuals) draw_arrow(ctx, visual);

        for (auto &row : _cells)
        {
            for (auto *cell : row)
            {
                if (!cell) continue;
                DrawCtx cell_ctx = ctx;
                cell_ctx.emit_hit_rect = false;
                cell->draw(cell_ctx);
            }
        }
    }

    void TableTree::on_click(MouseKey key, KeyPressState state, u32 click_count)
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

    void TableTree::on_hover(HoverState state)
    {
        auto &ctx = detail::get_context();
        detail::CursorID::enum_type cursor = detail::CursorID::arrow;
        bool resize_border_state = state == HoverState::leave;
        if (state != HoverState::leave && column_resizable() && ctx.hover_id.widget_id == id() &&
            ctx.hover_id.tag_id == AUIK_TAG_TABLE_RESIZE_BORDER_V)
        {
            cursor = detail::CursorID::resize_ew;
            resize_border_state = true;
        }
        detail::set_window_cursor(cursor, ctx.window_ctx);
        if (resize_border_state)
        {
            ctx.dirty_flags |= DirtyFlagBits::redraw;
            detail::mark_host_refresh_request();
        }
    }

    void TableTree::on_drag(const amal::vec2 &delta, KeyPressState state)
    {
        const auto drag_id = detail::get_context().io.drag_id;
        if (state == KeyPressState::press)
        {
            _resizing_column = static_cast<size_t>(-1);
            _resize_drag_accum = {0.0f, 0.0f};
            _resize_size_basis.clear();
            if (!column_resizable() || drag_id.widget_id != id() || drag_id.tag_id != AUIK_TAG_TABLE_RESIZE_BORDER_V)
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
        const bool changed = column_resizable() &&
                             detail::apply_table_column_resize(
                                 _layout_metrics, _size_overrides, _resize_size_basis, _column_count,
                                 _resizing_column, _resize_drag_accum.x,
                                 detail::has_table_flag(_tree_flags, AUIK_TABLE_TREE_FLAG_COLUMN_SIZE_OVERRIDES),
                                 [this](size_t column) { return to_layout_settings(settings_for_column(column)); });
        if (!changed) return;
        detail::set_table_flag(_tree_flags, AUIK_TABLE_TREE_FLAG_COLUMN_SIZE_OVERRIDES, true);
        update_own_layout();
    }

    amal::vec4 TableTree::get_content_clip_rect() const
    {
        if (clip_id() == 0xFFFFu) return parent() ? parent()->get_content_clip_rect() : get_main_viewport();
        return get_clip_rect(content_clip_id());
    }

    void TableTree::on_attach()
    {
        detail::get_context().id_map.emplace(id(), this);
        sync_cell_parents();
    }

    void TableTree::on_detach() { detail::get_context().id_map.erase(id()); }

    void TableTree::rebuild_visible_nodes()
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

    void TableTree::rebuild_cells()
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
            row.push_back(make_cell_text(node.label));
            for (const auto &value : node.cells) row.push_back(make_cell_text(value));
        }
        sync_cell_parents();
        update_depth(depth_range());
    }

    void TableTree::clear_cells(bool invalidate_draw)
    {
        if (invalidate_draw) invalidate_visual_draw_records();
        for (auto &row : _cells)
            for (auto *cell : row)
                if (cell)
                {
                    if (invalidate_draw) cell->invalidate_draw_records();
                    acul::release(cell);
                }
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

    void TableTree::invalidate_visual_draw_records()
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

    void TableTree::clear_tree_line_draw_records()
    {
        if (_tree_line_draws.empty()) return;
        auto *quads_stream = get_primary_quads_stream();
        if (quads_stream && quads_stream->invalidate_data_batch_in_stream)
            invalidate_data_batch_in_stream(quads_stream, _tree_line_draws.data(),
                                            static_cast<u32>(_tree_line_draws.size()));
        _tree_line_draws.clear();
    }

    void TableTree::draw_tree_lines(DrawCtx &ctx, DrawStream *stream)
    {
        if (!stream) return;
        if (ctx.is_invalidating())
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

        if (ctx.is_recording() || _tree_line_draws.empty())
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

    Text *TableTree::make_cell_text(const acul::string &value)
    {
        auto *text = acul::alloc<Text>(AUIK_TAG_TEXT, value, amal::vec2{0.0f, 0.0f},
                                       WidgetFlagBits::visible | WidgetFlagBits::fixed, this, _cell_style.tag_id,
                                       detail::TextOverflowMode::ellipsis, detail::TextVerticalAlign::center);
        if (detail::g_context) text->update_style();
        return text;
    }

    size_t TableTree::node_depth(size_t node) const
    {
        size_t depth = 0u;
        while (node < _nodes.size() && _nodes[node].parent != invalid_node)
        {
            node = _nodes[node].parent;
            ++depth;
        }
        return depth;
    }

    bool TableTree::node_is_last_sibling(size_t node) const
    {
        if (node >= _nodes.size()) return true;
        const size_t parent = _nodes[node].parent;
        for (size_t index = node + 1u; index < _nodes.size(); ++index)
            if (_nodes[index].parent == parent) return false;
        return true;
    }

    size_t TableTree::node_ancestor_at_depth(size_t node, size_t depth) const
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

    size_t TableTree::resolve_column_count() const
    {
        size_t count = 1u;
        for (const auto &node : _nodes) count = amal::max(count, node.cells.size() + 1u);
        return count;
    }

    u32 TableTree::cell_element_id(size_t visible_row, size_t column) const
    {
        return static_cast<u32>(visible_row * _column_count + column);
    }

    Text *TableTree::cell_text(size_t visible_row, size_t column) const
    {
        if (visible_row >= _cells.size()) return nullptr;
        return column < _cells[visible_row].size() ? _cells[visible_row][column] : nullptr;
    }

    const TableColumnSettings &TableTree::settings_for_column(size_t column) const
    {
        return column < _column_settings.size() ? _column_settings[column] : _default_column_settings;
    }

    void TableTree::update_column_widths(f32 inner_width)
    {
        detail::update_table_column_widths(
            _layout_metrics, _column_count, inner_width, _size_overrides,
            detail::has_table_flag(_tree_flags, AUIK_TABLE_TREE_FLAG_COLUMN_SIZE_OVERRIDES),
            [this](size_t column) { return to_layout_settings(settings_for_column(column)); });
    }

    void TableTree::resize_visuals()
    {
        _row_visuals.resize(_visible_nodes.size());
        _cell_visuals.resize(_visible_nodes.size() * _column_count);
        _alt_row_visuals.resize(_visible_nodes.size());
        _arrow_visuals.resize(_visible_nodes.size());
        _resize_border_hit_visuals.resize(_column_count > 0u ? _column_count - 1u : 0u);
    }

    void TableTree::update_cell_clip_rects()
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

    void TableTree::sync_cell_parents()
    {
        for (auto &row : _cells)
            for (auto *cell : row)
                if (cell) cell->set_parent(this);
    }

    void TableTree::invalidate_layout()
    {
        auto &ctx = detail::get_context();
        ctx.dirty_flags |= DirtyFlagBits::redraw | DirtyFlagBits::hit_rect_update;
        auto *layout_parent = parent();
        if (!layout_parent) return;
        layout_parent->update_layout(false);
        layout_parent->update_draw_commands(DrawReasonBits::layout);
        detail::mark_host_refresh_request();
    }

    void TableTree::update_own_layout()
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

    StyleUpdateFlags TableTree::update_resize_indicator()
    {
        const detail::RectData prev_rect = _resize_indicator_visual.rect;
        const bool prev_active =
            detail::has_table_flag(_tree_flags, AUIK_TABLE_TREE_FLAG_RESIZE_INDICATOR_ACTIVE);
        const auto &ctx = detail::get_context();
        const auto transition = detail::get_widget_style_selector_transition(id());
        auto target = ctx.io.mouse_down ? ctx.io.drag_id : detail::ElementID{};
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
            _resize_indicator_visual.rect = detail::make_rect_data(
                id(), AUIK_TAG_TABLE_RESIZE_BORDER_V, {{0.0f, 0.0f}, {0.0f, 0.0f}}, clip_id());
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

    void TableTree::ensure_arrow_resources()
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

    TableTree::ArrowAnimation *TableTree::find_arrow_animation(size_t node)
    {
        for (auto &animation : _arrow_animations)
            if (animation.node == node) return &animation;
        return nullptr;
    }

    const TableTree::ArrowAnimation *TableTree::find_arrow_animation(size_t node) const
    {
        for (const auto &animation : _arrow_animations)
            if (animation.node == node) return &animation;
        return nullptr;
    }

    void TableTree::start_arrow_animation(size_t node, bool opening)
    {
        if (!detail::g_context) return;
        auto *rotate_effect = get_rotate_post_effect();
        if (!rotate_effect) return;

        auto *animation = find_arrow_animation(node);
        if (!animation)
        {
            _arrow_animations.push_back({});
            animation = &_arrow_animations.back();
            animation->node = node;
            animation->rotate_post_id = create_rotate_post_effect_data(rotate_effect, this);
            if (animation->rotate_post_id == AUIK_INVALID_POST_EFFECT_DATA_ID)
            {
                _arrow_animations.erase(_arrow_animations.end() - 1);
                return;
            }
        }

        detail::update_window_time(detail::get_context().window_ctx);
        auto *rotate_data = get_rotate_post_effect_data(rotate_effect, animation->rotate_post_id);
        if (!rotate_data) return;
        rotate_data->animation_start = detail::get_context().window_ctx->time;
        rotate_data->animation_from = opening ? 0.0f : amal::half_pi<f32>();
        rotate_data->animation_to = opening ? amal::half_pi<f32>() : 0.0f;
        rotate_data->angle = rotate_data->animation_from;
        rotate_data->animating = true;
        push_widget_to_transient_cache(this);
        detail::mark_host_refresh_request();
        schedule_arrow_tick();
    }

    void TableTree::schedule_arrow_tick()
    {
        if (!detail::g_context || _arrow_tick_scheduled) return;
        detail::update_window_time(detail::get_context().window_ctx);
        const f64 delay = get_max_animation_delay() > 0.0 ? get_max_animation_delay() : (1.0 / 60.0);
        _arrow_tick_scheduled = true;
        schedule_delayed_host_task(id(), detail::get_context().window_ctx->time + delay, [this]() {
            _arrow_tick_scheduled = false;
            tick_arrow_animations();
        });
    }

    void TableTree::tick_arrow_animations()
    {
        if (!detail::g_context) return;
        auto *rotate_effect = get_rotate_post_effect();
        if (!rotate_effect) return;
        detail::update_window_time(detail::get_context().window_ctx);
        const f64 now = detail::get_context().window_ctx->time;

        bool any_active = false;
        for (size_t index = 0; index < _arrow_animations.size();)
        {
            auto &animation = _arrow_animations[index];
            auto *rotate_data = get_rotate_post_effect_data(rotate_effect, animation.rotate_post_id);
            if (!rotate_data || !rotate_data->animating)
            {
                clear_arrow_animation_draw(animation);
                if (animation.rotate_post_id != AUIK_INVALID_POST_EFFECT_DATA_ID)
                    release_rotate_post_effect_data(rotate_effect, animation.rotate_post_id);
                _arrow_animations.erase(_arrow_animations.begin() + index);
                continue;
            }

            f64 raw_t = (now - rotate_data->animation_start) / AUIK_TABLE_TREE_ARROW_ROTATE_DURATION;
            raw_t = amal::clamp(raw_t, 0.0, 1.0);
            const f32 t = static_cast<f32>(raw_t);
            const f32 eased = 1.0f - (1.0f - t) * (1.0f - t);
            rotate_data->angle =
                rotate_data->animation_from + (rotate_data->animation_to - rotate_data->animation_from) * eased;
            if (t >= 1.0f)
            {
                rotate_data->angle = rotate_data->animation_to;
                rotate_data->animating = false;
                clear_arrow_animation_draw(animation);
                if (animation.rotate_post_id != AUIK_INVALID_POST_EFFECT_DATA_ID)
                    release_rotate_post_effect_data(rotate_effect, animation.rotate_post_id);
                _arrow_animations.erase(_arrow_animations.begin() + index);
                continue;
            }
            else any_active = true;
            ++index;
        }

        if (any_active) schedule_arrow_tick();
        else erase_widget_from_transient_cache(this);

        update_draw_commands(DrawReasonBits::external);
        detail::get_context().dirty_flags |= DirtyFlagBits::redraw;
        detail::mark_host_refresh_request();
    }

    void TableTree::clear_arrow_animation_draw(ArrowAnimation &animation)
    {
        if (animation.draw.render_id == AUIK_INVALID_DRAW_DATA_ID) return;
        auto *stream = get_primary_textured_vertex_stream();
        if (!stream || !stream->invalidate_data_in_stream) return;
        stream->invalidate_data_in_stream(stream, animation.draw);
        animation.draw = {};
    }

    void TableTree::release_arrow_animations()
    {
        auto *rotate_effect = get_rotate_post_effect();
        for (auto &animation : _arrow_animations)
        {
            clear_arrow_animation_draw(animation);
            if (rotate_effect && animation.rotate_post_id != AUIK_INVALID_POST_EFFECT_DATA_ID)
                release_rotate_post_effect_data(rotate_effect, animation.rotate_post_id);
        }
        _arrow_animations.clear();
        erase_widget_from_transient_cache(this);
        _arrow_tick_scheduled = false;
    }

    void TableTree::draw_arrow(DrawCtx &ctx, ArrowVisual &visual)
    {
        if (visual.node >= _nodes.size()) return;
        detail::emit_table_service_hit_rect(ctx, visual.hit_draw, visual.rect, ctx.emit_hit_rect);
        if (!node_has_children(visual.node)) return;
        ensure_arrow_resources();
        if (_arrow_texture.handle == 0) return;

        auto *animation = find_arrow_animation(visual.node);
        auto *rotate_effect = get_rotate_post_effect();
        auto *rotate_data =
            animation && rotate_effect ? get_rotate_post_effect_data(rotate_effect, animation->rotate_post_id) : nullptr;
        const bool animating = rotate_data && rotate_data->animating;

        TextureID closed_texture{};
        amal::rect closed_uv_rect{};
        amal::vec2 closed_size{};
        if (!resolve_table_tree_icon(AUIK_ICON_CHEVRON_RIGHT, closed_texture, closed_uv_rect, closed_size)) return;

        TextureID static_texture = closed_texture;
        amal::rect static_uv_rect = closed_uv_rect;
        amal::vec2 icon_size = closed_size;
        if (!animating && _nodes[visual.node].expanded)
            resolve_table_tree_icon(AUIK_ICON_CHEVRON_DOWN, static_texture, static_uv_rect, icon_size);

        if (icon_size.x <= 0.0f || icon_size.y <= 0.0f)
        {
            const auto &style = get_theme()->get_style(_cell_style.id);
            icon_size = {style.text_size(), style.text_size()};
        }
        amal::rect icon_rect{
            {amal::round(visual.rect.bounds.offset.x + amal::max((visual.rect.bounds.size.x - icon_size.x) * 0.5f, 0.0f)),
             amal::round(visual.rect.bounds.offset.y + amal::max((visual.rect.bounds.size.y - icon_size.y) * 0.5f, 0.0f))},
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
        icon.tint_color = animating ? 0u : get_theme()->get_style(_collapse_icon_style.id).text_color();
        icon.z_order = visual.rect.depth;
        icon.texture_id = static_cast<u16>(static_texture.bind_slot);
        icon.clip_id = visual.rect.clip_id;
        icon.flags = AUIK_TEXTURE_INSTANCE_TEXT_BIT;
        ctx.emit(stream, visual.icon_draw, &icon, visual.rect, false);

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
            RotatePostData rotate_post{animation->rotate_post_id};
            DrawCtx rotated_ctx = ctx;
            rotated_ctx.post_effect = rotate_effect;
            rotated_ctx.post_data = &rotate_post;
            rotated_ctx.emit(vertex_stream, animation->draw, &batch, visual.rect, false);
        }
    }
} // namespace auik::v2
