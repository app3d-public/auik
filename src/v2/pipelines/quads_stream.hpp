#pragma once
#include <auik/v2/draw.hpp>

namespace auik::v2::detail
{
    struct QuadsStream
    {
        void *draw_instances;
        VkDescriptorSet descriptor_set;
    };

    struct QuadsStreamIm final : QuadsStream
    {
        acul::vector<Widget *> widgets_cache;
    };
} // namespace auik::v2::detail