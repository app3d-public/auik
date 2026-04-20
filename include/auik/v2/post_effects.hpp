#pragma once

#include <amal/vector.hpp>
#include "draw.hpp"

namespace auik::v2
{
    class Widget;

#define AUIK_POST_EFFECT_DISABLED 0
#define AUIK_POST_EFFECT_FADE_IN  1
#define AUIK_POST_EFFECT_FADE_OUT 2
#define AUIK_POST_EFFECT_ROTATE   3
#define AUIK_POST_EFFECT_COUNT    4
#define AUIK_INVALID_POST_EFFECT_DATA_ID 0xFFFFFFFFu

    struct RotatePostData
    {
        u32 id = AUIK_INVALID_POST_EFFECT_DATA_ID;
        amal::vec2 center{0.0f, 0.0f};
        f32 angle = 0.0f;
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

    APPLIB_API u32 get_default_post_effects_count();
    APPLIB_API u32 get_post_effects_count();
    APPLIB_API PostEffect *create_post_effect();
    APPLIB_API PostEffectNode *create_post_effect_node(void *data, PostEffectRecordFn record, PostEffectUpdateFn update,
                                                       PostEffectDestroyFn destroy = nullptr);
    APPLIB_API u32 push_post_effect_instance(PostEffect *effect, Widget *owner, const void *data = nullptr);
    APPLIB_API void update_post_effect_instance(PostEffect *effect, u32 id, Widget *owner, const void *data = nullptr);
    APPLIB_API void retain_post_effect_instance(PostEffect *effect, u32 id);
    APPLIB_API void release_post_effect_instance(PostEffect *effect, u32 id);
    APPLIB_API bool is_post_effect_instance_valid(const PostEffect *effect, u32 id);
    APPLIB_API bool add_post_effect_handler(PostEffect *effect, DrawStream *stream, PostEffectNode *node);
    APPLIB_API u32 add_post_effect(PostEffect *effect);
    APPLIB_API PostEffect *get_post_effect(u32 index);

    APPLIB_API void create_default_post_effects();
    APPLIB_API PostEffect *create_default_disabled_post_effect();
    APPLIB_API PostEffect *create_default_fade_in_post_effect();
    APPLIB_API PostEffect *create_default_fade_out_post_effect();
    APPLIB_API PostEffect *create_default_rotate_post_effect();
    APPLIB_API PostEffect *get_disabled_post_effect();
    APPLIB_API PostEffect *get_fade_in_post_effect();
    APPLIB_API PostEffect *get_fade_out_post_effect();
    APPLIB_API PostEffect *get_rotate_post_effect();
    APPLIB_API u32 create_fade_post_effect_data(PostEffect *effect, Widget *owner, f32 duration_sec);
    APPLIB_API void retain_fade_post_effect_data(PostEffect *effect, u32 id);
    APPLIB_API void release_fade_post_effect_data(PostEffect *effect, u32 id);
    APPLIB_API bool is_fade_post_effect_data_valid(PostEffect *effect, u32 id);
    APPLIB_API void reset_fade_post_effect_data(PostEffect *effect, u32 id, Widget *owner, f32 duration_sec);
} // namespace auik::v2
