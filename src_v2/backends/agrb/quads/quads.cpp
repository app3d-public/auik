#include <acul/scalars.hpp>
#include <acul/string/string.hpp>
#include <agrb/vector.hpp>
#include <auik/shaders.h>
#include <auik/v2/backends/agrb/agrb.hpp>
#include <auik/v2/backends/agrb/quads_pipeline.hpp>
#include <auik/v2/detail/quads_dispatch.hpp>
#include <auik/v2/pipelines.hpp>
#include "../context.hpp"

namespace auik::v2::detail
{
    struct QuadsStream
    {
        agrb::vector<auik::v2::QuadsInstanceData> draw_instances;
        vk::DescriptorSet descriptor_set;
    };

    void push_data_to_quads_stream(DrawStream *stream, void *data)
    {
        u32 frame_id = get_context().frame_id;
        auto &gpu_data = static_cast<QuadsStream *>(stream->stream_instances)[frame_id];
        gpu_data.draw_instances.push_back(*static_cast<QuadsInstanceData *>(data));
        ++stream->draw_sizes[frame_id];
    }

    void clear_quads_stream(DrawStream *stream, u32 frame_id)
    {
        auto &gpu_data = static_cast<QuadsStream *>(stream->stream_instances)[frame_id];
        gpu_data.draw_instances.clear();
    }

    void destroy_quads_stream_gpu_data(DrawStream *stream)
    {
        u32 count = get_context().frames_in_flight;
        for (u32 i = 0; i < count; i++)
        {
            auto &gpu_data = static_cast<QuadsStream *>(stream->stream_instances)[i];
            gpu_data.draw_instances.destroy();
        }
        acul::release(static_cast<QuadsStream *>(stream->stream_instances), count);
    }

    void *create_quads_stream_gpu_data(u32 instance_count, void *gpu_backend)
    {
        auto *data = acul::alloc_n<QuadsStream>(instance_count);
        auto &device = get_agrb_device(gpu_backend);
        agrb::managed_buffer buf{.required_flags = vk::MemoryPropertyFlagBits::eHostVisible |
                                                   vk::MemoryPropertyFlagBits::eHostCoherent,
                                 .buffer_usage = vk::BufferUsageFlagBits::eStorageBuffer,
                                 .vma_usage = VMA_MEMORY_USAGE_CPU_TO_GPU};
        for (u32 i = 0; i < instance_count; i++)
        {
            data[i].draw_instances.init(device, buf);
        }
        return data;
    }

    static bool update_quads_descriptor_set(DrawStream *stream, QuadsStream &gpu_data, void *gpu_backend)
    {
        auto *pipeline = stream->pipeline;
        auto *ctx = get_agrb_context(gpu_backend);
        if (!pipeline || !pipeline->descriptor_set_layout || !ctx || !ctx->descriptor_pool) return false;

        vk::DescriptorBufferInfo buffer_info{gpu_data.draw_instances.data().vk_buffer, 0, VK_WHOLE_SIZE};
        agrb::descriptor_writer writer(*pipeline->descriptor_set_layout, *ctx->descriptor_pool);
        writer.write_buffer(0, &buffer_info);
        if (!gpu_data.descriptor_set) return writer.build(gpu_data.descriptor_set);
        writer.overwrite(gpu_data.descriptor_set);
        return true;
    }

    void render_quads_stream(DrawStream *stream, void *render_ctx, void *gpu_backend, u32 frame_id)
    {
        if (stream->draw_sizes[frame_id] == 0) return;
        auto &gpu_data = static_cast<QuadsStream *>(stream->stream_instances)[frame_id];
        if (!update_quads_descriptor_set(stream, gpu_data, gpu_backend)) return;
        auto *pipeline = stream->pipeline;
        auto &device = get_agrb_device(gpu_backend);
        auto &cmd = *static_cast<vk::CommandBuffer *>(render_ctx);
        auto &loader = device.loader;
        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline->handle, loader);
        cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline->layout, 0, 1, &gpu_data.descriptor_set, 0,
                               nullptr, loader);
        cmd.pushConstants(pipeline->layout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(acul::point2D<u32>),
                          &get_context().window_size, loader);
        cmd.draw(6, stream->draw_sizes[frame_id], 0, 0, loader);
    }

    void init_quads_pipeline_calls(QuadsGPUDispatch &dispatch)
    {
        dispatch.push_data_to_quads_stream = &push_data_to_quads_stream;
        dispatch.clear_quads_stream = &clear_quads_stream;
        dispatch.render_quads_stream = &render_quads_stream;
        dispatch.create_quads_stream_gpu_data = &create_quads_stream_gpu_data;
        dispatch.destroy_quads_stream_gpu_data = &destroy_quads_stream_gpu_data;
    }

} // namespace auik::v2::detail

namespace auik::v2
{
    bool construct_quads_pipeline(DrawPipeline &pipeline, agrb::device &device)
    {
        pipeline.descriptor_set_layout =
            agrb::descriptor_set_layout::builder()
                .add_binding(0, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eVertex)
                .build(device);
        if (!pipeline.descriptor_set_layout) return false;

        const vk::DescriptorSetLayout set_layouts[] = {pipeline.descriptor_set_layout->layout()};
        const vk::PushConstantRange push_constant{vk::ShaderStageFlagBits::eVertex, 0, sizeof(acul::point2D<u32>)};
        vk::PipelineLayoutCreateInfo pipeline_layout_info{};
        pipeline_layout_info.setLayoutCount = 1;
        pipeline_layout_info.pSetLayouts = set_layouts;
        pipeline_layout_info.pushConstantRangeCount = 1;
        pipeline_layout_info.pPushConstantRanges = &push_constant;
        pipeline.layout = device.vk_device.createPipelineLayout(pipeline_layout_info, nullptr, device.loader);
        return pipeline.layout != nullptr;
    }

    bool configure_quads_pipeline(agrb::graphics_pipeline_batch::artifact &artifact, vk::RenderPass render_pass,
                                  DrawPipeline &pipeline, agrb::device &device)
    {
        auto *tmp = static_cast<agrb::graphics_pipeline_batch::artifact::custom_data_t<u32> *>(artifact.tmp);
        if (!tmp) return false;

        artifact.config.load_defaults().enable_alpha_blending();
        artifact.config.depth_stencil_info.setDepthTestEnable(true).setDepthWriteEnable(true);
        artifact.config.render_pass = render_pass;
        artifact.config.pipeline_layout = pipeline.layout;
        artifact.config.subpass = tmp->value;

        auto *ctx = detail::get_agrb_context(detail::get_context().gpu_backend);
        if (!ctx) return false;

        const auto &path = detail::get_shader_library_path();
        vk::ShaderModule shaders[2];
        auto vs = agrb::get_shader(AS_AUIK_QUADS_VS, shaders[0], ctx->shader_cache, device, path);
        if (!vs.success()) return false;
        auto fs = agrb::get_shader(AS_AUIK_QUADS_FS, shaders[1], ctx->shader_cache, device, path);
        if (!fs.success()) return false;
        agrb::prepare_base_graphics_pipeline(artifact, shaders, device);
        return true;
    }
} // namespace auik::v2
