#include <auik/v2/auik.hpp>
#include <auik/v2/pipelines.hpp>
#include <auik/v2/widgets/checkbox.hpp>
#include <auik/v2/widgets/image.hpp>

namespace auik::v2
{
    Checkbox::Checkbox(u32 id, bool *value, WidgetFlags widget_flags, Widget *parent)
        : Widget(id, widget_flags, EventFlagBits::click, parent, {{0.0f, 0.0f}, {0.0f, 0.0f}}, AUIK_TAG_CHECKBOX),
          _value(value),
          _checkmark_rect(detail::make_rect_data(AUIK_TAG_CHECKBOX_CHECKMARK, AUIK_TAG_CHECKBOX_CHECKMARK))
    {
        ensure_checkmark_resource();
    }

    void Checkbox::ensure_checkmark_resource()
    {
        if (_checkmark_texture.handle != 0) return;
        auto *cached = get_cached_image(AUIK_ICON_CHECKMARK);
        if (!cached) return;

        _checkmark_texture = cached->texture_id();
        _checkmark_size = cached->size();
        _checkmark_uv_rect = {cached->uv_offset(), cached->uv_size()};
    }

    amal::vec2 Checkbox::resolve_box_size(const Style &style) const
    {
        const amal::vec4 padding = style.padding();
        const amal::vec2 base_size =
            _checkmark_size.x > 0.0f && _checkmark_size.y > 0.0f ? _checkmark_size : amal::vec2{12.0f, 12.0f};
        const f32 side =
            amal::max(amal::max(base_size.x + padding.x + padding.z, base_size.y + padding.y + padding.w), 16.0f);
        return {side, side};
    }

    StyleUpdateFlags Checkbox::update_style()
    {
        const u32 parent_id = parent() ? parent()->id() : 0u;
        return resolve_style_selector(_style, id(), parent_id, style_state());
    }

    void Checkbox::update_layout_min_size()
    {
        ensure_checkmark_resource();

        const auto &style = get_theme()->get_style(_style.id);
        const amal::vec4 margin = style.margin();
        const amal::vec2 box_size = resolve_box_size(style);

        amal::vec2 min_size = size();
        if (!is_fixed()) min_size.x = 0.0f;

        if (min_size.x <= 0.0f) min_size.x = box_size.x;
        else min_size.x = amal::max(min_size.x, box_size.x);
        if (min_size.y <= 0.0f) min_size.y = box_size.y;
        else min_size.y = amal::max(min_size.y, box_size.y);

        set_required_size(
            {min_size.x + margin.x + margin.z, min_size.y + margin.y + margin.w});
    }

    void Checkbox::rebuild_checkmark_layout()
    {
        if (_checkmark_texture.handle == 0) return;
        const amal::vec2 mark_size = _checkmark_size;
        const amal::vec2 mark_pos = {_box_rect.offset.x + amal::max((_box_rect.size.x - mark_size.x) * 0.5f, 0.0f),
                                     _box_rect.offset.y + amal::max((_box_rect.size.y - mark_size.y) * 0.5f, 0.0f)};
        _checkmark_rect.bounds = {mark_pos, mark_size};
        _checkmark_rect.clip_id = clip_id();
        _checkmark_rect.depth = next_depth(_content_depth_range);
    }

    void Checkbox::update_layout(bool min_size_known)
    {
        if (!min_size_known) update_layout_min_size();

        ensure_checkmark_resource();

        const auto &style = get_theme()->get_style(_style.id);
        const amal::vec4 margin = style.margin();
        const amal::vec2 layout_origin = position();
        const amal::vec2 min_required = {amal::max(required_size().x - margin.x - margin.z, 0.0f),
                                         amal::max(required_size().y - margin.y - margin.w, 0.0f)};

        amal::vec2 widget_size = size();
        widget_size.x = amal::max(widget_size.x, min_required.x);
        widget_size.y = amal::max(widget_size.y, min_required.y);

        const amal::vec2 pos = {layout_origin.x + margin.x, layout_origin.y + margin.y};
        set_position(pos);
        set_size(widget_size);
        Widget::update_layout(true);
        assert(parent() && "Checkbox must have parent");
        set_clip_id(parent()->content_clip_id());

        const amal::vec2 box_size = resolve_box_size(style);
        const f32 outer_h = widget_size.y + margin.y + margin.w;
        _box_rect.offset = {pos.x + amal::max((widget_size.x - box_size.x) * 0.5f, 0.0f),
                            layout_origin.y + amal::max((outer_h - box_size.y) * 0.5f, 0.0f)};
        _box_rect.size = box_size;

        rebuild_checkmark_layout();
    }

    void Checkbox::translate(const amal::vec2 &delta)
    {
        if (delta.x == 0.0f && delta.y == 0.0f) return;
        Widget::translate(delta);
        _box_rect.offset += delta;
        _checkmark_rect.bounds.offset += delta;
    }

    void Checkbox::rebuild_clip_rects()
    {
        assert(parent() && "Checkbox must have parent");
        set_clip_id(parent()->content_clip_id());
        _box_bg.hit_id = AUIK_INVALID_DRAW_DATA_ID;
        _checkmark_draw.hit_id = AUIK_INVALID_DRAW_DATA_ID;
        _checkmark_rect.clip_id = clip_id();
    }

    void Checkbox::update_depth(const amal::vec2 &depth_range)
    {
        Widget::update_depth(depth_range);
        assign_next_depth(this->depth_range(), _box_depth_range);
        assign_next_depth(_box_depth_range, _content_depth_range);
        _checkmark_rect.depth = next_depth(_content_depth_range);
    }

    void Checkbox::draw(DrawCtx &ctx)
    {
        auto *quads_stream = get_primary_quads_stream();

        QuadsInstanceData box_data{};
        box_data.rect = _box_rect;
        box_data.z_order = next_depth(_box_depth_range);
        const bool box_visible = fill_quads_instance_by_style(get_theme()->get_style(_style.id), clip_id(), box_data);
        auto hit_rect = get_rect();
        hit_rect.bounds = _box_rect;
        emit_quads_instance(ctx, quads_stream, _box_bg, box_data, hit_rect, box_visible, ctx.emit_hit_rect);

        ensure_checkmark_resource();
        auto *textured_quads_stream = get_primary_textured_quads_stream();
        if (textured_quads_stream && _checkmark_texture.handle != 0)
        {
            if ((detail::get_context().dirty_flags & DirtyFlagBits::textures) ||
                _checkmark_texture.bind_slot == AUIK_INVALID_DRAW_DATA_ID)
                _checkmark_texture.bind_slot = get_texture_bind_slot(_checkmark_texture.handle);

            if (_checkmark_texture.bind_slot != AUIK_INVALID_DRAW_DATA_ID)
            {
                TexturesInstanceData checkmark_data{};
                checkmark_data.rect = _checkmark_rect.bounds;
                checkmark_data.tint_color = value() ? get_theme()->get_style(_style.id).text_color() : 0;
                checkmark_data.uv_rect = _checkmark_uv_rect;
                checkmark_data.z_order = _checkmark_rect.depth;
                checkmark_data.texture_id = static_cast<u16>(_checkmark_texture.bind_slot);
                checkmark_data.clip_id = clip_id();
                checkmark_data.flags = AUIK_TEXTURE_INSTANCE_TEXT_BIT;
                ctx.emit(textured_quads_stream, _checkmark_draw, &checkmark_data, _checkmark_rect, false);
            }
        }
    }

    bool Checkbox::has_draw_record() const
    {
        if (_box_bg.render_id == AUIK_INVALID_DRAW_DATA_ID) return false;
        if (_checkmark_texture.handle != 0 && _checkmark_draw.render_id == AUIK_INVALID_DRAW_DATA_ID) return false;
        return true;
    }

    void Checkbox::set_value(bool new_value)
    {
        if (!_value || *_value == new_value) return;
        *_value = new_value;
        dispatch_change();
        redraw_external(has_draw_record());
    }

    void Checkbox::on_click(MouseKey key, KeyPressState state, u32 click_count)
    {
        (void)click_count;
        if (key != MouseKey::left || state != KeyPressState::press) return;
        if (!_value) return;
        *_value = !*_value;
        dispatch_change();
        add_render_command<detail::ClickEventTraits>(this, [this]() { redraw_external(has_draw_record()); });
    }
} // namespace auik::v2
