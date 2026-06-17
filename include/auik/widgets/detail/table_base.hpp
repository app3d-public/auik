#pragma once

#include <acul/pair.hpp>
#include <acul/vector.hpp>
#include <auik/detail/rect.hpp>
#include <auik/pipelines.hpp>
#include <auik/widgets/text.hpp>

namespace auik::detail
{
    struct TableCellVisual
    {
        RectData rect{};
        DrawDataID draw{};
    };

    struct TableTrackMetrics
    {
        f32 value = 0.0f;
        f32 min_value = 0.0f;
    };

    struct TableColumnLayoutSettings
    {
        u8 sizing = 2u;
        f32 value = 1.0f;
        f32 min_width = 0.0f;
    };

    inline bool has_table_flag(u32 flags, u32 flag) { return (flags & flag) != 0u; }

    inline void set_table_flag(u32 &flags, u32 flag, bool value)
    {
        if (value) flags |= flag;
        else flags &= ~flag;
    }

    inline void resize_table_size_points(acul::vector<acul::point2D<f32>> &values, size_t size)
    {
        const size_t old_size = values.size();
        values.resize(size);
        for (size_t index = old_size; index < size; ++index) values[index] = {0.0f, 0.0f};
    }

    inline StyleState resolve_table_element_state(const RectData &rect)
    {
        const auto &ctx = get_context();
        if (ctx.io.drag_id == rect.id) return StyleState::active;
        if (ctx.hover_id == rect.id) return StyleState::hover;
        return StyleState::normal;
    }

    inline void apply_table_cell_alignment(Widget *cell, HAlign halign, VAlign valign)
    {
        if (!cell) return;
        auto *text = dynamic_cast<Text *>(cell);
        if (!text) return;
        text->set_horizontal_align(halign);
        if (valign != VAlign::none) text->set_vertical_align(valign);
    }

    template <class Metrics, class Overrides, class SettingsFn>
    inline void update_table_column_widths(Metrics &metrics, size_t column_count, f32 inner_width,
                                           const Overrides &overrides, bool has_overrides,
                                           SettingsFn settings_for_column)
    {
        if (metrics.size() < column_count) metrics.resize(column_count);
        for (size_t column = 0; column < column_count; ++column) metrics[column].x.value = 0.0f;

        f32 non_fill_width = 0.0f;
        f32 stretch_weight = 0.0f;
        for (size_t column = 0; column < column_count; ++column)
        {
            const TableColumnLayoutSettings settings = settings_for_column(column);
            const f32 measured = metrics[column].x.min_value;
            const f32 min_width = amal::max(settings.min_width, 0.0f);
            switch (settings.sizing)
            {
                case 1u:
                    metrics[column].x.value =
                        settings.value > 0.0f ? amal::max(settings.value, min_width) : amal::max(measured, min_width);
                    non_fill_width += metrics[column].x.value;
                    break;
                case 0u:
                    metrics[column].x.value = amal::max(measured, min_width);
                    non_fill_width += metrics[column].x.value;
                    break;
                default:
                    metrics[column].x.value = amal::max(min_width, 0.0f);
                    stretch_weight += settings.value > 0.0f ? settings.value : 1.0f;
                    break;
            }
        }

        if (stretch_weight <= 0.0f)
        {
            if (inner_width > non_fill_width && column_count != 0u)
            {
                const f32 extra_width = (inner_width - non_fill_width) / static_cast<f32>(column_count);
                for (size_t column = 0; column < column_count; ++column) metrics[column].x.value += extra_width;
            }
        }
        else
        {
            const f32 fill_width = amal::max(inner_width - non_fill_width, 0.0f);
            for (size_t column = 0; column < column_count; ++column)
            {
                const TableColumnLayoutSettings settings = settings_for_column(column);
                if (settings.sizing != 2u) continue;
                const f32 weight = settings.value > 0.0f ? settings.value : 1.0f;
                const f32 min_width = metrics[column].x.value;
                metrics[column].x.value = amal::max(fill_width * (weight / stretch_weight), min_width);
            }
        }

        if (!has_overrides) return;
        f32 overrides_width = 0.0f;
        for (size_t column = 0; column < column_count && column < overrides.size(); ++column)
            overrides_width += overrides[column].x;
        if (overrides_width <= 0.0f) return;

        const f32 scale = inner_width > 0.0f ? inner_width / overrides_width : 1.0f;
        for (size_t column = 0; column < column_count; ++column)
        {
            const TableColumnLayoutSettings settings = settings_for_column(column);
            const f32 min_width = amal::max(settings.min_width, 0.0f);
            const f32 override_width = column < overrides.size() ? overrides[column].x : 0.0f;
            metrics[column].x.value = amal::max(override_width * scale, min_width);
        }
    }

    template <class Metrics, class Overrides, class Basis, class SettingsFn>
    inline bool apply_table_column_resize(Metrics &metrics, Overrides &overrides, Basis &basis, size_t column_count,
                                          size_t resizing_column, f32 drag_delta, bool has_overrides,
                                          SettingsFn settings_for_column)
    {
        if (resizing_column == static_cast<size_t>(-1) || resizing_column >= column_count) return false;
        if (basis.size() < column_count)
        {
            resize_table_size_points(basis, column_count);
            if (has_overrides)
            {
                for (size_t column = 0; column < column_count; ++column)
                    basis[column].x = column < overrides.size() ? overrides[column].x : 0.0f;
            }
            else
            {
                for (size_t column = 0; column < column_count; ++column) basis[column].x = metrics[column].x.value;
            }
        }

        if (overrides.size() < column_count) resize_table_size_points(overrides, column_count);
        for (size_t column = 0; column < column_count; ++column) overrides[column].x = basis[column].x;

        auto shrink_columns = [&](size_t first, size_t last, f32 amount) -> f32 {
            if (amount <= 0.0f || first > last || last >= overrides.size()) return 0.0f;
            f32 remaining = amount;
            size_t active_count = last - first + 1u;
            while (remaining > 0.0f && active_count > 0u)
            {
                const f32 share = remaining / static_cast<f32>(active_count);
                f32 taken = 0.0f;
                size_t next_active_count = 0u;
                for (size_t column = first; column <= last; ++column)
                {
                    const TableColumnLayoutSettings settings = settings_for_column(column);
                    const f32 min_width = amal::max(settings.min_width, 1.0f);
                    const f32 available = amal::max(overrides[column].x - min_width, 0.0f);
                    if (available <= 0.0f) continue;
                    const f32 take = amal::min(share, available);
                    overrides[column].x -= take;
                    taken += take;
                    if (available > take) ++next_active_count;
                }
                if (taken <= 0.0f) break;
                remaining -= taken;
                active_count = next_active_count;
            }
            return amount - remaining;
        };

        if (drag_delta > 0.0f)
        {
            const f32 taken = shrink_columns(resizing_column + 1u, column_count - 1u, drag_delta);
            overrides[resizing_column].x += taken;
            return taken > 0.0f;
        }
        if (drag_delta < 0.0f && resizing_column + 1u < column_count)
        {
            const f32 taken = shrink_columns(0u, resizing_column, -drag_delta);
            overrides[resizing_column + 1u].x += taken;
            return taken > 0.0f;
        }
        return false;
    }

    inline void draw_table_cell_visual(DrawCtx &ctx, DrawStream *stream, TableCellVisual &visual, const Style &style,
                                       u16 clip_id, bool is_hit_allowed)
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

    inline void draw_table_resize_border_visual(DrawCtx &ctx, DrawStream *stream, TableCellVisual &visual,
                                                Theme *theme, u32 style_tag_id, u32 widget_id, u16 clip_id,
                                                bool is_hit_allowed)
    {
        const StyleState state = resolve_table_element_state(visual.rect);
        const StyleID style_id = theme->get_resolved_style(style_tag_id, style_tag_id, widget_id, state);
        const Style &style = theme->get_style(style_id);
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
        const bool vertical = visual.rect.bounds.size.x <= visual.rect.bounds.size.y;
        if (vertical)
        {
            const f32 visual_w = amal::max(style.border_thickness(), 1.0f);
            data.rect.offset.x += data.rect.size.x * 0.5f - visual_w * 0.5f;
            data.rect.size.x = visual_w;
        }
        else
        {
            const f32 visual_h = amal::max(style.border_thickness(), 1.0f);
            data.rect.offset.y += data.rect.size.y * 0.5f - visual_h * 0.5f;
            data.rect.size.y = visual_h;
        }
        emit_quads_instance(ctx, stream, visual.draw, data, visual.rect, visible, is_hit_allowed);
    }

    inline void emit_table_service_hit_rect(const DrawCtx &ctx, DrawDataID &draw_id, const RectData &rect,
                                            bool is_hit_allowed)
    {
        if (!is_hit_allowed || (ctx.reason & DrawReasonBits::invalidate)) return;
        if ((ctx.reason & DrawReasonBits::record)) draw_id.hit_id = AUIK_INVALID_DRAW_DATA_ID;
        auto &global_ctx = get_context();
        const bool force_update = (ctx.reason & DrawReasonBits::record) || (global_ctx.dirty_flags & DirtyFlagBits::hit_rect_update);
        auik::update_hit_rect(draw_id.hit_id, rect, force_update);
    }
} // namespace auik::detail
