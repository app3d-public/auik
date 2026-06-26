#include <auik/auik.hpp>
#include <auik/detail/vertex_draw.hpp>
#include <auik/pipelines.hpp>
#include <auik/widgets/image.hpp>
#include "../core/session_stream_utils.hpp"

namespace auik
{
    void Image::update_layout_min_size()
    {
        set_required_size({is_size_concrete(requested_size().x) ? requested_size().x : 0.0f,
                           is_size_concrete(requested_size().y) ? requested_size().y : 0.0f});
    }

    void Image::update_layout(bool min_size_known)
    {
        if (!min_size_known) update_layout_min_size();
        const amal::vec2 layout_origin = position();
        set_position(layout_origin);
        set_layout_size(required_size());
        Widget::update_layout(true);
        assert(parent() && "Image must have parent");
        set_clip_id(parent()->content_clip_id());
    }

    void Image::rebuild_clip_rects()
    {
        assert(parent() && "Image must have parent");
        set_clip_id(parent()->content_clip_id());
    }

    void Image::reset_draw_records()
    {
        _image = {};
    }

    void Image::draw(DrawCtx &ctx)
    {
        auto *textured_quads_stream = get_primary_textured_quads_stream();
        if (!textured_quads_stream || _texture_id.handle == 0) return;

        if ((detail::get_context().dirty_flags & DirtyFlagBits::textures) ||
            _texture_id.bind_slot == AUIK_INVALID_DRAW_DATA_ID)
            _texture_id.bind_slot = get_texture_bind_slot(_texture_id.handle);

        if (_texture_id.bind_slot == AUIK_INVALID_DRAW_DATA_ID) return;
        assert(_texture_id.bind_slot <= 0xFFFFu && "AUIK Image texture slot overflow");

        TexturesInstanceData image_data{};
        image_data.rect = bounds();
        image_data.tint_color = detail::pack_rgba8(255, 255, 255, 255);
        image_data.uv_rect = _uv_rect;
        image_data.z_order = get_z_order();
        image_data.texture_id = static_cast<u16>(_texture_id.bind_slot);
        image_data.clip_id = clip_id();
        image_data.flags = _coverage_mode ? AUIK_TEXTURE_INSTANCE_TEXT_BIT : 0u;
        emit_context_draw(ctx, textured_quads_stream, _image, &image_data, get_rect(), false);
    }

    void Image::on_detach()
    {
        auto &cache = detail::get_context().image_cache;
        auto it = cache.find(id());
        if (it != cache.end() && it->second == this) cache.erase(it);
        Widget::on_detach();
    }

    StyleUpdateFlags CheckerImage::update_style()
    {
        const u32 parent_id = parent() ? parent()->id() : 0u;
        return resolve_style_selector(_style, id(), parent_id, style_state());
    }

    void CheckerImage::update_layout_min_size()
    {
        set_required_size({is_size_concrete(requested_size().x) ? requested_size().x : 0.0f,
                           is_size_concrete(requested_size().y) ? requested_size().y : 0.0f});
    }

    void CheckerImage::update_layout(bool min_size_known)
    {
        if (!min_size_known) update_layout_min_size();
        const amal::vec2 layout_origin = position();
        set_position(layout_origin);
        set_layout_size(required_size());
        Widget::update_layout(true);
        assert(parent() && "CheckerImage must have parent");
        set_clip_id(parent()->content_clip_id());
    }

    void CheckerImage::rebuild_clip_rects()
    {
        assert(parent() && "CheckerImage must have parent");
        set_clip_id(parent()->content_clip_id());
    }

    void CheckerImage::reset_draw_records()
    {
        _checker = {};
    }

    void CheckerImage::draw(DrawCtx &ctx)
    {
        auto *quads_stream = get_primary_quads_stream();
        if (!quads_stream) return;

        const auto &style = get_theme()->get_style(_style.id);
        QuadsInstanceData data{};
        data.rect = bounds();
        data.background_color = 0u;
        data.border_radius = detail::clamp_corner_rounding(data.rect, style.border_radius(), style.corner_mask());
        data.z_order = get_z_order();
        u32 flags = AUIK_HAS_CHECKER_BIT;
        if (data.border_radius > 0.0f && style.corner_mask() != 0u) flags |= AUIK_HAS_RADIUS_BIT;
        data.mask = static_cast<u32>(clip_id()) | ((style.corner_mask() & 0xFu) << 16u) | (flags << 20u);
        emit_context_draw(ctx, quads_stream, _checker, &data, get_rect(), false);
    }

    namespace
    {
        void write_image(acul::bin_stream &stream, umbf::Block *block)
        {
            auto *widget = static_cast<Image *>(block);
            detail::write_widget_common_data(stream, *widget);
            stream.write(widget->coverage_mode());
        }

        umbf::Block *read_image(acul::bin_stream &stream)
        {
            const auto common = detail::read_widget_common_data(stream);
            bool coverage_mode = false;
            stream.read(coverage_mode);

            auto *widget = acul::alloc<Image>(common.id, AUIK_INVALID_TEXTURE_ID, common.requested_size,
                                              amal::rect{{0.0f, 0.0f}, {1.0f, 1.0f}},
                                              WidgetFlags(common.widget_flags));
            widget->set_coverage_mode(coverage_mode);
            detail::apply_widget_common_data(widget, common);
            return widget;
        }

        void write_checker_image(acul::bin_stream &stream, umbf::Block *block)
        {
            auto *widget = static_cast<CheckerImage *>(block);
            detail::write_widget_common_data(stream, *widget);
            stream.write(widget->style_tag());
        }

        umbf::Block *read_checker_image(acul::bin_stream &stream)
        {
            const auto common = detail::read_widget_common_data(stream);
            u32 style_tag = AUIK_STYLE_TAG_GRADIENT_SLIDER;
            stream.read(style_tag);

            auto *widget = acul::alloc<CheckerImage>(common.id, common.requested_size, style_tag,
                                                     WidgetFlags(common.widget_flags));
            detail::apply_widget_common_data(widget, common);
            return widget;
        }
    } // namespace

    namespace streams
    {
        AUIK_EXPORT const umbf::streams::Stream image{read_image, write_image};
        AUIK_EXPORT const umbf::streams::Stream checker_image{read_checker_image, write_checker_image};
    } // namespace streams
} // namespace auik
