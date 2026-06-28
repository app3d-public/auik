#include <auik/auik.hpp>
#include <auik/detail/rect.hpp>
#include <auik/widgets/detail/draw_cull.hpp>
#include <auik/widgets/column.hpp>
#include <auik/widgets/text.hpp>
#include "../core/session_stream_utils.hpp"

namespace auik
{
    class Column::Slot final : public Widget
    {
    public:
        acul::vector<Widget *> children;
        acul::vector<ChildLayoutFlags> child_layouts;
        f32 inline_spacing = 0.0f;

        explicit Slot(Column *parent, ColumnChildren slot_children)
            : Widget(parent->id(), WidgetFlagBits::visible, EventFlagBits::none, parent)
        {
            for (auto *child : slot_children) add_child(child);
        }

        ~Slot() override { clear_children(); }

        void clear_children()
        {
            for (auto *child : children)
            {
                if (!child) continue;
                if (child->widget_flags & WidgetFlagBits::attachable) child->on_detach();
                acul::release(child);
            }
            children.clear();
            child_layouts.clear();
        }

        void add_child(Widget *child, ChildLayoutFlags layout = default_child_layout_flags())
        {
            assert(child && "child is null");
            child->set_parent(this);
            child->set_focus_parent(this);
            child->update_style();
            children.push_back(child);
            child_layouts.push_back(layout);
        }

        void set_child_layout(size_t row_index, ChildLayoutFlags layout)
        {
            assert(row_index < child_layouts.size() && "row index out of range");
            child_layouts[row_index] = layout;
        }

        StyleUpdateFlags update_style() override
        {
            StyleUpdateFlags out = StyleUpdateFlagBits::none;
            for (auto *child : children)
            {
                if (!child) continue;
                out |= child->update_style();
            }
            return out;
        }

        void update_layout_min_size() override
        {
            amal::vec2 required{0.0f, 0.0f};
            bool has_child = false;
            for (auto *child : children)
            {
                if (!child) continue;
                child->update_layout_min_size();
                const amal::vec2 child_required = child->required_size();
                if (has_child) required.y += inline_spacing;
                required.x = amal::max(required.x, child_required.x);
                required.y += child_required.y;
                has_child = true;
            }
            set_required_size(required);
        }

        static f32 row_child_offset_y(ChildLayoutFlags layout, f32 row_height, f32 child_height)
        {
            if (layout & ChildLayoutFlagBits::vcenter)
                return amal::floor(amal::max(row_height - child_height, 0.0f) * 0.5f);
            if (layout & ChildLayoutFlagBits::bottom) return amal::max(row_height - child_height, 0.0f);
            return 0.0f;
        }

        static bool has_row_vertical_align(ChildLayoutFlags layout)
        {
            return layout & (ChildLayoutFlagBits::vcenter | ChildLayoutFlagBits::bottom);
        }

        void update_layout_with_rows(bool min_size_known, const acul::vector<f32> &row_heights)
        {
            if (!min_size_known) update_layout_min_size();

            Widget::update_layout(true);
            f32 cursor_y = position().y;
            for (size_t i = 0; i < children.size(); ++i)
            {
                auto *child = children[i];
                if (!child) continue;
                if (i > 0u) cursor_y += inline_spacing;
                const f32 row_height = i < row_heights.size() ? row_heights[i] : child->required_size().y;
                const ChildLayoutFlags layout =
                    i < child_layouts.size() ? child_layouts[i] : default_child_layout_flags();
                f32 child_y = cursor_y;
                if (has_row_vertical_align(layout))
                {
                    const f32 child_height = child->required_size().y;
                    child_y += row_child_offset_y(layout, row_height, child_height);
                }
                const amal::vec2 child_size{child->fill_width() ? size().x : child->required_size().x,
                                            child->required_size().y};
                child->set_position({position().x, child_y});
                child->set_layout_size(child_size);
                child->update_layout(true);
                child->set_clip_id(clip_id());
                cursor_y += row_height;
            }
        }
        void update_layout(bool min_size_known) override
        {
            if (!min_size_known) update_layout_min_size();

            Widget::update_layout(true);
            f32 cursor_y = position().y;
            bool has_child = false;
            for (auto *child : children)
            {
                if (!child) continue;
                if (has_child) cursor_y += inline_spacing;
                const amal::vec2 child_size{child->fill_width() ? size().x : child->required_size().x,
                                            child->required_size().y};
                child->set_position({position().x, cursor_y});
                child->set_layout_size(child_size);
                child->update_layout(true);
                child->set_clip_id(clip_id());
                cursor_y += child->required_size().y;
                has_child = true;
            }
        }

        void translate(const amal::vec2 &delta) override
        {
            if (delta.x == 0.0f && delta.y == 0.0f) return;
            Widget::translate(delta);
            if (clip_id() != 0xFFFFu)
            {
                auto rect = get_clip_rect(clip_id());
                rect.x += delta.x;
                rect.y += delta.y;
                update_clip_rect(clip_id(), rect);
            }
            for (auto *child : children)
            {
                if (!child) continue;
                child->translate(delta);
            }
        }

        void rebuild_clip_rects() override
        {
            auto *column = static_cast<Column *>(parent());
            const amal::vec4 parent_clip = column->get_content_clip_rect();
            if (parent() && clip_id() == parent()->content_clip_id()) set_clip_id(0xFFFFu);
            ensure_own_clip_rect(
                detail::intersect_rects(parent_clip, {position().x, position().y, size().x, size().y}));
            for (auto *child : children)
            {
                if (!child) continue;
                child->set_clip_id(clip_id());
                child->rebuild_clip_rects();
                child->set_clip_id(clip_id());
            }
        }

        void reset_clip_rect_records() override
        {
            Widget::reset_clip_rect_records();
            for (auto *child : children)
                if (child) child->reset_clip_rect_records();
        }

        void reset_draw_records() override
        {
            for (auto *child : children)
            {
                if (!child) continue;
                child->reset_draw_records();
            }
        }

        void update_depth(const amal::vec2 &depth_range) override
        {
            Widget::update_depth(depth_range);
            for (auto *child : children)
            {
                if (!child) continue;
                child->update_depth(this->depth_range());
            }
        }

        void back_hit_depth() override
        {
            Widget::back_hit_depth();
            for (auto *child : children)
                if (child) child->back_hit_depth();
        }

        void restore_hit_depth() override
        {
            Widget::restore_hit_depth();
            for (auto *child : children)
                if (child) child->restore_hit_depth();
        }

        void draw(DrawCtx &ctx) override
        {
            if (!(widget_flags & WidgetFlagBits::visible)) return;
            const amal::vec4 content_clip = get_content_clip_rect();
            for (auto *child : children)
            {
                if (!child) continue;
                detail::draw_child_in_clip(child, ctx, content_clip);
            }
        }

        u16 content_clip_id() const override { return clip_id(); }

        amal::vec4 get_content_clip_rect() const override { return get_clip_rect(content_clip_id()); }

        void on_attach() override
        {
            for (auto *child : children)
                if (child && (child->widget_flags & WidgetFlagBits::attachable)) child->on_attach();
        }

        void on_detach() override
        {
            for (auto *child : children)
                if (child && (child->widget_flags & WidgetFlagBits::attachable)) child->on_detach();
        }
    };

    Column::Column(u32 id, ColumnItems columns, amal::vec2 size, WidgetFlags flags, Widget *parent, u32 style_tag_id)
        : Widget(id, flags, EventFlagBits::none, parent, {{0.0f, 0.0f}, size}, style_tag_id),
          _style({Theme::STYLE_ID_INVALID, style_tag_id})
    {
        set_columns(std::move(columns));
    }

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
        _row_heights.clear();
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
            for (ModelRecordID record_id : _model_binding->records)
            {
                auto *record = model->find_record(record_id);
                ColumnChildren children;
                if (record)
                {
                    const auto &field_ids = _model_binding->presenter.field_ids;
                    children.reserve(field_ids.size());
                    for (ModelFieldID field_id : field_ids)
                        if (auto *widget = present_model_field(*_model_binding, *record, field_id))
                            children.push_back(widget);
                }
                add_column(std::move(children));
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
        return _columns[index]->child_layouts;
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
            if (!slot) continue;
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
        _row_heights.clear();

        amal::vec2 content_required{0.0f, 0.0f};
        f32 max_column_width = 0.0f;
        size_t column_count = 0u;
        bool has_column = false;
        for (auto *slot : _columns)
        {
            if (!slot) continue;
            slot->inline_spacing = spacing;
            slot->update_layout_min_size();
            const amal::vec2 slot_required = slot->required_size();
            if (has_column) content_required.x += spacing;
            content_required.x += slot_required.x;
            max_column_width = amal::max(max_column_width, slot_required.x);
            _column_widths.push_back(slot_required.x);
            for (size_t i = 0; i < slot->children.size(); ++i)
            {
                auto *child = slot->children[i];
                if (!child) continue;
                while (i >= _row_heights.size()) _row_heights.push_back(0.0f);
                _row_heights[i] = amal::max(_row_heights[i], child->required_size().y);
            }
            has_column = true;
            ++column_count;
        }
        if (!is_width_fixed() && column_count > 0u)
            content_required.x = max_column_width * static_cast<f32>(column_count) +
                                 spacing * static_cast<f32>(column_count - 1u);
        for (size_t i = 0; i < _row_heights.size(); ++i)
        {
            if (i > 0u) content_required.y += spacing;
            content_required.y += _row_heights[i];
        }

        f32 required_width = 0.0f;
        if (is_size_concrete(requested_size().x)) required_width = amal::max(requested_size().x, 0.0f);
        else if (!fill_width()) required_width = content_required.x + padding.x + padding.z;
        if (required_width > 0.0f)
            required_width = amal::max(required_width, content_required.x + padding.x + padding.z);

        f32 required_height = 0.0f;
        if (is_size_concrete(requested_size().y)) required_height = amal::max(requested_size().y, 0.0f);
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

        amal::vec2 outer_size = size();
        if (!is_width_fixed()) outer_size.x = amal::max(outer_size.x - margin.x - margin.z, required_inner.x);
        else if (outer_size.x <= 0.0f) outer_size.x = required_inner.x;
        if (outer_size.y <= 0.0f) outer_size.y = required_inner.y;
        if (!is_height_fixed()) { outer_size.y = amal::max(outer_size.y, required_inner.y); }

        set_position(outer_pos);
        set_layout_size(outer_size);
        Widget::update_layout(true);
        update_column_clip_rects();

        const amal::vec2 inner_pos = outer_pos + amal::vec2{padding.x, padding.y};
        const amal::vec2 inner_size = {amal::max(outer_size.x - padding.x - padding.z, 0.0f),
                                       amal::max(outer_size.y - padding.y - padding.w, 0.0f)};
        size_t visible_column_count = 0;
        for (auto *slot : _columns)
        {
            if (!slot) continue;
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
            if (!slot) continue;
            const f32 column_width = is_width_fixed()
                                         ? (width_index < _column_widths.size() ? _column_widths[width_index] : 0.0f)
                                         : stretch_column_width;
            slot->inline_spacing = spacing;
            slot->set_position({cursor_x, inner_pos.y});
            slot->set_layout_size({column_width, inner_size.y});
            slot->update_layout_with_rows(true, _row_heights);
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
        for (auto *slot : _columns)
            if (slot) slot->reset_clip_rect_records();
    }

    void Column::rebuild_clip_rects()
    {
        _rect.clip_id = 0xFFFFu;
        update_column_clip_rects();
        for (auto *slot : _columns)
        {
            if (!slot) continue;
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
            if (!slot) continue;
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
        if (!(widget_flags & WidgetFlagBits::visible)) return;
        const amal::vec4 content_clip = get_content_clip_rect();
        for (auto *slot : _columns)
        {
            if (!slot) continue;
            DrawCtx slot_ctx = ctx;
            slot_ctx.is_hit_allowed = false;
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
            if (slot) slot->on_attach();
        }
    }

    void Column::on_detach()
    {
        for (auto *slot : _columns)
        {
            if (slot) slot->on_detach();
        }
        Widget::on_detach();
    }

    void Column::add_slot(ColumnChildren children)
    {
        auto *slot = acul::alloc<Slot>(this, std::move(children));
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
                if (child) child->set_clip_id(slot->clip_id());
        }
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

            auto *column = acul::alloc<Column>(common.id, Column::ColumnItems{}, common.requested_size,
                                               WidgetFlags(common.widget_flags), nullptr, style_tag);
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
