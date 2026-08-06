#include <auik/pipelines.hpp>
#include <auik/detail/depth.hpp>
#include <auik/widgets/detail/draw_cull.hpp>
#include <auik/widgets/grid_layout.hpp>

namespace auik
{
    namespace
    {
        amal::vec2 resize_helper_depth_range(const amal::vec2 &grid_depth_range)
        {
            return detail::depth_foreground_range(grid_depth_range);
        }

        f32 resize_helper_depth(const amal::vec2 &grid_depth_range)
        {
            return next_depth(resize_helper_depth_range(grid_depth_range));
        }

        f32 active_resize_helper_depth(const amal::vec2 &grid_depth_range)
        {
            return next_depth(detail::depth_foreground_range(resize_helper_depth_range(grid_depth_range)));
        }

        amal::vec2 align_pos(const amal::rect &bounds, const amal::vec2 &size, ChildLayoutFlags layout)
        {
            amal::vec2 pos = bounds.offset;
            if (layout & ChildLayoutFlagBits::aright) pos.x += amal::max(bounds.size.x - size.x, 0.0f);
            else if (layout & ChildLayoutFlagBits::hcenter)
                pos.x += amal::floor(amal::max(bounds.size.x - size.x, 0.0f) * 0.5f);

            if (layout & ChildLayoutFlagBits::bottom) pos.y += amal::max(bounds.size.y - size.y, 0.0f);
            else if (layout & ChildLayoutFlagBits::vcenter)
                pos.y += amal::floor(amal::max(bounds.size.y - size.y, 0.0f) * 0.5f);
            return pos;
        }

        StyleState resolve_resize_helper_state(const detail::RectData &rect)
        {
            const auto &ctx = detail::get_context();
            if (ctx.io.drag_id == rect.id) return StyleState::active;
            if (ctx.hover_id == rect.id) return StyleState::hover;
            return StyleState::normal;
        }
    } // namespace

    GridLayout::GridLayout(u32 id, size_t rows, size_t columns, amal::vec2 inline_size, WidgetFlags flags)
        : Widget(id, flags, EventFlagBits::hover | EventFlagBits::drag, {{0.0f, 0.0f}, inline_size},
                 AUIK_TAG_GRID_LAYOUT),
          _rows(rows), _columns(columns)
    {
        _cells.resize(_rows * _columns);
        _row_weights.assign(_rows, 1.0f);
        _column_weights.assign(_columns, 1.0f);
        _row_sizes.assign(_rows, 0.0f);
        _column_sizes.assign(_columns, 0.0f);
    }

    GridLayout::~GridLayout() { clear(); }

    void GridLayout::set_cell(size_t row, size_t column, Widget *widget, ChildLayoutFlags layout)
    {
        if (row >= _rows || column >= _columns) return;
        auto &cell = _cells[cell_index(row, column)];
        detach_cell(cell);
        cell.widget = widget;
        cell.layout = layout;
        attach_cell(cell);
    }

    void GridLayout::clear_cell(size_t row, size_t column)
    {
        if (row >= _rows || column >= _columns) return;
        auto &cell = _cells[cell_index(row, column)];
        detach_cell(cell);
        cell = {};
    }

    void GridLayout::clear()
    {
        for (auto &cell : _cells)
        {
            detach_cell(cell);
            cell = {};
        }
    }

    void GridLayout::set_column_resizable(bool value) { _column_resizable = value; }
    void GridLayout::set_row_resizable(bool value) { _row_resizable = value; }
    void GridLayout::set_track_min_size(amal::vec2 value) { _track_min_size = value; }
    void GridLayout::set_resize_helper_style_tag(u32 tag_id)
    {
        if (_resize_helper_style.tag_id == tag_id) return;
        _resize_helper_style = {Theme::STYLE_ID_INVALID, tag_id};
    }

    StyleUpdateFlags GridLayout::update_style()
    {
        StyleUpdateFlags flags = resolve_style_selector(_resize_helper_style, id(), id(), StyleState::normal);
        flags |= resolve_style_selector(_resize_helper_drag_style, id(), id(), StyleState::normal);
        const auto transition = detail::get_widget_style_selector_transition(id());
        if (transition.prev_id.widget_id == id() || transition.current_id.widget_id == id())
        {
            if (transition.prev_id.tag_id == AUIK_TAG_GRID_LAYOUT_RESIZE_HELPER_V ||
                transition.prev_id.tag_id == AUIK_TAG_GRID_LAYOUT_RESIZE_HELPER_H ||
                transition.current_id.tag_id == AUIK_TAG_GRID_LAYOUT_RESIZE_HELPER_V ||
                transition.current_id.tag_id == AUIK_TAG_GRID_LAYOUT_RESIZE_HELPER_H)
                flags |= StyleUpdateFlagBits::redraw;
        }
        for (auto &cell : _cells)
            if (cell.widget && cell.widget->is_visible()) flags |= cell.widget->update_style();
        return flags;
    }

    void GridLayout::update_layout_min_size()
    {
        acul::vector<f32> column_required(_columns, _track_min_size.x);
        acul::vector<f32> row_required(_rows, _track_min_size.y);
        for (size_t row = 0u; row < _rows; ++row)
        {
            for (size_t column = 0u; column < _columns; ++column)
            {
                auto &cell = _cells[cell_index(row, column)];
                if (!cell.widget || !cell.widget->is_visible()) continue;
                cell.widget->update_layout_min_size();
                const amal::vec2 cell_required = cell.widget->required_size();
                column_required[column] = amal::max(column_required[column], cell_required.x);
                row_required[row] = amal::max(row_required[row], cell_required.y);
            }
        }

        amal::vec2 required{};
        for (const f32 value : column_required) required.x += value;
        for (const f32 value : row_required) required.y += value;
        if (is_size_concrete(style_size().x)) required.x = amal::max(required.x, style_size().x);
        if (is_size_concrete(style_size().y)) required.y = amal::max(required.y, style_size().y);
        set_required_size(required);
    }

    void GridLayout::update_layout(bool min_size_known)
    {
        if (!min_size_known) update_layout_min_size();
        set_layout_size(resolve_layout_size_from_required());
        Widget::update_layout(true);
        set_clip_id(parent() ? parent()->content_clip_id() : clip_id());

        resolve_tracks(_column_weights, _column_sizes, size().x, _track_min_size.x);
        resolve_tracks(_row_weights, _row_sizes, size().y, _track_min_size.y);

        f32 y = position().y;
        for (size_t row = 0; row < _rows; ++row)
        {
            f32 x = position().x;
            for (size_t column = 0; column < _columns; ++column)
            {
                layout_cell({{x, y}, {_column_sizes[column], _row_sizes[row]}}, _cells[cell_index(row, column)]);
                x += _column_sizes[column];
            }
            y += _row_sizes[row];
        }
        update_resize_helpers();
    }

    void GridLayout::translate(const amal::vec2 &delta)
    {
        if (delta.x == 0.0f && delta.y == 0.0f) return;
        Widget::translate(delta);
        for (auto &cell : _cells)
            if (cell.widget) cell.widget->translate(delta);
        for (auto &helper : _helpers) helper.rect.bounds.offset += delta;
        if (clip_id() != 0xFFFFu) update_clip_rect(clip_id(), {position().x, position().y, size().x, size().y});
    }

    void GridLayout::reset_clip_rect_records()
    {
        Widget::reset_clip_rect_records();
        for (auto &cell : _cells)
            if (cell.widget) cell.widget->reset_clip_rect_records();
    }

    void GridLayout::rebuild_clip_rects()
    {
        set_clip_id(parent() ? parent()->content_clip_id() : clip_id());
        for (auto &cell : _cells)
            if (cell.widget && cell.widget->is_visible()) cell.widget->rebuild_clip_rects();
        update_resize_helpers();
    }

    void GridLayout::reset_draw_records()
    {
        Widget::reset_draw_records();
        for (auto &cell : _cells)
            if (cell.widget) cell.widget->reset_draw_records();
        for (auto &helper : _helpers) helper.draw = {};
    }

    u32 GridLayout::get_depth_requirement() const
    {
        u32 requirement = 1u;
        for (const auto &cell : _cells)
            if (cell.widget && cell.widget->is_visible())
                requirement += amal::max(cell.widget->get_depth_requirement(), 1u);
        return requirement + 1u;
    }

    void GridLayout::update_depth(const amal::vec2 &depth_range)
    {
        Widget::update_depth(depth_range);
        u32 content_requirement = 0u;
        for (const auto &cell : _cells)
            if (cell.widget && cell.widget->is_visible())
                content_requirement += amal::max(cell.widget->get_depth_requirement(), 1u);
        DepthCursor cursor(detail::depth_work_range(this->depth_range()), amal::max(content_requirement, 1u));
        for (auto &cell : _cells)
            if (cell.widget && cell.widget->is_visible())
                cell.widget->update_depth(cursor.next(amal::max(cell.widget->get_depth_requirement(), 1u)));
        const f32 helper_depth = resize_helper_depth(this->depth_range());
        for (size_t i = 0u; i < _helpers.size(); ++i)
        {
            auto &helper = _helpers[i];
            const f32 depth =
                i == _resizing_helper ? active_resize_helper_depth(this->depth_range()) : helper_depth;
            helper.rect.depth = depth;
            helper.rect.hit_depth = depth;
        }
    }

    void GridLayout::draw(DrawCtx &ctx)
    {
        if (!is_visible() && !(ctx.reason & DrawReasonBits::invalidate)) return;
        const amal::vec4 content_clip = get_content_clip_rect();
        for (auto &cell : _cells)
            if (cell.widget && (cell.widget->is_visible() || (ctx.reason & DrawReasonBits::invalidate)))
                detail::draw_child_in_clip(cell.widget, ctx, content_clip);

        auto *stream = get_primary_quads_stream();
        if (!stream) return;
        for (auto &helper : _helpers)
        {
            const StyleState state = resolve_resize_helper_state(helper.rect);
            const bool active = state == StyleState::active;
            const u32 style_tag = active ? _resize_helper_drag_style.tag_id : _resize_helper_style.tag_id;
            const StyleID style_id = get_theme()->get_resolved_style(style_tag, style_tag, id(), state);
            const Style &style = get_theme()->get_style(style_id);
            QuadsInstanceData data{};
            data.rect = helper.rect.bounds;
            data.z_order = helper.rect.depth;
            const bool visible = (helper.visible || active) && fill_quads_instance_by_style(style, clip_id(), data);
            emit_quads_instance(ctx, stream, helper.draw, data, helper.rect, visible, true);
        }
    }

    void GridLayout::on_hover(HoverState state)
    {
        auto &ctx = detail::get_context();
        detail::CursorID::enum_type cursor = detail::CursorID::arrow;
        if (state != HoverState::leave && ctx.hover_id.widget_id == id())
        {
            if (ctx.hover_id.tag_id == AUIK_TAG_GRID_LAYOUT_RESIZE_HELPER_V) cursor = detail::CursorID::resize_ew;
            else if (ctx.hover_id.tag_id == AUIK_TAG_GRID_LAYOUT_RESIZE_HELPER_H) cursor = detail::CursorID::resize_ns;
        }
        detail::set_window_cursor(cursor, ctx.window_ctx);
    }

    void GridLayout::on_drag(const amal::vec2 &delta, KeyPressState state)
    {
        auto &ctx = detail::get_context();
        if (state == KeyPressState::press)
        {
            _resizing_helper = static_cast<size_t>(-1);
            _resize_drag_accum = {};
            _resize_basis.clear();
            if (ctx.io.drag_id.widget_id != id()) return;
            auto *helper = helper_from_element(ctx.io.drag_id.element_id);
            if (!helper) return;
            if (ctx.io.drag_id.tag_id != (helper->axis == amal::axis::x ? AUIK_TAG_GRID_LAYOUT_RESIZE_HELPER_V
                                                                        : AUIK_TAG_GRID_LAYOUT_RESIZE_HELPER_H))
                return;
            _resizing_helper = ctx.io.drag_id.element_id;
            _resize_basis = helper->axis == amal::axis::x ? _column_sizes : _row_sizes;
            const f32 active_depth = active_resize_helper_depth(this->depth_range());
            helper->rect.depth = active_depth;
            helper->rect.hit_depth = active_depth;
            detail::set_window_cursor(helper->axis == amal::axis::x ? detail::CursorID::resize_ew
                                                                    : detail::CursorID::resize_ns,
                                      ctx.window_ctx);
            detail::get_context().dirty_flags |= DirtyFlagBits::redraw | DirtyFlagBits::hit_rect_update;
            update_draw_commands(DrawReasonBits::external);
            mark_host_refresh_request();
            return;
        }

        if (state == KeyPressState::release)
        {
            const bool was_resizing = _resizing_helper != static_cast<size_t>(-1);
            if (was_resizing)
            {
                if (auto *helper = helper_from_element(static_cast<u32>(_resizing_helper)))
                {
                    helper->rect.depth = resize_helper_depth(this->depth_range());
                    helper->rect.hit_depth = helper->rect.depth;
                }
            }
            _resizing_helper = static_cast<size_t>(-1);
            _resize_drag_accum = {};
            _resize_basis.clear();
            detail::set_window_cursor(detail::CursorID::arrow, ctx.window_ctx);
            if (was_resizing)
            {
                update_layout(false);
                update_draw_commands(DrawReasonBits::layout);
                detail::get_context().dirty_flags |= DirtyFlagBits::redraw | DirtyFlagBits::hit_rect_update;
            }
            mark_host_refresh_request();
            return;
        }

        auto *helper = helper_from_element(static_cast<u32>(_resizing_helper));
        if (!helper || _resize_basis.empty()) return;
        auto &weights = helper->axis == amal::axis::x ? _column_weights : _row_weights;
        auto &sizes = helper->axis == amal::axis::x ? _column_sizes : _row_sizes;
        const f32 min_size = helper->axis == amal::axis::x ? _track_min_size.x : _track_min_size.y;
        if (helper->track + 1u >= _resize_basis.size()) return;

        _resize_drag_accum += delta;
        const f32 requested_delta = helper->axis == amal::axis::x ? _resize_drag_accum.x : _resize_drag_accum.y;
        acul::vector<f32> next = _resize_basis;
        const f32 before = amal::max(_resize_basis[helper->track] + requested_delta, min_size);
        const f32 applied = before - _resize_basis[helper->track];
        const f32 after = amal::max(_resize_basis[helper->track + 1u] - applied, min_size);
        const f32 actual_applied = _resize_basis[helper->track + 1u] - after;
        next[helper->track] = _resize_basis[helper->track] + actual_applied;
        next[helper->track + 1u] = after;
        if (next == sizes) return;
        sizes = next;
        weights = sizes;
        normalize_weights(weights);
        detail::mark_fast_update_dirty();
        update_layout(true);
        update_draw_commands(DrawReasonBits::layout);
        detail::get_context().dirty_flags |= DirtyFlagBits::redraw | DirtyFlagBits::hit_rect_update;
        mark_host_refresh_request();
    }

    void GridLayout::on_attach()
    {
        Widget::on_attach();
        for (auto &cell : _cells)
            if (cell.widget && (cell.widget->widget_flags & WidgetFlagBits::attachable)) cell.widget->on_attach();
    }

    void GridLayout::on_detach()
    {
        for (auto &cell : _cells)
            if (cell.widget && (cell.widget->widget_flags & WidgetFlagBits::attachable)) cell.widget->on_detach();
        Widget::on_detach();
    }

    void GridLayout::attach_cell(Cell &cell)
    {
        if (!cell.widget) return;
        cell.widget->set_parent(this);
        cell.widget->set_focus_parent(this);
        cell.widget->update_style();
        if (detail::get_context().id_map.find(id()) != detail::get_context().id_map.end() &&
            (cell.widget->widget_flags & WidgetFlagBits::attachable))
            cell.widget->on_attach();
    }

    void GridLayout::detach_cell(Cell &cell)
    {
        if (!cell.widget) return;
        if (detail::get_context().id_map.find(cell.widget->id()) != detail::get_context().id_map.end() &&
            (cell.widget->widget_flags & WidgetFlagBits::attachable))
            cell.widget->on_detach();
        cell.widget->set_parent(nullptr);
        cell.widget->set_focus_parent(nullptr);
    }

    void GridLayout::layout_cell(const amal::rect &bounds, Cell &cell)
    {
        if (!cell.widget) return;
        const amal::vec2 required = cell.widget->required_size();
        amal::vec2 layout_size = required;
        if (cell.widget->fill_width()) layout_size.x = bounds.size.x;
        if (cell.widget->fill_height()) layout_size.y = bounds.size.y;
        layout_size = amal::min(layout_size, bounds.size);
        cell.widget->set_position(align_pos(bounds, layout_size, cell.layout));
        cell.widget->set_layout_size(layout_size);
        cell.widget->update_layout(true);
        cell.widget->rebuild_clip_rects();
    }

    void GridLayout::resolve_tracks(const acul::vector<f32> &weights, acul::vector<f32> &sizes, f32 available,
                                    f32 min_size)
    {
        if (weights.empty()) return;
        min_size = amal::max(min_size, 0.0f);
        sizes.assign(weights.size(), min_size);
        const f32 min_total = min_size * static_cast<f32>(weights.size());
        const f32 usable = amal::max(available, min_total);
        acul::vector<f32> effective_weights(weights.size());
        f32 weight_sum = 0.0f;
        for (size_t i = 0u; i < weights.size(); ++i)
        {
            effective_weights[i] = amal::max(weights[i], 0.0f);
            weight_sum += effective_weights[i];
        }
        if (weight_sum <= 0.0f)
        {
            effective_weights.assign(weights.size(), 1.0f);
            weight_sum = static_cast<f32>(weights.size());
        }

        f32 remaining = usable;
        size_t active_count = weights.size();
        acul::vector<u8> active(weights.size(), 1u);
        while (active_count > 0u)
        {
            bool clamped = false;
            for (size_t i = 0u; i < weights.size(); ++i)
            {
                if (!active[i]) continue;
                const f32 share = weight_sum > 0.0f ? remaining * effective_weights[i] / weight_sum
                                                    : remaining / static_cast<f32>(active_count);
                if (share >= min_size) continue;
                active[i] = 0u;
                --active_count;
                remaining = amal::max(remaining - min_size, 0.0f);
                weight_sum -= effective_weights[i];
                clamped = true;
            }
            if (clamped) continue;

            f32 assigned = 0.0f;
            size_t assigned_count = 0u;
            for (size_t i = 0u; i < weights.size(); ++i)
            {
                if (!active[i]) continue;
                ++assigned_count;
                const bool last = assigned_count == active_count;
                sizes[i] = last ? remaining - assigned
                                : remaining * effective_weights[i] / weight_sum;
                assigned += sizes[i];
            }
            break;
        }
    }

    void GridLayout::normalize_weights(acul::vector<f32> &weights)
    {
        f32 sum = 0.0f;
        for (const f32 value : weights) sum += amal::max(value, 0.0f);
        if (sum <= 0.0f) return;
        for (auto &value : weights) value = amal::max(value, 0.0f) / sum;
    }

    void GridLayout::update_resize_helpers()
    {
        const size_t column_helpers = _column_resizable && _columns > 1u ? _columns - 1u : 0u;
        const size_t row_helpers = _row_resizable && _rows > 1u ? _rows - 1u : 0u;
        _helpers.resize(column_helpers + row_helpers);

        size_t helper_i = 0u;
        f32 x = position().x;
        for (size_t column = 0; column + 1u < _columns && _column_resizable; ++column)
        {
            x += _column_sizes[column];
            auto &helper = _helpers[helper_i];
            helper.axis = amal::axis::x;
            helper.track = column;
            helper.visible = true;
            helper.rect = detail::make_rect_data(id(), AUIK_TAG_GRID_LAYOUT_RESIZE_HELPER_V,
                                                 {{x - 2.0f, position().y}, {4.0f, size().y}}, clip_id(),
                                                 resize_helper_depth(this->depth_range()), 0u,
                                                 static_cast<u32>(helper_i));
            if (helper_i == _resizing_helper)
            {
                helper.rect.depth = active_resize_helper_depth(this->depth_range());
                helper.rect.hit_depth = helper.rect.depth;
            }
            ++helper_i;
        }

        f32 y = position().y;
        for (size_t row = 0; row + 1u < _rows && _row_resizable; ++row)
        {
            y += _row_sizes[row];
            auto &helper = _helpers[helper_i];
            helper.axis = amal::axis::y;
            helper.track = row;
            helper.visible = true;
            helper.rect = detail::make_rect_data(id(), AUIK_TAG_GRID_LAYOUT_RESIZE_HELPER_H,
                                                 {{position().x, y - 2.0f}, {size().x, 4.0f}}, clip_id(),
                                                 resize_helper_depth(this->depth_range()), 0u,
                                                 static_cast<u32>(helper_i));
            if (helper_i == _resizing_helper)
            {
                helper.rect.depth = active_resize_helper_depth(this->depth_range());
                helper.rect.hit_depth = helper.rect.depth;
            }
            ++helper_i;
        }
    }

    GridLayout::ResizeHelper *GridLayout::helper_from_element(u32 element_id)
    {
        if (element_id >= _helpers.size()) return nullptr;
        return &_helpers[element_id];
    }

    const GridLayout::ResizeHelper *GridLayout::helper_from_element(u32 element_id) const
    {
        if (element_id >= _helpers.size()) return nullptr;
        return &_helpers[element_id];
    }
} // namespace auik
