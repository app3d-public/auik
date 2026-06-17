#include <auik/pipelines.hpp>
#include <auik/detail/depth.hpp>
#include <auik/widgets/detail/selectable.hpp>
#include <auik/widgets/image.hpp>

namespace auik::detail
{
    struct SelectableStyleScope
    {
        Selectable &widget;
        StyleID previous_id;

        explicit SelectableStyleScope(Selectable &widget)
            : widget(widget),
              previous_id(widget._style.id)
        {
            const StyleID next_id = widget.effective_layout_style_id();
            if (next_id != Theme::STYLE_ID_INVALID) widget._style.id = next_id;
        }

        ~SelectableStyleScope() { widget._style.id = previous_id; }
    };

    StyleUpdateFlags Selectable::update_style()
    {
        const u32 parent_id = parent() ? parent()->id() : 0u;
        const StyleState current_state = style_state();
        const StyleID prev_layout_style = _layout_style_id;
        auto flags = resolve_style_selector(_style, id(), parent_id, current_state);
        if (selected_style_enabled())
            flags |= resolve_style_selector(_selected_style, id(), parent_id,
                                            _selected ? current_state : _selected_style_state);
        _layout_style_id = _selected && selected_style_enabled() ? _selected_style.id : _style.id;
        const auto &style = get_theme()->get_style(_layout_style_id);
        _layout_config.size_px = round_font_px(style.text_size());
        const u32 text_color = style.text_color();
        if (_render_config.tint_color != text_color)
        {
            _render_config.tint_color = text_color;
            _instances_gpu_dirty = true;
        }
        if (prev_layout_style != Theme::STYLE_ID_INVALID && prev_layout_style != _layout_style_id)
            flags |= make_style_update_flags(get_theme()->get_style(prev_layout_style), style);
        return flags;
    }

    void Selectable::update_layout_min_size()
    {
        SelectableStyleScope scope(*this);
        Text::update_layout_min_size();
        if (selected_icon_enabled())
        {
            const auto &style = get_theme()->get_style(effective_layout_style_id());
            const f32 icon_slot = selected_icon_slot_width(style);
            auto required = required_size();
            required.x += icon_slot;
            required.y = amal::max(required.y, selected_icon_size(style).y);
            set_required_size(required);
        }
    }

    void Selectable::rebuild_clip_rects()
    {
        update_content_clip_rect();
        _bg.hit_id = AUIK_INVALID_DRAW_DATA_ID;
        _selected_bg.hit_id = AUIK_INVALID_DRAW_DATA_ID;
        _selected_icon_draw.hit_id = AUIK_INVALID_DRAW_DATA_ID;
        _selected_icon_rect.clip_id = clip_id();
    }

    void Selectable::update_layout(bool min_size_known)
    {
        SelectableStyleScope scope(*this);
        if (!fill_width() && !is_width_fixed() && size().x <= 0.0f)
        {
            set_layout_size({0.0f, size().y});
        }
        Text::update_layout(min_size_known);
        if (selected_icon_enabled())
        {
            const auto &style = get_theme()->get_style(effective_layout_style_id());
            const f32 icon_slot = selected_icon_slot_width(style);
            for (auto &instance : _instances) instance.rect.offset.x += icon_slot;
            _content_bounds.offset.x += icon_slot;
            _content_bounds.size.x = amal::max(_content_bounds.size.x - icon_slot, 0.0f);
            _instances_gpu_dirty = true;
            rebuild_selected_icon_layout(style);
        }
        update_content_clip_rect();
    }

    void Selectable::translate(const amal::vec2 &delta)
    {
        Text::translate(delta);
        update_content_clip_rect();
        _selected_icon_rect.bounds.offset += delta;
    }

    void Selectable::reset_draw_records()
    {
        Text::reset_draw_records();
        _bg = {};
        _selected_bg = {};
        _selected_icon_draw = {};
    }

    void Selectable::update_depth(const amal::vec2 &depth_range)
    {
        Widget::update_depth(depth_range);
        const amal::vec2 selected_bg_range = depth_background_range(this->depth_range());
        const amal::vec2 bg_range = depth_work_range(this->depth_range());
        const amal::vec2 text_range = depth_foreground_range(this->depth_range());
        _selected_bg_z = next_depth(selected_bg_range);
        _bg_z = next_depth(bg_range);
        _selected_icon_z = next_depth(text_range);
        const f32 text_z = next_depth(text_range);
        if (_rect.depth != text_z)
        {
            _rect.depth = text_z;
            _rect.hit_depth = _rect.depth;
            _instances_gpu_dirty = true;
        }
    }

    void Selectable::draw(DrawCtx &ctx)
    {
        auto *quads_stream = get_primary_quads_stream();
        if ((ctx.reason & DrawReasonBits::invalidate))
        {
            emit_context_draw(ctx, quads_stream, _selected_bg, nullptr, get_rect(), false);
            emit_context_draw(ctx, quads_stream, _bg, nullptr, get_rect(), false);
            emit_context_draw(ctx, get_primary_textured_quads_stream(), _selected_icon_draw, nullptr, get_rect(), false);
            DrawCtx text_ctx = ctx;
            text_ctx.is_hit_allowed = false;
            Text::draw(text_ctx);
            return;
        }

        const u16 bg_clip_id = clip_id();
        if (bg_clip_id == 0xFFFFu)
        {
            if (!(ctx.reason & DrawReasonBits::record))
            {
                DrawCtx invalidate_ctx = ctx;
                invalidate_ctx.reason |= DrawReasonBits::invalidate;
                invalidate_ctx.is_hit_allowed = false;
                draw(invalidate_ctx);
            }
            return;
        }
        QuadsInstanceData selected_bg{};
        selected_bg.rect = bounds();
        selected_bg.z_order = _selected_bg_z;
        const bool selected_visible =
            _selected && selected_style_enabled() &&
            fill_quads_instance_by_style(get_theme()->get_style(_selected_style.id), bg_clip_id, selected_bg);
        emit_quads_instance(ctx, quads_stream, _selected_bg, selected_bg, get_rect(), selected_visible, false);

        QuadsInstanceData bg{};
        bg.rect = bounds();
        bg.z_order = _bg_z;
        const bool draw_state_bg = !_selected || !selected_style_enabled();
        const bool bg_visible =
            draw_state_bg && fill_quads_instance_by_style(get_theme()->get_style(_style.id), bg_clip_id, bg);
        emit_quads_instance(ctx, quads_stream, _bg, bg, get_rect(), bg_visible, can_emit_hit(ctx));

        DrawCtx text_ctx = ctx;
        text_ctx.is_hit_allowed = false;
        set_clip_id(bg_clip_id);
        Text::draw(text_ctx);
        draw_selected_icon(ctx);
    }

    void Selectable::set_selected_style_options(const SelectableStyleOptions *options)
    {
        _selected_style_options = options;
        const u32 tag_id = options ? options->tag_id : AUIK_SELECTABLE_STYLE_NONE;
        _selected_style = {Theme::STYLE_ID_INVALID, tag_id};
        _style = {Theme::STYLE_ID_INVALID, active_item_style_tag()};
        _layout_style_id = Theme::STYLE_ID_INVALID;
        reset_draw_records();
    }

    void Selectable::ensure_selected_icon_resource()
    {
        if (!selected_icon_enabled())
        {
            _selected_icon_texture = {};
            _selected_icon_size = {0.0f, 0.0f};
            return;
        }
        if (_selected_icon_texture.handle != 0) return;
        auto *cached = get_cached_image(_selected_style_options->icon_id);
        if (!cached) return;
        _selected_icon_texture = cached->texture_id();
        _selected_icon_size = cached->size();
        _selected_icon_uv_rect = {cached->uv_offset(), cached->uv_size()};
    }

    void Selectable::rebuild_selected_icon_layout(const Style &style)
    {
        if (!selected_icon_enabled()) return;
        ensure_selected_icon_resource();
        const amal::vec4 padding = style.padding();
        const amal::vec2 icon_size = selected_icon_size(style);
        _selected_icon_rect.bounds = {{position().x + padding.x,
                                       position().y + amal::max((size().y - icon_size.y) * 0.5f, 0.0f)},
                                      icon_size};
        _selected_icon_rect.clip_id = clip_id();
        _selected_icon_rect.depth = _selected_icon_z;
        _selected_icon_rect.hit_depth = _selected_icon_z;
    }

    void Selectable::draw_selected_icon(DrawCtx &ctx)
    {
        if (!_selected || !selected_icon_enabled()) return;
        ensure_selected_icon_resource();
        auto *stream = get_primary_textured_quads_stream();
        if (!stream || _selected_icon_texture.handle == 0) return;
        if ((detail::get_context().dirty_flags & DirtyFlagBits::textures) ||
            _selected_icon_texture.bind_slot == AUIK_INVALID_DRAW_DATA_ID)
            _selected_icon_texture.bind_slot = get_texture_bind_slot(_selected_icon_texture.handle);
        if (_selected_icon_texture.bind_slot == AUIK_INVALID_DRAW_DATA_ID) return;

        TexturesInstanceData icon{};
        icon.rect = _selected_icon_rect.bounds;
        icon.uv_rect = _selected_icon_uv_rect;
        icon.tint_color = get_theme()->get_style(effective_layout_style_id()).text_color();
        icon.z_order = _selected_icon_z;
        icon.texture_id = static_cast<u16>(_selected_icon_texture.bind_slot);
        icon.clip_id = clip_id();
        icon.flags = AUIK_TEXTURE_INSTANCE_TEXT_BIT;
        emit_context_draw(ctx, stream, _selected_icon_draw, &icon, _selected_icon_rect, false);
    }
} // namespace auik::detail
