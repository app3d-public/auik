#pragma once

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

    using PFN_push_data_to_stream = DrawDataID (*)(DrawStream *, const void *, u32);
    using PFN_update_stream_data = void (*)(DrawStream *, DrawDataID, const void *, u32);
    using PFN_clear_stream = void (*)(DrawStream *, u32);
    using PFN_copy_stream_frame_data = void (*)(DrawStream *, u32, u32);
    using PFN_sync_stream_cache = bool (*)(DrawStream *, void *, GPUContext *, u32);
    using PFN_render_stream = void (*)(DrawStream *, void *, GPUContext *, u32);
    using PFN_create_stream_gpu_data = void *(*)(u32, GPUContext *);
    using PFN_destroy_stream_gpu_data = void (*)(DrawStream *);

    struct StreamGPUDispatch
    {
        PFN_push_data_to_stream push_data_to_stream = nullptr;
        PFN_update_stream_data update_stream_data = nullptr;
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
        StreamGPUDispatch quads{};
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
