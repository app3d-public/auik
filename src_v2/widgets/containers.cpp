#include <auik/v2/auik.hpp>
#include <auik/v2/pipelines.hpp>
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
        const u32 style_tag = _style_tag_id != 0u ? _style_tag_id : get_rect().id.tag_id;
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

    CollapseHeader::CollapseHeader(u32 id, acul::string label, bool expanded, WidgetFlags widget_flags, Widget *parent,
                                   u32 style_tag_id)
        : Block(id, widget_flags, parent, style_tag_id, style_tag_id),
          _style({Theme::STYLE_ID_INVALID, style_tag_id}),
          _expanded(expanded)
    {
        add_event_flags(EventFlagBits::click);
        set_rect_tag_id(current_header_style_tag());
        _label = acul::alloc<Text>(AUIK_TAG_TEXT, label, amal::vec2{0.0f, 0.0f},
                                   WidgetFlagBits::visible | WidgetFlagBits::fixed, this, AUIK_STYLE_TAG_NO_PAD,
                                   detail::TextOverflowMode::ellipsis, detail::TextVerticalAlign::center);
        _trigger = acul::alloc<detail::PopupTrigger>(_trigger_style_tag, AUIK_TAG_COLLAPSE_HEADER_TRIGGER,
                                                     AUIK_ICON_CHEVRON_RIGHT, AUIK_ICON_CHEVRON_DOWN, true,
                                                     amal::half_pi<f32>());
        _trigger->set_owner(this);
        _trigger->set_open(_expanded);
        _header_rect = detail::make_rect_data(id, current_header_style_tag());
        _content_rect = detail::make_rect_data(id, _content_style.tag_id);
    }

    CollapseHeader::~CollapseHeader()
    {
        if (_label)
        {
            acul::release(_label);
            _label = nullptr;
        }
        if (_trigger)
        {
            acul::release(_trigger);
            _trigger = nullptr;
        }
    }

    void CollapseHeader::set_label(acul::string value)
    {
        if (!_label || _label->text() == value) return;
        _label->set_text(value);
        invalidate_layout();
    }

    const acul::string &CollapseHeader::label() const
    {
        static const acul::string empty;
        return _label ? _label->text() : empty;
    }

    void CollapseHeader::set_expanded(bool value)
    {
        if (_expanded == value) return;
        _expanded = value;
        _style.id = Theme::STYLE_ID_INVALID;
        if (_expanded) _content_style.id = Theme::STYLE_ID_INVALID;
        set_required_size({0.0f, 0.0f});
        set_rect_tag_id(current_header_style_tag());
        _header_rect.id.tag_id = current_header_style_tag();
        if (!_expanded)
        {
            for (auto *child : children)
                if (child) child->invalidate_draw_commands(DrawReasonBits::layout);
            if (_content_bg.render_id != AUIK_INVALID_DRAW_DATA_ID)
            {
                if (auto *stream = get_primary_quads_stream(); stream && stream->invalidate_data_in_stream)
                    stream->invalidate_data_in_stream(stream, _content_bg);
                _content_bg = {};
            }
        }
        if (_trigger)
        {
            _trigger->set_open(_expanded);
            _trigger->start_icon_animation(_expanded);
        }
        invalidate_layout();
    }

    void CollapseHeader::set_style_tag(u32 tag_id)
    {
        if (_style.tag_id == tag_id) return;
        _style = {Theme::STYLE_ID_INVALID, tag_id};
        set_rect_tag_id(current_header_style_tag());
        _header_rect.id.tag_id = current_header_style_tag();
        invalidate_layout();
    }

    void CollapseHeader::set_closed_style_tag(u32 tag_id)
    {
        if (_closed_style_tag == tag_id) return;
        _closed_style_tag = tag_id;
        if (!_expanded)
        {
            set_rect_tag_id(current_header_style_tag());
            _header_rect.id.tag_id = current_header_style_tag();
        }
        invalidate_layout();
    }

    void CollapseHeader::set_content_style_tag(u32 tag_id)
    {
        if (_content_style.tag_id == tag_id) return;
        _content_style = {Theme::STYLE_ID_INVALID, tag_id};
        _content_rect.id.tag_id = tag_id;
        invalidate_layout();
    }

    void CollapseHeader::set_trigger_style_tag(u32 tag_id)
    {
        if (_trigger_style_tag == tag_id) return;
        _trigger_style_tag = tag_id;
        if (_trigger)
        {
            acul::release(_trigger);
            _trigger = acul::alloc<detail::PopupTrigger>(_trigger_style_tag, AUIK_TAG_COLLAPSE_HEADER_TRIGGER,
                                                         AUIK_ICON_CHEVRON_RIGHT, AUIK_ICON_CHEVRON_DOWN, true,
                                                         amal::half_pi<f32>());
            _trigger->set_owner(this);
            _trigger->set_open(_expanded);
        }
        invalidate_layout();
    }

    StyleUpdateFlags CollapseHeader::update_style()
    {
        const u32 parent_id = parent() ? parent()->id() : 0u;
        const u32 header_style_tag = current_header_style_tag();
        if (get_rect().id.tag_id != header_style_tag) set_rect_tag_id(header_style_tag);
        _header_rect.id.tag_id = header_style_tag;
        StyleSelector header_style{_style.id, header_style_tag};
        StyleUpdateFlags out = resolve_style_selector(header_style, id(), parent_id, style_state());
        _style.id = header_style.id;
        if (_label) out |= _label->update_style();
        if (_trigger) out |= _trigger->update_style(id(), parent_id, style_state());
        if (_expanded) out |= resolve_style_selector(_content_style, _content_style.tag_id, id(), StyleState::normal);
        if (_expanded)
        {
            for (auto *child : children)
                if (child) out |= child->update_style();
        }
        return out;
    }

    amal::vec2 CollapseHeader::compute_content_min_size()
    {
        amal::vec2 required{0.0f, 0.0f};
        if (!_expanded) return required;
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

    void CollapseHeader::update_layout_min_size()
    {
        if (_style.id == Theme::STYLE_ID_INVALID ||
            (_expanded && _content_style.id == Theme::STYLE_ID_INVALID))
            update_style();
        const auto &style = get_theme()->get_style(_style.id);
        const amal::vec4 margin = style.margin();
        const amal::vec4 padding = style.padding();
        amal::vec4 content_margin{0.0f};
        amal::vec4 content_padding{0.0f};
        if (_expanded)
        {
            const auto &content_style = get_theme()->get_style(_content_style.id);
            content_margin = content_style.margin();
            content_padding = content_style.padding();
        }

        if (_trigger) _trigger->update_layout_min_size({0.0f, 0.0f}, true);
        if (_label) _label->update_layout_min_size();
        const amal::vec2 trigger_required = _trigger ? _trigger->required_size() : amal::vec2{0.0f, 0.0f};
        const amal::vec2 label_required = _label ? _label->required_size() : amal::vec2{0.0f, 0.0f};
        const f32 spacing = amal::max(style.inline_spacing(), 0.0f);
        const f32 header_w = padding.x + padding.z + trigger_required.x + spacing + label_required.x;
        const f32 header_h = padding.y + padding.w + amal::max(trigger_required.y, label_required.y);

        const amal::vec2 content_required = compute_content_min_size();
        const amal::vec2 content_outer = _expanded
                                             ? amal::vec2{content_margin.x + content_margin.z + content_padding.x +
                                                              content_padding.z + content_required.x,
                                                          content_margin.y + content_margin.w + content_padding.y +
                                                              content_padding.w + content_required.y}
                                             : amal::vec2{0.0f, 0.0f};
        const f32 resolved_w = is_fixed() ? amal::max(header_w, content_outer.x) : 0.0f;
        set_required_size({margin.x + margin.z + resolved_w, margin.y + margin.w + header_h + content_outer.y});
    }

    void CollapseHeader::update_layout(bool min_size_known)
    {
        if (!min_size_known) update_layout_min_size();
        if (_style.id == Theme::STYLE_ID_INVALID ||
            (_expanded && _content_style.id == Theme::STYLE_ID_INVALID))
            update_style();

        const auto &style = get_theme()->get_style(_style.id);
        const amal::vec4 margin = style.margin();
        const amal::vec4 padding = style.padding();
        amal::vec4 content_margin{0.0f};
        amal::vec4 content_padding{0.0f};
        if (_expanded)
        {
            const auto &content_style = get_theme()->get_style(_content_style.id);
            content_margin = content_style.margin();
            content_padding = content_style.padding();
        }
        const f32 spacing = amal::max(style.inline_spacing(), 0.0f);
        const amal::vec2 layout_origin = position();
        const amal::vec2 inner_required = {amal::max(required_size().x - margin.x - margin.z, 0.0f),
                                           amal::max(required_size().y - margin.y - margin.w, 0.0f)};
        amal::vec2 inner_size = size();
        if (is_fixed() || inner_size.x <= 0.0f) inner_size.x = amal::max(inner_size.x, inner_required.x);
        inner_size.y = amal::max(inner_size.y, inner_required.y);

        set_position({layout_origin.x + margin.x, layout_origin.y + margin.y});
        set_size(inner_size);
        Widget::update_layout(true);
        set_clip_id(content_clip_id());

        const amal::vec2 trigger_required = _trigger ? _trigger->required_size() : amal::vec2{0.0f, 0.0f};
        const amal::vec2 label_required = _label ? _label->required_size() : amal::vec2{0.0f, 0.0f};
        const f32 header_h = padding.y + padding.w + amal::max(trigger_required.y, label_required.y);
        _header_rect = detail::make_rect_data(id(), current_header_style_tag(), {position(), {size().x, header_h}},
                                              clip_id(), next_depth(depth_range()));

        const f32 content_y = position().y + padding.y;
        f32 cursor_x = position().x + padding.x;
        if (_trigger)
        {
            const f32 trigger_y = content_y + amal::max((header_h - padding.y - padding.w - trigger_required.y) * 0.5f,
                                                        0.0f);
            _trigger->update_layout({{cursor_x, trigger_y}, trigger_required}, clip_id());
            cursor_x += trigger_required.x + spacing;
        }
        if (_label)
        {
            const f32 label_y = content_y + amal::max((header_h - padding.y - padding.w - label_required.y) * 0.5f,
                                                      0.0f);
            const f32 label_w = amal::max(size().x - padding.z - (cursor_x - position().x), 0.0f);
            _label->set_position({cursor_x, label_y});
            _label->set_size({label_w, label_required.y});
            _label->update_layout(true);
        }

        if (_expanded)
        {
            const amal::vec2 content_pos{position().x + content_margin.x, position().y + header_h + content_margin.y};
            const amal::vec2 content_size{amal::max(size().x - content_margin.x - content_margin.z, 0.0f),
                                          amal::max(size().y - header_h - content_margin.y - content_margin.w, 0.0f)};
            _content_rect = detail::make_rect_data(id(), _content_style.tag_id, {content_pos, content_size}, clip_id(),
                                                   next_depth(depth_range()));
            const amal::vec2 child_pos{content_pos.x + content_padding.x, content_pos.y + content_padding.y};
            const amal::vec2 child_size{amal::max(content_size.x - content_padding.x - content_padding.z, 0.0f),
                                        amal::max(content_size.y - content_padding.y - content_padding.w, 0.0f)};
            layout_children(child_pos, child_size);
        }
    }

    void CollapseHeader::layout_children(const amal::vec2 &content_pos, const amal::vec2 &)
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

    void CollapseHeader::translate(const amal::vec2 &delta)
    {
        if (delta.x == 0.0f && delta.y == 0.0f) return;
        Widget::translate(delta);
        _header_rect.bounds.offset += delta;
        _content_rect.bounds.offset += delta;
        if (_trigger) _trigger->translate(delta);
        if (_label) _label->translate(delta);
        if (_expanded)
        {
            for (auto *child : children)
                if (child) child->translate(delta);
        }
    }

    void CollapseHeader::rebuild_clip_rects()
    {
        set_clip_id(content_clip_id());
        _header_rect.clip_id = clip_id();
        _content_rect.clip_id = clip_id();
        if (_trigger) _trigger->rebuild_clip_rects(clip_id());
        if (_label)
        {
            _label->set_clip_id(content_clip_id());
            _label->rebuild_clip_rects();
        }
        if (_expanded)
        {
            for (auto *child : children)
                if (child) child->rebuild_clip_rects();
        }
    }

    void CollapseHeader::reset_draw_records()
    {
        _header_bg = {};
        _content_bg = {};
        if (_trigger) _trigger->reset_draw_records();
        if (_label) _label->reset_draw_records();
        for (auto *child : children)
            if (child) child->reset_draw_records();
    }

    void CollapseHeader::update_depth(const amal::vec2 &depth_range)
    {
        Widget::update_depth(depth_range);
        amal::vec2 next_range = this->depth_range();
        if (_trigger)
        {
            _trigger->update_depth(next_range);
            assign_next_depth(next_range, next_range);
        }
        if (_label)
        {
            amal::vec2 label_range{};
            assign_next_depth(next_range, label_range);
            _label->update_depth(label_range);
            next_range = label_range;
        }
        for (auto *child : children)
        {
            if (!child) continue;
            amal::vec2 child_range{};
            assign_next_depth(next_range, child_range);
            child->update_depth(child_range);
            next_range = child_range;
        }
        _header_rect.depth = next_depth(this->depth_range());
        _content_rect.depth = next_depth(this->depth_range());
    }

    void CollapseHeader::draw(DrawCtx &ctx)
    {
        if (!is_visible()) return;
        auto *quads_stream = get_primary_quads_stream();
        QuadsInstanceData bg{};
        bg.rect = _header_rect.bounds;
        bg.z_order = _header_rect.depth;
        const bool bg_visible = fill_quads_instance_by_style(get_theme()->get_style(_style.id), clip_id(), bg);
        emit_quads_instance(ctx, quads_stream, _header_bg, bg, _header_rect, bg_visible, ctx.emit_hit_rect);
        if (_trigger) _trigger->draw(ctx, false);
        if (_label)
        {
            DrawCtx label_ctx = ctx;
            label_ctx.emit_hit_rect = false;
            _label->draw(label_ctx);
        }
        if (_expanded)
        {
            QuadsInstanceData content_bg{};
            content_bg.rect = _content_rect.bounds;
            content_bg.z_order = _content_rect.depth;
            const bool content_bg_visible =
                fill_quads_instance_by_style(get_theme()->get_style(_content_style.id), clip_id(), content_bg);
            emit_quads_instance(ctx, quads_stream, _content_bg, content_bg, _content_rect, content_bg_visible, false);
            for (auto *child : children)
            {
                if (!child) continue;
                DrawCtx child_ctx = ctx;
                child_ctx.emit_hit_rect = child->is_hittable();
                child->draw(child_ctx);
            }
        }
    }

    void CollapseHeader::on_click(MouseKey key, KeyPressState state, u32 click_count)
    {
        (void)click_count;
        if (key != MouseKey::left || state != KeyPressState::press) return;
        const auto hover = detail::get_context().hover_id;
        if (hover.widget_id != id() || hover.tag_id != current_header_style_tag()) return;
        add_render_command<detail::ClickEventTraits>(this, [this]() { toggle(); });
        detail::mark_host_refresh_request();
    }

    u32 CollapseHeader::current_header_style_tag() const { return _expanded ? _style.tag_id : _closed_style_tag; }

    void CollapseHeader::on_attach()
    {
        detail::get_context().id_map.emplace(id(), this);
        Block::on_attach();
    }

    void CollapseHeader::on_detach()
    {
        Block::on_detach();
        detail::get_context().id_map.erase(id());
    }

    void CollapseHeader::invalidate_layout()
    {
        auto &ctx = detail::get_context();
        ctx.dirty_flags |= DirtyFlagBits::redraw | DirtyFlagBits::hit_rect_update;
        if (auto *layout_parent = parent())
        {
            layout_parent->update_layout(false);
            layout_parent->update_draw_commands(DrawReasonBits::layout);
        }
        else
        {
            update_layout(false);
            update_draw_commands(DrawReasonBits::layout);
        }
        detail::mark_host_refresh_request();
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
