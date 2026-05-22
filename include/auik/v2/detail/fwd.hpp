#pragma once
#include <acul/scalars.hpp>

namespace auik::v2
{
    constexpr u32 AUIK_INVALID_DRAW_DATA_ID = 0xFFFFFFFFu;
    enum class StyleState : u8;

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

    class Theme;
    class Widget;
    class Image;
    class ImageButton;
    class Tooltip;
    struct DrawStream;
    struct DrawPipeline;
    struct PostEffect;
    struct SoundContext;

    namespace detail
    {
        struct WindowContext;
        struct Context;
        struct GPUContext;
    } // namespace detail
} // namespace auik::v2
