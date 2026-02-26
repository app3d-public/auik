#pragma once

#include <acul/api.hpp>
#include "quads_dispatch.hpp"

namespace auik::v2::detail
{
    using PFN_destroy_gpu_context = void (*)(GPUContext *);
    struct GPUContext
    {
        PFN_destroy_gpu_context destroy_context;
        QuadsGPUDispatch quads;
    };

    inline void destroy_gpu_context(GPUContext *gpu_context)
    {
        if (gpu_context) gpu_context->destroy_context(gpu_context);
    }
} // namespace auik::v2::detail