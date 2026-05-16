#pragma once

#include <acul/memory/alloc.hpp>
#include <acul/string/string.hpp>
#include <auik/v2/detail/text.hpp>
#include "../theme.hpp"
#include "widget.hpp"

#define AUIK_TAG_TOOLTIP 0x1E1CB209u
#define AUIK_ID_TOOLTIP  AUIK_TAG_TOOLTIP

namespace auik::v2
{
    class APPLIB_API Tooltip final : public Widget
    {
    public:
        explicit Tooltip(u32 id = AUIK_ID_TOOLTIP, Widget *parent = nullptr);
        void show_at(f32 x, const acul::string *text_source);
        void hide();
        void clear_if_source(const acul::string *text_source);
        bool has_source() const { return _text_source != nullptr; }
        bool has_draw_record() const { return _bg.render_id != AUIK_INVALID_DRAW_DATA_ID; }

        StyleUpdateFlags update_style() override;
        void update_layout_min_size() override;
        void update_layout(bool min_size_known) override;
        void update_depth(const amal::vec2 &depth_range) override;
        void translate(const amal::vec2 &delta) override;
        void rebuild_clip_rects() override;
        void draw(DrawCtx &ctx) override;

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
} // namespace auik::v2
