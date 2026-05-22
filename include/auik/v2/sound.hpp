#pragma once

#include <acul/api.hpp>
#include <acul/scalars.hpp>

namespace auik::v2
{
    struct SoundContext;

    using PFN_play_system_hand_sound = void (*)(SoundContext *);

    struct SoundContext
    {
        PFN_play_system_hand_sound play_system_hand_sound = nullptr;
    };

    APPLIB_API SoundContext *init_sound_system();
    APPLIB_API void destroy_sound_system(SoundContext *ctx);
    APPLIB_API SoundContext *get_sound_context();
    APPLIB_API void play_system_hand_sound(SoundContext *ctx);
    APPLIB_API void play_system_hand_sound();
} // namespace auik::v2
