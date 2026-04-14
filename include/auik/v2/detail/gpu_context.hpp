#pragma once

#include <cstddef>
#include <acul/api.hpp>
#include <amal/vector.hpp>
#include "events.hpp"
#include "fwd.hpp"

namespace auik::v2::detail
{
    using PFN_destroy_gpu_context = void (*)(GPUContext *);
    using PFN_push_clip_rect = u16 (*)(GPUContext *, const amal::vec4 &);
    using PFN_reset_clip_rects = void (*)(GPUContext *);
    using PFN_update_clip_rect = void (*)(GPUContext *, u16, const amal::vec4 &);
    using PFN_get_clip_rect = amal::vec4 *(*)(GPUContext *, u16);
    using PFN_copy_rects_frame = void (*)(GPUContext *, u32, u32);
    using PFN_push_hit_rect = u32 (*)(GPUContext *, const RectData &);
    using PFN_update_hit_rect = void (*)(GPUContext *, u32, const RectData &);
    using PFN_clear_hit_rects = void (*)(GPUContext *);
    using PFN_update_hover_id = void (*)(GPUContext *, void *);
    using PFN_create_gpu_resources = bool (*)(GPUContext *);
    struct AtlasTextureResource
    {
        void *handle = nullptr;
        TextureID texture_id = AUIK_INVALID_TEXTURE_ID;
        u32 width = 0;
        u32 height = 0;
    };
    struct ImageTextureResource
    {
        void *handle = nullptr;
        TextureID texture_id = AUIK_INVALID_TEXTURE_ID;
        u32 width = 0;
        u32 height = 0;
    };
    using PFN_create_atlas_texture = bool (*)(GPUContext *, AtlasTextureResource *, const umbf::Image2D &);
    using PFN_destroy_atlas_texture = void (*)(GPUContext *, AtlasTextureResource *);
    using PFN_upload_atlas_texture =
        bool (*)(GPUContext *, AtlasTextureResource *, const umbf::Image2D &, u32, u32, i32, i32);
    using PFN_create_image_texture = bool (*)(GPUContext *, ImageTextureResource *, const umbf::Image2D &);
    using PFN_destroy_image_texture = void (*)(GPUContext *, ImageTextureResource *);

    using PFN_push_data_to_stream = DrawDataID (*)(DrawStream *, const void *, u32);
    using PFN_push_data_batch_to_stream = void (*)(DrawStream *, const void *, u32, DrawDataID *, u32);
    using PFN_update_stream_data = void (*)(DrawStream *, DrawDataID, const void *, u32);
    using PFN_update_stream_data_batch = void (*)(DrawStream *, const DrawDataID *, const void *, u32, u32);
    using PFN_clear_stream = void (*)(DrawStream *, u32);
    using PFN_copy_stream_frame_data = void (*)(DrawStream *, u32, u32);
    using PFN_sync_stream_cache = bool (*)(DrawStream *, void *, GPUContext *, u32);
    using PFN_render_stream = void (*)(DrawStream *, void *, GPUContext *, u32);
    using PFN_create_stream_gpu_data = void *(*)(u32, GPUContext *);
    using PFN_destroy_stream_gpu_data = void (*)(DrawStream *);

    struct StreamGPUDispatch
    {
        PFN_push_data_to_stream push_data_to_stream = nullptr;
        PFN_push_data_batch_to_stream push_data_batch_to_stream = nullptr;
        PFN_update_stream_data update_stream_data = nullptr;
        PFN_update_stream_data_batch update_stream_data_batch = nullptr;
        PFN_clear_stream clear_stream = nullptr;
        PFN_copy_stream_frame_data copy_stream_frame_data = nullptr;
        PFN_sync_stream_cache sync_stream_cache = nullptr;
        PFN_render_stream render_stream = nullptr;
        PFN_create_stream_gpu_data create_stream_gpu_data = nullptr;
        PFN_destroy_stream_gpu_data destroy_stream_gpu_data = nullptr;
    };

    struct GPUContext
    {
        PFN_create_gpu_resources create_resources = nullptr;
        PFN_destroy_gpu_context destroy_context = nullptr;
        PFN_push_clip_rect push_clip_rect = nullptr;
        PFN_reset_clip_rects reset_clip_rects = nullptr;
        PFN_update_clip_rect update_clip_rect = nullptr;
        PFN_get_clip_rect get_clip_rect = nullptr;
        PFN_copy_rects_frame copy_clip_rects_frame = nullptr;
        PFN_push_hit_rect push_hit_rect = nullptr;
        PFN_update_hit_rect update_hit_rect = nullptr;
        PFN_clear_hit_rects clear_hit_rects = nullptr;
        PFN_copy_rects_frame copy_hit_rects_frame = nullptr;
        PFN_update_hover_id update_hover_id = nullptr;
        PFN_create_atlas_texture create_atlas_texture = nullptr;
        PFN_destroy_atlas_texture destroy_atlas_texture = nullptr;
        PFN_upload_atlas_texture upload_atlas_texture = nullptr;
        PFN_create_image_texture create_image_texture = nullptr;
        PFN_destroy_image_texture destroy_image_texture = nullptr;
        StreamGPUDispatch quads{};
        StreamGPUDispatch textures{};
        StreamGPUDispatch vertex_stream{};
    };

    inline u32 push_hit_rect(GPUContext *gpu_context, const RectData &rect)
    {
        return gpu_context->push_hit_rect(gpu_context, rect);
    }

    inline void update_hit_rect(GPUContext *gpu_context, u32 id, const RectData &rect)
    {
        gpu_context->update_hit_rect(gpu_context, id, rect);
    }

    inline void update_hover_id(GPUContext *gpu_context, void *sync_ctx)
    {
        gpu_context->update_hover_id(gpu_context, sync_ctx);
    }

    inline bool create_gpu_resources(GPUContext *gpu_context) { return gpu_context->create_resources(gpu_context); }

    inline bool create_atlas_texture(GPUContext *gpu_context, AtlasTextureResource *resource, const umbf::Image2D &image)
    {
        assert(gpu_context->create_atlas_texture);
        return gpu_context->create_atlas_texture(gpu_context, resource, image);
    }

    inline void destroy_atlas_texture(GPUContext *gpu_context, AtlasTextureResource *resource)
    {
        if (!resource || !gpu_context->destroy_atlas_texture) return;
        gpu_context->destroy_atlas_texture(gpu_context, resource);
    }

    inline bool upload_atlas_texture(GPUContext *gpu_context, AtlasTextureResource *resource, const umbf::Image2D &image,
                                     u32 width, u32 height, i32 x, i32 y)
    {
        assert(gpu_context->upload_atlas_texture);
        return gpu_context->upload_atlas_texture(gpu_context, resource, image, width, height, x, y);
    }

    inline bool create_image_texture(GPUContext *gpu_context, ImageTextureResource *resource, const umbf::Image2D &image)
    {
        assert(gpu_context->create_image_texture);
        return gpu_context->create_image_texture(gpu_context, resource, image);
    }

    inline void destroy_image_texture(GPUContext *gpu_context, ImageTextureResource *resource)
    {
        if (!resource || !gpu_context->destroy_image_texture) return;
        gpu_context->destroy_image_texture(gpu_context, resource);
    }

    inline void destroy_gpu_context(GPUContext *gpu_context)
    {
        if (gpu_context) gpu_context->destroy_context(gpu_context);
    }

    inline void copy_clip_rects_frame(GPUContext *gpu_context, u32 frame_id, u32 master_id)
    {
        assert(gpu_context->copy_clip_rects_frame);
        gpu_context->copy_clip_rects_frame(gpu_context, frame_id, master_id);
    }

    inline void copy_hit_rects_frame(GPUContext *gpu_context, u32 frame_id, u32 master_id)
    {
        assert(gpu_context->copy_hit_rects_frame);
        gpu_context->copy_hit_rects_frame(gpu_context, frame_id, master_id);
    }
} // namespace auik::v2::detail
