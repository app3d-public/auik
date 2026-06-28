#pragma once

#include <acul/scalars.hpp>

namespace auik
{
    constexpr u32 AUIK_INVALID_DRAW_DATA_ID = 0xFFFFFFFFu;

    struct TextureID
    {
        u64 handle = 0;
        u32 bind_slot = AUIK_INVALID_DRAW_DATA_ID;
    };

    constexpr TextureID AUIK_INVALID_TEXTURE_ID{};

    struct DrawDataID
    {
        u32 render_id = AUIK_INVALID_DRAW_DATA_ID;
        u32 hit_id = AUIK_INVALID_DRAW_DATA_ID;
    };

    namespace detail
    {
        inline bool has_render_record(const DrawDataID &draw_id)
        { return draw_id.render_id != AUIK_INVALID_DRAW_DATA_ID; }
    } // namespace detail
} // namespace auik
