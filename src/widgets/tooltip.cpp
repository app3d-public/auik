#include <auik/auik.hpp>
#include <auik/detail/depth.hpp>
#include <auik/pipelines.hpp>
#include <auik/widgets/tooltip.hpp>

#define AUIK_TOOLTIP_SCREEN_PADDING 8.0f
#define AUIK_TOOLTIP_MOUSE_OFFSET_Y 18.0f

namespace auik
{
    namespace
    {
        static inline amal::vec4 intersect_rect(const amal::vec4 &a, const amal::vec4 &b)
        {
            const amal::vec2 a_min = {a.x, a.y};
            const amal::vec2 a_max = {a.x + a.z, a.y + a.w};
            const amal::vec2 b_min = {b.x, b.y};
            const amal::vec2 b_max = {b.x + b.z, b.y + b.w};

            const amal::vec2 out_min = {amal::max(a_min.x, b_min.x), amal::max(a_min.y, b_min.y)};
            const amal::vec2 out_max = {amal::min(a_max.x, b_max.x), amal::min(a_max.y, b_max.y)};
            const amal::vec2 out_size = {amal::max(out_max.x - out_min.x, 0.0f),
                                         amal::max(out_max.y - out_min.y, 0.0f)};
            return {out_min, out_size};
        }

        static inline f32 clamp_axis(f32 value, f32 min_value, f32 max_value)
        { return amal::max(min_value, amal::min(value, max_value)); }
    } // namespace

    Tooltip::Tooltip(u32 id)
        : Widget(id, get_default_widget_flags(), EventFlagBits::none, {{0.0f, 0.0f}, AUIK_SIZE_INHERIT},
                 AUIK_TAG_TOOLTIP),
          _text(AUIK_TAG_ETEXT, StringView{""}, AUIK_SIZE_FIT, WidgetFlagBits::visible,
                make_text_layout_flags(TextOverflowMode::ellipsis, TextWrapMode::none, TextLayoutWidthMode::bounds),
                TextAnchorY::baseline)
    {
        _text.set_parent(this);
        _text.set_style_tag(AUIK_STYLE_TAG_TOOLTIP);
        unset_visible();
        sync_widget_flags();
    }

    void Tooltip::reset_source_state()
    {
        _dismissed_for_current_source = false;
        _text_source = nullptr;
    }

    void Tooltip::show_at(f32 x, const acul::string *text_source)
    {
        if (!text_source || text_source->empty())
        {
            hide();
            reset_source_state();
            return;
        }

        if (_text_source == text_source && _dismissed_for_current_source) return;
        if (_text_source != text_source) _dismissed_for_current_source = false;

        _anchor_x = x;
        _text_source = text_source;
        _text.set_text(*text_source);
        set_visible();
        sync_widget_flags();
        update_style();
        update_layout(false);

        redraw_external(has_draw_record(), DrawReasonBits::layout | DrawReasonBits::external);
        detail::mark_host_refresh_request();
    }

    void Tooltip::hide()
    {
        unset_visible();
        sync_widget_flags();
        if (_text_source) _dismissed_for_current_source = true;
        _rect.clip_id = 0xFFFFu;
        _bg = {};
        _text.set_text(StringView{""});
        _text.reset_draw_records();
    }

    void Tooltip::clear_if_source(const acul::string *text_source)
    {
        if (_text_source != text_source) return;
        hide();
        reset_source_state();
    }

    StyleUpdateFlags Tooltip::update_style()
    {
        auto flags = resolve_style_selector(_style, id(), 0, style_state());
        flags |= _text.update_style();
        return flags;
    }

    void Tooltip::update_layout_min_size()
    {
        const auto &style = get_theme()->get_style(_style.id);
        const amal::vec2 display = get_display_size();
        const f32 max_outer_width = amal::max(display.x - AUIK_TOOLTIP_SCREEN_PADDING * 2.0f, 0.0f);
        const amal::vec4 padding = style.padding();
        const f32 max_text_width = amal::max(max_outer_width - padding.x - padding.z, 1.0f);
        _text.set_max_width(max_text_width);

        if (!_text_source || _text_source->empty())
        {
            _text.update_layout_min_size();
            set_required_size(_text.required_size());
            return;
        }

        _text.update_layout_min_size();
        set_required_size(_text.required_size());
    }

    void Tooltip::update_layout(bool min_size_known)
    {
        if (!min_size_known) update_layout_min_size();

        Widget::update_layout(true);
        const amal::vec2 display = get_display_size();

        set_layout_size(required_size());

        const f32 max_x = amal::max(display.x - size().x - AUIK_TOOLTIP_SCREEN_PADDING, AUIK_TOOLTIP_SCREEN_PADDING);
        const f32 pos_x = clamp_axis(_anchor_x, AUIK_TOOLTIP_SCREEN_PADDING, max_x);

        const f32 mouse_y = get_mouse_pos().y + AUIK_TOOLTIP_MOUSE_OFFSET_Y;
        const f32 max_y = amal::max(display.y - size().y - AUIK_TOOLTIP_SCREEN_PADDING, AUIK_TOOLTIP_SCREEN_PADDING);
        f32 pos_y = clamp_axis(mouse_y, AUIK_TOOLTIP_SCREEN_PADDING, max_y);
        if (mouse_y > max_y)
            pos_y = clamp_axis(get_mouse_pos().y - size().y - AUIK_TOOLTIP_MOUSE_OFFSET_Y, AUIK_TOOLTIP_SCREEN_PADDING,
                               max_y);

        set_position({pos_x, pos_y});

        const amal::vec4 self_clip_rect =
            intersect_rect({position().x, position().y, size().x, size().y}, {0.0f, 0.0f, display.x, display.y});
        ensure_own_clip_rect(self_clip_rect);
        _text.set_position(position());
        _text.set_layout_size(size());
        _text.update_layout(true);
    }

    void Tooltip::update_depth(const amal::vec2 &depth_range)
    {
        Widget::update_depth(depth_range);
        amal::vec2 text_range{};
        assign_next_depth(this->depth_range(), text_range);
        _text.update_depth(text_range);
    }

    void Tooltip::translate(const amal::vec2 &delta)
    {
        if (delta.x == 0.0f && delta.y == 0.0f) return;
        Widget::translate(delta);
        if (clip_id() != 0xFFFFu)
        {
            auto clip = get_clip_rect(clip_id());
            clip.x += delta.x;
            clip.y += delta.y;
            update_clip_rect(clip_id(), clip);
        }
        _text.translate(delta);
    }

    void Tooltip::rebuild_clip_rects()
    {
        _rect.clip_id = 0xFFFFu;
        const amal::vec2 display = get_display_size();
        const amal::vec4 self_clip_rect =
            intersect_rect({position().x, position().y, size().x, size().y}, {0.0f, 0.0f, display.x, display.y});
        ensure_own_clip_rect(self_clip_rect);
        _bg = {};
        _text.set_clip_id(clip_id());
        _text.reset_draw_records();
        _text.rebuild_clip_rects();
    }

    void Tooltip::draw(DrawCtx &ctx)
    {
        if ((!is_visible() || !_text_source || _text_source->empty()) && !(ctx.reason & DrawReasonBits::invalidate))
            return;

        auto *quads_stream = get_primary_quads_stream();
        auto *theme = get_theme();
        QuadsInstanceData bg{};
        bg.rect = bounds();
        bg.z_order = get_z_order();
        const bool bg_visible = fill_quads_instance_by_style(theme->get_style(_style.id), clip_id(), bg);
        emit_quads_instance(ctx, quads_stream, _bg, bg, get_rect(), bg_visible, false);

        DrawCtx text_ctx = ctx;
        text_ctx.is_hit_allowed = false;
        _text.draw(text_ctx);
    }
} // namespace auik
