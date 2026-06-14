#include <auik/draw.hpp>
#include <auik/pipelines.hpp>

namespace auik
{
    static PostEffectNode *get_effect_handler(const PostFxChain *chain, const DrawStream *stream)
    {
        if (!chain || !chain->post_effect || !stream) return nullptr;
        if (stream->post_slot_id == 0xFFFFu || stream->post_slot_id >= chain->post_effect->slot_count) return nullptr;
        return chain->post_effect->slots[stream->post_slot_id];
    }

    static u32 infer_stream_item_stride(const DrawStream *stream)
    {
        if (!stream) return 0u;
        switch (stream->post_slot_id)
        {
            case 0u:
                return sizeof(QuadsInstanceData);
            case 1u:
                return sizeof(TexturesInstanceData);
            case 2u:
                return sizeof(VertexStreamBatchData);
            case 4u:
                return sizeof(TexturedVertexStreamBatchData);
            default:
                return 0u;
        }
    }

    struct PostFxProxyData
    {
        const DrawCtx *ctx = nullptr;
        DrawStream *stream = nullptr;
        PostFxChain *next = nullptr;
    };

    static DrawDataID record_post_fx_chain(const DrawCtx &ctx, DrawStream *stream, const void *data,
                                           const PostFxChain *chain);
    static void update_post_fx_chain(const DrawCtx &ctx, DrawStream *stream, DrawDataID draw_id, const void *data,
                                     PostFxChain *chain);
    static void record_post_fx_chain_batch(const DrawCtx &ctx, DrawStream *stream, DrawDataID *draw_ids,
                                           const void *data, u32 data_stride, u32 count, const PostFxChain *chain);
    static void update_post_fx_chain_batch(const DrawCtx &ctx, DrawStream *stream, DrawDataID *draw_ids,
                                           const void *data, u32 data_stride, u32 count, PostFxChain *chain);

    static DrawDataID post_fx_proxy_push(DrawStream *proxy, const void *data)
    {
        auto *proxy_data = static_cast<PostFxProxyData *>(proxy->runtime_data);
        return record_post_fx_chain(*proxy_data->ctx, proxy_data->stream, data, proxy_data->next);
    }

    static void post_fx_proxy_update(DrawStream *proxy, DrawDataID draw_id, const void *data)
    {
        auto *proxy_data = static_cast<PostFxProxyData *>(proxy->runtime_data);
        update_post_fx_chain(*proxy_data->ctx, proxy_data->stream, draw_id, data, proxy_data->next);
    }

    static void post_fx_proxy_push_batch(DrawStream *proxy, const void *data, u32 count, DrawDataID *out_draw_ids)
    {
        auto *proxy_data = static_cast<PostFxProxyData *>(proxy->runtime_data);
        record_post_fx_chain_batch(*proxy_data->ctx, proxy_data->stream, out_draw_ids, data,
                                   infer_stream_item_stride(proxy_data->stream), count, proxy_data->next);
    }

    static void post_fx_proxy_update_batch(DrawStream *proxy, const DrawDataID *draw_ids, const void *data, u32 count)
    {
        auto *proxy_data = static_cast<PostFxProxyData *>(proxy->runtime_data);
        update_post_fx_chain_batch(*proxy_data->ctx, proxy_data->stream, const_cast<DrawDataID *>(draw_ids), data,
                                   infer_stream_item_stride(proxy_data->stream), count, proxy_data->next);
    }

    static DrawStream make_post_fx_proxy(DrawStream *stream, PostFxProxyData *proxy_data)
    {
        DrawStream proxy = *stream;
        proxy.runtime_data = proxy_data;
        proxy.push_data_to_stream = &post_fx_proxy_push;
        proxy.update_data_in_stream = &post_fx_proxy_update;
        proxy.push_data_batch_to_stream = &post_fx_proxy_push_batch;
        proxy.update_data_batch_in_stream = &post_fx_proxy_update_batch;
        return proxy;
    }

    static DrawDataID record_post_fx_chain(const DrawCtx &ctx, DrawStream *stream, const void *data,
                                           const PostFxChain *chain)
    {
        if (!chain) return stream->push_data_to_stream(stream, data);
        const auto *handler = get_effect_handler(chain, stream);
        if (!handler || !handler->record) return record_post_fx_chain(ctx, stream, data, chain->next);

        PostFxProxyData proxy_data{&ctx, stream, chain->next};
        DrawStream proxy = make_post_fx_proxy(stream, &proxy_data);
        return handler->record(handler->data, &proxy, data, chain->post_data);
    }

    static void update_post_fx_chain(const DrawCtx &ctx, DrawStream *stream, DrawDataID draw_id, const void *data,
                                     PostFxChain *chain)
    {
        if (!chain)
        {
            stream->update_data_in_stream(stream, draw_id, data);
            return;
        }
        const auto *handler = get_effect_handler(chain, stream);
        if (!handler || !handler->update)
        {
            update_post_fx_chain(ctx, stream, draw_id, data, chain->next);
            return;
        }

        PostFxProxyData proxy_data{&ctx, stream, chain->next};
        DrawStream proxy = make_post_fx_proxy(stream, &proxy_data);
        handler->update(handler->data, &proxy, draw_id, data, chain->post_data);
    }

    static void record_post_fx_chain_batch(const DrawCtx &ctx, DrawStream *stream, DrawDataID *draw_ids,
                                           const void *data, u32 data_stride, u32 count, const PostFxChain *chain)
    {
        if (count == 0u) return;
        if (!chain)
        {
            stream->push_data_batch_to_stream(stream, data, count, draw_ids);
            return;
        }
        const auto *handler = get_effect_handler(chain, stream);
        if (!handler) return record_post_fx_chain_batch(ctx, stream, draw_ids, data, data_stride, count, chain->next);

        PostFxProxyData proxy_data{&ctx, stream, chain->next};
        DrawStream proxy = make_post_fx_proxy(stream, &proxy_data);
        if (handler->record_batch)
        {
            handler->record_batch(handler->data, &proxy, draw_ids, data, count, chain->post_data);
            return;
        }

        if (!handler->record)
            return record_post_fx_chain_batch(ctx, stream, draw_ids, data, data_stride, count, chain->next);

        const auto *bytes = static_cast<const u8 *>(data);
        for (u32 i = 0u; i < count; ++i)
        {
            const void *item = data_stride ? bytes + static_cast<size_t>(i) * data_stride : data;
            draw_ids[i] = handler->record(handler->data, &proxy, item, chain->post_data);
        }
    }

    static void update_post_fx_chain_batch(const DrawCtx &ctx, DrawStream *stream, DrawDataID *draw_ids,
                                           const void *data, u32 data_stride, u32 count, PostFxChain *chain)
    {
        if (count == 0u) return;
        if (!chain)
        {
            stream->update_data_batch_in_stream(stream, draw_ids, data, count);
            return;
        }
        const auto *handler = get_effect_handler(chain, stream);
        if (!handler) return update_post_fx_chain_batch(ctx, stream, draw_ids, data, data_stride, count, chain->next);

        PostFxProxyData proxy_data{&ctx, stream, chain->next};
        DrawStream proxy = make_post_fx_proxy(stream, &proxy_data);
        if (handler->update_batch)
        {
            handler->update_batch(handler->data, &proxy, draw_ids, data, count, chain->post_data);
            return;
        }

        if (!handler->update)
            return update_post_fx_chain_batch(ctx, stream, draw_ids, data, data_stride, count, chain->next);

        const auto *bytes = static_cast<const u8 *>(data);
        for (u32 i = 0u; i < count; ++i)
        {
            const void *item = data_stride ? bytes + static_cast<size_t>(i) * data_stride : data;
            handler->update(handler->data, &proxy, draw_ids[i], item, chain->post_data);
        }
    }

    bool is_post_effect_supported(const DrawStream *stream, const PostEffect *effect)
    {
        if (!stream || !effect) return true;
        if (stream->post_slot_id == 0xFFFFu) return false;
        return stream->post_slot_id < effect->slot_count;
    }

    void destroy_post_effect(PostEffect *effect)
    {
        if (!effect) return;
        for (u32 slot_id = 0; slot_id < effect->slot_count; ++slot_id)
        {
            PostEffectNode *node = effect->slots ? effect->slots[slot_id] : nullptr;
            if (!node) continue;
            if (node->destroy) node->destroy(node->data);
            acul::release(node);
        }
        if (effect->slots) acul::release(effect->slots, effect->slot_count);
        if (effect->destroy_runtime) effect->destroy_runtime(effect->runtime_data);
        acul::release(effect);
    }

    static void hide_hit_rect(DrawDataID &draw_id, const detail::RectData &rect)
    {
        if (draw_id.hit_id == AUIK_INVALID_DRAW_DATA_ID) return;
        detail::RectData hidden_rect = rect;
        hidden_rect.bounds.size = {0.0f, 0.0f};
        update_hit_rect(draw_id.hit_id, hidden_rect, true);
    }

    static DrawDataID emit_draw_record(const DrawCtx &ctx, DrawStream *stream, DrawDataID &draw_id, const void *data,
                                       const detail::RectData &rect, bool is_hit_allowed)
    {
        assert(stream);
        if (!is_hit_allowed) hide_hit_rect(draw_id, rect);
        const DrawDataID stream_id = record_post_fx_chain(ctx, stream, data, ctx.post_fx_chain);
        draw_id.render_id = stream_id.render_id;
        draw_id.hit_id = AUIK_INVALID_DRAW_DATA_ID;
        if (is_hit_allowed) update_hit_rect(draw_id.hit_id, rect, true);
        return draw_id;
    }

    static DrawDataID emit_draw_update(const DrawCtx &ctx, DrawStream *stream, DrawDataID &draw_id, const void *data,
                                       const detail::RectData &rect, bool is_hit_allowed)
    {
        assert(stream);
        assert(draw_id.render_id != AUIK_INVALID_DRAW_DATA_ID && "Update called before record");
        update_post_fx_chain(ctx, stream, draw_id, data, ctx.post_fx_chain);

        if (is_hit_allowed)
        {
            const bool is_dirty_hit_rect_update = detail::get_context().dirty_flags & DirtyFlagBits::hit_rect_update;
            update_hit_rect(draw_id.hit_id, rect, is_dirty_hit_rect_update);
        }
        else hide_hit_rect(draw_id, rect);
        return draw_id;
    }

    static DrawDataID emit_draw_invalidate(const DrawCtx &ctx, DrawStream *stream, DrawDataID &draw_id, const void *data,
                                           const detail::RectData &rect, bool is_hit_allowed)
    {
        (void)data;
        assert(stream);
        if (draw_id.render_id != AUIK_INVALID_DRAW_DATA_ID && stream->invalidate_data_in_stream)
            stream->invalidate_data_in_stream(stream, draw_id);
        draw_id.render_id = AUIK_INVALID_DRAW_DATA_ID;

        hide_hit_rect(draw_id, rect);
        return draw_id;
    }

    DrawDataID emit_context_draw(DrawCtx &ctx, DrawStream *stream, DrawDataID &draw_id, const void *data,
                                 const detail::RectData &rect, bool emit_hit_rect)
    {
        const bool need_hit_rect = emit_hit_rect && ctx.is_hit_allowed;
        if (ctx.reason & DrawReasonBits::invalidate)
            return emit_draw_invalidate(ctx, stream, draw_id, data, rect, need_hit_rect);
        if ((ctx.reason & DrawReasonBits::record) || draw_id.render_id == AUIK_INVALID_DRAW_DATA_ID)
            return emit_draw_record(ctx, stream, draw_id, data, rect, need_hit_rect);
        return emit_draw_update(ctx, stream, draw_id, data, rect, need_hit_rect);
    }

    void emit_context_draw_batch(DrawCtx &ctx, DrawStream *stream, DrawDataID *draw_ids, const void *data, u32 count,
                                 const detail::RectData *rects, bool emit_hit_rects)
    {
        if (count == 0u) return;
        assert(stream && "stream is null");
        assert(draw_ids && "draw_ids is null");

        const bool need_hit_rects = emit_hit_rects && ctx.is_hit_allowed;
        if (ctx.reason & DrawReasonBits::invalidate)
        {
            if (!need_hit_rects)
            {
                invalidate_data_batch_in_stream(stream, draw_ids, count);
                for (u32 i = 0u; i < count; ++i) draw_ids[i].render_id = AUIK_INVALID_DRAW_DATA_ID;
                return;
            }

            for (u32 i = 0u; i < count; ++i)
            {
                const detail::RectData rect = rects ? rects[i] : detail::RectData{};
                emit_draw_invalidate(ctx, stream, draw_ids[i], nullptr, rect, need_hit_rects);
            }
            return;
        }

        assert(data && "batch data is null");
        bool has_invalid_draw = false;
        bool has_valid_draw = false;
        for (u32 i = 0u; i < count; ++i)
        {
            if (draw_ids[i].render_id == AUIK_INVALID_DRAW_DATA_ID) has_invalid_draw = true;
            else has_valid_draw = true;
        }

        const bool should_record = (ctx.reason & DrawReasonBits::record) || !has_valid_draw;
        if (!need_hit_rects && (!has_invalid_draw || should_record))
        {
            if (should_record) record_post_fx_chain_batch(ctx, stream, draw_ids, data, infer_stream_item_stride(stream),
                                                          count, ctx.post_fx_chain);
            else update_post_fx_chain_batch(ctx, stream, draw_ids, data, infer_stream_item_stride(stream), count,
                                            ctx.post_fx_chain);
            return;
        }

        const u32 data_stride = infer_stream_item_stride(stream);
        assert(data_stride && "Cannot resolve stream item stride for mixed batch emit");
        const auto *bytes = static_cast<const u8 *>(data);
        for (u32 i = 0u; i < count; ++i)
        {
            const detail::RectData rect = rects ? rects[i] : detail::RectData{};
            emit_context_draw(ctx, stream, draw_ids[i], bytes + static_cast<size_t>(i) * data_stride, rect,
                              need_hit_rects);
        }
    }
} // namespace auik
