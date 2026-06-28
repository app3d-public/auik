#pragma once

#include <acul/scalars.hpp>
#include <amal/vector.hpp>
#include "post_effects.hpp"

namespace auik
{
    class Widget;
    struct AnimationState;

    using PFN_animation_start = void (*)(AnimationState *, Widget *owner, void *user_data, f64 now);
    using PFN_animation_tick = bool (*)(AnimationState *, Widget *owner, void *user_data, f64 now);
    using PFN_animation_finish = void (*)(AnimationState *, Widget *owner, void *user_data);
    using PFN_animation_destroy = void (*)(AnimationState *, Widget *owner, void *user_data);
    using PFN_scale_animation_finish = void (*)(AnimationState *, Widget *owner, void *user_data,
                                                const ScalePostRuntimeData &data);
    using PFN_rotate_animation_finish = void (*)(AnimationState *, Widget *owner, void *user_data,
                                                 const RotatePostRuntimeData &data);

    struct AnimationState
    {
        u32 post_data_id = AUIK_INVALID_POST_EFFECT_DATA_ID;
        f64 duration = 0.0;
        f32 from = 0.0f;
        f32 to = 1.0f;
        f32 current = 1.0f;
        amal::vec2 center{0.0f, 0.0f};
        PFN_animation_start at_start = nullptr;
        PFN_animation_tick tick = nullptr;
        PFN_animation_finish at_finish = nullptr;
        PFN_animation_destroy destroy = nullptr;
        PFN_scale_animation_finish scale_finish = nullptr;
        PFN_rotate_animation_finish rotate_finish = nullptr;
        bool is_active = false;
        bool tick_scheduled = false;

        AUIK_EXPORT bool next_frame(f64 now, Widget *owner, void *user_data = nullptr);
        AUIK_EXPORT void clear(Widget *owner = nullptr, void *user_data = nullptr);
        AUIK_EXPORT void cancel(Widget *owner = nullptr, void *user_data = nullptr);
        bool active() const { return is_active; }
    };

    AUIK_EXPORT void configure_scale_animation(AnimationState &state, f64 duration, f32 from = 0.0f, f32 to = 1.0f,
                                               PFN_scale_animation_finish finish = nullptr);
    AUIK_EXPORT void configure_rotate_animation(AnimationState &state, f64 duration, amal::vec2 center, f32 from,
                                                f32 to, PFN_rotate_animation_finish finish = nullptr);
    AUIK_EXPORT AnimationState *start_animation(AnimationState &state, Widget *owner, void *user_data = nullptr);
    AUIK_EXPORT bool get_animation_state_data(const AnimationState &state, f32 *value);
    AUIK_EXPORT f32 get_default_animation_progress(f64 start_time, f64 duration, f64 now);
    inline f32 get_default_animation_f32(f32 from, f32 to, f64 start_time, f64 duration, f64 now)
    {
        const f32 t = get_default_animation_progress(start_time, duration, now);
        return from + (to - from) * t;
    }
    AUIK_EXPORT f64 current_animation_time();
    AUIK_EXPORT bool next_animation_frame(AnimationState &state, Widget *owner, void *user_data = nullptr);
} // namespace auik
