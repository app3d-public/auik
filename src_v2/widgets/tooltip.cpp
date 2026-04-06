#include <auik/v2/auik.hpp>
#include <auik/v2/detail/depth.hpp>
#include <auik/v2/detail/text.hpp>
#include <auik/v2/pipelines.hpp>
#include <auik/v2/widgets/tooltip.hpp>

namespace auik::v2
{
    namespace
    {
        constexpr f32 g_tooltip_screen_padding = 8.0f;
        constexpr f32 g_tooltip_mouse_offset_y = 18.0f;

        static inline amal::vec4 intersect_rect(const amal::vec4 &a, const amal::vec4 &b)
        {
            const amal::vec2 a_min = {a.x, a.y};
            const amal::vec2 a_max = {a.x + a.z, a.y + a.w};
            const amal::vec2 b_min = {b.x, b.y};
            const amal::vec2 b_max = {b.x + b.z, b.y + b.w};

            const amal::vec2 out_min = {amal::max(a_min.x, b_min.x), amal::max(a_min.y, b_min.y)};
            const amal::vec2 out_max = {amal::min(a_max.x, b_max.x), amal::min(a_max.y, b_max.y)};
            const amal::vec2 out_size = {amal::max(out_max.x - out_min.x, 0.0f), amal::max(out_max.y - out_min.y, 0.0f)};
            return {out_min, out_size};
        }

        static inline f32 clamp_axis(f32 value, f32 min_value, f32 max_value)
        {
            return amal::max(min_value, amal::min(value, max_value));
        }
    } // namespace

    Tooltip::Tooltip(u32 id, Widget *parent)
        : Widget(id, get_default_widget_flags() | WidgetFlagBits::fixed | WidgetFlagBits::foreground,
                 EventFlagBits::none, parent, {}, AUIK_TAG_TOOLTIP)
    {
        _layout_config.wrap = detail::TextWrapMode::word;
        _layout_config.overflow = detail::TextOverflowMode::clip;
        _render_config.horizontal_align = detail::TextHorizontalAlign::left;
        _render_config.vertical_align = detail::TextVerticalAlign::top;
        Widget::hide();
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
        Widget::show();
        update_style();
        update_layout(false);

        if (has_draw_record()) update_draw_commands(DrawReasonBits::layout | DrawReasonBits::external);
        else record_draw_commands(DrawReasonBits::full_redraw);

        auto &ctx = detail::get_context();
        ctx.dirty_flags |= DirtyFlagBits::redraw;
        detail::mark_host_refresh_request();
    }

    void Tooltip::hide()
    {
        Widget::hide();
        if (_text_source) _dismissed_for_current_source = true;
        _rect.clip_id = 0xFFFFu;
        _bg = {};
        _draw_ids.clear();
        _applied_clip_id = 0xFFFFu;
        _instances_gpu_dirty = true;
    }

    void Tooltip::clear_if_source(const acul::string *text_source)
    {
        if (_text_source != text_source) return;
        hide();
        reset_source_state();
    }

    StyleUpdateFlags Tooltip::update_style()
    {
        const auto flags = resolve_style_selector(_style, id(), 0, style_state());
        const auto &style = get_theme()->get_style(_style.id);
        _layout_config.size_px = round_font_px(style.text_size());
        _render_config.tint_color = style.text_color();
        return flags;
    }

    void Tooltip::update_layout_min_size()
    {
        _layout_result.clear();
        _instances.clear();
        _draw_ids.clear();
        _instances_gpu_dirty = true;

        const auto &style = get_theme()->get_style(_style.id);
        const amal::vec4 padding = style.padding();
        const amal::vec2 display = get_display_size();
        const f32 max_outer_width = amal::max(display.x - g_tooltip_screen_padding * 2.0f, 0.0f);
        const f32 max_content_width = amal::max(max_outer_width - padding.x - padding.z, 1.0f);

        auto *font = style.font();
        if (!font || !_text_source || _text_source->empty() || _layout_config.size_px == 0)
        {
            set_required_size({padding.x + padding.z, padding.y + padding.w});
            return;
        }

        auto measure_config = _layout_config;
        if (!detail::layout_single_line(*font, *_text_source, measure_config, _layout_result))
        {
            set_required_size({padding.x + padding.z, padding.y + padding.w});
            return;
        }

        const f32 natural_content_width = amal::max(_layout_result.size.x, 1.0f);
        const f32 resolved_content_width = amal::min(natural_content_width, max_content_width);
        measure_config.max_width = resolved_content_width;
        if (!detail::layout_multiline(*font, *_text_source, measure_config, _layout_result))
        {
            set_required_size({padding.x + padding.z, padding.y + padding.w});
            return;
        }

        set_required_size({_layout_result.size.x + padding.x + padding.z, _layout_result.size.y + padding.y + padding.w});
    }

    void Tooltip::update_layout(bool min_size_known)
    {
        if (!min_size_known) update_layout_min_size();

        Widget::update_layout(true);
        const auto &style = get_theme()->get_style(_style.id);
        const amal::vec4 padding = style.padding();
        const amal::vec2 display = get_display_size();

        set_size(required_size());

        const f32 max_x = amal::max(display.x - size().x - g_tooltip_screen_padding, g_tooltip_screen_padding);
        const f32 pos_x = clamp_axis(_anchor_x, g_tooltip_screen_padding, max_x);

        const f32 mouse_y = get_mouse_pos().y + g_tooltip_mouse_offset_y;
        const f32 max_y = amal::max(display.y - size().y - g_tooltip_screen_padding, g_tooltip_screen_padding);
        f32 pos_y = clamp_axis(mouse_y, g_tooltip_screen_padding, max_y);
        if (mouse_y > max_y) pos_y = clamp_axis(get_mouse_pos().y - size().y - g_tooltip_mouse_offset_y,
                                                g_tooltip_screen_padding, max_y);

        set_position({pos_x, pos_y});

        const amal::vec4 self_clip_rect =
            intersect_rect({position().x, position().y, size().x, size().y}, {0.0f, 0.0f, display.x, display.y});
        ensure_own_clip_rect(self_clip_rect);

        rebuild_text_buffers({amal::max(size().x - padding.x - padding.z, 1.0f), amal::max(size().y - padding.y - padding.w, 0.0f)});
    }

    void Tooltip::update_depth(const amal::vec2 &depth_range)
    {
        Widget::update_depth(depth_range);
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
        for (auto &instance : _instances) instance.rect.offset += delta;
        if (!_instances.empty()) _instances_gpu_dirty = true;
    }

    void Tooltip::rebuild_clip_rects()
    {
        _rect.clip_id = 0xFFFFu;
        const amal::vec2 display = get_display_size();
        const amal::vec4 self_clip_rect =
            intersect_rect({position().x, position().y, size().x, size().y}, {0.0f, 0.0f, display.x, display.y});
        ensure_own_clip_rect(self_clip_rect);
        _bg = {};
        _draw_ids.clear();
        _applied_clip_id = 0xFFFFu;
    }

    void Tooltip::draw(DrawCtx &ctx)
    {
        if (!is_visible() || !_text_source || _text_source->empty()) return;

        auto *quads_stream = get_primary_quad_stream();
        auto *image_stream = get_primary_image_stream();
        auto *theme = get_theme();
        QuadsInstanceData bg{};
        bg.rect = bounds();
        bg.z_order = get_z_order();
        fill_quads_instance_by_style(theme->get_style(_style.id), clip_id(), bg);
        ctx.emit(quads_stream, _bg, &bg, get_rect(), false);

        if (!image_stream || _instances.empty()) return;

        const u16 current_clip = clip_id();
        const f32 current_z = get_z_order();
        const bool draw_state_changed = (_applied_clip_id != current_clip);
        if (draw_state_changed || _instances_gpu_dirty)
        {
            for (auto &instance : _instances)
            {
                instance.clip_id = current_clip;
                instance.z_order = current_z;
                instance.tint_color = detail::pack_rgba8(_render_config.tint_color);
            }
        }

        if (ctx.emit == &emit_draw_record)
        {
            _draw_ids.resize(_instances.size());
            push_textures_batch_to_stream(image_stream, _instances.data(), static_cast<u32>(_instances.size()),
                                          _draw_ids.data());
        }
        else if (draw_state_changed || _instances_gpu_dirty)
            update_textures_batch_in_stream(image_stream, _draw_ids.data(), _instances.data(),
                                            static_cast<u32>(_instances.size()));

        _applied_clip_id = current_clip;
        _instances_gpu_dirty = false;
    }

    bool Tooltip::rebuild_text_buffers(const amal::vec2 &bounds_size)
    {
        _instances.clear();
        _instances_gpu_dirty = true;
        if (!_text_source || _text_source->empty()) return false;

        auto *font = get_theme()->get_style(_style.id).font();
        if (!font || _layout_config.size_px == 0) return false;

        _render_config.bounds = {position() + amal::vec2{get_theme()->get_style(_style.id).padding().x,
                                                         get_theme()->get_style(_style.id).padding().y},
                                 bounds_size};
        _render_config.z_order = get_z_order();
        _render_config.clip_id = clip_id();

        return detail::build_multiline_instances(*font, *_text_source, _layout_config, _render_config, _instances,
                                                 &_layout_result);
    }
} // namespace auik::v2
