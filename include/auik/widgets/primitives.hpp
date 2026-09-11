#pragma once

#include <amal/geometric.hpp>
#include "widget.hpp"

#define AUIK_TAG_WLINE 0x8CE9217Du
#define AUIK_TAG_WRECT 0xD4816E02u

namespace auik
{
    class WLine final : public Widget, public umbf::Block
    {
    public:
        AUIK_EXPORT explicit WLine(u32 id, amal::axis axis, WidgetFlags flags);

        amal::axis axis() const { return _axis; }
        u32 style_tag() const { return _style.tag_id; }
        AUIK_EXPORT void set_style_tag(u32 tag_id);

        AUIK_EXPORT StyleUpdateFlags update_style() override;
        AUIK_EXPORT void update_layout_min_size_force() override;
        AUIK_EXPORT void update_layout(bool min_size_known) override;
        AUIK_EXPORT void translate(const amal::vec2 &delta) override;
        AUIK_EXPORT void rebuild_clip_rects() override;
        AUIK_EXPORT void reset_draw_records() override;
        AUIK_EXPORT void draw(DrawCtx &ctx) override;
        u32 signature() const noexcept override { return AUIK_TAG_WLINE; }
        umbf::Block *as_snapshot_block() noexcept override { return this; }

    private:
        amal::axis _axis = amal::axis::x;
        StyleSelector _style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_SEPARATOR};
        DrawDataID _draw{};
    };

    class AUIK_CLASS_EXPORT WRect final : public Widget, public umbf::Block
    {
    public:
        AUIK_EXPORT explicit WRect(u32 id, const amal::rect &bounds, WidgetFlags flags);

        u32 style_tag() const { return _style.tag_id; }
        AUIK_EXPORT void set_style_tag(u32 tag_id);

        AUIK_EXPORT StyleUpdateFlags update_style() override;
        AUIK_EXPORT void update_layout_min_size_force() override;
        AUIK_EXPORT void update_layout(bool min_size_known) override;
        AUIK_EXPORT void rebuild_clip_rects() override;
        AUIK_EXPORT void reset_draw_records() override;
        AUIK_EXPORT void draw(DrawCtx &ctx) override;
        u32 signature() const noexcept override { return AUIK_TAG_WRECT; }
        umbf::Block *as_snapshot_block() noexcept override { return this; }

    private:
        StyleSelector _style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_SEPARATOR};
        DrawDataID _draw{};
    };

    inline WLine *make_w_line(u32 id, amal::axis axis = amal::axis::x)
    {
        constexpr WidgetFlags widget_flags =
            WidgetFlagBits::visible | WidgetFlagBits::attachable | WidgetFlagBits::cache_snapshot;
        return acul::alloc<WLine>(id, axis, widget_flags);
    }

    inline WRect *make_w_rect(u32 id, const amal::rect &bounds = {{0.0f, 0.0f}, AUIK_SIZE_INHERIT})
    {
        constexpr WidgetFlags widget_flags =
            WidgetFlagBits::visible | WidgetFlagBits::attachable | WidgetFlagBits::cache_snapshot;
        return acul::alloc<WRect>(id, bounds, widget_flags);
    }

    namespace streams
    {
        extern AUIK_EXPORT const umbf::registry::BlockStream w_line;
        extern AUIK_EXPORT const umbf::registry::BlockStream w_rect;
    } // namespace streams
} // namespace auik
