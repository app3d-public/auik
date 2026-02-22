#pragma once

#include <acul/api.hpp>
#include "quads_dispatch.hpp"

namespace auik::v2::detail
{

    extern APPLIB_API struct GPUDispatch
    {
        QuadsGPUDispatch quads;
    } g_gpu_dispatch;
} // namespace auik::v2::detail