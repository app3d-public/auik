#pragma once

#include <acul/memory/alloc.hpp>
#include "widget.hpp"

#define AUIK_TAG_IMAGE 0x8F9A3C21

namespace auik::v2
{
    constexpr inline WidgetFlags get_default_image_flags()
    {
        return get_default_widget_flags() | WidgetFlagBits::fixed;
    }

    class APPLIB_API Image : public Widget
    {
    public:
        Image(u32 id, TextureID texture_id, amal::vec2 size, amal::vec2 uv_size, amal::vec2 uv_offset,
              Widget *parent = nullptr)
            : Widget(id, get_default_image_flags(), EventFlagBits::none, parent, {0.0f, 0.0f}, size, AUIK_TAG_IMAGE),
              _texture_id(texture_id),
              _uv_size(uv_size),
              _uv_offset(uv_offset)
        {
        }

        StyleUpdateFlags update_style() override { return StyleUpdateFlagBits::none; }
        void update_layout_min_size() override;
        void update_layout(bool min_size_known) override;
        void rebuild_clip_rects() override;
        void draw(DrawCtx &ctx) override;
        void on_detach() override;

        const TextureID &texture_id() const { return _texture_id; }
        void set_texture_id(TextureID texture_id) { _texture_id = texture_id; }

        const amal::vec2 &uv_size() const { return _uv_size; }
        void set_uv_size(const amal::vec2 &uv_size) { _uv_size = uv_size; }

        const amal::vec2 &uv_offset() const { return _uv_offset; }
        void set_uv_offset(const amal::vec2 &uv_offset) { _uv_offset = uv_offset; }

    private:
        DrawDataID _image{};
        TextureID _texture_id{};
        amal::vec2 _uv_size{1.0f, 1.0f};
        amal::vec2 _uv_offset{0.0f, 0.0f};
    };

    inline Image *make_image(u32 id, TextureID texture_id, amal::vec2 size, amal::vec2 uv_size = {1.0f, 1.0f},
                             amal::vec2 uv_offset = {0.0f, 0.0f})
    {
        return acul::alloc<Image>(id, texture_id, size, uv_size, uv_offset, nullptr);
    }

    inline void cache_image(u32 id, Image *image)
    {
        assert(image && "image is null");
        detail::get_context().image_cache[id] = image;
    }

    inline void cache_image(Image *image)
    {
        assert(image && "image is null");
        cache_image(image->id(), image);
    }

    inline Image *get_cached_image(u32 id)
    {
        assert(id > 0 && "invalid id");
        auto &cache = detail::get_context().image_cache;
        auto it = cache.find(id);
        return it == cache.end() ? nullptr : it->second;
    }

    inline bool erase_cached_image(u32 id)
    {
        if (!id) return false;
        auto &cache = detail::get_context().image_cache;
        auto it = cache.find(id);
        if (it == cache.end()) return false;
        cache.erase(it);
        return true;
    }

    inline void clear_image_cache() { detail::get_context().image_cache.clear(); }
} // namespace auik::v2
