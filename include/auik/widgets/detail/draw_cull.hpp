#pragma once

#include <auik/widgets/widget.hpp>

namespace auik::detail
{
    // Invalidating recorded commands can precede the first layout after session restore.
    inline amal::vec4 child_culling_rect(const Widget &owner, const DrawCtx &ctx)
    {
        return ctx.reason & DrawReasonBits::invalidate ? amal::vec4{} : owner.get_content_clip_rect();
    }

    inline void draw_child_in_clip(Widget *child, DrawCtx &ctx, const amal::vec4 &clip_rect)
    {
        if (!child) return;
        if (!child->is_visible() && !(ctx.reason & DrawReasonBits::invalidate)) return;
        if (ctx.reason & DrawReasonBits::invalidate)
        {
            DrawCtx child_ctx = ctx;
            child->draw_local(child_ctx);
            return;
        }
        if (ctx.reason & (DrawReasonBits::record | DrawReasonBits::full_redraw))
        {
            child->reset_external_draw_cull_state();
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
