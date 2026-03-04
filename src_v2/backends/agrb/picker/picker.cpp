#include "picker.hpp"
#include <agrb/defaults.hpp>
#include <agrb/utils/buffer.hpp>
#include <agrb/utils/image.hpp>
#include <array>
#include <auik/shaders.h>
#include <auik/v2/backends/agrb/agrb.hpp>
#include <auik/v2/detail/context.hpp>
#include <cstring>
#include "../context.hpp"

namespace auik::v2::detail
{
    static constexpr vk::DeviceSize AUIK_PICK_RESULT_SIZE = sizeof(u32) * 2;

    void GPUPicker::create_render_pass(agrb::device &device)
    {
        vk::AttachmentDescription2 attachments[2];
        attachments[0]
            .setFormat(vk::Format::eR32G32Uint)
            .setSamples(vk::SampleCountFlagBits::e1)
            .setLoadOp(vk::AttachmentLoadOp::eClear)
            .setStoreOp(vk::AttachmentStoreOp::eStore)
            .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
            .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
            .setInitialLayout(vk::ImageLayout::eUndefined)
            .setFinalLayout(vk::ImageLayout::eTransferSrcOptimal);
        attachments[1]
            .setFormat(_depth_format)
            .setSamples(vk::SampleCountFlagBits::e1)
            .setLoadOp(vk::AttachmentLoadOp::eClear)
            .setStoreOp(vk::AttachmentStoreOp::eDontCare)
            .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
            .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
            .setInitialLayout(vk::ImageLayout::eUndefined)
            .setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

        vk::AttachmentReference2 color_ref;
        color_ref.setAttachment(0)
            .setLayout(vk::ImageLayout::eColorAttachmentOptimal)
            .setAspectMask(vk::ImageAspectFlagBits::eColor);
        vk::AttachmentReference2 depth_ref;
        depth_ref.setAttachment(1)
            .setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
            .setAspectMask(vk::ImageAspectFlagBits::eDepth);

        vk::SubpassDescription2 subpass;
        subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
            .setColorAttachmentCount(1)
            .setPColorAttachments(&color_ref)
            .setPDepthStencilAttachment(&depth_ref);

        vk::SubpassDependency2 deps[2];
        deps[0]
            .setSrcSubpass(vk::SubpassExternal)
            .setDstSubpass(0)
            .setSrcStageMask(vk::PipelineStageFlagBits::eTopOfPipe)
            .setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput |
                             vk::PipelineStageFlagBits::eEarlyFragmentTests |
                             vk::PipelineStageFlagBits::eLateFragmentTests)
            .setSrcAccessMask(vk::AccessFlagBits::eNone)
            .setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite |
                              vk::AccessFlagBits::eDepthStencilAttachmentWrite)
            .setDependencyFlags(vk::DependencyFlagBits::eByRegion);
        deps[1]
            .setSrcSubpass(0)
            .setDstSubpass(vk::SubpassExternal)
            .setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput |
                             vk::PipelineStageFlagBits::eLateFragmentTests)
            .setDstStageMask(vk::PipelineStageFlagBits::eTransfer)
            .setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite |
                              vk::AccessFlagBits::eDepthStencilAttachmentWrite)
            .setDstAccessMask(vk::AccessFlagBits::eTransferRead)
            .setDependencyFlags(vk::DependencyFlagBits::eByRegion);

        vk::RenderPassCreateInfo2 ci;
        ci.setAttachmentCount(2)
            .setPAttachments(attachments)
            .setSubpassCount(1)
            .setPSubpasses(&subpass)
            .setDependencyCount(2)
            .setPDependencies(deps);
        rp_group.value(device.vk_device.createRenderPass2(ci, nullptr, device.loader));
    }

    static bool create_color_image(agrb::fb_image_slot &slot, vk::Extent2D extent, agrb::device &device)
    {
        auto &image = slot.attachments[0];
        vk::ImageCreateInfo image_info;
        image_info.setImageType(vk::ImageType::e2D)
            .setExtent({1, 1, 1})
            .setMipLevels(1)
            .setArrayLayers(1)
            .setFormat(vk::Format::eR32G32Uint)
            .setTiling(vk::ImageTiling::eOptimal)
            .setInitialLayout(vk::ImageLayout::eUndefined)
            .setUsage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc)
            .setSamples(vk::SampleCountFlagBits::e1)
            .setSharingMode(vk::SharingMode::eExclusive);
        auto create_info = agrb::make_alloc_info(VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, {},
                                                 vk::MemoryPropertyFlagBits::eDeviceLocal, 1.0f);
        if (!agrb::create_image(image_info, image.image, image.memory, device.allocator, create_info)) return false;

        vk::ImageViewCreateInfo view_info;
        view_info.setImage(image.image)
            .setViewType(vk::ImageViewType::e2D)
            .setFormat(vk::Format::eR32G32Uint)
            .setSubresourceRange(agrb::defaults::subresource_range_color);
        return device.vk_device.createImageView(&view_info, nullptr, &image.get_view(), device.loader) ==
               vk::Result::eSuccess;
    }

    static bool create_depth_image(agrb::fb_image_slot &slot, vk::Extent2D extent, vk::Format image_format,
                                   agrb::device &device)
    {
        auto &image = slot.attachments[1];
        vk::ImageCreateInfo image_info;
        image_info.setImageType(vk::ImageType::e2D)
            .setExtent({1, 1, 1})
            .setMipLevels(1)
            .setArrayLayers(1)
            .setFormat(image_format)
            .setTiling(vk::ImageTiling::eOptimal)
            .setInitialLayout(vk::ImageLayout::eUndefined)
            .setUsage(vk::ImageUsageFlagBits::eDepthStencilAttachment)
            .setSamples(vk::SampleCountFlagBits::e1)
            .setSharingMode(vk::SharingMode::eExclusive);
        auto create_info = agrb::make_alloc_info(VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, {},
                                                 vk::MemoryPropertyFlagBits::eDeviceLocal, 1.0f);
        if (!agrb::create_image(image_info, image.image, image.memory, device.allocator, create_info)) return false;

        vk::ImageViewCreateInfo view_info;
        view_info.setImage(image.image)
            .setViewType(vk::ImageViewType::e2D)
            .setFormat(image_format)
            .setSubresourceRange(agrb::defaults::subresource_range_depth);
        return device.vk_device.createImageView(&view_info, nullptr, &image.get_view(), device.loader) ==
               vk::Result::eSuccess;
    }

    bool GPUPicker::create_attachments(agrb::device &device)
    {
        u32 frames_in_flight = get_context().frames_in_flight;
        attachments = acul::alloc<agrb::fb_attachments>(vk::Extent2D{1, 1});
        attachments->attachment_count = 2;
        attachments->image_count = frames_in_flight;
        attachments->images = acul::alloc_n<agrb::fb_image_slot>(frames_in_flight);

        for (u32 i = 0; i < frames_in_flight; ++i)
        {
            auto &slot = attachments->images[i];
            slot.attachments.resize(2);

            if (!create_color_image(slot, attachments->extent, device)) return false;
            if (!create_depth_image(slot, attachments->extent, _depth_format, device)) return false;
        }

        if (!clear_values)
        {
            clear_values = acul::alloc_n<vk::ClearValue>(2);
            clear_values[0] = vk::ClearValue(vk::ClearColorValue(std::array<u32, 4>{0u, 0u, 0u, 0u}));
            clear_values[1] = vk::ClearValue(vk::ClearDepthStencilValue(0.0f, 0));
        }

        return agrb::create_fb_handles(this, device);
    }

    bool GPUPicker::create_descriptor_resources(agrb::device &device)
    {
        agrb::managed_buffer buf{.required_flags = vk::MemoryPropertyFlagBits::eHostVisible |
                                                   vk::MemoryPropertyFlagBits::eHostCoherent,
                                 .buffer_usage = vk::BufferUsageFlagBits::eStorageBuffer,
                                 .vma_usage = VMA_MEMORY_USAGE_CPU_TO_GPU};
        buf.instance_count = 1;
        _rects.init(device, buf);

        _descriptor_set_layout =
            agrb::descriptor_set_layout::builder()
                .add_binding(0, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eVertex)
                .add_binding(1, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eFragment)
                .build(device);
        if (!_descriptor_set_layout) return false;
        auto &global_ctx = get_context();
        u32 frames_in_flight = global_ctx.frames_in_flight;
        _descriptor_sets.resize(frames_in_flight);

        const vk::Buffer instance_buffer = _rects.data().vk_buffer;
        auto *agrb_ctx = get_agrb_context(global_ctx.gpu_ctx);
        const vk::Buffer clip_rects_buffer = agrb_ctx->clip_rects.data().vk_buffer;
        if (!instance_buffer || !clip_rects_buffer) return false;

        vk::DescriptorBufferInfo instance_info{instance_buffer, 0, VK_WHOLE_SIZE};
        for (u32 i = 0; i < frames_in_flight; ++i)
        {
            vk::DescriptorBufferInfo clip_rects_info{clip_rects_buffer, 0, VK_WHOLE_SIZE};
            agrb::descriptor_writer writer(*_descriptor_set_layout, *agrb_ctx->descriptor_pool);
            writer.write_buffer(0, &instance_info);
            writer.write_buffer(1, &clip_rects_info);
            if (!writer.build(_descriptor_sets[i])) return false;
        }

        _descriptor_buffer_instances = instance_buffer;
        _descriptor_buffer_clip_rects = clip_rects_buffer;
        return true;
    }

    bool GPUPicker::create_readback_resources(agrb::device &device)
    {
        const u32 frames_in_flight = get_context().frames_in_flight;
        _readback_buffers.resize(frames_in_flight);
        _submit_fences.resize(frames_in_flight);
        device.rd->fence_pool.request(_submit_fences.data(), _submit_fences.size());

        for (u32 i = 0; i < frames_in_flight; ++i)
        {
            auto &buffer = _readback_buffers[i];
            buffer.instance_count = 1;
            buffer.alignment_size = AUIK_PICK_RESULT_SIZE;
            buffer.buffer_size = AUIK_PICK_RESULT_SIZE;
            buffer.buffer_usage = vk::BufferUsageFlagBits::eTransferDst;
            buffer.vma_usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
            buffer.required_flags =
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
            buffer.prefered_flags = vk::MemoryPropertyFlagBits::eHostCached;

            const auto alloc_info =
                agrb::make_alloc_info(buffer.vma_usage, buffer.required_flags, buffer.prefered_flags, 1.0f);
            VmaAllocationCreateInfo host_alloc_info = alloc_info;
            host_alloc_info.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
            if (!agrb::allocate_buffer(buffer, host_alloc_info, buffer.buffer_usage, device)) return false;
            if (!agrb::map_buffer(buffer, device)) return false;
            std::memset(buffer.mapped, 0, static_cast<size_t>(AUIK_PICK_RESULT_SIZE));
        }
        return true;
    }

    bool GPUPicker::update_descriptors(AgrbContext *ctx)
    {
        assert(!_descriptor_sets.empty());

        const vk::Buffer instance_buffer = _rects.data().vk_buffer;
        const vk::Buffer clip_rects_buffer = ctx->clip_rects.data().vk_buffer;
        if (!instance_buffer || !clip_rects_buffer) return false;
        if (_descriptor_buffer_instances == instance_buffer && _descriptor_buffer_clip_rects == clip_rects_buffer)
            return true;

        vk::DescriptorBufferInfo instance_info{instance_buffer, 0, VK_WHOLE_SIZE};
        for (u32 i = 0; i < _descriptor_sets.size(); ++i)
        {
            vk::DescriptorBufferInfo clip_rects_info{clip_rects_buffer, 0, VK_WHOLE_SIZE};
            agrb::descriptor_writer writer(*_descriptor_set_layout, *ctx->descriptor_pool);
            writer.write_buffer(0, &instance_info);
            writer.write_buffer(1, &clip_rects_info);
            writer.overwrite(_descriptor_sets[i]);
        }

        _descriptor_buffer_instances = instance_buffer;
        _descriptor_buffer_clip_rects = clip_rects_buffer;
        return true;
    }

    void GPUPicker::destroy(agrb::device &device)
    {
        if (!_submit_fences.empty()) device.rd->fence_pool.release(_submit_fences.data(), _submit_fences.size());
        _submit_fences.clear();

        for (auto &buffer : _readback_buffers)
            if (buffer.vk_buffer) agrb::destroy_buffer(buffer, device);
        _readback_buffers.clear();

        device.rd->queues.graphics.pool.primary.release(_command_buffers.data(), _command_buffers.size());
        if (attachments) agrb::destroy_framebuffer(*this, device);
        if (_rects.is_inited()) _rects.destroy();
        _descriptor_set_layout.reset();
        _descriptor_sets.clear();
        _descriptor_buffer_instances = nullptr;
        _descriptor_buffer_clip_rects = nullptr;
        _pipeline = nullptr;
        _depth_format = vk::Format::eUndefined;
    }

    bool GPUPicker::prepare(AgrbContext *context)
    {
        u32 frames_in_flight = get_context().frames_in_flight;
        _command_buffers.resize(frames_in_flight);
        auto &device = context->device;
        device.rd->queues.graphics.pool.primary.request(_command_buffers.data(), frames_in_flight);
        create_render_pass(device);
        if (!create_attachments(device)) goto err;
        if (!create_descriptor_resources(device)) goto err;
        if (!create_readback_resources(device)) goto err;
        return true;
    err:
        destroy(device);
        return false;
    }

    bool GPUPicker::construct_pipeline(agrb::device &device, DrawPipeline &pipeline)
    {
        if (pipeline.layout) return true;
        _pipeline = &pipeline;
        if (!_descriptor_set_layout)
        {
            _descriptor_set_layout =
                agrb::descriptor_set_layout::builder()
                    .add_binding(0, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eVertex)
                    .add_binding(1, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eFragment)
                    .build(device);
            if (!_descriptor_set_layout) return false;
        }

        pipeline.descriptor_set_layout = _descriptor_set_layout;
        const vk::DescriptorSetLayout set_layouts[] = {pipeline.descriptor_set_layout->layout()};
        const vk::PushConstantRange push_constant{vk::ShaderStageFlagBits::eVertex, 0, sizeof(amal::vec2)};
        vk::PipelineLayoutCreateInfo pipeline_layout_info{};
        pipeline_layout_info.setSetLayoutCount(1)
            .setPSetLayouts(set_layouts)
            .setPushConstantRangeCount(1)
            .setPPushConstantRanges(&push_constant);
        pipeline.layout = device.vk_device.createPipelineLayout(pipeline_layout_info, nullptr, device.loader);
        return pipeline.layout != nullptr;
    }

    bool GPUPicker::configure_pipeline(AgrbContext *ctx, agrb::graphics_pipeline_batch::artifact &artifact,
                                       DrawPipeline &pipeline)
    {
        auto *tmp = static_cast<agrb::graphics_pipeline_batch::artifact::custom_data_t<u32> *>(artifact.tmp);
        assert(tmp);

        artifact.config.load_defaults();
        artifact.config.color_blend_attachment.setBlendEnable(false);
        artifact.config.depth_stencil_info.setDepthTestEnable(true).setDepthWriteEnable(true).setDepthCompareOp(
            vk::CompareOp::eGreaterOrEqual);
        artifact.config.render_pass = get_rp();
        artifact.config.pipeline_layout = pipeline.layout;
        artifact.config.subpass = tmp->value;

        const auto &path = get_shader_library_path();
        auto &device = ctx->device;
        vk::ShaderModule shaders[2];
        auto vs = agrb::get_shader(AS_AUIK_PICKER_VS, shaders[0], ctx->shader_cache, device, path);
        if (!vs.success()) return false;
        auto fs = agrb::get_shader(AS_AUIK_PICKER_FS, shaders[1], ctx->shader_cache, device, path);
        if (!fs.success()) return false;
        agrb::prepare_base_graphics_pipeline(artifact, shaders, device);
        return true;
    }

    static inline bool check_mouse_bounds(const acul::point2D<i32> &mouse_pos)
    {
        const auto &dimensions = get_context().window_size;
        return mouse_pos.x >= 0 && mouse_pos.y >= 0 && mouse_pos.x < dimensions.x && mouse_pos.y < dimensions.y;
    }

    static void begin_render_pass(agrb::framebuffer &fb, u32 frame_id, vk::CommandBuffer cmd,
                                  const vk::DispatchLoaderDynamic &loader)
    {
        vk::RenderPassBeginInfo rp_info;
        rp_info.setRenderPass(fb.get_rp())
            .setFramebuffer(fb.get_fb(frame_id))
            .setRenderArea({{0, 0}, {1, 1}})
            .setClearValueCount(2)
            .setPClearValues(fb.clear_values);
        cmd.beginRenderPass(&rp_info, vk::SubpassContents::eInline, loader);
    }

    static bool begin_recording(vk::CommandBuffer cmd, const vk::DispatchLoaderDynamic &loader)
    {
        vk::CommandBufferBeginInfo begin_info;
        begin_info.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
        cmd.begin(begin_info, loader);
        return true;
    }

    void GPUPicker::render(AgrbContext *ctx)
    {
        auto &global_ctx = get_context();
        u32 frame_id = global_ctx.frame_id;
        auto &cmd = _command_buffers[frame_id];
        auto &loader = ctx->device.loader;
        if (!begin_recording(cmd, loader)) return;

        begin_render_pass(*this, frame_id, cmd, loader);
        vk::Viewport viewport = {-static_cast<f32>(global_ctx.mouse_position.x),
                                 -static_cast<f32>(global_ctx.mouse_position.y),
                                 static_cast<f32>(global_ctx.window_size.x),
                                 static_cast<f32>(global_ctx.window_size.y),
                                 0.0f,
                                 1.0f};
        vk::Rect2D scissor = {{0, 0}, {1, 1}};
        cmd.setViewport(0, 1, &viewport, loader);
        cmd.setScissor(0, 1, &scissor, loader);

        if (!_rects.empty() && update_descriptors(ctx))
        {
            cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, _pipeline->handle, loader);
            cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, _pipeline->layout, 0, 1,
                                   &_descriptor_sets[frame_id], 0, nullptr, loader);
            cmd.pushConstants(_pipeline->layout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(acul::point2D<u32>),
                              &global_ctx.window_size, loader);
            cmd.draw(6, _rects.size(), 0, 0, loader);
        }
        cmd.endRenderPass(loader);
        {
            vk::BufferImageCopy region{};
            region.setBufferOffset(0)
                .setBufferRowLength(0)
                .setBufferImageHeight(0)
                .setImageSubresource({vk::ImageAspectFlagBits::eColor, 0, 0, 1})
                .setImageOffset({0, 0, 0})
                .setImageExtent({1, 1, 1});
            cmd.copyImageToBuffer(attachments->images[frame_id].attachments[0].image,
                                  vk::ImageLayout::eTransferSrcOptimal, _readback_buffers[frame_id].vk_buffer, 1,
                                  &region, loader);
        }

        cmd.end(loader);
        const vk::Fence fence = _submit_fences[frame_id];
        const vk::Result reset_result = ctx->device.vk_device.resetFences(1, &fence, ctx->device.loader);
        if (reset_result != vk::Result::eSuccess) return;
        vk::SubmitInfo submit_info;
        submit_info.setCommandBufferCount(1).setPCommandBuffers(&cmd);
        auto &queue = ctx->device.rd->queues.graphics.vk_queue;
        const vk::Result submit_result = queue.submit(1, &submit_info, fence, ctx->device.loader);
        if (submit_result != vk::Result::eSuccess) return;
    }

    void GPUPicker::pick(AgrbContext *ctx, u32 read_frame_id)
    {
        if (read_frame_id >= _readback_buffers.size() || read_frame_id >= _submit_fences.size()) return;

        const vk::Fence fence = _submit_fences[read_frame_id];
        const vk::Result status = ctx->device.vk_device.getFenceStatus(fence, ctx->device.loader);
        if (status != vk::Result::eSuccess) return;

        auto *data = static_cast<const PickValue *>(_readback_buffers[read_frame_id].mapped);
        if (!data) return;
        auto &global_ctx = get_context();
        global_ctx.hover_widget_id = data->widget_id;
        global_ctx.hover_tag_id = data->tag_id;
    }

    u32 GPUPicker::push_hover_rect(const RectData &rect)
    {
        const u32 id = static_cast<u32>(_rects.size());
        _rects.push_back(rect);
        return id;
    }

    void GPUPicker::update_hover_rect(u32 id, const RectData &rect)
    {
        if (id >= _rects.size()) return;
        _rects[id] = rect;
    }

    void GPUPicker::clear_hover_rects() { _rects.clear(); }

    void update_hover_id_impl(GPUContext *gpu_context)
    {
        auto &global_ctx = get_context();
        if (!check_mouse_bounds(global_ctx.mouse_position)) return;
        auto *agrb_ctx = get_agrb_context(gpu_context);
        auto &picker = agrb_ctx->picker;
        assert(picker->attachments);
        picker->render(agrb_ctx);
        const u32 frames_in_flight = global_ctx.frames_in_flight;
        assert(frames_in_flight > 0);
        const u32 read_frame_id = (global_ctx.frame_id + frames_in_flight - 1) % frames_in_flight;
        picker->pick(agrb_ctx, read_frame_id);
    }
} // namespace auik::v2::detail
