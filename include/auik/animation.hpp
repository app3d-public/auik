#pragma once

#include <acul/scalars.hpp>
#include <amal/geometric.hpp>
#include "post_effects.hpp"

namespace auik
{
    class Widget;
    struct ScrollData;
    struct AnimationState;

    using PFN_animation_start = void (*)(AnimationState *, Widget *owner, void *user_data, f64 now);
    using PFN_animation_tick = bool (*)(AnimationState *, Widget *owner, void *user_data, f64 now);
    using PFN_animation_finish = void (*)(AnimationState *, Widget *owner, void *user_data);
    using PFN_animation_destroy = void (*)(AnimationState *, Widget *owner, void *user_data);
    using PFN_animation_complete = void (*)(AnimationState *, Widget *owner, void *user_data, void *runtime_data);

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
        PFN_animation_complete on_complete = nullptr;
        bool is_active = false;
        bool tick_scheduled = false;
        u64 task_owner_id = 0u;

        AUIK_EXPORT bool next_frame(f64 now, Widget *owner, void *user_data = nullptr);
        AUIK_EXPORT void clear(Widget *owner = nullptr, void *user_data = nullptr);
        AUIK_EXPORT void cancel(Widget *owner = nullptr, void *user_data = nullptr);
        bool active() const { return is_active; }
    };

    AUIK_EXPORT void configure_scale_animation(AnimationState &state, f64 duration, f32 from = 0.0f, f32 to = 1.0f,
                                               PFN_animation_complete complete = nullptr);
    AUIK_EXPORT void configure_rotate_animation(AnimationState &state, f64 duration, amal::vec2 center, f32 from,
                                                f32 to, PFN_animation_complete complete = nullptr);
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
    AUIK_EXPORT AnimationState *animate_scroll_to(ScrollData &scroll, f32 offset, amal::axis axis, f64 duration,
                                                  PFN_animation_complete complete = nullptr);
} // namespace auik
