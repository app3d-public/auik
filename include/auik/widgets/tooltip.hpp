#pragma once

#include <acul/memory/alloc.hpp>
#include <acul/string/string.hpp>
#include <auik/detail/text.hpp>
#include "../theme.hpp"
#include "widget.hpp"

#define AUIK_TAG_TOOLTIP 0x1E1CB209u
#define AUIK_ID_TOOLTIP  AUIK_TAG_TOOLTIP

namespace auik
{
    class Tooltip final : public Widget
    {
    public:
        AUIK_EXPORT explicit Tooltip(u32 id = AUIK_ID_TOOLTIP, Widget *parent = nullptr);
        AUIK_EXPORT void show_at(f32 x, const acul::string *text_source);
        AUIK_EXPORT void hide();
        AUIK_EXPORT void clear_if_source(const acul::string *text_source);
        bool has_source() const { return _text_source != nullptr; }
        bool has_draw_record() const { return _bg.render_id != AUIK_INVALID_DRAW_DATA_ID; }

        AUIK_EXPORT StyleUpdateFlags update_style() override;
        AUIK_EXPORT void update_layout_min_size() override;
        AUIK_EXPORT void update_layout(bool min_size_known) override;
        AUIK_EXPORT void update_depth(const amal::vec2 &depth_range) override;
        AUIK_EXPORT void translate(const amal::vec2 &delta) override;
        AUIK_EXPORT void rebuild_clip_rects() override;
        AUIK_EXPORT void draw(DrawCtx &ctx) override;

    private:
        bool rebuild_text_buffers(const amal::vec2 &bounds_size);
        void reset_source_state();

        DrawDataID _bg;
        StyleSelector _style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_TOOLTIP};
        f32 _anchor_x = 0.0f;
        const acul::string *_text_source = nullptr;
        bool _dismissed_for_current_source = false;
        detail::TextLayoutConfig _layout_config{};
        detail::TextRenderConfig _render_config{};
        detail::TextLayoutResult _layout_result{};
        acul::vector<TexturesInstanceData> _instances;
        acul::vector<DrawDataID> _draw_ids;
        bool _instances_gpu_dirty = true;
        u16 _applied_clip_id = 0xFFFFu;
    };

    inline Tooltip *make_tooltip(u32 id = AUIK_ID_TOOLTIP) { return acul::alloc<Tooltip>(id, nullptr); }
} // namespace auik
