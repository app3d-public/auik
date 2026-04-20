#include <auik/v2/draw.hpp>
#include <auik/v2/pipelines.hpp>

namespace auik::v2
{
    namespace
    {
        static PostEffectNode *get_effect_handler(const DrawCtx &ctx, const DrawStream *stream)
        {
            if (!ctx.post_effect || !stream) return nullptr;
            if (stream->post_slot_id == 0xFFFFu || stream->post_slot_id >= ctx.post_effect->slot_count) return nullptr;
            return ctx.post_effect->slots[stream->post_slot_id];
        }
    } // namespace

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

    DrawDataID emit_draw_record(const DrawCtx &ctx, DrawStream *stream, DrawDataID &draw_id, const void *data,
                                const detail::RectData &rect, bool emit_hit_rect)
    {
        assert(stream);
        const auto *handler = get_effect_handler(ctx, stream);
        const DrawDataID stream_id = (handler && handler->record) ? handler->record(handler->data, stream, data, ctx.post_data)
                                                                  : stream->push_data_to_stream(stream, data);
        draw_id.render_id = stream_id.render_id;
        if (emit_hit_rect) update_hit_rect(draw_id.hit_id, rect, true);
        return draw_id;
    }

    DrawDataID emit_draw_update(const DrawCtx &ctx, DrawStream *stream, DrawDataID &draw_id, const void *data,
                                const detail::RectData &rect, bool emit_hit_rect)
    {
        assert(stream);
        assert(draw_id.render_id != AUIK_INVALID_DRAW_DATA_ID && "Update called before record");
        if (const auto *handler = get_effect_handler(ctx, stream); handler && handler->update)
            handler->update(handler->data, stream, draw_id, data, ctx.post_data);
        else
            stream->update_data_in_stream(stream, draw_id, data);

        if (emit_hit_rect)
        {
            const bool is_dirty_hit_rect_update = detail::get_context().dirty_flags & DirtyFlagBits::hit_rect_update;
            update_hit_rect(draw_id.hit_id, rect, is_dirty_hit_rect_update);
        }
        return draw_id;
    }
} // namespace auik::v2
