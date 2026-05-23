#include <acul/memory/alloc.hpp>
#include <auik/v2/detail/context.hpp>
#include <auik/v2/sound.hpp>

#ifdef _WIN32
    #include <windows.h>
    // Include windows.h first
    #include <playsoundapi.h>
#elif defined(__linux__)
    #include <dlfcn.h>
#endif

namespace auik::v2
{
    namespace
    {
        [[maybe_unused]] void noop_play_system_hand_sound(SoundContext *) {}

        void release_sound_context(SoundContext *ctx) { acul::release(ctx); }

        void noop_init_sound_system(SoundContext *) {}

#ifdef _WIN32
        void win32_play_system_hand_sound(SoundContext *)
        {
            PlaySound(TEXT("SystemHand"), NULL, SND_ALIAS | SND_ASYNC);
        }
#elif defined(__linux__)
        struct ca_context;

        using PFN_ca_context_create = int (*)(ca_context **);
        using PFN_ca_context_destroy = void (*)(ca_context *);
        using PFN_ca_context_play = int (*)(ca_context *, u32, ...);

        struct LinuxSoundContext : SoundContext
        {
            void *canberra = nullptr;
            ca_context *ca = nullptr;
            PFN_ca_context_destroy ca_context_destroy = nullptr;
            PFN_ca_context_play ca_context_play = nullptr;
        };

        void linux_play_system_hand_sound(SoundContext *ctx)
        {
            auto *linux_ctx = static_cast<LinuxSoundContext *>(ctx);
            if (!linux_ctx || !linux_ctx->ca || !linux_ctx->ca_context_play) return;

            linux_ctx->ca_context_play(linux_ctx->ca, 0u, "event.id", "dialog-error", "event.description",
                                       "System hand", nullptr);
        }

        void *open_canberra()
        {
            if (void *handle = dlopen("libcanberra.so.0", RTLD_LAZY | RTLD_LOCAL)) return handle;
            return dlopen("libcanberra.so", RTLD_LAZY | RTLD_LOCAL);
        }

        void linux_destroy_sound_system(SoundContext *ctx)
        {
            auto *linux_ctx = static_cast<LinuxSoundContext *>(ctx);
            auto *ca_context_destroy = linux_ctx->ca_context_destroy;
            auto *ca = linux_ctx->ca;
            auto *canberra = linux_ctx->canberra;
            if (ca && ca_context_destroy) ca_context_destroy(ca);
            if (canberra) dlclose(canberra);
            acul::release(linux_ctx);
        }

        void linux_init_sound_system(SoundContext *ctx)
        {
            auto *linux_ctx = static_cast<LinuxSoundContext *>(ctx);
            linux_ctx->play_system_hand_sound = &noop_play_system_hand_sound;
            linux_ctx->canberra = open_canberra();
            if (!linux_ctx->canberra) return;

            auto ca_context_create =
                reinterpret_cast<PFN_ca_context_create>(dlsym(linux_ctx->canberra, "ca_context_create"));
            linux_ctx->ca_context_destroy =
                reinterpret_cast<PFN_ca_context_destroy>(dlsym(linux_ctx->canberra, "ca_context_destroy"));
            linux_ctx->ca_context_play =
                reinterpret_cast<PFN_ca_context_play>(dlsym(linux_ctx->canberra, "ca_context_play"));

            if (!ca_context_create || !linux_ctx->ca_context_destroy || !linux_ctx->ca_context_play ||
                ca_context_create(&linux_ctx->ca) != 0 || !linux_ctx->ca)
            {
                if (linux_ctx->canberra) dlclose(linux_ctx->canberra);
                linux_ctx->canberra = nullptr;
                linux_ctx->ca = nullptr;
                linux_ctx->ca_context_destroy = nullptr;
                linux_ctx->ca_context_play = nullptr;
                return;
            }

            linux_ctx->play_system_hand_sound = &linux_play_system_hand_sound;
        }
#endif
    } // namespace

    APPLIB_API SoundContext *get_default_sound_context()
    {
#ifdef _WIN32
        SoundContext *ctx = acul::alloc<SoundContext>();
        ctx->init = &noop_init_sound_system;
        ctx->destroy = &release_sound_context;
        ctx->play_system_hand_sound = &win32_play_system_hand_sound;
#elif defined(__linux__)
        LinuxSoundContext *ctx = acul::alloc<LinuxSoundContext>();
        ctx->init = &linux_init_sound_system;
        ctx->destroy = &linux_destroy_sound_system;
        ctx->play_system_hand_sound = &noop_play_system_hand_sound;
#else
        SoundContext *ctx = acul::alloc<SoundContext>();
        ctx->init = &noop_init_sound_system;
        ctx->destroy = &release_sound_context;
        ctx->play_system_hand_sound = &noop_play_system_hand_sound;
#endif
        return ctx;
    }

} // namespace auik::v2
