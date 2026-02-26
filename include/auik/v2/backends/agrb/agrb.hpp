#pragma once

#include <agrb/descriptors.hpp>
#include <agrb/device.hpp>
#include <agrb/pipeline.hpp>
#include "../../draw.hpp"

namespace auik::v2
{
    APPLIB_API detail::GPUContext *create_agrb_backend(agrb::device &device, agrb::descriptor_pool *descriptor_pool);
    APPLIB_API void clear_shader_cache(agrb::device &device);

    struct DrawPipeline
    {
        vk::Pipeline handle = nullptr;
        vk::PipelineLayout layout = nullptr;
        acul::shared_ptr<agrb::descriptor_set_layout> descriptor_set_layout = nullptr;
    };

    inline void construct_pipeline_artifact(agrb::graphics_pipeline_batch::artifact &artifact, u32 subpass,
                                            DrawPipeline *pipeline)
    {
        artifact.tmp = acul::alloc<agrb::graphics_pipeline_batch::artifact::custom_data_t<u32>>(subpass);
        artifact.commit = [pipeline](vk::Pipeline handle) { pipeline->handle = handle; };
    }

    APPLIB_API void destroy_draw_pipeline(DrawPipeline &pipeline, agrb::device &device);
} // namespace auik::v2
