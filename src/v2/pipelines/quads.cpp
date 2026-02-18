#include <auik/v2/pipelines.hpp>
#include "quads_stream.hpp"

namespace auik::v2::detail
{
    void push_data_to_quads_stream(DrawStream *, void *);

    static void push_widget_quads_stream_immediate(DrawStream *stream, Widget *widget)
    {
        auto *stream_im = static_cast<QuadsStreamIm *>(stream->stream_instances[get_context().frame_id]);
        stream_im->widgets_cache.push_back(widget);
    }

    void clear_quads_stream(DrawStream *, VkCommandBuffer, u32 frame_id);

    static void render_quads_stream(DrawStream *stream, VkCommandBuffer cmd)
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

    static void render_quads_stream_retained(DrawStream *stream, VkCommandBuffer cmd)
    {
        clear_quads_stream(stream, cmd, stream->write_id);
        render_quads_stream(stream, cmd);
    }

    static void render_quads_stream_immediate(DrawStream *stream, VkCommandBuffer cmd)
    {
        u32 frame_id = get_context().frame_id;
        clear_quads_stream(stream, cmd, frame_id);
        auto *stream_im = static_cast<QuadsStreamIm *>(stream->stream_instances[frame_id]);
        for (auto &widget : stream_im->widgets_cache) widget->update_immediate_commands();
        render_quads_stream(stream, cmd);
    }

    void *allocate_quads_stream_buffer();

    void destroy_quads_stream_gpu_data(DrawStream *stream);

    static inline void create_quads_stream(DrawPipeline *pipeline, DrawStream &stream)
    {
        stream.push_data_to_stream = push_data_to_quads_stream;
        stream.destroy = destroy_quads_stream_gpu_data;
        stream.pipeline = pipeline;
        stream.sizes = (u32 *)acul::alloc_n<u32>(get_context().frames_in_flight);
    }

    void create_quads_stream_retained(DrawPipeline *pipeline, DrawStream &stream)
    {
        create_quads_stream(pipeline, stream);
        auto &ctx = get_context();
        stream.stream_instances = (void **)acul::alloc_n<QuadsStream *>(ctx.frames_in_flight);
        for (u32 i = 0; i < ctx.frames_in_flight; i++)
            stream.stream_instances[i] = acul::alloc<QuadsStream>(allocate_quads_stream_buffer());
        stream.render = render_quads_stream_retained;
    }

    void create_quads_stream_immediate(DrawPipeline *pipeline, DrawStream &stream)
    {
        create_quads_stream(pipeline, stream);
        stream.push_widget_to_cache = push_widget_quads_stream_immediate;
        auto &ctx = get_context();
        stream.stream_instances = (void **)acul::alloc_n<QuadsStreamIm *>(ctx.frames_in_flight);
        for (u32 i = 0; i < ctx.frames_in_flight; i++)
        {
            stream.stream_instances[i] = acul::alloc<QuadsStreamIm>();
            auto *stream_im = static_cast<QuadsStreamIm *>(stream.stream_instances[i]);
            stream_im->draw_instances = allocate_quads_stream_buffer();
        }
        stream.render = render_quads_stream_immediate;
    }
} // namespace auik::v2