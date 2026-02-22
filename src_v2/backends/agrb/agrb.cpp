#include <auik/v2/backends/agrb/agrb.hpp>
#include <auik/v2/detail/context.hpp>
#include <auik/v2/detail/gpu_dispatch.hpp>
#include "context.hpp"

namespace auik::v2
{
    namespace detail
    {
        void init_quads_pipeline_calls(QuadsGPUDispatch &dispatch);

        static void init_agrb_dispatcher() { detail::init_quads_pipeline_calls(detail::g_gpu_dispatch.quads); }
    } // namespace detail

    APPLIB_API void *create_agrb_backend(agrb::device &device, agrb::descriptor_pool *descriptor_pool)
    {
        detail::init_agrb_dispatcher();
        auto *agrb_ctx = acul::alloc<detail::AgrbContext>(device, descriptor_pool);
        return agrb_ctx;
    }

    APPLIB_API void clear_shader_cache(agrb::device &device)
    {
        void *gpu_backend = detail::get_context().gpu_backend;
        agrb::clear_shader_cache(device, detail::get_agrb_context(gpu_backend)->shader_cache);
    }

    APPLIB_API void destroy_draw_pipeline(DrawPipeline &pipeline, agrb::device &device)
    {
        if (pipeline.handle) device.vk_device.destroyPipeline(pipeline.handle, nullptr, device.loader);
        if (pipeline.layout) device.vk_device.destroyPipelineLayout(pipeline.layout, nullptr, device.loader);
        pipeline.descriptor_set_layout.reset();
    }
} // namespace auik::v2