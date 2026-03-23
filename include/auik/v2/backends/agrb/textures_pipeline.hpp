#pragma once

#include "agrb.hpp"
#include <agrb/pipeline.hpp>

namespace auik::v2
{
    APPLIB_API bool construct_textures_pipeline(DrawPipeline &pipeline, agrb::device &device);
    APPLIB_API bool configure_textures_pipeline(agrb::graphics_pipeline_batch::artifact &artifact,
                                                vk::RenderPass render_pass, DrawPipeline &pipeline,
                                                agrb::device &device);
}
