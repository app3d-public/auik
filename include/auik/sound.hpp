#pragma once

#include <acul/scalars.hpp>
#include <auik/symbol_export.h>
#include <cassert>

namespace auik
{
    struct SoundContext;

    using PFN_init_sound_system = void (*)(SoundContext *);
    using PFN_destroy_sound_system = void (*)(SoundContext *);
    using PFN_play_system_hand_sound = void (*)(SoundContext *);

    struct SoundContext
    {
        PFN_init_sound_system init = nullptr;
        PFN_destroy_sound_system destroy = nullptr;
        PFN_play_system_hand_sound play_system_hand_sound = nullptr;
    };

    AUIK_EXPORT SoundContext *get_default_sound_context();

    inline void init_sound_system(SoundContext *ctx)
    {
        assert(ctx && "invalid sound context");
        auto *init = ctx->init;
        if (init) init(ctx);
    }

    inline void destroy_sound_system(SoundContext *ctx)
    {
        assert(ctx && "invalid sound context");
        auto *destroy = ctx->destroy;
        if (destroy) destroy(ctx);
    }

    inline void play_system_hand_sound(SoundContext *ctx)
    {
        assert(ctx && "invalid sound context");
        auto *play = ctx->play_system_hand_sound;
        if (play) play(ctx);
    }
} // namespace auik
