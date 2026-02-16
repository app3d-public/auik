#pragma once

#include <amal/vector.hpp>
#include "../draw.hpp"

namespace auik::v2::detail
{
    struct QuadsInstanceData
    {
        amal::vec2 position;
        amal::vec2 size;
        amal::vec4 background_color;
        amal::vec4 border_color;
        f32 border_radius;
        f32 border_thickness;
        f32 z_order;
        u32 corner_mask;
    };

    struct QuadsStream
    {
        void *draw_instances;
        VkDescriptorSet descriptor_set;
    };

    APPLIB_API void push_data_to_quads_stream(DrawStream *, void *);
    APPLIB_API void clear_quads_stream(DrawStream *, VkCommandBuffer);

    inline void render_quads_stream(DrawStream *stream, VkCommandBuffer cmd)
    {
        u32 frame_id = get_context().frame_id;
        auto *gpu_data = static_cast<QuadsStream *>(stream->stream_instances[frame_id]);
        auto &pipeline = stream->pipeline;
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->handle);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->layout, 0, 1, &gpu_data->descriptor_set,
                                0, nullptr);
        vkCmdPushConstants(cmd, pipeline->layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(acul::point2D<u32>),
                           &get_context().window_size);
        vkCmdDraw(cmd, 6, stream->sizes[frame_id], 0, 0);
        stream->write_id = frame_id;
    }

    APPLIB_API void *allocate_quads_stream_buffer();
    APPLIB_API void destroy_quads_stream_gpu_data(DrawStream *stream);

    inline void create_ret_quads_stream(DrawPipeline *pipeline, DrawStream &stream)
    {
        stream.push_data_to_stream = push_data_to_quads_stream;
        stream.clear_stream = clear_quads_stream;
        stream.render = render_quads_stream;
        stream.next_call = render_quads_stream;
        stream.destroy = destroy_quads_stream_gpu_data;
        auto &ctx = get_context();
        stream.stream_instances = (void **)acul::alloc_n<QuadsStream *>(ctx.frames_in_flight);
        for (u32 i = 0; i < ctx.frames_in_flight; i++)
            stream.stream_instances[i] = acul::alloc<QuadsStream>(allocate_quads_stream_buffer());
        stream.pipeline = pipeline;
        stream.sizes = (u32 *)acul::alloc_n<u32>(ctx.frames_in_flight);
    }

    inline void create_im_quads_stream(DrawPipeline *pipeline, DrawStream &stream)
    {
        create_ret_quads_stream(pipeline, stream);
        stream.clear_stream = stream.render;
    }
} // namespace auik::v2::detail