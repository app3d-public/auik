#include <agrb/texture.hpp>
#include <auik/v2/backends/agrb/agrb.hpp>
#include <auik/v2/backends/agrb/quads_pipeline.hpp>
#include <auik/v2/backends/agrb/textures_pipeline.hpp>
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
        void init_textures_pipeline_calls(StreamGPUDispatch &dispatch);

        static bool create_text_atlas_sampler(agrb::texture &texture, agrb::device &device)
        {
            vk::SamplerCreateInfo sampler_create_info;
            sampler_create_info.setMagFilter(vk::Filter::eLinear)
                .setMinFilter(vk::Filter::eLinear)
                .setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
                .setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
                .setAddressModeW(vk::SamplerAddressMode::eClampToEdge)
                .setAnisotropyEnable(false)
                .setMaxAnisotropy(1.0f)
                .setBorderColor(vk::BorderColor::eIntOpaqueBlack)
                .setUnnormalizedCoordinates(false)
                .setCompareEnable(false)
                .setCompareOp(vk::CompareOp::eAlways)
                .setMipmapMode(vk::SamplerMipmapMode::eNearest)
                .setMipLodBias(0.0f)
                .setMinLod(0.0f)
                .setMaxLod(0.0f);

            return device.vk_device.createSampler(&sampler_create_info, nullptr, &texture.sampler, device.loader) ==
                   vk::Result::eSuccess;
        }

        static inline u64 image_view_to_handle(vk::ImageView image_view)
        {
            const VkImageView raw_view = image_view;
            static_assert(sizeof(VkImageView) <= sizeof(u64), "Image view handle doesn't fit into u64");
            if constexpr (std::is_pointer_v<VkImageView>)
                return static_cast<u64>(reinterpret_cast<uintptr_t>(raw_view));
            else return reinterpret_cast<u64>(raw_view);
        }

        static bool ensure_bindless_texture_table(AgrbContext *ctx)
        {
            assert(ctx && ctx->descriptor_pool);
            if (!ctx->bindless_texture_layout)
            {
                ctx->bindless_texture_layout =
                    agrb::descriptor_set_layout::builder()
                        .add_binding(0, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment,
                                     get_context().max_textures_size, vk::DescriptorBindingFlagBits::ePartiallyBound)
                        .build(ctx->device);
                if (!ctx->bindless_texture_layout) return false;
            }

            if (!ctx->bindless_texture_set && !ctx->descriptor_pool->allocate_descriptor(
                                                  ctx->bindless_texture_layout->layout(), ctx->bindless_texture_set))
                return false;
            return true;
        }

        static bool rewrite_bindless_texture_table(AgrbContext *ctx)
        {
            if (!ensure_bindless_texture_table(ctx)) return false;

            auto &global_ctx = get_context();
            global_ctx.texture_bind_slots.clear();
            for (u32 i = 0; i < global_ctx.textures.size(); ++i)
            {
                global_ctx.textures[i].bind_slot = i;
                global_ctx.texture_bind_slots[global_ctx.textures[i].handle] = i;
            }

            if (ctx->bindless_textures.empty()) return true;

            vk::WriteDescriptorSet write;
            write.setDstSet(ctx->bindless_texture_set)
                .setDstBinding(0)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(static_cast<u32>(ctx->bindless_textures.size()))
                .setPImageInfo(ctx->bindless_textures.data());
            ctx->device.vk_device.updateDescriptorSets(1, &write, 0, nullptr, ctx->device.loader);
            return true;
        }

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

        static bool create_atlas_texture_impl(GPUContext *gpu_context, AtlasTextureResource *resource, u32 width,
                                              u32 height, const void *pixels, size_t size)
        {
            if (!resource || !pixels || width == 0 || height == 0) return false;

            auto *ctx = get_agrb_context(gpu_context);
            auto *handle = acul::alloc<agrb::texture>();
            handle->format = vk::Format::eR8Unorm;
            handle->image_extent = vk::Extent3D{width, height, 1};
            handle->mip_levels = 1;
            handle->size = static_cast<vk::DeviceSize>(size);
            if (!agrb::allocate_texture(*handle, vk::ImageViewType::e2D, const_cast<void *>(pixels), ctx->device))
            {
                acul::release(handle);
                return false;
            }

            if (handle->sampler)
            {
                ctx->device.vk_device.destroySampler(handle->sampler, nullptr, ctx->device.loader);
                handle->sampler = nullptr;
            }
            if (!create_text_atlas_sampler(*handle, ctx->device))
            {
                agrb::destroy_texture(*handle, ctx->device);
                acul::release(handle);
                return false;
            }

            resource->texture_id =
                add_agrb_texture(handle->sampler, handle->image_view, vk::ImageLayout::eShaderReadOnlyOptimal);
            if (resource->texture_id.handle == 0)
            {
                agrb::destroy_texture(*handle, ctx->device);
                acul::release(handle);
                return false;
            }

            resource->handle = handle;
            resource->width = width;
            resource->height = height;
            return true;
        }

        static void destroy_atlas_texture_impl(GPUContext *gpu_context, AtlasTextureResource *resource)
        {
            if (!resource || !resource->handle) return;
            auto *ctx = get_agrb_context(gpu_context);
            auto *handle = static_cast<agrb::texture *>(resource->handle);
            if (handle->image_view) remove_agrb_texture(handle->image_view);
            if (handle->image) agrb::destroy_texture(*handle, ctx->device);
            acul::release(handle);
            *resource = {};
        }

        static bool upload_atlas_texture_impl(GPUContext *gpu_context, AtlasTextureResource *resource,
                                              const void *pixels, size_t size, u32 width, u32 height, i32 x, i32 y)
        {
            if (!resource || !resource->handle || !pixels || width == 0 || height == 0) return false;
            auto *ctx = get_agrb_context(gpu_context);
            auto *handle = static_cast<agrb::texture *>(resource->handle);
            return agrb::upload_texture_subimage(*handle, const_cast<void *>(pixels), static_cast<vk::DeviceSize>(size),
                                                 vk::Extent3D{width, height, 1}, vk::Offset3D{x, y, 0}, ctx->device);
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
        if (!detail::ensure_bindless_texture_table(agrb_ctx)) return false;
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
        agrb_ctx->create_atlas_texture = &detail::create_atlas_texture_impl;
        agrb_ctx->destroy_atlas_texture = &detail::destroy_atlas_texture_impl;
        agrb_ctx->upload_atlas_texture = &detail::upload_atlas_texture_impl;
        detail::init_quads_pipeline_calls(agrb_ctx->quads);
        detail::init_textures_pipeline_calls(agrb_ctx->textures);
        return agrb_ctx;
    }

    APPLIB_API TextureID add_agrb_texture(vk::Sampler sampler, vk::ImageView image_view, vk::ImageLayout image_layout)
    {
        auto &global_ctx = detail::get_context();
        auto *ctx = detail::get_agrb_context(global_ctx.gpu_ctx);
        assert(ctx->descriptor_pool && "AUIK AGRB descriptor pool is not initialized");
        assert(sampler && image_view && "AUIK texture descriptor requires valid sampler and image view");

        const u64 handle = detail::image_view_to_handle(image_view);
        const auto bind_it = global_ctx.texture_bind_slots.find(handle);
        if (bind_it != global_ctx.texture_bind_slots.end())
        {
            const u32 bind_slot = bind_it->second;
            assert(bind_slot < global_ctx.textures.size());
            return global_ctx.textures[bind_slot];
        }

        assert(global_ctx.textures.size() < global_ctx.max_textures_size && "AUIK texture capacity exceeded");
        if (global_ctx.textures.size() >= global_ctx.max_textures_size) return AUIK_INVALID_TEXTURE_ID;

        global_ctx.textures.push_back(TextureID{handle, static_cast<u32>(global_ctx.textures.size())});
        ctx->bindless_textures.push_back(
            vk::DescriptorImageInfo{}.setSampler(sampler).setImageView(image_view).setImageLayout(image_layout));

        if (!detail::rewrite_bindless_texture_table(ctx))
        {
            global_ctx.textures.pop_back();
            ctx->bindless_textures.pop_back();
            return AUIK_INVALID_TEXTURE_ID;
        }

        return global_ctx.textures.back();
    }

    APPLIB_API bool remove_agrb_texture(vk::ImageView image_view)
    {
        auto &global_ctx = detail::get_context();
        auto *ctx = detail::get_agrb_context(global_ctx.gpu_ctx);
        const u64 handle = detail::image_view_to_handle(image_view);
        const auto bind_it = global_ctx.texture_bind_slots.find(handle);
        if (bind_it == global_ctx.texture_bind_slots.end()) return false;

        const u32 bind_slot = bind_it->second;
        if (bind_slot >= global_ctx.textures.size()) return false;

        global_ctx.textures.erase(global_ctx.textures.begin() + bind_slot);
        ctx->bindless_textures.erase(ctx->bindless_textures.begin() + bind_slot);
        if (!detail::rewrite_bindless_texture_table(ctx)) return false;
        detail::mark_texture_bindings_mutation();
        return true;
    }

    APPLIB_API u32 get_agrb_texture_bind_slot(vk::ImageView image_view)
    {
        return get_texture_bind_slot(detail::image_view_to_handle(image_view));
    }

    APPLIB_API void clear_shader_cache(agrb::device &device)
    {
        detail::GPUContext *gpu_backend = detail::get_context().gpu_ctx;
        detail::get_agrb_context(gpu_backend)->shader_cache.reset(device);
    }

    APPLIB_API void destroy_draw_pipeline(DrawPipeline &pipeline, agrb::device &device)
    {
        if (pipeline.handle) device.vk_device.destroyPipeline(pipeline.handle, nullptr, device.loader);
        if (pipeline.layout) device.vk_device.destroyPipelineLayout(pipeline.layout, nullptr, device.loader);
        pipeline.descriptor_set_layout.reset();
    }

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

    bool configure_default_streams(agrb::graphics_pipeline_batch &batch, DrawPipeline *pipelines, DrawStream *streams,
                                   u32 subpass, vk::RenderPass render_pass)
    {
        auto &global_ctx = detail::get_context();
        auto &device = detail::get_agrb_context(global_ctx.gpu_ctx)->device;

        auto &cquads_stream = streams[0];
        auik::v2::create_quads_stream(cquads_stream);
        auik::v2::set_primary_quad_stream(&cquads_stream);
        auto &quads_pipeline = pipelines[0];
        if (!auik::v2::construct_quads_pipeline(quads_pipeline, device)) return false;
        cquads_stream.pipeline = &quads_pipeline;
        auto &cquads_artifact = batch.artifacts.emplace_back();
        auik::v2::construct_pipeline_artifact(cquads_artifact, subpass, &quads_pipeline);
        if (!auik::v2::configure_quads_pipeline(cquads_artifact, render_pass, quads_pipeline, device)) return false;

        auto &ctextures_stream = streams[1];
        auik::v2::create_textures_stream(ctextures_stream);
        auik::v2::set_primary_image_stream(&ctextures_stream);
        auto &textures_pipeline = pipelines[1];
        if (!auik::v2::construct_textures_pipeline(textures_pipeline, device)) return false;
        ctextures_stream.pipeline = &textures_pipeline;
        auto &ctextures_artifact = batch.artifacts.emplace_back();
        auik::v2::construct_pipeline_artifact(ctextures_artifact, subpass, &textures_pipeline);
        return auik::v2::configure_textures_pipeline(ctextures_artifact, render_pass, textures_pipeline, device);
    }
} // namespace auik::v2
