#pragma once

#include <acul/api.hpp>
#include <amal/vector.hpp>
#include "quads_dispatch.hpp"

namespace auik::v2::detail
{
    using PFN_destroy_gpu_context = void (*)(GPUContext *);
    using PFN_push_clip_rect = u16 (*)(GPUContext *, const amal::vec4 &);
    using PFN_update_clip_rect = void (*)(GPUContext *, u16, const amal::vec4 &);
    using PFN_get_clip_rect = amal::vec4 *(*)(GPUContext *, u16);
    
    struct GPUContext
    {
        PFN_destroy_gpu_context destroy_context = nullptr;
        PFN_push_clip_rect push_clip_rect = nullptr;
        PFN_update_clip_rect update_clip_rect = nullptr;
        PFN_get_clip_rect get_clip_rect = nullptr;
        QuadsGPUDispatch quads{};
    };

    inline void destroy_gpu_context(GPUContext *gpu_context)
    {
        if (gpu_context) gpu_context->destroy_context(gpu_context);
    }
} // namespace auik::v2::detail
