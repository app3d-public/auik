#include <auik/v2/backends/agrb/agrb.hpp>
#include <auik/v2/detail/context.hpp>
#include <auik/v2/detail/gpu_context.hpp>
#include "context.hpp"

namespace auik::v2
{
    namespace detail
    {
        void init_quads_pipeline_calls(QuadsGPUDispatch &dispatch);
    } // namespace detail

    static void destroy_agrb_backend(detail::GPUContext *gpu_context)
    {
        detail::AgrbContext *agrb_ctx = static_cast<detail::AgrbContext *>(gpu_context);
        clear_shader_cache(agrb_ctx->device);
        acul::release(agrb_ctx);
    }

    APPLIB_API detail::GPUContext *create_agrb_backend(agrb::device &device, agrb::descriptor_pool *descriptor_pool)
    {
        auto *agrb_ctx = acul::alloc<detail::AgrbContext>(device, descriptor_pool);
        agrb_ctx->destroy_context = &destroy_agrb_backend;
        detail::init_quads_pipeline_calls(agrb_ctx->quads);
        return agrb_ctx;
    }

    APPLIB_API void clear_shader_cache(agrb::device &device)
    {
        detail::GPUContext *gpu_backend = detail::get_context().gpu_ctx;
        agrb::clear_shader_cache(device, detail::get_agrb_context(gpu_backend)->shader_cache);
    }

    APPLIB_API void destroy_draw_pipeline(DrawPipeline &pipeline, agrb::device &device)
    {
        if (pipeline.handle) device.vk_device.destroyPipeline(pipeline.handle, nullptr, device.loader);
        if (pipeline.layout) device.vk_device.destroyPipelineLayout(pipeline.layout, nullptr, device.loader);
        pipeline.descriptor_set_layout.reset();
    }
} // namespace auik::v2