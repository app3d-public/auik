#pragma once

#include <amal/vector.hpp>
#include "draw.hpp"

#define AUIK_POST_EFFECT_DISABLED        0
#define AUIK_POST_EFFECT_FADE_IN         1
#define AUIK_POST_EFFECT_FADE_OUT        2
#define AUIK_POST_EFFECT_ROTATE          3
#define AUIK_POST_EFFECT_COUNT           4
#define AUIK_INVALID_POST_EFFECT_DATA_ID 0xFFFFFFFFu

namespace auik
{
    class Widget;

    struct RotatePostData
    {
        u32 id = AUIK_INVALID_POST_EFFECT_DATA_ID;
    };

    struct RotatePostRuntimeData
    {
        amal::vec2 center{0.0f, 0.0f};
        f32 angle = 0.0f;
        f32 animation_from = 0.0f;
        f32 animation_to = 0.0f;
        f64 animation_start = 0.0;
        bool animating = false;
    };

    struct FadePostData
    {
        u32 id = AUIK_INVALID_POST_EFFECT_DATA_ID;
    };

    template <class T>
    struct PostEffectInstanceEntry
    {
        u32 owner_id = 0u;
        u32 ref_count = 0u;
        bool valid = false;
        T payload{};
    };

    template <class T>
    struct PostEffectRuntimeState
    {
        acul::vector<PostEffectInstanceEntry<T>> entries;
        u32 active_count = 0u;
        u64 task_owner_id = 0u;
        bool ticking = false;
    };

    AUIK_EXPORT u32 get_default_post_effects_count();
    AUIK_EXPORT u32 get_post_effects_count();
    AUIK_EXPORT PostEffect *create_post_effect();
    AUIK_EXPORT PostEffectNode *create_post_effect_node(void *data, PFN_post_effect_record record,
                                                       PFN_post_effect_update update,
                                                       PFN_post_effect_destroy destroy = nullptr,
                                                       PFN_post_effect_record_batch record_batch = nullptr,
                                                       PFN_post_effect_update_batch update_batch = nullptr);
    AUIK_EXPORT u32 push_post_effect_instance(PostEffect *effect, Widget *owner, const void *data = nullptr);
    AUIK_EXPORT void update_post_effect_instance(PostEffect *effect, u32 id, Widget *owner, const void *data = nullptr);
    AUIK_EXPORT void retain_post_effect_instance(PostEffect *effect, u32 id);
    AUIK_EXPORT void release_post_effect_instance(PostEffect *effect, u32 id);
    AUIK_EXPORT bool is_post_effect_instance_valid(const PostEffect *effect, u32 id);
    AUIK_EXPORT bool add_post_effect_handler(PostEffect *effect, DrawStream *stream, PostEffectNode *node);
    AUIK_EXPORT u32 add_post_effect(PostEffect *effect);
    AUIK_EXPORT PostEffect *get_post_effect(u32 index);

    AUIK_EXPORT void create_default_post_effects();
    AUIK_EXPORT PostEffect *create_default_disabled_post_effect();
    AUIK_EXPORT PostEffect *create_default_fade_in_post_effect();
    AUIK_EXPORT PostEffect *create_default_fade_out_post_effect();
    AUIK_EXPORT PostEffect *create_default_rotate_post_effect();
    AUIK_EXPORT PostEffect *get_disabled_post_effect();
    AUIK_EXPORT PostEffect *get_fade_in_post_effect();
    AUIK_EXPORT PostEffect *get_fade_out_post_effect();
    AUIK_EXPORT PostEffect *get_rotate_post_effect();
    AUIK_EXPORT u32 create_rotate_post_effect_data(PostEffect *effect, Widget *owner);
    AUIK_EXPORT RotatePostRuntimeData *get_rotate_post_effect_data(PostEffect *effect, u32 id);
    AUIK_EXPORT const RotatePostRuntimeData *get_rotate_post_effect_data(const PostEffect *effect, u32 id);
    AUIK_EXPORT void retain_rotate_post_effect_data(PostEffect *effect, u32 id);
    AUIK_EXPORT void release_rotate_post_effect_data(PostEffect *effect, u32 id);
    AUIK_EXPORT bool is_rotate_post_effect_data_valid(PostEffect *effect, u32 id);
    AUIK_EXPORT u32 create_fade_post_effect_data(PostEffect *effect, Widget *owner, f32 duration_sec);
    AUIK_EXPORT void retain_fade_post_effect_data(PostEffect *effect, u32 id);
    AUIK_EXPORT void release_fade_post_effect_data(PostEffect *effect, u32 id);
    AUIK_EXPORT bool is_fade_post_effect_data_valid(PostEffect *effect, u32 id);
    AUIK_EXPORT void reset_fade_post_effect_data(PostEffect *effect, u32 id, Widget *owner, f32 duration_sec);
    AUIK_EXPORT PostFxChain *add_post_effect_to_widget(Widget *widget, PostEffect *effect,
                                                      const void *instance_data = nullptr,
                                                      const void *post_data = nullptr);
    AUIK_EXPORT bool remove_post_effect_from_widget(Widget *widget, PostFxChain *chain);
} // namespace auik
