#include <agrb/vector.hpp>
#include <auik/v2/pipelines/quads.hpp>

namespace auik::v2::detail
{
    void push_data_to_quads_stream(DrawStream *stream, void *data)
    {
        u32 frame_id = get_context().frame_id;
        stream->write_id = frame_id;
        auto *gpu_data = static_cast<QuadsStream *>(stream->stream_instances[frame_id]);
        auto *pVector = static_cast<agrb::vector<QuadsInstanceData> *>(gpu_data->draw_instances);
        pVector->push_back(*static_cast<QuadsInstanceData *>(data));
        ++stream->sizes[frame_id];
    }

    void clear_quads_stream(DrawStream *stream, VkCommandBuffer cmd)
    {
        auto *gpu_data = static_cast<QuadsStream *>(stream->stream_instances[stream->write_id]);
        auto *pVector = static_cast<agrb::vector<QuadsInstanceData> *>(gpu_data->draw_instances);
        pVector->clear();
        stream->sizes[stream->write_id] = 0;
    }

    void destroy_quads_stream_gpu_data(DrawStream *stream)
    {
        auto *gpu_data = static_cast<QuadsStream *>(stream->stream_instances[stream->write_id]);
        auto *pVector = static_cast<agrb::vector<QuadsInstanceData> *>(gpu_data->draw_instances);
        pVector->destroy();
        acul::release(gpu_data);
    }

    void *allocate_quads_stream_buffer()
    {
        auto *pVector = acul::alloc<agrb::vector<QuadsInstanceData>>();
        // todo: init by gpu context
        return pVector;
    }
} // namespace auik::v2::detail