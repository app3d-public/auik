#pragma once

#include <acul/scalars.hpp>

namespace auik
{
    enum class StyleState : u8;

    struct TextureID;
    struct DrawDataID;
    class Theme;
    class Widget;
    class Window;
    class Dockspace;
    class Image;
    class ImageButton;
    class Tooltip;
    struct Viewport;
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
} // namespace auik
