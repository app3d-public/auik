#include <auik/animation.hpp>
#include <auik/auik.hpp>
#include <amal/geometric.hpp>

namespace auik
{
    namespace
    {
        void start_scale_animation(AnimationState *state, Widget *owner, void *user_data, f64)
        {
            (void)user_data;
            if (!state || !owner) return;
            if (state->post_data_id != AUIK_INVALID_POST_EFFECT_DATA_ID)
                release_scale_post_effect_data(get_scale_post_effect(), state->post_data_id);
            state->post_data_id = create_scale_post_effect_data(get_scale_post_effect(), owner);
            auto *scale_data = get_scale_post_effect_data(get_scale_post_effect(), state->post_data_id);
            if (!scale_data) return;
            scale_data->animation_start = current_animation_time();
            scale_data->animation_from = {state->from, 1.0f};
            scale_data->animation_to = {state->to, 1.0f};
            scale_data->scale = scale_data->animation_from;
            scale_data->animating = true;
            state->current = state->from;
            push_widget_to_transient_cache(owner);
        }

        bool tick_scale_animation(AnimationState *state, Widget *, void *, f64 now)
        {
            auto *scale_data = state ? get_scale_post_effect_data(get_scale_post_effect(), state->post_data_id)
                                     : nullptr;
            if (!state || !scale_data || !scale_data->animating) return false;
            const f32 eased = get_default_animation_progress(scale_data->animation_start, state->duration, now);
            scale_data->scale =
                scale_data->animation_from + (scale_data->animation_to - scale_data->animation_from) * eased;
            state->current = state->from + (state->to - state->from) * eased;
            return eased < 1.0f;
        }

        void finish_scale_animation(AnimationState *state, Widget *owner, void *user_data)
        {
            auto *scale_data = state ? get_scale_post_effect_data(get_scale_post_effect(), state->post_data_id)
                                     : nullptr;
            if (scale_data)
            {
                scale_data->scale = scale_data->animation_to;
                scale_data->animating = false;
                state->current = state->to;
                if (state->scale_finish) state->scale_finish(state, owner, user_data, *scale_data);
            }
            if (state && state->post_data_id != AUIK_INVALID_POST_EFFECT_DATA_ID)
            {
                release_scale_post_effect_data(get_scale_post_effect(), state->post_data_id);
                state->post_data_id = AUIK_INVALID_POST_EFFECT_DATA_ID;
            }
            if (owner) erase_widget_from_transient_cache(owner);
        }

        void destroy_scale_animation(AnimationState *state, Widget *owner, void *user_data)
        {
            (void)user_data;
            if (!state) return;
            if (state->post_data_id != AUIK_INVALID_POST_EFFECT_DATA_ID)
            {
                release_scale_post_effect_data(get_scale_post_effect(), state->post_data_id);
                state->post_data_id = AUIK_INVALID_POST_EFFECT_DATA_ID;
            }
            if (owner) erase_widget_from_transient_cache(owner);
        }

        void start_rotate_animation(AnimationState *state, Widget *owner, void *user_data, f64)
        {
            (void)user_data;
            if (!state || !owner) return;
            if (state->post_data_id != AUIK_INVALID_POST_EFFECT_DATA_ID)
                release_rotate_post_effect_data(get_rotate_post_effect(), state->post_data_id);
            state->post_data_id = create_rotate_post_effect_data(get_rotate_post_effect(), owner);
            auto *rotate_data = get_rotate_post_effect_data(get_rotate_post_effect(), state->post_data_id);
            if (!rotate_data) return;
            rotate_data->animation_start = current_animation_time();
            rotate_data->animation_from = state->from;
            rotate_data->animation_to = state->to;
            rotate_data->angle = state->from;
            rotate_data->center = state->center;
            rotate_data->animating = true;
            push_widget_to_transient_cache(owner);
        }

        bool tick_rotate_animation(AnimationState *state, Widget *, void *, f64 now)
        {
            auto *rotate_data = state ? get_rotate_post_effect_data(get_rotate_post_effect(), state->post_data_id)
                                      : nullptr;
            if (!state || !rotate_data || !rotate_data->animating) return false;
            const f32 eased = get_default_animation_progress(rotate_data->animation_start, state->duration, now);
            rotate_data->angle = rotate_data->animation_from + (rotate_data->animation_to - rotate_data->animation_from) * eased;
            return eased < 1.0f;
        }

        void finish_rotate_animation(AnimationState *state, Widget *owner, void *user_data)
        {
            auto *rotate_data = state ? get_rotate_post_effect_data(get_rotate_post_effect(), state->post_data_id)
                                      : nullptr;
            if (rotate_data)
            {
                rotate_data->angle = rotate_data->animation_to;
                rotate_data->animating = false;
                if (state->rotate_finish) state->rotate_finish(state, owner, user_data, *rotate_data);
            }
            if (state && state->post_data_id != AUIK_INVALID_POST_EFFECT_DATA_ID)
            {
                release_rotate_post_effect_data(get_rotate_post_effect(), state->post_data_id);
                state->post_data_id = AUIK_INVALID_POST_EFFECT_DATA_ID;
            }
            if (owner) erase_widget_from_transient_cache(owner);
        }

        void destroy_rotate_animation(AnimationState *state, Widget *owner, void *user_data)
        {
            (void)user_data;
            if (!state) return;
            if (state->post_data_id != AUIK_INVALID_POST_EFFECT_DATA_ID)
            {
                release_rotate_post_effect_data(get_rotate_post_effect(), state->post_data_id);
                state->post_data_id = AUIK_INVALID_POST_EFFECT_DATA_ID;
            }
            if (owner) erase_widget_from_transient_cache(owner);
        }
    } // namespace

    bool AnimationState::next_frame(f64 now, Widget *owner, void *user_data)
    {
        if (!is_active) return false;
        if (!user_data) user_data = owner;
        const bool still_active = tick && tick(this, owner, user_data, now);
        if (still_active) return true;
        if (at_finish) at_finish(this, owner, user_data);
        is_active = false;
        return false;
    }

    void AnimationState::clear(Widget *owner, void *user_data)
    {
        if (!user_data) user_data = owner;
        if (is_active && at_finish) at_finish(this, owner, user_data);
        if (destroy) destroy(this, owner, user_data);
        post_data_id = AUIK_INVALID_POST_EFFECT_DATA_ID;
        duration = 0.0;
        from = 0.0f;
        to = 1.0f;
        current = 1.0f;
        center = {0.0f, 0.0f};
        at_start = nullptr;
        tick = nullptr;
        at_finish = nullptr;
        destroy = nullptr;
        scale_finish = nullptr;
        rotate_finish = nullptr;
        is_active = false;
        tick_scheduled = false;
    }

    void AnimationState::cancel(Widget *owner, void *user_data)
    {
        if (!user_data) user_data = owner;
        if (destroy) destroy(this, owner, user_data);
        post_data_id = AUIK_INVALID_POST_EFFECT_DATA_ID;
        is_active = false;
        tick_scheduled = false;
    }

    void configure_scale_animation(AnimationState &state, f64 duration, f32 from, f32 to,
                                   PFN_scale_animation_finish finish)
    {
        if (state.tick != tick_scale_animation)
        {
            state.clear();
            state.at_start = start_scale_animation;
            state.tick = tick_scale_animation;
            state.at_finish = finish_scale_animation;
            state.destroy = destroy_scale_animation;
        }
        state.duration = duration;
        state.from = from;
        state.to = to;
        state.current = from;
        state.scale_finish = finish;
        state.rotate_finish = nullptr;
    }

    void configure_rotate_animation(AnimationState &state, f64 duration, amal::vec2 center, f32 from, f32 to,
                                    PFN_rotate_animation_finish finish)
    {
        if (state.tick != tick_rotate_animation)
        {
            state.clear();
            state.at_start = start_rotate_animation;
            state.tick = tick_rotate_animation;
            state.at_finish = finish_rotate_animation;
            state.destroy = destroy_rotate_animation;
        }
        state.duration = duration;
        state.center = center;
        state.from = from;
        state.to = to;
        state.current = from;
        state.rotate_finish = finish;
        state.scale_finish = nullptr;
    }

    AnimationState *start_animation(AnimationState &state, Widget *owner, void *user_data)
    {
        if (!owner || !state.tick) return nullptr;
        if (!user_data) user_data = owner;
        state.cancel(owner, user_data);
        state.is_active = true;
        if (state.at_start) state.at_start(&state, owner, user_data, current_animation_time());
        next_animation_frame(state, owner, user_data);
        return &state;
    }

    bool get_animation_state_data(const AnimationState &state, f32 *value)
    {
        if (!value) return false;
        *value = state.current;
        return true;
    }


    f32 get_default_animation_progress(f64 start_time, f64 duration, f64 now)
    {
        if (duration <= 0.0) return 1.0f;
        f64 raw_t = (now - start_time) / duration;
        raw_t = amal::clamp(raw_t, 0.0, 1.0);
        const f32 t = static_cast<f32>(raw_t);
        return 1.0f - (1.0f - t) * (1.0f - t);
    }

    f64 current_animation_time()
    {
        if (!detail::g_context) return 0.0;
        detail::update_window_time(detail::get_context().window_ctx);
        return detail::get_context().window_ctx->time;
    }

    bool next_animation_frame(AnimationState &state, Widget *owner, void *user_data)
    {
        if (!detail::g_context || !owner || state.tick_scheduled) return false;
        if (!user_data) user_data = owner;
        const f64 now = current_animation_time();
        const f64 delay = get_max_animation_delay() > 0.0 ? get_max_animation_delay() : (1.0 / 60.0);
        state.tick_scheduled = true;
        schedule_delayed_host_task(owner->id(), now + delay, [&state, owner, user_data]() {
            state.tick_scheduled = false;
            const bool active = state.next_frame(current_animation_time(), owner, user_data);
            owner->update_draw_commands(DrawReasonBits::external);
            detail::get_context().dirty_flags |= DirtyFlagBits::redraw;
            detail::mark_host_refresh_request();
            if (active) next_animation_frame(state, owner, user_data);
        });
        return true;
    }
} // namespace auik
