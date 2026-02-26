#pragma once
#include <acul/scalars.hpp>
#include "fwd.hpp"

namespace auik::v2::detail
{
    using PFN_push_data_to_stream = DrawDataID (*)(DrawStream *, const void *, u32);
    using PFN_update_quads_stream_data = void (*)(DrawStream *, DrawDataID, const void *, u32);
    using PFN_clear_quads_stream = void (*)(DrawStream *, u32);
    using PFN_render_quads_stream = void (*)(DrawStream *, void *, GPUContext *, u32);
    using PFN_create_quads_stream_gpu_data = void *(*)(u32, GPUContext *);
    using PFN_destroy_quads_stream_gpu_data = void (*)(DrawStream *);

    struct QuadsGPUDispatch
    {
        PFN_push_data_to_stream push_data_to_stream = nullptr;
        PFN_update_quads_stream_data update_quads_stream_data = nullptr;
        PFN_clear_quads_stream clear_quads_stream = nullptr;
        PFN_render_quads_stream render_quads_stream = nullptr;
        PFN_create_quads_stream_gpu_data create_quads_stream_gpu_data = nullptr;
        PFN_destroy_quads_stream_gpu_data destroy_quads_stream_gpu_data = nullptr;
    };
} // namespace auik::v2::detail
