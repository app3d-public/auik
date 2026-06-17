#pragma once

#include "../theme.hpp"
#include "image.hpp"
#include "widget.hpp"

#define AUIK_TAG_IMAGE_BUTTON 0xF669C205u

namespace auik
{
    constexpr inline WidgetFlags get_default_image_button_flags()
    {
        return get_default_widget_flags() | WidgetFlagBits::hittable;
    }

    class ImageButton final : public Widget
    {
    public:
        AUIK_EXPORT ImageButton(u32 id, TextureID texture_id, amal::vec2 image_size = {0.0f, 0.0f},
                                amal::vec2 size = {0.0f, 0.0f}, amal::rect uv_rect = {{0.0f, 0.0f}, {1.0f, 1.0f}},
                                WidgetFlags widget_flags = get_default_image_button_flags(), Widget *parent = nullptr,
                                u32 style_tag = AUIK_STYLE_TAG_IMAGE_BUTTON);
        AUIK_EXPORT ImageButton(u32 id, Image *image, amal::vec2 image_size = {0.0f, 0.0f},
                                amal::vec2 size = {0.0f, 0.0f},
                                WidgetFlags widget_flags = get_default_image_button_flags(), Widget *parent = nullptr,
                                u32 style_tag = AUIK_STYLE_TAG_IMAGE_BUTTON);

        AUIK_EXPORT StyleUpdateFlags update_style() override;
        AUIK_EXPORT void update_layout_min_size() override;
        AUIK_EXPORT void update_layout(bool min_size_known) override;
        AUIK_EXPORT void translate(const amal::vec2 &delta) override;
        AUIK_EXPORT void rebuild_clip_rects() override;
        AUIK_EXPORT void reset_draw_records() override;
        AUIK_EXPORT void update_depth(const amal::vec2 &depth_range) override;
        AUIK_EXPORT void back_hit_depth() override;
        AUIK_EXPORT void restore_hit_depth() override;
        AUIK_EXPORT void draw(DrawCtx &ctx) override;

        const TextureID &texture_id() const { return _texture_id; }
        void set_texture_id(TextureID texture_id) { _texture_id = texture_id; }

        Image *image() const { return _image; }
        void set_image(Image *image) { _image = image; }

        const amal::vec2 &image_size() const { return _image_size; }
        void set_image_size(const amal::vec2 &size) { _image_size = size; }

        const amal::vec2 &uv_size() const { return _uv_rect.size; }
        void set_uv_size(const amal::vec2 &uv_size) { _uv_rect.size = uv_size; }

        const amal::vec2 &uv_offset() const { return _uv_rect.offset; }
        void set_uv_offset(const amal::vec2 &uv_offset) { _uv_rect.offset = uv_offset; }

        bool coverage_mode() const { return _coverage_mode; }
        void set_coverage_mode(bool value) { _coverage_mode = value; }
        u32 style_tag() const { return _style.tag_id; }
        void set_style_tag(u32 tag_id)
        {
            _style = {Theme::STYLE_ID_INVALID, tag_id};
            set_rect_tag_id(tag_id);
        }

        virtual u32 signature() const override { return AUIK_TAG_IMAGE_BUTTON; }

    private:
        DrawDataID _bg{};
        DrawDataID _image_draw{};
        StyleSelector _style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_IMAGE_BUTTON};
        TextureID _texture_id{};
        Image *_image = nullptr;
        amal::rect _uv_rect{};
        detail::RectData _image_rect{};
        amal::vec2 _image_size{0.0f, 0.0f};
        amal::vec2 _content_depth_range{0.0f, 1.0f};
        bool _coverage_mode = false;

        amal::vec2 resolve_image_size() const;
        TextureID resolve_texture_id() const;
        amal::rect resolve_uv_rect() const;
        bool resolve_coverage_mode() const;
        bool has_draw_record() const;
    };

    inline ImageButton *make_image_button(u32 id, TextureID texture_id, amal::vec2 image_size = {0.0f, 0.0f},
                                          amal::vec2 size = {0.0f, 0.0f},
                                          amal::rect uv_rect = {{0.0f, 0.0f}, {1.0f, 1.0f}})
    {
        return acul::alloc<ImageButton>(id, texture_id, image_size, size, uv_rect, get_default_image_button_flags(),
                                        nullptr);
    }

    inline ImageButton *make_image_button(u32 id, Image *image, amal::vec2 image_size = {0.0f, 0.0f},
                                          amal::vec2 size = {0.0f, 0.0f})
    {
        return acul::alloc<ImageButton>(id, image, image_size, size, get_default_image_button_flags(), nullptr);
    }

    namespace streams
    {
        extern AUIK_EXPORT const umbf::streams::Stream image_button;
    }
} // namespace auik
