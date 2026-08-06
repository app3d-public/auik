#include <auik/auik.hpp>
#include <auik/detail/rect.hpp>
#include <auik/widgets/column.hpp>
#include <auik/widgets/containers.hpp>
#include <auik/widgets/detail/draw_cull.hpp>
#include <auik/widgets/text.hpp>
#include "../core/session_stream_utils.hpp"

namespace auik
{
    Column::Column(u32 id, ColumnItems columns, amal::vec2 inline_size, WidgetFlags flags)
        : Widget(id, flags, EventFlagBits::none, {{0.0f, 0.0f}, inline_size}, AUIK_TAG_COLUMN),
          _style({Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_COLUMN})
    { set_columns(std::move(columns)); }

    Column::~Column()
    {
        if (_model_binding)
        {
            _model_binding->on_records = nullptr;
            detach_model_binding(*_model_binding);
        }
        clear_columns();
    }

    void Column::clear_columns()
    {
        for (auto *slot : _columns)
        {
            if (!slot) continue;
            acul::release(slot);
        }
        _columns.clear();
        _column_widths.clear();
    }

    void Column::set_model_binding(ModelBinding *binding, acul::vector<ModelFieldID> field_ids)
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

    void Column::set_columns(ColumnItems columns)
    {
        clear_columns();
        for (auto &column : columns) add_slot(std::move(column));
    }

    void Column::rebuild_from_model_binding()
    {
        if (!_model_binding || !is_model_binding_valid(*_model_binding))
        {
            set_columns({});
            return;
        }

        auto *model = find_model(_model_binding->db, _model_binding->model_id);
        set_columns({});
        if (model)
        {
            u32 record_index = 0u;
            for (ModelRecordID record_id : _model_binding->records)
            {
                auto *record = model->find_record(record_id);
                ColumnChildren children;
                if (record)
                {
                    const auto &field_ids = _model_binding->presenter.field_ids;
                    acul::vector<Widget *> widgets(field_ids.size(), nullptr);
                    _model_binding->presenter.present_record(_model_binding, *record, record_index, widgets.data(),
                                                             static_cast<u32>(widgets.size()),
                                                             _model_binding->presenter.data);
                    children.reserve(widgets.size());
                    for (auto *widget : widgets)
                        if (widget) children.push_back(widget);
                }
                add_column(std::move(children));
                ++record_index;
            }
        }
        mark_changed();
    }

    void Column::add_column(ColumnChildren children) { add_slot(std::move(children)); }

    void Column::add_child(size_t column_index, Widget *child, ChildLayoutFlags layout)
    {
        assert(column_index < _columns.size() && "column index out of range");
        _columns[column_index]->add_child(child, layout);
    }

    void Column::set_child_layout(size_t column_index, size_t row_index, ChildLayoutFlags layout)
    {
        assert(column_index < _columns.size() && "column index out of range");
        _columns[column_index]->set_child_layout(row_index, layout);
    }

    const Column::ColumnChildren &Column::column_children(size_t index) const
    {
        assert(index < _columns.size() && "column index out of range");
        return _columns[index]->children;
    }

    Column::ColumnChildren &Column::column_children(size_t index)
    {
        assert(index < _columns.size() && "column index out of range");
        return _columns[index]->children;
    }

    const acul::vector<ChildLayoutFlags> &Column::column_layouts(size_t index) const
    {
        assert(index < _columns.size() && "column index out of range");
        return _columns[index]->child_layouts();
    }

    ::auik::Block *&Column::operator[](size_t index)
    {
        assert(index < _columns.size() && "column index out of range");
        return _columns[index];
    }

    const ::auik::Block *Column::operator[](size_t index) const
    {
        assert(index < _columns.size() && "column index out of range");
        return _columns[index];
    }

    void Column::set_style_tag(u32 tag_id)
    {
        if (_style.tag_id == tag_id) return;
        _style = {Theme::STYLE_ID_INVALID, tag_id};
        set_rect_tag_id(tag_id);
    }

    StyleUpdateFlags Column::update_style()
    {
        const u32 parent_id = parent() ? parent()->id() : 0u;
        StyleUpdateFlags out = resolve_style_selector(_style, id(), parent_id, style_state());
        for (auto *slot : _columns)
        {
            if (!slot || !slot->is_visible()) continue;
            out |= slot->update_style();
        }
        return out;
    }

    void Column::update_layout_min_size()
    {
        const auto &style = get_theme()->get_style(_style.id);
        const amal::vec4 margin = style.margin();
        const amal::vec4 padding = style.padding();
        const f32 spacing = amal::max(style.inline_spacing(), 0.0f);

        _column_widths.clear();
        _column_widths.reserve(_columns.size());
        amal::vec2 content_required{0.0f, 0.0f};
        f32 max_column_width = 0.0f;
        size_t column_count = 0u;
        bool has_column = false;
        for (auto *slot : _columns)
        {
            if (!slot || !slot->is_visible()) continue;
            slot->set_inline_spacing(spacing);
            slot->update_layout_min_size();
            const amal::vec2 slot_required = slot->required_size();
            if (has_column) content_required.x += spacing;
            content_required.x += slot_required.x;
            content_required.y = amal::max(content_required.y, slot_required.y);
            max_column_width = amal::max(max_column_width, slot_required.x);
            _column_widths.push_back(slot_required.x);
            has_column = true;
            ++column_count;
        }
        if (!is_width_fixed() && column_count > 0u)
            content_required.x =
                max_column_width * static_cast<f32>(column_count) + spacing * static_cast<f32>(column_count - 1u);

        f32 required_width = 0.0f;
        if (is_size_concrete(style_size().x)) required_width = amal::max(style_size().x, 0.0f);
        else if (!fill_width()) required_width = content_required.x + padding.x + padding.z;
        if (required_width > 0.0f)
            required_width = amal::max(required_width, content_required.x + padding.x + padding.z);

        f32 required_height = 0.0f;
        if (is_size_concrete(style_size().y)) required_height = amal::max(style_size().y, 0.0f);
        else required_height = content_required.y + padding.y + padding.w;

        set_required_size({required_width + margin.x + margin.z, required_height + margin.y + margin.w});
    }

    void Column::update_layout(bool min_size_known)
    {
        if (!min_size_known) update_layout_min_size();

        const auto &style = get_theme()->get_style(_style.id);
        const amal::vec4 margin = style.margin();
        const amal::vec4 padding = style.padding();
        const f32 spacing = amal::max(style.inline_spacing(), 0.0f);
        const amal::vec2 layout_origin = position();
        const amal::vec2 outer_pos = {layout_origin.x + margin.x, layout_origin.y + margin.y};
        const amal::vec2 required_outer = required_size();
        const amal::vec2 required_inner = {amal::max(required_outer.x - margin.x - margin.z, 0.0f),
                                           amal::max(required_outer.y - margin.y - margin.w, 0.0f)};

        amal::vec2 outer_size = {amal::max(size().x - margin.x - margin.z, 0.0f),
                                 amal::max(size().y - margin.y - margin.w, 0.0f)};
        if (!is_width_fixed())
        {
            if (fill_width())
            {
                if (outer_size.x <= 0.0f) outer_size.x = required_inner.x;
            }
            else outer_size.x = amal::max(outer_size.x, required_inner.x);
        }
        else if (outer_size.x <= 0.0f) outer_size.x = required_inner.x;
        if (outer_size.y <= 0.0f) outer_size.y = required_inner.y;
        if (!is_height_fixed()) { outer_size.y = amal::max(outer_size.y, required_inner.y); }

        set_position(outer_pos);
        set_layout_size(outer_size);
        Widget::update_layout(true);
        const amal::vec4 parent_clip = parent() ? parent()->get_content_clip_rect() : get_main_viewport_rect();
        const amal::vec4 own_rect = {position().x, position().y, size().x, size().y};
        if (parent() && clip_id() == parent()->content_clip_id()) set_clip_id(0xFFFFu);
        ensure_own_clip_rect(detail::intersect_rects(parent_clip, own_rect));

        const amal::vec2 inner_pos = outer_pos + amal::vec2{padding.x, padding.y};
        const amal::vec2 inner_size = {amal::max(outer_size.x - padding.x - padding.z, 0.0f),
                                       amal::max(outer_size.y - padding.y - padding.w, 0.0f)};
        size_t visible_column_count = 0;
        for (auto *slot : _columns)
        {
            if (!slot || !slot->is_visible()) continue;
            ++visible_column_count;
        }
        const f32 total_spacing =
            visible_column_count > 0 ? spacing * static_cast<f32>(visible_column_count - 1) : 0.0f;
        const f32 stretch_column_width = visible_column_count > 0 ? amal::max(inner_size.x - total_spacing, 0.0f) /
                                                                        static_cast<f32>(visible_column_count)
                                                                  : 0.0f;

        f32 cursor_x = inner_pos.x;
        size_t width_index = 0;
        for (auto *slot : _columns)
        {
            if (!slot || !slot->is_visible()) continue;
            const f32 column_width = is_width_fixed()
                                         ? (width_index < _column_widths.size() ? _column_widths[width_index] : 0.0f)
                                         : stretch_column_width;
            slot->set_inline_spacing(spacing);
            slot->set_position({cursor_x, inner_pos.y});
            slot->set_layout_size({column_width, inner_size.y});
            const amal::vec4 own_clip = get_clip_rect(clip_id());
            const amal::vec4 slot_rect = {slot->position().x, slot->position().y, slot->size().x, slot->size().y};
            if (slot->clip_id() == content_clip_id()) slot->set_clip_id(0xFFFFu);
            slot->ensure_own_clip_rect(detail::intersect_rects(own_clip, slot_rect));
            for (auto *child : slot->children)
            {
                if (!child) continue;
                child->set_clip_id(slot->clip_id());
            }
            slot->update_layout(true);
            cursor_x += column_width + spacing;
            ++width_index;
        }

        update_column_clip_rects();
    }

    void Column::translate(const amal::vec2 &delta)
    {
        if (delta.x == 0.0f && delta.y == 0.0f) return;
        Widget::translate(delta);
        for (auto *slot : _columns)
        {
            if (!slot) continue;
            slot->translate(delta);
        }
        update_column_clip_rects();
    }

    void Column::reset_clip_rect_records()
    {
        Widget::reset_clip_rect_records();
        _clip_rects_need_layout = true;
        for (auto *slot : _columns)
            if (slot) slot->reset_clip_rect_records();
    }

    void Column::rebuild_clip_rects()
    {
        if (_clip_rects_need_layout) return;
        update_column_clip_rects();
        for (auto *slot : _columns)
        {
            if (!slot || !slot->is_visible()) continue;
            slot->rebuild_clip_rects();
        }
    }

    void Column::reset_draw_records()
    {
        for (auto *slot : _columns)
        {
            if (!slot) continue;
            slot->reset_draw_records();
        }
    }

    void Column::update_depth(const amal::vec2 &depth_range)
    {
        Widget::update_depth(depth_range);
        for (auto *slot : _columns)
        {
            if (!slot || !slot->is_visible()) continue;
            slot->update_depth(this->depth_range());
        }
    }

    void Column::back_hit_depth()
    {
        Widget::back_hit_depth();
        for (auto *slot : _columns)
            if (slot) slot->back_hit_depth();
    }

    void Column::restore_hit_depth()
    {
        Widget::restore_hit_depth();
        for (auto *slot : _columns)
            if (slot) slot->restore_hit_depth();
    }

    void Column::draw(DrawCtx &ctx)
    {
        if (!is_visible() && !(ctx.reason & DrawReasonBits::invalidate)) return;
        const amal::vec4 content_clip = get_content_clip_rect();
        for (auto *slot : _columns)
        {
            if (!slot || (!slot->is_visible() && !(ctx.reason & DrawReasonBits::invalidate))) continue;
            DrawCtx slot_ctx = ctx;
            detail::draw_child_in_clip(slot, slot_ctx, content_clip);
        }
    }

    amal::vec4 Column::get_content_clip_rect() const
    {
        if (clip_id() == 0xFFFFu) return parent() ? parent()->get_content_clip_rect() : get_main_viewport_rect();
        return get_clip_rect(content_clip_id());
    }

    void Column::on_attach()
    {
        Widget::on_attach();
        for (auto *slot : _columns)
        {
            if (!slot) continue;
            if (slot->widget_flags & WidgetFlagBits::attachable)
            {
                slot->on_attach();
                continue;
            }
            for (auto *child : slot->children)
                if (child && (child->widget_flags & WidgetFlagBits::attachable)) child->on_attach();
        }
    }

    void Column::on_detach()
    {
        for (auto *slot : _columns)
        {
            if (!slot) continue;
            if (slot->widget_flags & WidgetFlagBits::attachable)
            {
                slot->on_detach();
                continue;
            }
            for (auto *child : slot->children)
                if (child && (child->widget_flags & WidgetFlagBits::attachable)) child->on_detach();
        }
        Widget::on_detach();
    }

    void Column::add_slot(ColumnChildren children)
    {
        auto *slot = make_column_block(id());
        slot->set_parent(this);
        slot->set_focus_parent(this);
        for (auto *child : children) slot->add_child(child);
        _columns.push_back(slot);
        _column_widths.push_back(0.0f);
    }

    void Column::update_column_clip_rects()
    {
        const amal::vec4 parent_clip = parent() ? parent()->get_content_clip_rect() : get_main_viewport_rect();
        const amal::vec4 own_rect = {position().x, position().y, size().x, size().y};
        if (parent() && clip_id() == parent()->content_clip_id()) set_clip_id(0xFFFFu);
        ensure_own_clip_rect(detail::intersect_rects(parent_clip, own_rect));

        const amal::vec4 own_clip = get_clip_rect(clip_id());
        for (auto *slot : _columns)
        {
            if (!slot) continue;
            const amal::vec4 slot_rect = {slot->position().x, slot->position().y, slot->size().x, slot->size().y};
            if (slot->clip_id() == content_clip_id()) slot->set_clip_id(0xFFFFu);
            slot->ensure_own_clip_rect(detail::intersect_rects(own_clip, slot_rect));
            for (auto *child : slot->children)
            {
                if (!child) continue;
                child->set_clip_id(slot->clip_id());
                child->rebuild_clip_rects();
            }
        }
        _clip_rects_need_layout = false;
    }

    namespace
    {
        struct ColumnChildData
        {
            umbf::Block *block = nullptr;
            ChildLayoutFlags layout = default_child_layout_flags();
        };

        using ColumnData = acul::vector<acul::vector<ColumnChildData>>;

        ColumnData collect_column_data(const Column &column)
        {
            ColumnData out;
            for (size_t column_i = 0; column_i < column.column_count(); ++column_i)
            {
                const auto &children = column.column_children(column_i);
                const auto &layouts = column.column_layouts(column_i);

                acul::vector<ColumnChildData> items;
                for (size_t child_i = 0; child_i < children.size(); ++child_i)
                {
                    auto *child = children[child_i];
                    if (!(child->widget_flags & WidgetFlagBits::configurable)) continue;
                    const ChildLayoutFlags layout =
                        child_i < layouts.size() ? layouts[child_i] : default_child_layout_flags();
                    items.push_back({child, layout});
                }
                if (!items.empty()) out.push_back(std::move(items));
            }
            return out;
        }

        void write_column(acul::bin_stream &stream, umbf::Block *block)
        {
            auto *column = static_cast<Column *>(block);
            detail::write_widget_common_data(stream, *column);
            stream.write(column->style_tag());

            ColumnData columns = collect_column_data(*column);
            stream.write(static_cast<u32>(columns.size()));
            for (auto &children : columns)
            {
                stream.write(static_cast<u32>(children.size()));
                acul::vector<umbf::Block *> blocks;
                blocks.reserve(children.size());
                for (auto &child : children)
                {
                    stream.write(static_cast<u32>(child.layout));
                    blocks.push_back(child.block);
                }
                stream.write(blocks);
            }
        }

        umbf::Block *read_column(acul::bin_stream &stream)
        {
            const auto common = detail::read_widget_common_data(stream);
            u32 style_tag = AUIK_STYLE_TAG_COLUMN;
            stream.read(style_tag);

            auto *column =
                acul::alloc<Column>(common.id, Column::ColumnItems{}, common.inline_size,
                                    WidgetFlags(common.widget_flags));
            column->set_style_tag(style_tag);
            detail::apply_widget_common_data(column, common);

            u32 column_count = 0u;
            stream.read(column_count);
            for (u32 column_i = 0u; column_i < column_count; ++column_i)
            {
                column->add_column();

                u32 child_count = 0u;
                stream.read(child_count);
                acul::vector<ChildLayoutFlags> layouts;
                layouts.reserve(child_count);
                for (u32 child_i = 0u; child_i < child_count; ++child_i)
                {
                    u32 layout = 0u;
                    stream.read(layout);
                    layouts.push_back(ChildLayoutFlags(layout));
                }

                acul::vector<umbf::Block *> children;
                stream.read(children);
                for (u32 child_i = 0u; child_i < child_count; ++child_i)
                {
                    auto *child = static_cast<Widget *>(children[child_i]);
                    column->add_child(column_i, child, layouts[child_i]);
                }
            }
            return column;
        }
    } // namespace

    namespace streams
    {
        AUIK_EXPORT const umbf::streams::Stream column{read_column, write_column};
    } // namespace streams

} // namespace auik
