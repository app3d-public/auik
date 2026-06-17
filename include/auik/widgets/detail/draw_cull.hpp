#pragma once

#include <auik/widgets/widget.hpp>

namespace auik::detail
{
    inline void draw_child_in_clip(Widget *child, DrawCtx &ctx, const amal::vec4 &clip_rect)
    {
        if (!child) return;
        if (ctx.reason & DrawReasonBits::invalidate)
        {
            DrawCtx child_ctx = ctx;
            child->draw_local(child_ctx);
            return;
        }
        if (child->should_skip_external_draw_update(clip_rect)) return;
        if (child->is_external_draw_culled())
        {
            DrawCtx invalidate_ctx = ctx;
            invalidate_ctx.reason |= DrawReasonBits::invalidate;
            child->draw_local(invalidate_ctx);
            child->mark_external_draw_invalidated();
            return;
        }
        DrawCtx child_ctx = ctx;
        child->draw_local(child_ctx);
    }
} // namespace auik::detail
