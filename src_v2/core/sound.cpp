#include <auik/v2/auik.hpp>
#include <auik/v2/sound.hpp>
#include <acul/memory/alloc.hpp>

#ifdef _WIN32
    #include <playsoundapi.h>
#elif defined(__linux__)
    #include <dlfcn.h>
#endif

namespace auik::v2
{
    namespace
    {
        [[maybe_unused]] void noop_play_system_hand_sound(SoundContext *) {}

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
#endif
    } // namespace

    APPLIB_API SoundContext *init_sound_system()
    {
#ifdef _WIN32
        static SoundContext ctx{&win32_play_system_hand_sound};
        return &ctx;
#elif defined(__linux__)
        auto *ctx = acul::alloc<LinuxSoundContext>();
        ctx->play_system_hand_sound = &noop_play_system_hand_sound;
        ctx->canberra = open_canberra();
        if (!ctx->canberra) return ctx;

        auto ca_context_create =
            reinterpret_cast<PFN_ca_context_create>(dlsym(ctx->canberra, "ca_context_create"));
        ctx->ca_context_destroy =
            reinterpret_cast<PFN_ca_context_destroy>(dlsym(ctx->canberra, "ca_context_destroy"));
        ctx->ca_context_play = reinterpret_cast<PFN_ca_context_play>(dlsym(ctx->canberra, "ca_context_play"));

        if (!ca_context_create || !ctx->ca_context_destroy || !ctx->ca_context_play ||
            ca_context_create(&ctx->ca) != 0 || !ctx->ca)
        {
            destroy_sound_system(ctx);
            ctx = acul::alloc<LinuxSoundContext>();
            ctx->play_system_hand_sound = &noop_play_system_hand_sound;
            return ctx;
        }

        ctx->play_system_hand_sound = &linux_play_system_hand_sound;
        return ctx;
#else
        static SoundContext ctx{&noop_play_system_hand_sound};
        return &ctx;
#endif
    }

    APPLIB_API void destroy_sound_system(SoundContext *ctx)
    {
        if (!ctx) return;
#if defined(__linux__)
        auto *linux_ctx = static_cast<LinuxSoundContext *>(ctx);
        if (linux_ctx->ca && linux_ctx->ca_context_destroy) linux_ctx->ca_context_destroy(linux_ctx->ca);
        if (linux_ctx->canberra) dlclose(linux_ctx->canberra);
        acul::release(linux_ctx);
#else
        (void)ctx;
#endif
    }

    APPLIB_API SoundContext *get_sound_context()
    {
        if (!detail::g_context) return nullptr;
        return detail::get_context().sound_ctx;
    }

    APPLIB_API void play_system_hand_sound(SoundContext *ctx)
    {
        if (!ctx || !ctx->play_system_hand_sound) return;
        ctx->play_system_hand_sound(ctx);
    }

    APPLIB_API void play_system_hand_sound()
    {
        play_system_hand_sound(get_sound_context());
    }
} // namespace auik::v2
