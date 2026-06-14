#include <auik/auik.hpp>
#include <auik/pipelines.hpp>
#include <auik/widgets/image_button.hpp>

namespace auik
{
    ImageButton::ImageButton(u32 id, TextureID texture_id, amal::vec2 image_size, amal::vec2 size, amal::rect uv_rect,
                             WidgetFlags widget_flags, Widget *parent, u32 style_tag)
        : Widget(id, widget_flags, EventFlagBits::click, parent, {{0.0f, 0.0f}, size}, style_tag),
          _style({Theme::STYLE_ID_INVALID, style_tag}),
          _texture_id(texture_id),
          _uv_rect(uv_rect),
          _image_rect(detail::make_rect_data(AUIK_TAG_IMAGE, AUIK_TAG_IMAGE)),
          _image_size(image_size)
    {
    }

    ImageButton::ImageButton(u32 id, Image *image, amal::vec2 image_size, amal::vec2 size, WidgetFlags widget_flags,
                             Widget *parent, u32 style_tag)
        : ImageButton(id, AUIK_INVALID_TEXTURE_ID, image_size, size, {{0.0f, 0.0f}, {1.0f, 1.0f}}, widget_flags, parent,
                      style_tag)
    {
        _image = image;
    }

    amal::vec2 ImageButton::resolve_image_size() const
    {
        amal::vec2 fallback = _image ? _image->size() : amal::vec2{0.0f, 0.0f};
        return {_image_size.x > 0.0f ? _image_size.x : fallback.x, _image_size.y > 0.0f ? _image_size.y : fallback.y};
    }

    TextureID ImageButton::resolve_texture_id() const { return _image ? _image->texture_id() : _texture_id; }

    amal::rect ImageButton::resolve_uv_rect() const
    {
        return _image ? amal::rect{_image->uv_offset(), _image->uv_size()} : _uv_rect;
    }

    bool ImageButton::resolve_coverage_mode() const { return _image ? _image->coverage_mode() : _coverage_mode; }

    StyleUpdateFlags ImageButton::update_style()
    {
        const u32 parent_id = parent() ? parent()->id() : 0u;
        return resolve_style_selector(_style, id(), parent_id, style_state());
    }

    void ImageButton::update_layout_min_size()
    {
        const auto &style = get_theme()->get_style(_style.id);
        const amal::vec4 margin = style.margin();
        const amal::vec4 padding = style.padding();
        const amal::vec2 image_size = resolve_image_size();
        const amal::vec2 content_required = {image_size.x + padding.x + padding.z,
                                             image_size.y + padding.y + padding.w};

        amal::vec2 min_size = {is_size_concrete(requested_size().x) ? requested_size().x : 0.0f,
                               is_size_concrete(requested_size().y) ? requested_size().y : 0.0f};
        if (fill_width()) min_size.x = content_required.x;
        if (fill_height()) min_size.y = content_required.y;
        if (is_fixed() && min_size.x <= 0.0f && min_size.y <= 0.0f)
        {
            const f32 side = amal::max(content_required.x, content_required.y);
            min_size = {side, side};
        }
        else if (!is_fixed()) min_size.x = 0.0f;

        if (min_size.x <= 0.0f) min_size.x = content_required.x;
        else min_size.x = amal::max(min_size.x, content_required.x);
        if (min_size.y <= 0.0f) min_size.y = content_required.y;
        else min_size.y = amal::max(min_size.y, content_required.y);

        set_required_size({min_size.x + margin.x + margin.z, min_size.y + margin.y + margin.w});
    }

    void ImageButton::update_layout(bool min_size_known)
    {
        if (!min_size_known) update_layout_min_size();

        const auto &style = get_theme()->get_style(_style.id);
        const amal::vec4 margin = style.margin();
        const amal::vec4 padding = style.padding();
        const amal::vec2 layout_origin = position();
        const amal::vec2 min_required = {amal::max(required_size().x - margin.x - margin.z, 0.0f),
                                         amal::max(required_size().y - margin.y - margin.w, 0.0f)};

        amal::vec2 widget_size = size();
        if (fill_width())
            widget_size.x = amal::max(widget_size.x - margin.x - margin.z, min_required.x);
        else if (!is_fixed()) widget_size.x = amal::max(widget_size.x - margin.x - margin.z, min_required.x);
        else
            widget_size.x =
                amal::max(is_size_concrete(requested_size().x) && requested_size().x > 0.0f ? requested_size().x
                                                                                              : widget_size.x,
                          min_required.x);
        if (fill_height()) widget_size.y = amal::max(widget_size.y - margin.y - margin.w, min_required.y);
        else
            widget_size.y =
                amal::max(is_size_concrete(requested_size().y) && requested_size().y > 0.0f ? requested_size().y
                                                                                              : widget_size.y,
                          min_required.y);

        const amal::vec2 pos = {layout_origin.x + margin.x, layout_origin.y + margin.y};
        set_position(pos);
        set_layout_size(widget_size);
        Widget::update_layout(true);
        assert(parent() && "ImageButton must have parent");
        set_clip_id(parent()->content_clip_id());

        const amal::vec2 content_pos = {pos.x + padding.x, pos.y + padding.y};
        const amal::vec2 content_size = {amal::max(widget_size.x - padding.x - padding.z, 0.0f),
                                         amal::max(widget_size.y - padding.y - padding.w, 0.0f)};
        const amal::vec2 image_size = resolve_image_size();
        const amal::vec2 image_pos = {content_pos.x + amal::max((content_size.x - image_size.x) * 0.5f, 0.0f),
                                      content_pos.y + amal::max((content_size.y - image_size.y) * 0.5f, 0.0f)};
        _image_rect.bounds = {image_pos, image_size};
        _image_rect.clip_id = clip_id();
        _image_rect.depth = next_depth(_content_depth_range);
        _image_rect.hit_depth = _image_rect.depth;
    }

    void ImageButton::translate(const amal::vec2 &delta)
    {
        if (delta.x == 0.0f && delta.y == 0.0f) return;
        Widget::translate(delta);
        _image_rect.bounds.offset += delta;
    }

    void ImageButton::rebuild_clip_rects()
    {
        assert(parent() && "ImageButton must have parent");
        set_clip_id(parent()->content_clip_id());
        _bg.hit_id = AUIK_INVALID_DRAW_DATA_ID;
        _image_draw.hit_id = AUIK_INVALID_DRAW_DATA_ID;
        _image_rect.clip_id = clip_id();
    }

    void ImageButton::reset_draw_records()
    {
        _bg = {};
        _image_draw = {};
    }

    void ImageButton::update_depth(const amal::vec2 &depth_range)
    {
        Widget::update_depth(depth_range);
        assign_next_depth(this->depth_range(), _content_depth_range);
        _image_rect.depth = next_depth(_content_depth_range);
        _image_rect.hit_depth = _image_rect.depth;
    }

    void ImageButton::back_hit_depth()
    {
        Widget::back_hit_depth();
        _image_rect.hit_depth = get_rect().hit_depth;
    }

    void ImageButton::restore_hit_depth()
    {
        Widget::restore_hit_depth();
        _image_rect.hit_depth = _image_rect.depth;
    }

    void ImageButton::draw(DrawCtx &ctx)
    {
        auto *theme = get_theme();
        auto *quads_stream = get_primary_quads_stream();

        QuadsInstanceData bg_data{};
        bg_data.rect = bounds();
        bg_data.z_order = get_z_order();
        const auto &style = theme->get_style(_style.id);
        const bool bg_visible = fill_quads_instance_by_style(style, clip_id(), bg_data);
        emit_quads_instance(ctx, quads_stream, _bg, bg_data, get_rect(), bg_visible, can_emit_hit(ctx));

        auto *textured_quads_stream = get_primary_textured_quads_stream();
        TextureID texture_id = resolve_texture_id();
        if (!textured_quads_stream || texture_id.handle == 0) return;
        if ((detail::get_context().dirty_flags & DirtyFlagBits::textures) ||
            texture_id.bind_slot == AUIK_INVALID_DRAW_DATA_ID)
            texture_id.bind_slot = get_texture_bind_slot(texture_id.handle);
        if (texture_id.bind_slot == AUIK_INVALID_DRAW_DATA_ID) return;
        if (!_image) _texture_id.bind_slot = texture_id.bind_slot;
        assert(texture_id.bind_slot <= 0xFFFFu && "AUIK ImageButton texture slot overflow");

        TexturesInstanceData image_data{};
        image_data.rect = _image_rect.bounds;
        image_data.tint_color = style.text_color();
        image_data.uv_rect = resolve_uv_rect();
        image_data.z_order = _image_rect.depth;
        image_data.texture_id = static_cast<u16>(texture_id.bind_slot);
        image_data.clip_id = clip_id();
        image_data.flags = resolve_coverage_mode() ? AUIK_TEXTURE_INSTANCE_TEXT_BIT : 0u;
        emit_context_draw(ctx, textured_quads_stream, _image_draw, &image_data, _image_rect, false);
    }

    bool ImageButton::has_draw_record() const
    {
        if (_bg.render_id == AUIK_INVALID_DRAW_DATA_ID) return false;
        if (resolve_texture_id().handle != 0 && _image_draw.render_id == AUIK_INVALID_DRAW_DATA_ID) return false;
        return true;
    }
} // namespace auik
