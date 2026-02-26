#pragma once
#include <auik/v2/pipelines.hpp>

namespace auik::v2::detail
{
    struct TransientStreamData
    {
        acul::vector<Widget *> widgets_cache;
    };
    struct CachedStreamData
    {
        i32 write_id = -1;
    };
} // namespace auik::v2::detail
