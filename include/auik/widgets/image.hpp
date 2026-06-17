#pragma once

#include "widget.hpp"

#define AUIK_TAG_IMAGE         0x8F9A3C21u
#define AUIK_TAG_CHECKER_IMAGE 0x3F7C2B91u

namespace auik
{
    constexpr inline WidgetFlags get_default_image_flags()
    {
        return get_default_widget_flags();
    }

    class Image : public Widget
    {
    public:
        Image(u32 id, TextureID texture_id, amal::vec2 size, amal::rect uv_rect = {{0.0f, 0.0f}, {1.0f, 1.0f}},
              Widget *parent = nullptr, WidgetFlags flags = get_default_image_flags())
            : Widget(id, flags, EventFlagBits::none, parent, {{0.0f}, size}, AUIK_TAG_IMAGE),
              _texture_id(texture_id),
              _uv_rect(uv_rect)
        {
        }

        StyleUpdateFlags update_style() override { return StyleUpdateFlagBits::none; }
        AUIK_EXPORT void update_layout_min_size() override;
        AUIK_EXPORT void update_layout(bool min_size_known) override;
        AUIK_EXPORT void rebuild_clip_rects() override;
        AUIK_EXPORT void draw(DrawCtx &ctx) override;
        AUIK_EXPORT void on_detach() override;

        const TextureID &texture_id() const { return _texture_id; }
        void set_texture_id(TextureID texture_id) { _texture_id = texture_id; }

        const amal::vec2 &uv_size() const { return _uv_rect.size; }
        void set_uv_size(const amal::vec2 &uv_size) { _uv_rect.size = uv_size; }

        const amal::vec2 &uv_offset() const { return _uv_rect.offset; }
        void set_uv_offset(const amal::vec2 &uv_offset) { _uv_rect.offset = uv_offset; }

        bool coverage_mode() const { return _coverage_mode; }
        void set_coverage_mode(bool value) { _coverage_mode = value; }

        virtual u32 signature() const override { return AUIK_TAG_IMAGE; }

    private:
        friend class CheckerImage;

        DrawDataID _image{};
        TextureID _texture_id;
        amal::rect _uv_rect;
        bool _coverage_mode = false;
    };

    class CheckerImage : public Widget
    {
    public:
        CheckerImage(u32 id, amal::vec2 size, u32 style_tag = AUIK_STYLE_TAG_GRADIENT_SLIDER, Widget *parent = nullptr,
                     WidgetFlags flags = WidgetFlagBits::visible)
            : Widget(id, flags, EventFlagBits::none, parent, {{0.0f}, size}, AUIK_TAG_CHECKER_IMAGE),
              _style({Theme::STYLE_ID_INVALID, style_tag})
        {
        }

        AUIK_EXPORT StyleUpdateFlags update_style() override;
        AUIK_EXPORT void update_layout_min_size() override;
        AUIK_EXPORT void update_layout(bool min_size_known) override;
        AUIK_EXPORT void rebuild_clip_rects() override;
        AUIK_EXPORT void draw(DrawCtx &ctx) override;

        u32 style_tag() const { return _style.tag_id; }
        void set_style_tag(u32 tag_id)
        {
            _style = {Theme::STYLE_ID_INVALID, tag_id};
            set_rect_tag_id(tag_id);
        }

        virtual u32 signature() const override { return AUIK_TAG_CHECKER_IMAGE; }

    private:
        DrawDataID _checker{};
        StyleSelector _style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_GRADIENT_SLIDER};
    };

    inline Image *make_image(u32 id, TextureID texture_id, amal::vec2 size,
                             amal::rect uv_rect = {{0.0f, 0.0f}, {1.0f, 1.0f}})
    {
        return acul::alloc<Image>(id, texture_id, size, uv_rect, nullptr);
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

    namespace streams
    {
        extern AUIK_EXPORT const umbf::streams::Stream image;
        extern AUIK_EXPORT const umbf::streams::Stream checker_image;
    }
} // namespace auik
