#include <acul/scalars.hpp>
#include <agrb/vector.hpp>
#include <auik/v2/backends/agrb.hpp>
#include <auik/v2/detail/quads_dispatch.hpp>
#include <auik/v2/pipelines.hpp>

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
            // create descriptor set here
        }
        return data;
    }

    void render_quads_stream(DrawStream *stream, void *render_ctx, void *gpu_backend, u32 frame_id)
    {
        if (stream->draw_sizes[frame_id] == 0) return;
        auto &gpu_data = static_cast<QuadsStream *>(stream->stream_instances)[frame_id];
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
