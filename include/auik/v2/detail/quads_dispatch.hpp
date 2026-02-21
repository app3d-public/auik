#pragma once
#include <acul/scalars.hpp>
#include "fwd.hpp"

namespace auik::v2::detail
{
    using PFN_push_data_to_quads_stream = void (*)(DrawStream *, void *);
    using PFN_clear_quads_stream = void (*)(DrawStream *, u32);
    using PFN_render_quads_stream = void (*)(DrawStream *, void *, void *, u32);
    using PFN_create_quads_stream_gpu_data = void *(*)(u32, void *);
    using PFN_destroy_quads_stream_gpu_data = void (*)(DrawStream *);

    struct QuadsGPUDispatch
    {
        PFN_push_data_to_quads_stream push_data_to_quads_stream = nullptr;
        PFN_clear_quads_stream clear_quads_stream = nullptr;
        PFN_render_quads_stream render_quads_stream = nullptr;
        PFN_create_quads_stream_gpu_data create_quads_stream_gpu_data = nullptr;
        PFN_destroy_quads_stream_gpu_data destroy_quads_stream_gpu_data = nullptr;
    };

    inline void push_data_to_quads_stream(DrawStream *stream, void *data, const QuadsGPUDispatch &gpu_dispatch)
    {
        gpu_dispatch.push_data_to_quads_stream(stream, data);
    }

    inline void clear_quads_stream(DrawStream *stream, u32 frame_id, const QuadsGPUDispatch &gpu_dispatch)
    {
        gpu_dispatch.clear_quads_stream(stream, frame_id);
    }

    inline void render_quads_stream(DrawStream *stream, void *render_ctx, void *gpu_backend, u32 frame_id,
                                    const QuadsGPUDispatch &gpu_dispatch)
    {
        gpu_dispatch.render_quads_stream(stream, render_ctx, gpu_backend, frame_id);
    }

    inline void *create_quads_stream_gpu_data(u32 frames_in_flight, void *gpu_backend,
                                              const QuadsGPUDispatch &gpu_dispatch)
    {
        return gpu_dispatch.create_quads_stream_gpu_data(frames_in_flight, gpu_backend);
    }

    inline void destroy_quads_stream_gpu_data(DrawStream *stream, const QuadsGPUDispatch &gpu_dispatch)
    {
        gpu_dispatch.destroy_quads_stream_gpu_data(stream);
    }
} // namespace auik::v2::detail