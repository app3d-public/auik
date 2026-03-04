#pragma once

#include <acul/api.hpp>
#include <amal/vector.hpp>
#include "events.hpp"
#include "quads_dispatch.hpp"

namespace auik::v2::detail
{
    using PFN_destroy_gpu_context = void (*)(GPUContext *);
    using PFN_push_clip_rect = u16 (*)(GPUContext *, const amal::vec4 &);
    using PFN_reset_clip_rects = void (*)(GPUContext *);
    using PFN_update_clip_rect = void (*)(GPUContext *, u16, const amal::vec4 &);
    using PFN_get_clip_rect = amal::vec4 *(*)(GPUContext *, u16);
    using PFN_push_hover_rect = u32 (*)(GPUContext *, const RectData &);
    using PFN_update_hover_rect = void (*)(GPUContext *, u32, const RectData &);
    using PFN_clear_hover_rects = void (*)(GPUContext *);
    using PFN_update_hover_id = void (*)(GPUContext *);
    using PFN_create_gpu_resources = bool (*)(GPUContext *);

    struct GPUContext
    {
        PFN_create_gpu_resources create_resources = nullptr;
        PFN_destroy_gpu_context destroy_context = nullptr;
        PFN_push_clip_rect push_clip_rect = nullptr;
        PFN_reset_clip_rects reset_clip_rects = nullptr;
        PFN_update_clip_rect update_clip_rect = nullptr;
        PFN_get_clip_rect get_clip_rect = nullptr;
        PFN_push_hover_rect push_hover_rect = nullptr;
        PFN_update_hover_rect update_hover_rect = nullptr;
        PFN_clear_hover_rects clear_hover_rects = nullptr;
        PFN_update_hover_id update_hover_id = nullptr;
        QuadsGPUDispatch quads{};
    };

    inline u32 push_hover_rect(GPUContext *gpu_context, const RectData &rect)
    {
        return gpu_context->push_hover_rect(gpu_context, rect);
    }

    inline void update_hover_rect(GPUContext *gpu_context, u32 id, const RectData &rect)
    {
        gpu_context->update_hover_rect(gpu_context, id, rect);
    }

    inline void update_hover_id(GPUContext *gpu_context) { gpu_context->update_hover_id(gpu_context); }

    inline bool create_gpu_resources(GPUContext *gpu_context) { return gpu_context->create_resources(gpu_context); }

    inline void destroy_gpu_context(GPUContext *gpu_context)
    {
        if (gpu_context) gpu_context->destroy_context(gpu_context);
    }
} // namespace auik::v2::detail
