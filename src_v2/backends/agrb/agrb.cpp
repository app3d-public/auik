#include <auik/v2/backends/agrb/agrb.hpp>
#include <auik/v2/detail/context.hpp>
#include <auik/v2/detail/gpu_context.hpp>
#include "context.hpp"

namespace auik::v2
{
    namespace detail
    {
        void init_quads_pipeline_calls(QuadsGPUDispatch &dispatch);

        static u16 push_clip_rect(GPUContext *gpu_context, const amal::vec4 &rect)
        {
            auto *ctx = get_agrb_context(gpu_context);
            const u32 id = static_cast<u32>(ctx->clip_rects.size());
            assert(id <= 0xFFFFu && "Clip rect limit exceeded (u16)");
            ctx->clip_rects.push_back(rect);
            return static_cast<u16>(id);
        }

        static void update_clip_rect(GPUContext *gpu_context, u16 clip_rect_id, const amal::vec4 &rect)
        {
            auto *ctx = get_agrb_context(gpu_context);
            if (clip_rect_id >= ctx->clip_rects.size()) return;
            ctx->clip_rects[clip_rect_id] = rect;
        }

        static amal::vec4 *get_clip_rect(GPUContext *gpu_context, u16 clip_rect_id)
        {
            auto *ctx = get_agrb_context(gpu_context);
            if (clip_rect_id >= ctx->clip_rects.size()) return nullptr;
            return &ctx->clip_rects[clip_rect_id];
        }
    } // namespace detail

    static void destroy_agrb_backend(detail::GPUContext *gpu_context)
    {
        detail::AgrbContext *agrb_ctx = static_cast<detail::AgrbContext *>(gpu_context);
        agrb_ctx->clip_rects.destroy();
        clear_shader_cache(agrb_ctx->device);
        acul::release(agrb_ctx);
    }

    APPLIB_API detail::GPUContext *create_agrb_backend(agrb::device &device, agrb::descriptor_pool *descriptor_pool)
    {
        auto *agrb_ctx = acul::alloc<detail::AgrbContext>(device, descriptor_pool);
        agrb::managed_buffer clip_buf{.required_flags = vk::MemoryPropertyFlagBits::eHostVisible |
                                                        vk::MemoryPropertyFlagBits::eHostCoherent,
                                      .buffer_usage = vk::BufferUsageFlagBits::eStorageBuffer,
                                      .vma_usage = VMA_MEMORY_USAGE_CPU_TO_GPU};
        agrb_ctx->clip_rects.init(device, clip_buf);
        agrb_ctx->destroy_context = &destroy_agrb_backend;
        agrb_ctx->push_clip_rect = &detail::push_clip_rect;
        agrb_ctx->update_clip_rect = &detail::update_clip_rect;
        agrb_ctx->get_clip_rect = &detail::get_clip_rect;
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
