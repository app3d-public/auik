#include <auik/v2/auik.hpp>
#include <auik/v2/pipelines.hpp>
#include <auik/v2/post_effects.hpp>
#include <auik/v2/theme.hpp>
#include <auik/v2/widgets/widget.hpp>

namespace auik::v2
{
    namespace
    {
        struct AlphaHandlerData
        {
            f32 alpha = 1.0f;
        };

        struct FadeInstanceData
        {
            f32 remaining_time = 0.0f;
            f32 duration = 0.0f;
        };

        using FadeEffectRuntime = PostEffectRuntimeState<FadeInstanceData>;
        using RotateEffectRuntime = PostEffectRuntimeState<RotatePostData>;

        struct FadeHandlerData
        {
            FadeEffectRuntime *runtime = nullptr;
            bool is_fade_in = true;
        };

        struct RotateHandlerData
        {
            RotateEffectRuntime *runtime = nullptr;
        };

        static inline u32 scale_packed_alpha(u32 color, f32 factor)
        {
            factor = amal::clamp(factor, 0.0f, 1.0f);
            const u32 alpha = (color >> 24u) & 0xFFu;
            const u32 scaled = static_cast<u32>(static_cast<f32>(alpha) * factor + 0.5f) & 0xFFu;
            return (color & 0x00FFFFFFu) | (scaled << 24u);
        }

        static void apply_alpha_to_quads(QuadsInstanceData &data, f32 alpha)
        {
            data.background_color = scale_packed_alpha(data.background_color, alpha);
            data.border_color = scale_packed_alpha(data.border_color, alpha);
        }

        static void apply_alpha_to_textures(TexturesInstanceData &data, f32 alpha)
        {
            data.tint_color = scale_packed_alpha(data.tint_color, alpha);
        }

        static void apply_alpha_to_vertex_batch(VertexStreamBatchData &batch, f32 alpha)
        {
            auto *vertices = const_cast<VertexStreamVertex *>(batch.vertices);
            if (!vertices) return;
            for (u32 i = 0; i < batch.vertex_count; ++i)
                vertices[i].color = scale_packed_alpha(vertices[i].color, alpha);
        }

        static void apply_rotate_to_vertex_batch(VertexStreamBatchData &batch, const RotatePostData &rotate)
        {
            auto *vertices = const_cast<VertexStreamVertex *>(batch.vertices);
            if (!vertices) return;

            const f32 c = std::cos(rotate.angle);
            const f32 s = std::sin(rotate.angle);
            for (u32 i = 0; i < batch.vertex_count; ++i)
            {
                amal::vec2 local = vertices[i].position - rotate.center;
                vertices[i].position = {rotate.center.x + local.x * c - local.y * s,
                                        rotate.center.y + local.x * s + local.y * c};
            }
        }

        template <class T>
        static void destroy_typed_effect_data(void *effect_data)
        {
            if (!effect_data) return;
            acul::release(static_cast<T *>(effect_data));
        }

        static u32 resolve_post_slot_count()
        {
            auto &ctx = detail::get_context();
            u32 slot_count = 0u;
            for (u32 i = 0; i < ctx.streams.stream_count; ++i)
            {
                const auto &stream = ctx.streams.attached_streams[i];
                if (stream.post_slot_id == 0xFFFFu) continue;
                slot_count = amal::max(slot_count, static_cast<u32>(stream.post_slot_id) + 1u);
            }
            return slot_count;
        }

        static PostEffect *create_post_effect_impl()
        {
            const u32 slot_count = resolve_post_slot_count();
            if (slot_count == 0u) return nullptr;
            auto *effect = acul::alloc<PostEffect>();
            effect->slot_count = slot_count;
            effect->slots = acul::alloc_n<PostEffectNode *>(effect->slot_count);
            for (u32 i = 0; i < effect->slot_count; ++i) effect->slots[i] = nullptr;
            effect->runtime_data = nullptr;
            effect->clear_runtime = nullptr;
            effect->destroy_runtime = nullptr;
            effect->push_instance = nullptr;
            effect->update_instance = nullptr;
            effect->retain_instance = nullptr;
            effect->release_instance = nullptr;
            effect->is_instance_valid = nullptr;
            return effect;
        }

        static bool set_handler_impl(PostEffect *effect, const DrawStream *stream, PostEffectNode *node)
        {
            if (!effect || !stream || !node) return false;
            if (stream->post_slot_id == 0xFFFFu || stream->post_slot_id >= effect->slot_count) return false;
            PostEffectNode *&slot = effect->slots[stream->post_slot_id];
            if (slot)
            {
                if (slot->destroy) slot->destroy(slot->data);
                acul::release(slot);
            }
            slot = node;
            return true;
        }

        static void invalidate_owner_widget(Widget *owner)
        {
            if (!owner) return;
            owner->update_draw_commands(DrawReasonBits::external);
            detail::get_context().dirty_flags |= DirtyFlagBits::redraw;
            detail::mark_host_refresh_request();
        }

        template <class Runtime>
        static Runtime *as_runtime(PostEffect *effect)
        {
            return effect ? static_cast<Runtime *>(effect->runtime_data) : nullptr;
        }

        template <class Runtime>
        static const Runtime *as_runtime(const PostEffect *effect)
        {
            return effect ? static_cast<const Runtime *>(effect->runtime_data) : nullptr;
        }

        template <class Runtime, class Payload>
        static u32 push_instance_impl(PostEffect *effect, Widget *owner, const void *data)
        {
            auto *runtime = as_runtime<Runtime>(effect);
            if (!runtime) return AUIK_INVALID_POST_EFFECT_DATA_ID;

            Payload payload{};
            if (data) payload = *static_cast<const Payload *>(data);

            for (u32 i = 0; i < runtime->entries.size(); ++i)
            {
                auto &entry = runtime->entries[i];
                if (entry.valid || entry.ref_count > 0u) continue;
                entry.owner_id = owner ? owner->id() : 0u;
                entry.ref_count = 1u;
                entry.valid = true;
                entry.payload = payload;
                ++runtime->active_count;
                return i;
            }

            PostEffectInstanceEntry<Payload> entry{};
            entry.owner_id = owner ? owner->id() : 0u;
            entry.ref_count = 1u;
            entry.valid = true;
            entry.payload = payload;
            runtime->entries.push_back(entry);
            ++runtime->active_count;
            return static_cast<u32>(runtime->entries.size() - 1u);
        }

        template <class Runtime, class Payload>
        static void update_instance_impl(PostEffect *effect, u32 id, Widget *owner, const void *data)
        {
            auto *runtime = as_runtime<Runtime>(effect);
            if (!runtime || id >= runtime->entries.size()) return;
            auto &entry = runtime->entries[id];
            if (!entry.valid || entry.ref_count == 0u) return;
            if (owner) entry.owner_id = owner->id();
            if (data) entry.payload = *static_cast<const Payload *>(data);
        }

        template <class Runtime>
        static void retain_instance_impl(PostEffect *effect, u32 id)
        {
            auto *runtime = as_runtime<Runtime>(effect);
            if (!runtime || id >= runtime->entries.size()) return;
            auto &entry = runtime->entries[id];
            if (!entry.valid) return;
            ++entry.ref_count;
        }

        template <class Runtime>
        static void release_instance_impl(PostEffect *effect, u32 id)
        {
            auto *runtime = as_runtime<Runtime>(effect);
            if (!runtime || id >= runtime->entries.size()) return;
            auto &entry = runtime->entries[id];
            if (!entry.valid) return;
            if (entry.ref_count > 0u) --entry.ref_count;
            if (entry.ref_count > 0u) return;

            entry.valid = false;
            if (runtime->active_count > 0u) --runtime->active_count;
            if (runtime->active_count == 0u)
            {
                if (effect->clear_runtime) effect->clear_runtime(effect->runtime_data);
                return;
            }
            entry = {};
        }

        template <class Runtime>
        static bool is_instance_valid_impl(const PostEffect *effect, u32 id)
        {
            const auto *runtime = as_runtime<Runtime>(effect);
            if (!runtime || id >= runtime->entries.size()) return false;
            const auto &entry = runtime->entries[id];
            return entry.valid && entry.ref_count > 0u;
        }

        static f32 resolve_fade_alpha(const FadeInstanceData &entry, bool is_fade_in)
        {
            const f32 duration = entry.duration > 0.0f ? entry.duration : static_cast<f32>(get_max_animation_delay());
            const f32 progress =
                duration > 0.0f ? (1.0f - amal::clamp(entry.remaining_time / duration, 0.0f, 1.0f)) : 1.0f;
            return is_fade_in ? progress : (1.0f - progress);
        }

        static bool has_active_fade_entries(const FadeEffectRuntime &runtime)
        {
            if (runtime.active_count == 0u) return false;
            for (const auto &entry : runtime.entries)
            {
                if (!entry.valid || entry.ref_count == 0u) continue;
                if (entry.payload.remaining_time > 0.0f) return true;
            }
            return false;
        }

        static void schedule_fade_tick(PostEffect *effect);

        static void tick_fade_effect(PostEffect *effect)
        {
            if (!effect || !detail::g_context) return;
            auto *runtime = as_runtime<FadeEffectRuntime>(effect);
            if (!runtime) return;

            auto &ctx = detail::get_context();
            detail::update_window_time(ctx.window_ctx);
            const f32 dt = static_cast<f32>(get_max_animation_delay());
            runtime->ticking = false;

            acul::vector<u32> owner_ids;
            for (auto &entry : runtime->entries)
            {
                if (!entry.valid || entry.ref_count == 0u) continue;
                const f32 prev_time = entry.payload.remaining_time;
                entry.payload.remaining_time = amal::max(entry.payload.remaining_time - dt, 0.0f);
                if (entry.owner_id != 0u && (prev_time > 0.0f || entry.payload.remaining_time > 0.0f))
                {
                    bool exists = false;
                    for (u32 owner_id : owner_ids)
                    {
                        if (owner_id == entry.owner_id)
                        {
                            exists = true;
                            break;
                        }
                    }
                    if (!exists) owner_ids.push_back(entry.owner_id);
                }
            }

            for (u32 owner_id : owner_ids)
            {
                auto it = ctx.id_map.find(owner_id);
                if (it == ctx.id_map.end() || !it->second) continue;
                invalidate_owner_widget(it->second);
            }
            if (has_active_fade_entries(*runtime)) schedule_fade_tick(effect);
        }

        static void schedule_fade_tick(PostEffect *effect)
        {
            if (!effect || !detail::g_context) return;
            auto *runtime = as_runtime<FadeEffectRuntime>(effect);
            if (!runtime || runtime->ticking) return;
            auto &ctx = detail::get_context();
            detail::update_window_time(ctx.window_ctx);
            runtime->ticking = true;
            const f64 delay = get_max_animation_delay();
            schedule_delayed_host_task(runtime->task_owner_id, ctx.window_ctx->time + delay,
                                       [effect]() { tick_fade_effect(effect); });
        }

        static PostEffectNode *make_node(void *data, PostEffectRecordFn record, PostEffectUpdateFn update,
                                         PostEffectDestroyFn destroy)
        {
            auto *node = acul::alloc<PostEffectNode>();
            node->data = data;
            node->record = record;
            node->update = update;
            node->destroy = destroy;
            return node;
        }

        static DrawStream *get_default_stream(u32 default_stream_index)
        {
            auto *defaults = detail::get_context().streams.default_streams;
            assert(defaults && "Default streams are not initialized");
            return defaults[default_stream_index];
        }

        static PostEffectNode *make_disabled_quads_node()
        {
            auto *data = acul::alloc<AlphaHandlerData>();
            data->alpha = 0.5f;
            return make_node(
                data,
                [](void *effect_data, DrawStream *stream, const void *draw_data, const void *) -> DrawDataID {
                    auto copy = *static_cast<const QuadsInstanceData *>(draw_data);
                    apply_alpha_to_quads(copy, static_cast<AlphaHandlerData *>(effect_data)->alpha);
                    return stream->push_data_to_stream(stream, &copy);
                },
                [](void *effect_data, DrawStream *stream, DrawDataID draw_id, const void *draw_data, const void *) {
                    auto copy = *static_cast<const QuadsInstanceData *>(draw_data);
                    apply_alpha_to_quads(copy, static_cast<AlphaHandlerData *>(effect_data)->alpha);
                    stream->update_data_in_stream(stream, draw_id, &copy);
                },
                &destroy_typed_effect_data<AlphaHandlerData>);
        }

        static PostEffectNode *make_disabled_textures_node()
        {
            auto *data = acul::alloc<AlphaHandlerData>();
            data->alpha = 0.5f;
            return make_node(
                data,
                [](void *effect_data, DrawStream *stream, const void *draw_data, const void *) -> DrawDataID {
                    auto copy = *static_cast<const TexturesInstanceData *>(draw_data);
                    apply_alpha_to_textures(copy, static_cast<AlphaHandlerData *>(effect_data)->alpha);
                    return stream->push_data_to_stream(stream, &copy);
                },
                [](void *effect_data, DrawStream *stream, DrawDataID draw_id, const void *draw_data, const void *) {
                    auto copy = *static_cast<const TexturesInstanceData *>(draw_data);
                    apply_alpha_to_textures(copy, static_cast<AlphaHandlerData *>(effect_data)->alpha);
                    stream->update_data_in_stream(stream, draw_id, &copy);
                },
                &destroy_typed_effect_data<AlphaHandlerData>);
        }

        static PostEffectNode *make_disabled_vertex_node()
        {
            auto *data = acul::alloc<AlphaHandlerData>();
            data->alpha = 0.5f;
            return make_node(
                data,
                [](void *effect_data, DrawStream *stream, const void *draw_data, const void *) -> DrawDataID {
                    auto copy = *static_cast<const VertexStreamBatchData *>(draw_data);
                    acul::vector<VertexStreamVertex> scratch;
                    if (copy.vertices && copy.vertex_count > 0u)
                    {
                        scratch.resize(copy.vertex_count);
                        std::memcpy(scratch.data(), copy.vertices, sizeof(VertexStreamVertex) * copy.vertex_count);
                        copy.vertices = scratch.data();
                        apply_alpha_to_vertex_batch(copy, static_cast<AlphaHandlerData *>(effect_data)->alpha);
                    }
                    return stream->push_data_to_stream(stream, &copy);
                },
                [](void *effect_data, DrawStream *stream, DrawDataID draw_id, const void *draw_data, const void *) {
                    auto copy = *static_cast<const VertexStreamBatchData *>(draw_data);
                    acul::vector<VertexStreamVertex> scratch;
                    if (copy.vertices && copy.vertex_count > 0u)
                    {
                        scratch.resize(copy.vertex_count);
                        std::memcpy(scratch.data(), copy.vertices, sizeof(VertexStreamVertex) * copy.vertex_count);
                        copy.vertices = scratch.data();
                        apply_alpha_to_vertex_batch(copy, static_cast<AlphaHandlerData *>(effect_data)->alpha);
                    }
                    stream->update_data_in_stream(stream, draw_id, &copy);
                },
                &destroy_typed_effect_data<AlphaHandlerData>);
        }

        static PostEffectNode *make_rotate_vertex_node(PostEffect *effect)
        {
            auto *data = acul::alloc<RotateHandlerData>();
            data->runtime = as_runtime<RotateEffectRuntime>(effect);
            return make_node(
                data,
                [](void *effect_data, DrawStream *stream, const void *draw_data, const void *post_data) -> DrawDataID {
                    const auto *handler = static_cast<const RotateHandlerData *>(effect_data);
                    const auto *rotate_post = static_cast<const RotatePostData *>(post_data);
                    if (!handler || !handler->runtime || !rotate_post) return stream->push_data_to_stream(stream, draw_data);
                    if (rotate_post->id == AUIK_INVALID_POST_EFFECT_DATA_ID ||
                        rotate_post->id >= handler->runtime->entries.size())
                        return stream->push_data_to_stream(stream, draw_data);

                    const auto &entry = handler->runtime->entries[rotate_post->id];
                    if (!entry.valid || entry.ref_count == 0u) return stream->push_data_to_stream(stream, draw_data);

                    auto copy = *static_cast<const VertexStreamBatchData *>(draw_data);
                    acul::vector<VertexStreamVertex> scratch;
                    if (copy.vertices && copy.vertex_count > 0u)
                    {
                        scratch.resize(copy.vertex_count);
                        std::memcpy(scratch.data(), copy.vertices, sizeof(VertexStreamVertex) * copy.vertex_count);
                        copy.vertices = scratch.data();
                        apply_rotate_to_vertex_batch(copy, entry.payload);
                    }
                    return stream->push_data_to_stream(stream, &copy);
                },
                [](void *effect_data, DrawStream *stream, DrawDataID draw_id, const void *draw_data,
                   const void *post_data) {
                    const auto *handler = static_cast<const RotateHandlerData *>(effect_data);
                    const auto *rotate_post = static_cast<const RotatePostData *>(post_data);
                    if (!handler || !handler->runtime || !rotate_post)
                    {
                        stream->update_data_in_stream(stream, draw_id, draw_data);
                        return;
                    }
                    if (rotate_post->id == AUIK_INVALID_POST_EFFECT_DATA_ID ||
                        rotate_post->id >= handler->runtime->entries.size())
                    {
                        stream->update_data_in_stream(stream, draw_id, draw_data);
                        return;
                    }

                    const auto &entry = handler->runtime->entries[rotate_post->id];
                    if (!entry.valid || entry.ref_count == 0u)
                    {
                        stream->update_data_in_stream(stream, draw_id, draw_data);
                        return;
                    }

                    auto copy = *static_cast<const VertexStreamBatchData *>(draw_data);
                    acul::vector<VertexStreamVertex> scratch;
                    if (copy.vertices && copy.vertex_count > 0u)
                    {
                        scratch.resize(copy.vertex_count);
                        std::memcpy(scratch.data(), copy.vertices, sizeof(VertexStreamVertex) * copy.vertex_count);
                        copy.vertices = scratch.data();
                        apply_rotate_to_vertex_batch(copy, entry.payload);
                    }
                    stream->update_data_in_stream(stream, draw_id, &copy);
                },
                &destroy_typed_effect_data<RotateHandlerData>);
        }

        static PostEffectNode *make_fade_quads_node(PostEffect *effect, bool is_fade_in)
        {
            auto *data = acul::alloc<FadeHandlerData>();
            data->runtime = as_runtime<FadeEffectRuntime>(effect);
            data->is_fade_in = is_fade_in;
            return make_node(
                data,
                [](void *effect_data, DrawStream *stream, const void *draw_data, const void *post_data) -> DrawDataID {
                    const auto *handler = static_cast<const FadeHandlerData *>(effect_data);
                    const auto *fade_post = static_cast<const FadePostData *>(post_data);
                    auto copy = *static_cast<const QuadsInstanceData *>(draw_data);
                    if (handler && handler->runtime && fade_post && fade_post->id < handler->runtime->entries.size())
                    {
                        const auto &entry = handler->runtime->entries[fade_post->id];
                        if (entry.valid && entry.ref_count > 0u)
                            apply_alpha_to_quads(copy, resolve_fade_alpha(entry.payload, handler->is_fade_in));
                    }
                    return stream->push_data_to_stream(stream, &copy);
                },
                [](void *effect_data, DrawStream *stream, DrawDataID draw_id, const void *draw_data,
                   const void *post_data) {
                    const auto *handler = static_cast<const FadeHandlerData *>(effect_data);
                    const auto *fade_post = static_cast<const FadePostData *>(post_data);
                    auto copy = *static_cast<const QuadsInstanceData *>(draw_data);
                    if (handler && handler->runtime && fade_post && fade_post->id < handler->runtime->entries.size())
                    {
                        const auto &entry = handler->runtime->entries[fade_post->id];
                        if (entry.valid && entry.ref_count > 0u)
                            apply_alpha_to_quads(copy, resolve_fade_alpha(entry.payload, handler->is_fade_in));
                    }
                    stream->update_data_in_stream(stream, draw_id, &copy);
                },
                &destroy_typed_effect_data<FadeHandlerData>);
        }

        static PostEffectNode *make_fade_textures_node(PostEffect *effect, bool is_fade_in)
        {
            auto *data = acul::alloc<FadeHandlerData>();
            data->runtime = as_runtime<FadeEffectRuntime>(effect);
            data->is_fade_in = is_fade_in;
            return make_node(
                data,
                [](void *effect_data, DrawStream *stream, const void *draw_data, const void *post_data) -> DrawDataID {
                    const auto *handler = static_cast<const FadeHandlerData *>(effect_data);
                    const auto *fade_post = static_cast<const FadePostData *>(post_data);
                    auto copy = *static_cast<const TexturesInstanceData *>(draw_data);
                    if (handler && handler->runtime && fade_post && fade_post->id < handler->runtime->entries.size())
                    {
                        const auto &entry = handler->runtime->entries[fade_post->id];
                        if (entry.valid && entry.ref_count > 0u)
                            apply_alpha_to_textures(copy, resolve_fade_alpha(entry.payload, handler->is_fade_in));
                    }
                    return stream->push_data_to_stream(stream, &copy);
                },
                [](void *effect_data, DrawStream *stream, DrawDataID draw_id, const void *draw_data,
                   const void *post_data) {
                    const auto *handler = static_cast<const FadeHandlerData *>(effect_data);
                    const auto *fade_post = static_cast<const FadePostData *>(post_data);
                    auto copy = *static_cast<const TexturesInstanceData *>(draw_data);
                    if (handler && handler->runtime && fade_post && fade_post->id < handler->runtime->entries.size())
                    {
                        const auto &entry = handler->runtime->entries[fade_post->id];
                        if (entry.valid && entry.ref_count > 0u)
                            apply_alpha_to_textures(copy, resolve_fade_alpha(entry.payload, handler->is_fade_in));
                    }
                    stream->update_data_in_stream(stream, draw_id, &copy);
                },
                &destroy_typed_effect_data<FadeHandlerData>);
        }

        static PostEffectNode *make_fade_vertex_node(PostEffect *effect, bool is_fade_in)
        {
            auto *data = acul::alloc<FadeHandlerData>();
            data->runtime = as_runtime<FadeEffectRuntime>(effect);
            data->is_fade_in = is_fade_in;
            return make_node(
                data,
                [](void *effect_data, DrawStream *stream, const void *draw_data, const void *post_data) -> DrawDataID {
                    const auto *handler = static_cast<const FadeHandlerData *>(effect_data);
                    const auto *fade_post = static_cast<const FadePostData *>(post_data);
                    auto copy = *static_cast<const VertexStreamBatchData *>(draw_data);
                    acul::vector<VertexStreamVertex> scratch;
                    if (copy.vertices && copy.vertex_count > 0u)
                    {
                        scratch.resize(copy.vertex_count);
                        std::memcpy(scratch.data(), copy.vertices, sizeof(VertexStreamVertex) * copy.vertex_count);
                        copy.vertices = scratch.data();
                        if (handler && handler->runtime && fade_post && fade_post->id < handler->runtime->entries.size())
                        {
                            const auto &entry = handler->runtime->entries[fade_post->id];
                            if (entry.valid && entry.ref_count > 0u)
                                apply_alpha_to_vertex_batch(copy, resolve_fade_alpha(entry.payload, handler->is_fade_in));
                        }
                    }
                    return stream->push_data_to_stream(stream, &copy);
                },
                [](void *effect_data, DrawStream *stream, DrawDataID draw_id, const void *draw_data,
                   const void *post_data) {
                    const auto *handler = static_cast<const FadeHandlerData *>(effect_data);
                    const auto *fade_post = static_cast<const FadePostData *>(post_data);
                    auto copy = *static_cast<const VertexStreamBatchData *>(draw_data);
                    acul::vector<VertexStreamVertex> scratch;
                    if (copy.vertices && copy.vertex_count > 0u)
                    {
                        scratch.resize(copy.vertex_count);
                        std::memcpy(scratch.data(), copy.vertices, sizeof(VertexStreamVertex) * copy.vertex_count);
                        copy.vertices = scratch.data();
                        if (handler && handler->runtime && fade_post && fade_post->id < handler->runtime->entries.size())
                        {
                            const auto &entry = handler->runtime->entries[fade_post->id];
                            if (entry.valid && entry.ref_count > 0u)
                                apply_alpha_to_vertex_batch(copy, resolve_fade_alpha(entry.payload, handler->is_fade_in));
                        }
                    }
                    stream->update_data_in_stream(stream, draw_id, &copy);
                },
                &destroy_typed_effect_data<FadeHandlerData>);
        }

        static PostEffect *get_post_effect_by_index(size_t index)
        {
            auto &effects = detail::get_context().post_effects;
            if (index >= effects.size()) return nullptr;
            return effects[index];
        }

        static void set_default_post_effect_by_index(size_t index, PostEffect *effect)
        {
            auto &effects = detail::get_context().post_effects;
            effects[index] = effect;
        }
    } // namespace

    u32 get_default_post_effects_count() { return AUIK_POST_EFFECT_COUNT; }
    u32 get_post_effects_count() { return static_cast<u32>(detail::get_context().post_effects.size()); }
    PostEffect *create_post_effect() { return create_post_effect_impl(); }

    PostEffectNode *create_post_effect_node(void *data, PostEffectRecordFn record, PostEffectUpdateFn update,
                                            PostEffectDestroyFn destroy)
    {
        return make_node(data, record, update, destroy);
    }

    u32 push_post_effect_instance(PostEffect *effect, Widget *owner, const void *data)
    {
        return (effect && effect->push_instance) ? effect->push_instance(effect, owner, data)
                                                 : AUIK_INVALID_POST_EFFECT_DATA_ID;
    }

    void update_post_effect_instance(PostEffect *effect, u32 id, Widget *owner, const void *data)
    {
        if (effect && effect->update_instance) effect->update_instance(effect, id, owner, data);
    }

    void retain_post_effect_instance(PostEffect *effect, u32 id)
    {
        if (effect && effect->retain_instance) effect->retain_instance(effect, id);
    }

    void release_post_effect_instance(PostEffect *effect, u32 id)
    {
        if (effect && effect->release_instance) effect->release_instance(effect, id);
    }

    bool is_post_effect_instance_valid(const PostEffect *effect, u32 id)
    {
        return (effect && effect->is_instance_valid) ? effect->is_instance_valid(effect, id) : false;
    }

    bool add_post_effect_handler(PostEffect *effect, DrawStream *stream, PostEffectNode *node)
    {
        return set_handler_impl(effect, stream, node);
    }

    u32 add_post_effect(PostEffect *effect)
    {
        if (!effect) return AUIK_INVALID_POST_EFFECT_DATA_ID;
        auto &effects = detail::get_context().post_effects;
        effects.push_back(effect);
        return static_cast<u32>(effects.size() - 1u);
    }

    PostEffect *get_post_effect(u32 index) { return get_post_effect_by_index(index); }

    void create_default_post_effects()
    {
        auto &effects = detail::get_context().post_effects;
        effects.resize(AUIK_POST_EFFECT_COUNT);
        set_default_post_effect_by_index(AUIK_POST_EFFECT_DISABLED, create_default_disabled_post_effect());
        set_default_post_effect_by_index(AUIK_POST_EFFECT_FADE_IN, create_default_fade_in_post_effect());
        set_default_post_effect_by_index(AUIK_POST_EFFECT_FADE_OUT, create_default_fade_out_post_effect());
        set_default_post_effect_by_index(AUIK_POST_EFFECT_ROTATE, create_default_rotate_post_effect());
    }

    PostEffect *create_default_disabled_post_effect()
    {
        auto *effect = create_post_effect_impl();
        if (!effect) return nullptr;
        set_handler_impl(effect, get_default_stream(AUIK_PRIMARY_QUAD_STREAM), make_disabled_quads_node());
        set_handler_impl(effect, get_default_stream(AUIK_PRIMARY_OVERLAY_QUADS_STREAM), make_disabled_quads_node());
        set_handler_impl(effect, get_default_stream(AUIK_PRIMARY_TEXTURED_QUADS_STREAM), make_disabled_textures_node());
        set_handler_impl(effect, get_default_stream(AUIK_PRIMARY_VERTEX_STREAM), make_disabled_vertex_node());
        return effect;
    }

    PostEffect *create_default_rotate_post_effect()
    {
        auto *effect = create_post_effect_impl();
        if (!effect) return nullptr;
        auto *runtime = acul::alloc<RotateEffectRuntime>();
        effect->runtime_data = runtime;
        effect->clear_runtime = [](void *runtime_data) {
            auto *rotate_runtime = static_cast<RotateEffectRuntime *>(runtime_data);
            if (!rotate_runtime) return;
            rotate_runtime->entries.clear();
            rotate_runtime->active_count = 0u;
            rotate_runtime->ticking = false;
        };
        effect->destroy_runtime = [](void *runtime_data) {
            auto *rotate_runtime = static_cast<RotateEffectRuntime *>(runtime_data);
            if (!rotate_runtime) return;
            acul::release(rotate_runtime);
        };
        effect->push_instance = &push_instance_impl<RotateEffectRuntime, RotatePostData>;
        effect->update_instance = &update_instance_impl<RotateEffectRuntime, RotatePostData>;
        effect->retain_instance = &retain_instance_impl<RotateEffectRuntime>;
        effect->release_instance = &release_instance_impl<RotateEffectRuntime>;
        effect->is_instance_valid = &is_instance_valid_impl<RotateEffectRuntime>;
        set_handler_impl(effect, get_default_stream(AUIK_PRIMARY_VERTEX_STREAM), make_rotate_vertex_node(effect));
        return effect;
    }

    PostEffect *get_disabled_post_effect() { return get_post_effect_by_index(AUIK_POST_EFFECT_DISABLED); }

    PostEffect *create_default_fade_in_post_effect()
    {
        auto *effect = create_post_effect_impl();
        if (!effect) return nullptr;

        auto *runtime = acul::alloc<FadeEffectRuntime>();
        runtime->task_owner_id = reinterpret_cast<u64>(effect);
        effect->runtime_data = runtime;
        effect->clear_runtime = [](void *runtime_data) {
            auto *fade_runtime = static_cast<FadeEffectRuntime *>(runtime_data);
            if (!fade_runtime) return;
            fade_runtime->entries.clear();
            fade_runtime->active_count = 0u;
            fade_runtime->ticking = false;
            cancel_delayed_tasks(fade_runtime->task_owner_id);
        };
        effect->destroy_runtime = [](void *runtime_data) {
            auto *fade_runtime = static_cast<FadeEffectRuntime *>(runtime_data);
            if (!fade_runtime) return;
            cancel_delayed_tasks(fade_runtime->task_owner_id);
            acul::release(fade_runtime);
        };
        effect->push_instance = [](PostEffect *effect, Widget *owner, const void *data) {
            const f32 duration = data ? *static_cast<const f32 *>(data) : 0.0f;
            FadeInstanceData payload{};
            payload.remaining_time = amal::max(duration, 0.0f);
            payload.duration = amal::max(duration, 0.0f);
            const u32 id = push_instance_impl<FadeEffectRuntime, FadeInstanceData>(effect, owner, &payload);
            if (id != AUIK_INVALID_POST_EFFECT_DATA_ID && payload.remaining_time > 0.0f) schedule_fade_tick(effect);
            return id;
        };
        effect->update_instance = [](PostEffect *effect, u32 id, Widget *owner, const void *data) {
            const f32 duration = data ? *static_cast<const f32 *>(data) : 0.0f;
            FadeInstanceData payload{};
            payload.remaining_time = amal::max(duration, 0.0f);
            payload.duration = amal::max(duration, 0.0f);
            update_instance_impl<FadeEffectRuntime, FadeInstanceData>(effect, id, owner, &payload);
            if (payload.remaining_time > 0.0f) schedule_fade_tick(effect);
        };
        effect->retain_instance = &retain_instance_impl<FadeEffectRuntime>;
        effect->release_instance = &release_instance_impl<FadeEffectRuntime>;
        effect->is_instance_valid = &is_instance_valid_impl<FadeEffectRuntime>;
        set_handler_impl(effect, get_default_stream(AUIK_PRIMARY_QUAD_STREAM), make_fade_quads_node(effect, true));
        set_handler_impl(effect, get_default_stream(AUIK_PRIMARY_OVERLAY_QUADS_STREAM), make_fade_quads_node(effect, true));
        set_handler_impl(effect, get_default_stream(AUIK_PRIMARY_TEXTURED_QUADS_STREAM),
                         make_fade_textures_node(effect, true));
        set_handler_impl(effect, get_default_stream(AUIK_PRIMARY_VERTEX_STREAM), make_fade_vertex_node(effect, true));
        return effect;
    }

    PostEffect *create_default_fade_out_post_effect()
    {
        auto *effect = create_post_effect_impl();
        if (!effect) return nullptr;

        auto *runtime = acul::alloc<FadeEffectRuntime>();
        runtime->task_owner_id = reinterpret_cast<u64>(effect);
        effect->runtime_data = runtime;
        effect->clear_runtime = [](void *runtime_data) {
            auto *fade_runtime = static_cast<FadeEffectRuntime *>(runtime_data);
            if (!fade_runtime) return;
            fade_runtime->entries.clear();
            fade_runtime->active_count = 0u;
            fade_runtime->ticking = false;
            cancel_delayed_tasks(fade_runtime->task_owner_id);
        };
        effect->destroy_runtime = [](void *runtime_data) {
            auto *fade_runtime = static_cast<FadeEffectRuntime *>(runtime_data);
            if (!fade_runtime) return;
            cancel_delayed_tasks(fade_runtime->task_owner_id);
            acul::release(fade_runtime);
        };
        effect->push_instance = [](PostEffect *effect, Widget *owner, const void *data) {
            const f32 duration = data ? *static_cast<const f32 *>(data) : 0.0f;
            FadeInstanceData payload{};
            payload.remaining_time = amal::max(duration, 0.0f);
            payload.duration = amal::max(duration, 0.0f);
            const u32 id = push_instance_impl<FadeEffectRuntime, FadeInstanceData>(effect, owner, &payload);
            if (id != AUIK_INVALID_POST_EFFECT_DATA_ID && payload.remaining_time > 0.0f) schedule_fade_tick(effect);
            return id;
        };
        effect->update_instance = [](PostEffect *effect, u32 id, Widget *owner, const void *data) {
            const f32 duration = data ? *static_cast<const f32 *>(data) : 0.0f;
            FadeInstanceData payload{};
            payload.remaining_time = amal::max(duration, 0.0f);
            payload.duration = amal::max(duration, 0.0f);
            update_instance_impl<FadeEffectRuntime, FadeInstanceData>(effect, id, owner, &payload);
            if (payload.remaining_time > 0.0f) schedule_fade_tick(effect);
        };
        effect->retain_instance = &retain_instance_impl<FadeEffectRuntime>;
        effect->release_instance = &release_instance_impl<FadeEffectRuntime>;
        effect->is_instance_valid = &is_instance_valid_impl<FadeEffectRuntime>;
        set_handler_impl(effect, get_default_stream(AUIK_PRIMARY_QUAD_STREAM), make_fade_quads_node(effect, false));
        set_handler_impl(effect, get_default_stream(AUIK_PRIMARY_OVERLAY_QUADS_STREAM),
                         make_fade_quads_node(effect, false));
        set_handler_impl(effect, get_default_stream(AUIK_PRIMARY_TEXTURED_QUADS_STREAM),
                         make_fade_textures_node(effect, false));
        set_handler_impl(effect, get_default_stream(AUIK_PRIMARY_VERTEX_STREAM), make_fade_vertex_node(effect, false));
        return effect;
    }

    PostEffect *get_fade_in_post_effect() { return get_post_effect_by_index(AUIK_POST_EFFECT_FADE_IN); }
    PostEffect *get_fade_out_post_effect() { return get_post_effect_by_index(AUIK_POST_EFFECT_FADE_OUT); }
    PostEffect *get_rotate_post_effect() { return get_post_effect_by_index(AUIK_POST_EFFECT_ROTATE); }

    u32 create_fade_post_effect_data(PostEffect *effect, Widget *owner, f32 duration_sec)
    {
        return push_post_effect_instance(effect, owner, &duration_sec);
    }

    void retain_fade_post_effect_data(PostEffect *effect, u32 id) { retain_post_effect_instance(effect, id); }
    void release_fade_post_effect_data(PostEffect *effect, u32 id) { release_post_effect_instance(effect, id); }
    bool is_fade_post_effect_data_valid(PostEffect *effect, u32 id) { return is_post_effect_instance_valid(effect, id); }
    void reset_fade_post_effect_data(PostEffect *effect, u32 id, Widget *owner, f32 duration_sec)
    {
        update_post_effect_instance(effect, id, owner, &duration_sec);
    }
} // namespace auik::v2
