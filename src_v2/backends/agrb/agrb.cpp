#include <auik/v2/backends/agrb/agrb.hpp>
#include <auik/v2/detail/context.hpp>
#include <auik/v2/detail/gpu_context.hpp>
#include "context.hpp"
#include "picker/picker.hpp"

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

        static void reset_clip_rects(GPUContext *gpu_context)
        {
            auto *ctx = get_agrb_context(gpu_context);
            ctx->clip_rects.clear();
        }

        static amal::vec4 *get_clip_rect(GPUContext *gpu_context, u16 clip_rect_id)
        {
            auto *ctx = get_agrb_context(gpu_context);
            if (clip_rect_id >= ctx->clip_rects.size()) return nullptr;
            return &ctx->clip_rects[clip_rect_id];
        }

        static u32 push_hover_rect_impl(GPUContext *gpu_context, const RectData &rect)
        {
            auto *ctx = get_agrb_context(gpu_context);
            return ctx->picker->push_hover_rect(rect);
        }

        static void update_hover_rect_impl(GPUContext *gpu_context, u32 id, const RectData &rect)
        {
            auto *ctx = get_agrb_context(gpu_context);
            ctx->picker->update_hover_rect(id, rect);
        }

        static void clear_hover_rects_impl(GPUContext *gpu_context)
        {
            auto *ctx = get_agrb_context(gpu_context);
            ctx->picker->clear_hover_rects();
        }
    } // namespace detail

    static void destroy_agrb_backend(detail::GPUContext *gpu_context)
    {
        detail::AgrbContext *agrb_ctx = static_cast<detail::AgrbContext *>(gpu_context);
        agrb_ctx->picker->destroy(agrb_ctx->device);
        agrb_ctx->picker.reset();
        agrb_ctx->clip_rects.destroy();
        clear_shader_cache(agrb_ctx->device);
        acul::release(agrb_ctx);
    }

    static bool create_agrb_resources(detail::GPUContext *gpu_context)
    {
        auto *agrb_ctx = static_cast<detail::AgrbContext *>(gpu_context);
        agrb::managed_buffer clip_buf{.required_flags = vk::MemoryPropertyFlagBits::eHostVisible |
                                                        vk::MemoryPropertyFlagBits::eHostCoherent,
                                      .buffer_usage = vk::BufferUsageFlagBits::eStorageBuffer,
                                      .vma_usage = VMA_MEMORY_USAGE_CPU_TO_GPU};
        clip_buf.instance_count = 1;
        agrb_ctx->clip_rects.init(agrb_ctx->device, clip_buf);
        agrb_ctx->picker = acul::make_unique<detail::GPUPicker>(agrb_ctx->device);
        return agrb_ctx->picker->prepare(agrb_ctx);
    }

    APPLIB_API detail::GPUContext *create_agrb_backend(agrb::device &device, agrb::descriptor_pool *descriptor_pool)
    {
        auto *agrb_ctx = acul::alloc<detail::AgrbContext>(device, descriptor_pool);
        agrb_ctx->create_resources = &create_agrb_resources;
        agrb_ctx->destroy_context = &destroy_agrb_backend;
        agrb_ctx->push_clip_rect = &detail::push_clip_rect;
        agrb_ctx->reset_clip_rects = &detail::reset_clip_rects;
        agrb_ctx->update_clip_rect = &detail::update_clip_rect;
        agrb_ctx->get_clip_rect = &detail::get_clip_rect;
        agrb_ctx->push_hover_rect = &detail::push_hover_rect_impl;
        agrb_ctx->update_hover_rect = &detail::update_hover_rect_impl;
        agrb_ctx->clear_hover_rects = &detail::clear_hover_rects_impl;
        agrb_ctx->update_hover_id = &detail::update_hover_id_impl;
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

    APPLIB_API u32 get_service_pipelines_count() { return 1; }

    bool configure_service_pipelines(agrb::graphics_pipeline_batch &batch, DrawPipeline *pipelines)
    {
        auto &global_ctx = detail::get_context();
        auto &picker_artifact = batch.artifacts.emplace_back();
        construct_pipeline_artifact(picker_artifact, 0, &pipelines[0]);
        auto *gpu_ctx = detail::get_agrb_context(global_ctx.gpu_ctx);
        auto &picker = gpu_ctx->picker;
        if (!picker->construct_pipeline(gpu_ctx->device, pipelines[0])) return false;
        return picker->configure_pipeline(gpu_ctx, picker_artifact, pipelines[0]);
    }
} // namespace auik::v2
