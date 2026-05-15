#include <auik/v2/auik.hpp>
#include <auik/v2/detail/rect.hpp>
#include <auik/v2/widgets/column.hpp>

namespace auik::v2
{
    class Column::Slot final : public Widget
    {
    public:
        acul::vector<Widget *> children;
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
        }

        void add_child(Widget *child)
        {
            assert(child && "child is null");
            child->set_parent(this);
            child->set_focus_parent(this);
            child->update_style();
            children.push_back(child);
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
                if (!child->is_fixed()) child->set_size({size().x, child->size().y});
                child->set_position({position().x, cursor_y});
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
            for (auto *child : children)
            {
                if (!child) continue;
                child->translate(delta);
            }
            if (clip_id() != 0xFFFFu)
            {
                auto rect = get_clip_rect(clip_id());
                rect.x += delta.x;
                rect.y += delta.y;
                update_clip_rect(clip_id(), rect);
            }
        }

        void rebuild_clip_rects() override
        {
            auto *column = static_cast<Column *>(parent());
            const amal::vec4 parent_clip = column->get_content_clip_rect();
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
            amal::vec2 next_range = this->depth_range();
            for (auto *child : children)
            {
                if (!child) continue;
                amal::vec2 child_range{};
                assign_next_depth(next_range, child_range);
                child->update_depth(child_range);
                next_range = child_range;
            }
        }

        void draw(DrawCtx &ctx) override
        {
            if (!(widget_flags & WidgetFlagBits::visible)) return;
            for (auto *child : children)
            {
                if (!child) continue;
                DrawCtx child_ctx = ctx;
                child_ctx.emit_hit_rect = child->is_hittable();
                child->draw(child_ctx);
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

    Column::~Column() { clear_columns(); }

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

    void Column::set_columns(ColumnItems columns)
    {
        clear_columns();
        for (auto &column : columns) add_slot(std::move(column));
        detail::get_context().dirty_flags |= DirtyFlagBits::layout;
    }

    void Column::add_column(ColumnChildren children)
    {
        add_slot(std::move(children));
        detail::get_context().dirty_flags |= DirtyFlagBits::layout;
    }

    void Column::add_child(size_t column_index, Widget *child)
    {
        assert(column_index < _columns.size() && "column index out of range");
        _columns[column_index]->add_child(child);
        detail::get_context().dirty_flags |= DirtyFlagBits::layout;
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

    void Column::set_style_tag(u32 tag_id)
    {
        if (_style.tag_id == tag_id) return;
        _style = {Theme::STYLE_ID_INVALID, tag_id};
        set_rect_tag_id(tag_id);
        detail::get_context().dirty_flags |= DirtyFlagBits::layout;
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

        amal::vec2 content_required{0.0f, 0.0f};
        bool has_column = false;
        for (auto *slot : _columns)
        {
            if (!slot) continue;
            slot->inline_spacing = spacing;
            slot->update_layout_min_size();
            const amal::vec2 slot_required = slot->required_size();
            if (has_column) content_required.x += spacing;
            content_required.x += slot_required.x;
            content_required.y = amal::max(content_required.y, slot_required.y);
            _column_widths.push_back(slot_required.x);
            has_column = true;
        }

        f32 required_width = size().x;
        if (!is_fixed()) required_width = 0.0f;
        else if (required_width <= 0.0f) required_width = content_required.x + padding.x + padding.z;
        if (required_width > 0.0f)
            required_width = amal::max(required_width, content_required.x + padding.x + padding.z);

        f32 required_height = size().y;
        if (required_height <= 0.0f) required_height = content_required.y + padding.y + padding.w;

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
        if (!is_fixed()) outer_size.x = amal::max(outer_size.x - margin.x - margin.z, required_inner.x);
        else if (outer_size.x <= 0.0f) outer_size.x = required_inner.x;
        if (outer_size.y <= 0.0f) outer_size.y = required_inner.y;
        if (!is_fixed()) { outer_size.y = amal::max(outer_size.y, required_inner.y); }

        set_position(outer_pos);
        set_size(outer_size);
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
            const f32 column_width = is_fixed()
                                         ? (width_index < _column_widths.size() ? _column_widths[width_index] : 0.0f)
                                         : stretch_column_width;
            slot->inline_spacing = spacing;
            slot->set_position({cursor_x, inner_pos.y});
            slot->set_size({column_width, inner_size.y});
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
        amal::vec2 next_range = this->depth_range();
        for (auto *slot : _columns)
        {
            if (!slot) continue;
            amal::vec2 slot_range{};
            assign_next_depth(next_range, slot_range);
            slot->update_depth(slot_range);
            next_range = slot_range;
        }
    }

    void Column::draw(DrawCtx &ctx)
    {
        if (!(widget_flags & WidgetFlagBits::visible)) return;
        for (auto *slot : _columns)
        {
            if (!slot) continue;
            DrawCtx slot_ctx = ctx;
            slot_ctx.emit_hit_rect = false;
            slot->draw(slot_ctx);
        }
    }

    amal::vec4 Column::get_content_clip_rect() const
    {
        if (clip_id() == 0xFFFFu) return parent() ? parent()->get_content_clip_rect() : get_main_viewport();
        return get_clip_rect(content_clip_id());
    }

    void Column::on_attach()
    {
        detail::get_context().id_map.emplace(id(), this);
        for (auto *slot : _columns)
        {
            if (slot) slot->on_attach();
        }
    }

    void Column::on_detach()
    {
        auto &map = detail::get_context().id_map;
        map.erase(id());
        for (auto *slot : _columns)
        {
            if (slot) slot->on_detach();
        }
    }

    void Column::add_slot(ColumnChildren children)
    {
        _columns.push_back(acul::alloc<Slot>(this, std::move(children)));
        _column_widths.push_back(0.0f);
    }

    void Column::update_column_clip_rects()
    {
        const amal::vec4 parent_clip = parent() ? parent()->get_content_clip_rect() : get_main_viewport();
        const amal::vec4 own_rect = {position().x, position().y, size().x, size().y};
        ensure_own_clip_rect(detail::intersect_rects(parent_clip, own_rect));

        const amal::vec4 own_clip = get_clip_rect(clip_id());
        for (auto *slot : _columns)
        {
            if (!slot) continue;
            const amal::vec4 slot_rect = {slot->position().x, slot->position().y, slot->size().x, slot->size().y};
            slot->ensure_own_clip_rect(detail::intersect_rects(own_clip, slot_rect));
            for (auto *child : slot->children)
                if (child) child->set_clip_id(slot->clip_id());
        }
    }

} // namespace auik::v2
