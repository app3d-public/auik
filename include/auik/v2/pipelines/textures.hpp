#pragma once

#include <acul/scalars.hpp>
#include <amal/vector.hpp>

namespace auik::v2
{
    struct TexturesInstanceData
    {
        amal::vec2 position;
        amal::vec2 size;
        f32 z_order;
        u32 texture_id;
        amal::vec2 texture_offset;
    };
} // namespace auik::v2