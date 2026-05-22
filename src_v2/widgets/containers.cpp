#include <auik/v2/widgets/containers.hpp>

namespace auik::v2
{
    static amal::vec4 zero_vec4() { return {0.0f, 0.0f, 0.0f, 0.0f}; }

    const Style *Group::group_style() const
    {
        if (_style_tag_id == 0u || _style.id == Theme::STYLE_ID_INVALID) return nullptr;
        return &get_theme()->get_style(_style.id);
    }

    amal::vec4 Group::group_margin() const
    {
        const auto *style = group_style();
        return style ? style->margin() : zero_vec4();
    }

    amal::vec4 Group::group_padding() const
    {
        const auto *style = group_style();
        return style ? style->padding() : zero_vec4();
    }

    f32 Group::resolved_inline_spacing() const
    {
        auto *theme = get_theme();
        const u32 parent_id = parent() ? parent()->id() : 0u;
        const u32 style_tag = _style_tag_id != 0u ? _style_tag_id : get_rect().tag_id;
        const StyleID style_id = theme->get_resolved_style(style_tag, id(), parent_id, style_state());
        return amal::max(theme->get_style(style_id).inline_spacing(), 0.0f);
    }

    void Group::clear_children()
    {
        for (auto *child : children)
        {
            if (!child) continue;
            if (child->widget_flags & WidgetFlagBits::attachable) child->on_detach();
            acul::release(child);
        }
        children.clear();
    }

    void Group::add_child(Widget *child)
    {
        assert(child && "child is null");
        child->set_parent(this);
        child->set_focus_parent(this);
        child->update_style();
        children.push_back(child);
    }

    StyleUpdateFlags Group::update_style()
    {
        StyleUpdateFlags out = StyleUpdateFlagBits::none;
        if (_style_tag_id != 0u) out |= resolve_style_selector(_style, id(), parent() ? parent()->id() : 0u, style_state());
        for (auto *child : children)
        {
            if (!child) continue;
            out |= child->update_style();
        }
        return out;
    }

    amal::vec2 Group::compute_content_min_size()
    {
        amal::vec2 required{0.0f, 0.0f};
        for (auto *child : children)
        {
            if (!child) continue;
            child->update_layout_min_size();
            const auto child_required = child->required_size();
            required.x = amal::max(required.x, child_required.x);
            required.y = amal::max(required.y, child_required.y);
        }
        return required;
    }

    void Group::layout_children(const amal::vec2 &content_pos, const amal::vec2 &)
    {
        for (auto *child : children)
        {
            if (!child) continue;
            child->set_position(content_pos);
            child->update_layout(true);
        }
    }

    void Group::update_layout_min_size()
    {
        const amal::vec4 margin = group_margin();
        const amal::vec4 padding = group_padding();
        const amal::vec2 content_required = compute_content_min_size();
        set_required_size({margin.x + margin.z + padding.x + padding.z + content_required.x,
                           margin.y + margin.w + padding.y + padding.w + content_required.y});
    }

    void Group::update_layout(bool min_size_known)
    {
        if (!min_size_known) update_layout_min_size();

        const amal::vec4 margin = group_margin();
        const amal::vec4 padding = group_padding();
        const amal::vec2 layout_origin = position();
        const amal::vec2 inner_required = {amal::max(required_size().x - margin.x - margin.z, 0.0f),
                                           amal::max(required_size().y - margin.y - margin.w, 0.0f)};
        amal::vec2 inner_size = size();
        inner_size.x = amal::max(inner_size.x, inner_required.x);
        inner_size.y = amal::max(inner_size.y, inner_required.y);

        set_position({layout_origin.x + margin.x, layout_origin.y + margin.y});
        set_size(inner_size);
        Widget::update_layout(true);
        set_clip_id(content_clip_id());

        const amal::vec2 content_pos = position() + amal::vec2{padding.x, padding.y};
        const amal::vec2 content_size = {amal::max(size().x - padding.x - padding.z, 0.0f),
                                         amal::max(size().y - padding.y - padding.w, 0.0f)};
        layout_children(content_pos, content_size);
    }

    void Group::translate(const amal::vec2 &delta)
    {
        if (delta.x == 0.0f && delta.y == 0.0f) return;
        Widget::translate(delta);
        for (auto *child : children)
        {
            if (!child) continue;
            child->translate(delta);
        }
    }

    void Group::rebuild_clip_rects()
    {
        set_clip_id(content_clip_id());
        for (auto *child : children)
        {
            if (!child) continue;
            child->rebuild_clip_rects();
        }
    }

    void Group::reset_draw_records()
    {
        for (auto *child : children)
        {
            if (!child) continue;
            child->reset_draw_records();
        }
    }

    void Group::update_depth(const amal::vec2 &depth_range)
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

    void Group::draw(DrawCtx &ctx)
    {
        if (!is_visible()) return;
        for (auto *child : children)
        {
            if (!child) continue;
            DrawCtx child_ctx = ctx;
            child_ctx.emit_hit_rect = child->is_hittable();
            child->draw(child_ctx);
        }
    }

    void Group::on_attach()
    {
        for (auto *child : children)
            if (child && (child->widget_flags & WidgetFlagBits::attachable)) child->on_attach();
    }

    void Group::on_detach()
    {
        for (auto *child : children)
            if (child && (child->widget_flags & WidgetFlagBits::attachable)) child->on_detach();
    }

    amal::vec2 Block::compute_content_min_size()
    {
        amal::vec2 required{0.0f, 0.0f};
        for (auto *child : children)
        {
            if (!child) continue;
            child->update_layout_min_size();
            const auto child_required = child->required_size();
            required.x = amal::max(required.x, child_required.x);
            required.y += child_required.y;
        }
        return required;
    }

    void Block::layout_children(const amal::vec2 &content_pos, const amal::vec2 &)
    {
        f32 cursor_y = content_pos.y;
        for (auto *child : children)
        {
            if (!child) continue;
            child->set_position({content_pos.x, cursor_y});
            child->update_layout(true);
            cursor_y += child->required_size().y;
        }
    }

    amal::vec2 InlineBlock::compute_content_min_size()
    {
        amal::vec2 required{0.0f, 0.0f};
        bool has_child = false;
        const f32 spacing = resolved_inline_spacing();
        for (auto *child : children)
        {
            if (!child) continue;
            child->update_layout_min_size();
            const auto child_required = child->required_size();
            if (has_child) required.x += spacing;
            required.x += child_required.x;
            required.y = amal::max(required.y, child_required.y);
            has_child = true;
        }
        return required;
    }

    void InlineBlock::layout_children(const amal::vec2 &content_pos, const amal::vec2 &content_size)
    {
        f32 cursor_x = content_pos.x;
        bool has_child = false;
        const f32 spacing = resolved_inline_spacing();
        for (auto *child : children)
        {
            if (!child) continue;
            if (has_child) cursor_x += spacing;
            const auto child_required = child->required_size();
            const f32 child_y = content_pos.y + amal::max((content_size.y - child_required.y) * 0.5f, 0.0f);
            child->set_position({cursor_x, child_y});
            child->update_layout(true);
            cursor_x += child->required_size().x;
            has_child = true;
        }
    }

    f32 LabelWidget::resolve_label_width() const
    {
        if (_width_key == 0u) return _label_width;
        const f32 width = get_theme()->get_var<f32>(_width_key);
        return width > 0.0f ? width : AUIK_DEFAULT_LABEL_WIDGET_LABEL_W;
    }

    void LabelWidget::apply_label_width(f32 width)
    {
        _label_width = width;
        if (_label) _label->set_size({width, _label->size().y});
    }

    void LabelWidget::set_label_width(f32 value)
    {
        if (_label_width == value) return;
        _width_key = 0u;
        apply_label_width(value);
    }

    void LabelWidget::set_width_key(u32 key)
    {
        if (_width_key == key) return;
        _width_key = key;
        apply_label_width(resolve_label_width());
    }

    StyleUpdateFlags LabelWidget::update_style()
    {
        StyleUpdateFlags out = Block::update_style();
        const f32 next_width = resolve_label_width();
        if (next_width != _label_width)
        {
            apply_label_width(next_width);
            out |= StyleUpdateFlagBits::layout;
        }
        return out;
    }

    amal::vec2 LabelWidget::compute_content_min_size()
    {
        amal::vec2 required{0.0f, 0.0f};
        bool has_child = false;
        for (auto *child : children)
        {
            if (!child) continue;
            child->update_layout_min_size();
            const auto child_required = child->required_size();
            required.x += child_required.x;
            required.y = amal::max(required.y, child_required.y);
            has_child = true;
        }
        return has_child ? required : amal::vec2{0.0f, 0.0f};
    }

    void LabelWidget::layout_children(const amal::vec2 &content_pos, const amal::vec2 &content_size)
    {
        f32 cursor_x = content_pos.x;
        for (auto *child : children)
        {
            if (!child) continue;
            const auto child_required = child->required_size();
            const f32 child_y = content_pos.y + amal::max((content_size.y - child_required.y) * 0.5f, 0.0f);
            child->set_position({cursor_x, child_y});
            child->update_layout(true);
            cursor_x += child->required_size().x;
        }
    }

    StyleUpdateFlags Dummy::update_style()
    {
        if (_style_tag_id == 0u) return StyleUpdateFlagBits::none;
        return resolve_style_selector(_style, id(), parent() ? parent()->id() : 0u, style_state());
    }

    void Dummy::update_layout_min_size()
    {
        amal::vec4 margin{0.0f};
        amal::vec4 padding{0.0f};
        if (_style_tag_id != 0u && _style.id != Theme::STYLE_ID_INVALID)
        {
            const auto &style = get_theme()->get_style(_style.id);
            margin = style.margin();
            padding = style.padding();
        }
        set_required_size({margin.x + margin.z + padding.x + padding.z + size().x,
                           margin.y + margin.w + padding.y + padding.w + size().y});
    }

    void Dummy::update_layout(bool min_size_known)
    {
        if (!min_size_known) update_layout_min_size();
        set_size(required_size());
        Widget::update_layout(true);
    }
} // namespace auik::v2
