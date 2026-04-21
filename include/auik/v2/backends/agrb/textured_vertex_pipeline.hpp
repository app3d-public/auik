#pragma once

#include "agrb.hpp"
#include <agrb/pipeline.hpp>

namespace auik::v2
{
    APPLIB_API bool construct_textured_vertex_pipeline(DrawPipeline &pipeline, agrb::device &device);
    APPLIB_API bool configure_textured_vertex_pipeline(agrb::graphics_pipeline_batch::artifact &artifact,
                                                       vk::RenderPass render_pass, DrawPipeline &pipeline,
                                                       agrb::device &device);
}
