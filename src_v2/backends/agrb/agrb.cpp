#include <auik/v2/backends/agrb/agrb.hpp>
#include <auik/v2/backends/agrb/quads_pipeline.hpp>
#include <auik/v2/detail/context.hpp>
#include <auik/v2/detail/gpu_context.hpp>
#include <auik/v2/draw.hpp>
#include <auik/v2/pipelines.hpp>
#include "context.hpp"
#include "picker/picker.hpp"

namespace auik::v2
{
    namespace detail
    {
        void init_quads_pipeline_calls(StreamGPUDispatch &dispatch);

        static inline agrb::vector<amal::vec4> &get_current_clip_rects(AgrbContext *ctx)
        {
            assert(ctx && ctx->clip_rects);
            const u32 frame_id = get_context().frame_id;
            assert(frame_id < get_context().frames_in_flight);
            return ctx->clip_rects[frame_id];
        }

        static u16 push_clip_rect(GPUContext *gpu_context, const amal::vec4 &rect)
        {
            auto *ctx = get_agrb_context(gpu_context);
            auto &clip_rects = get_current_clip_rects(ctx);
            const u32 id = static_cast<u32>(clip_rects.size());
            assert(id <= 0xFFFFu && "Clip rect limit exceeded (u16)");
            clip_rects.push_back(rect);
            return static_cast<u16>(id);
        }

        static void update_clip_rect(GPUContext *gpu_context, u16 clip_id, const amal::vec4 &rect)
        {
            auto *ctx = get_agrb_context(gpu_context);
            auto &clip_rects = get_current_clip_rects(ctx);
            if (clip_id >= clip_rects.size()) return;
            clip_rects[clip_id] = rect;
        }

        static void reset_clip_rects(GPUContext *gpu_context)
        {
            auto *ctx = get_agrb_context(gpu_context);
            get_current_clip_rects(ctx).clear();
        }

        static amal::vec4 *get_clip_rect(GPUContext *gpu_context, u16 clip_id)
        {
            auto *ctx = get_agrb_context(gpu_context);
            auto &clip_rects = get_current_clip_rects(ctx);
            if (clip_id >= clip_rects.size()) return nullptr;
            return &clip_rects[clip_id];
        }

        static void copy_clip_rects_frame_impl(GPUContext *gpu_context, u32 dst_frame_id, u32 src_frame_id)
        {
            auto *ctx = get_agrb_context(gpu_context);
            assert(ctx->clip_rects);
            const u32 frames = get_context().frames_in_flight;
            if (dst_frame_id >= frames || src_frame_id >= frames || dst_frame_id == src_frame_id) return;
            auto &dst = ctx->clip_rects[dst_frame_id];
            auto &src = ctx->clip_rects[src_frame_id];
            const u32 src_size = static_cast<u32>(src.size());
            if (src_size == 0) return;
            dst.resize(src_size);
            memcpy(dst.data().mapped, src.data().mapped, src_size * sizeof(amal::vec4));
        }

        static u32 push_hit_rect_impl(GPUContext *gpu_context, const RectData &rect)
        {
            auto *ctx = get_agrb_context(gpu_context);
            return ctx->picker->push_hit_rect(rect);
        }

        static void update_hit_rect_impl(GPUContext *gpu_context, u32 id, const RectData &rect)
        {
            auto *ctx = get_agrb_context(gpu_context);
            ctx->picker->update_hit_rect(id, rect);
        }

        static void clear_hit_rects_impl(GPUContext *gpu_context)
        {
            auto *ctx = get_agrb_context(gpu_context);
            ctx->picker->clear_hit_rects();
        }

        static void copy_hit_rects_frame_impl(GPUContext *gpu_context, u32 dst_frame_id, u32 src_frame_id)
        {
            auto *ctx = get_agrb_context(gpu_context);
            ctx->picker->copy_frame_data(dst_frame_id, src_frame_id);
        }
    } // namespace detail

    static void destroy_agrb_backend(detail::GPUContext *gpu_context)
    {
        detail::AgrbContext *agrb_ctx = static_cast<detail::AgrbContext *>(gpu_context);
        agrb_ctx->picker->destroy(agrb_ctx->device);
        agrb_ctx->picker.reset();
        if (agrb_ctx->clip_rects)
        {
            const u32 frames = detail::get_context().frames_in_flight;
            for (u32 i = 0; i < frames; ++i) agrb_ctx->clip_rects[i].destroy();
            acul::release(agrb_ctx->clip_rects, frames);
            agrb_ctx->clip_rects = nullptr;
        }
        clear_shader_cache(agrb_ctx->device);
        acul::release(agrb_ctx);
    }

    static bool create_agrb_resources(detail::GPUContext *gpu_context)
    {
        auto *agrb_ctx = static_cast<detail::AgrbContext *>(gpu_context);
        agrb::managed_buffer clip_buf{
            .required_flags = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
            .buffer_usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferSrc |
                            vk::BufferUsageFlagBits::eTransferDst,
            .vma_usage = VMA_MEMORY_USAGE_CPU_TO_GPU};
        clip_buf.instance_count = 1;
        const u32 frames = detail::get_context().frames_in_flight;
        agrb_ctx->clip_rects = acul::alloc_n<agrb::vector<amal::vec4>>(frames);
        for (u32 i = 0; i < frames; ++i) agrb_ctx->clip_rects[i].init(agrb_ctx->device, clip_buf);
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
        agrb_ctx->copy_clip_rects_frame = &detail::copy_clip_rects_frame_impl;
        agrb_ctx->push_hit_rect = &detail::push_hit_rect_impl;
        agrb_ctx->update_hit_rect = &detail::update_hit_rect_impl;
        agrb_ctx->clear_hit_rects = &detail::clear_hit_rects_impl;
        agrb_ctx->copy_hit_rects_frame = &detail::copy_hit_rects_frame_impl;
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

    APPLIB_API u32 get_default_streams_pipelines_count() { return 1; }

    APPLIB_API u32 get_default_streams_count() { return 1; }

    bool configure_default_streams(agrb::graphics_pipeline_batch &batch, DrawPipeline *pipelines, DrawStream *streams,
                                   u32 subpass, vk::RenderPass render_pass)
    {
        auto &global_ctx = detail::get_context();
        auto &device = detail::get_agrb_context(global_ctx.gpu_ctx)->device;

        auto &cquads_stream = streams[0];
        auik::v2::create_quads_stream_cached(cquads_stream);
        auik::v2::set_primary_quad_stream(&cquads_stream);
        auto &quads_pipeline = pipelines[0];
        if (!auik::v2::construct_quads_pipeline(quads_pipeline, device)) return false;
        cquads_stream.pipeline = &quads_pipeline;
        auto &cquads_artifact = batch.artifacts.emplace_back();
        auik::v2::construct_pipeline_artifact(cquads_artifact, subpass, &quads_pipeline);
        return auik::v2::configure_quads_pipeline(cquads_artifact, render_pass, quads_pipeline, device);
    }
} // namespace auik::v2
