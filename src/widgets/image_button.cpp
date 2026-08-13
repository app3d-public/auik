#include <auik/auik.hpp>
#include <auik/detail/pixel_snap.hpp>
#include <auik/pipelines.hpp>
#include <auik/widgets/image_button.hpp>
#include "../core/session_stream_utils.hpp"

namespace auik
{
    namespace
    {
        static bool is_limited_content_axis(f32 size) { return size >= 0.0f; }

        static amal::vec2 fit_image_size(const amal::vec2 &image_size, const amal::vec2 &content_size)
        {
            if (image_size.x <= 0.0f || image_size.y <= 0.0f) return {0.0f, 0.0f};
            f32 scale = 1.0f;
            if (is_limited_content_axis(content_size.x))
                scale = amal::min(scale, amal::max(content_size.x, 0.0f) / image_size.x);
            if (is_limited_content_axis(content_size.y))
                scale = amal::min(scale, amal::max(content_size.y, 0.0f) / image_size.y);
            return {image_size.x * scale, image_size.y * scale};
        }

        static amal::vec2 resolve_image_button_content_size(const amal::vec2 &button_size, const amal::vec4 &padding)
        {
            return {button_size.x >= 0.0f ? amal::max(button_size.x - padding.x - padding.z, 0.0f) : -1.0f,
                    button_size.y >= 0.0f ? amal::max(button_size.y - padding.y - padding.w, 0.0f) : -1.0f};
        }

        static amal::vec2 resolve_image_button_size(const amal::vec2 &style_size, const amal::vec2 &image_size,
                                                    const amal::vec4 &padding)
        {
            amal::vec2 button_size = {is_size_concrete(style_size.x) ? style_size.x : -1.0f,
                                      is_size_concrete(style_size.y) ? style_size.y : -1.0f};
            const amal::vec2 fitted_image =
                fit_image_size(image_size, resolve_image_button_content_size(button_size, padding));
            if (button_size.x < 0.0f) button_size.x = fitted_image.x + padding.x + padding.z;
            if (button_size.y < 0.0f) button_size.y = fitted_image.y + padding.y + padding.w;
            return button_size;
        }

        static amal::vec2 align_rect_pos(const amal::vec2 &bounds_pos, const amal::vec2 &bounds_size,
                                         const amal::vec2 &rect_size, ChildLayoutFlags layout)
        {
            amal::vec2 out = bounds_pos;
            const amal::vec2 free_size = {amal::max(bounds_size.x - rect_size.x, 0.0f),
                                          amal::max(bounds_size.y - rect_size.y, 0.0f)};
            if (layout & ChildLayoutFlagBits::aright) out.x += free_size.x;
            else if (layout & ChildLayoutFlagBits::hcenter) out.x += free_size.x * 0.5f;

            if (layout & ChildLayoutFlagBits::bottom) out.y += free_size.y;
            else if (layout & ChildLayoutFlagBits::vcenter) out.y += free_size.y * 0.5f;
            return out;
        }

        static ChildLayoutFlags resolve_style_align_layout(const Style &style)
        {
            const auto *align = style.align_settings();
            return align ? ChildLayoutFlags(align->flags) : default_child_layout_flags();
        }
    } // namespace

    ImageButton::ImageButton(u32 id, TextureID texture_id, amal::vec2 image_size, amal::vec2 size, amal::rect uv_rect,
                             WidgetFlags widget_flags, u32 style_tag)
        : Widget(id, widget_flags, EventFlagBits::click, {{0.0f, 0.0f}, size}, style_tag),
          _style({Theme::STYLE_ID_INVALID, style_tag}),
          _texture_id(texture_id),
          _uv_rect(uv_rect),
          _image_rect(detail::make_rect_data(AUIK_TAG_IMAGE, AUIK_TAG_IMAGE)),
          _image_size(image_size)
    {
    }

    ImageButton::ImageButton(u32 id, Image *image, amal::vec2 image_size, amal::vec2 size, WidgetFlags widget_flags,
                             u32 style_tag)
        : ImageButton(id, AUIK_INVALID_TEXTURE_ID, image_size, size, {{0.0f, 0.0f}, {1.0f, 1.0f}}, widget_flags,
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

    bool ImageButton::resolve_coverage_mode() const { return _coverage_mode || (_image && _image->coverage_mode()); }

    void ImageButton::set_selected(bool selected)
    {
        if (_selected == selected) return;
        _selected = selected;
        mark_changed();
    }

    StyleUpdateFlags ImageButton::update_style()
    {
        const u32 parent_id = parent() ? parent()->id() : 0u;
        const auto flags = resolve_style_selector(_style, id(), parent_id, style_state());
        apply_style_layout(get_theme()->get_style(_style.id));
        return flags;
    }

    void ImageButton::update_layout_min_size_force()
    {
        const auto &style = get_theme()->get_style(_style.id);
        const amal::vec4 margin = style.margin();
        const amal::vec4 padding = style.padding();
        const amal::vec2 button_size = resolve_image_button_size(style_size(), resolve_image_size(), padding);
        set_required_size({button_size.x + margin.x + margin.z, button_size.y + margin.y + margin.w});
    }

    void ImageButton::update_layout(bool min_size_known)
    {
        if (layout_measure_required(min_size_known)) update_layout_min_size_force();

        const auto &style = get_theme()->get_style(_style.id);
        const amal::vec4 margin = style.margin();
        const amal::vec4 padding = style.padding();
        const amal::vec2 layout_origin = position();
        const amal::vec2 min_required = {amal::max(required_size().x - margin.x - margin.z, 0.0f),
                                         amal::max(required_size().y - margin.y - margin.w, 0.0f)};

        amal::vec2 widget_size = {amal::max(size().x - margin.x - margin.z, 0.0f),
                                  amal::max(size().y - margin.y - margin.w, 0.0f)};
        if (fill_width() || is_width_fixed()) widget_size.x = amal::max(widget_size.x, min_required.x);
        else widget_size.x = min_required.x;
        if (fill_height() || is_height_fixed()) widget_size.y = amal::max(widget_size.y, min_required.y);
        else widget_size.y = min_required.y;

        const amal::vec2 pos = {layout_origin.x + margin.x, layout_origin.y + margin.y};
        set_position(pos);
        set_layout_size(widget_size);
        Widget::update_layout(true);
        assert(parent() && "ImageButton must have parent");
        set_clip_id(parent()->content_clip_id());

        const amal::vec2 max_image_size = {amal::max(widget_size.x - padding.x - padding.z, 0.0f),
                                           amal::max(widget_size.y - padding.y - padding.w, 0.0f)};
        const amal::vec2 image_size = fit_image_size(resolve_image_size(), max_image_size);
        const amal::vec2 padded_image_size = {image_size.x + padding.x + padding.z,
                                              image_size.y + padding.y + padding.w};
        const ChildLayoutFlags image_layout = resolve_style_align_layout(style);
        const amal::vec2 padded_image_pos = align_rect_pos(pos, widget_size, padded_image_size, image_layout);
        _image_rect.bounds = {{padded_image_pos.x + padding.x, padded_image_pos.y + padding.y}, image_size};
        _image_rect.bounds = detail::snap_rect_offset_to_pixel_grid(_image_rect.bounds);
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
        DrawDataID *hit_ids[] = {&_bg, &_image_draw};
        invalidate_hit_rect_batch(hit_ids, 2);
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
        image_data.flags = resolve_coverage_mode() ? AUIK_TEXTURE_INSTANCE_TINT_BIT : 0u;
        emit_context_draw(ctx, textured_quads_stream, _image_draw, &image_data, _image_rect, false);
    }

    bool ImageButton::has_draw_record() const
    {
        if (_bg.render_id == AUIK_INVALID_DRAW_DATA_ID) return false;
        if (resolve_texture_id().handle != 0 && _image_draw.render_id == AUIK_INVALID_DRAW_DATA_ID) return false;
        return true;
    }

    namespace
    {
        void write_image_button(acul::bin_stream &stream, umbf::Block *block)
        {
            auto *widget = static_cast<ImageButton *>(block);
            detail::write_widget_common_data(stream, *widget);
            stream.write(widget->image_size()).write(widget->coverage_mode()).write(widget->style_tag());
        }

        umbf::Block *read_image_button(acul::bin_stream &stream)
        {
            const auto common = detail::read_widget_common_data(stream);
            amal::vec2 image_size{};
            bool coverage_mode = false;
            u32 style_tag = AUIK_STYLE_TAG_IMAGE_BUTTON;
            stream.read(image_size).read(coverage_mode).read(style_tag);

            auto *widget = acul::alloc<ImageButton>(common.id, AUIK_INVALID_TEXTURE_ID, image_size, common.inline_size,
                                                    amal::rect{{0.0f, 0.0f}, {1.0f, 1.0f}},
                                                    WidgetFlags(common.widget_flags), style_tag);
            widget->set_coverage_mode(coverage_mode);
            detail::apply_widget_common_data(widget, common);
            return widget;
        }
    } // namespace

    namespace streams
    {
        AUIK_EXPORT const umbf::streams::Stream image_button{read_image_button, write_image_button};
    } // namespace streams
} // namespace auik
