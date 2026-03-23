#include <auik/v2/auik.hpp>
#include <auik/v2/pipelines.hpp>
#include <auik/v2/widgets/image.hpp>

namespace auik::v2
{
    void Image::update_layout_min_size() { set_required_size(size()); }

    void Image::update_layout(bool min_size_known)
    {
        if (!min_size_known) update_layout_min_size();
        const amal::vec2 cursor = detail::get_context().screen_cursor;
        set_position(cursor);
        set_size(required_size());
        Widget::update_layout(true);
        inherit_parent_content_clip_rect();
        detail::get_context().screen_cursor = {cursor.x, cursor.y + size().y};
    }

    void Image::rebuild_clip_rects()
    {
        inherit_parent_content_clip_rect();
        _image = {};
    }

    void Image::draw(DrawCtx &ctx)
    {
        auto *image_stream = get_primary_image_stream();
        if (!image_stream || _texture_id.handle == 0) return;

        if ((detail::get_context().dirty_flags & DirtyFlagBits::textures) ||
            _texture_id.bind_slot == AUIK_INVALID_DRAW_DATA_ID)
            _texture_id.bind_slot = get_texture_bind_slot(_texture_id.handle);

        if (_texture_id.bind_slot == AUIK_INVALID_DRAW_DATA_ID) return;
        assert(_texture_id.bind_slot <= 0xFFFFu && "AUIK Image texture slot overflow");

        TexturesInstanceData image_data{};
        image_data.position = position();
        image_data.size = size();
        image_data.uv_size = _uv_size;
        image_data.uv_offset = _uv_offset;
        image_data.z_order = get_z_order();
        image_data.texture_id = static_cast<u16>(_texture_id.bind_slot);
        image_data.clip_id = clip_id();
        ctx.emit(image_stream, _image, &image_data, get_rect(), false);
    }

    void Image::on_detach()
    {
        auto &cache = detail::get_context().image_cache;
        auto it = cache.find(id());
        if (it != cache.end() && it->second == this) cache.erase(it);
        Widget::on_detach();
    }
} // namespace auik::v2
