#include <auik/v2/auik.hpp>
#include <auik/v2/detail/context.hpp>
#include <auik/v2/pipelines.hpp>
#include <auik/v2/widgets/text.hpp>

namespace auik::v2
{
    static amal::vec2 resolve_text_size(const Text &widget, const amal::vec2 &measured_size)
    {
        amal::vec2 out = widget.size();
        if (out.x <= 0.0f) out.x = measured_size.x;
        if (out.y <= 0.0f) out.y = measured_size.y;
        return out;
    }

    StyleUpdateFlags Text::update_style()
    {
        const u32 parent_id = parent() ? parent()->id() : 0u;
        const auto flags = resolve_style_selector(_style, id(), parent_id, style_state());
        const auto &style = get_theme()->get_style(_style.id);
        _layout_config.size_px = round_font_px(style.text_size());
        if (_use_style_text_color) _render_config.tint_color = style.text_color();
        return flags;
    }

    void Text::update_layout_min_size()
    {
        _layout_result.clear();
        _instances.clear();
        _instances_gpu_dirty = true;
        const auto &style = get_theme()->get_style(_style.id);
        const amal::vec4 margin = style.margin();

        auto *font = get_theme()->get_style(_style.id).font();
        if (!font || _text.empty() || _layout_config.size_px == 0)
        {
            const amal::vec2 content_size = is_fixed() ? size() : amal::vec2{0.0f, 0.0f};
            set_required_size({content_size.x + margin.x + margin.z, content_size.y + margin.y + margin.w});
            return;
        }

        auto measure_config = _layout_config;
        if (size().x > 0.0f && (is_fixed() || multiline())) measure_config.max_width = size().x;

        const bool is_ok = multiline() ? detail::layout_multiline(*font, _text, measure_config, _layout_result)
                                       : detail::layout_single_line(*font, _text, measure_config, _layout_result);
        if (!is_ok)
        {
            _layout_result.clear();
            const amal::vec2 content_size = is_fixed() ? size() : amal::vec2{0.0f, 0.0f};
            set_required_size({content_size.x + margin.x + margin.z, content_size.y + margin.y + margin.w});
            return;
        }

        amal::vec2 min_size = _layout_result.size;
        if (is_fixed())
        {
            if (size().x > 0.0f) min_size.x = size().x;
            if (size().y > 0.0f) min_size.y = size().y;
        }
        else if (size().y > 0.0f) min_size.y = amal::max(min_size.y, size().y);

        set_required_size({min_size.x + margin.x + margin.z, min_size.y + margin.y + margin.w});
    }

    void Text::update_layout(bool min_size_known)
    {
        if (!min_size_known) update_layout_min_size();

        const amal::vec2 cursor = detail::get_context().screen_cursor;
        const auto &style = get_theme()->get_style(_style.id);
        const amal::vec4 margin = style.margin();
        const amal::vec2 required_outer = required_size();
        const amal::vec2 required_content = {amal::max(required_outer.x - margin.x - margin.z, 0.0f),
                                             amal::max(required_outer.y - margin.y - margin.w, 0.0f)};
        const bool auto_width = size().x <= 0.0f;
        const bool auto_height = size().y <= 0.0f;
        const amal::vec2 content_pos = {cursor.x + margin.x, cursor.y + margin.y};
        set_position(content_pos);

        amal::vec2 text_size = resolve_text_size(*this, required_content);
        set_size(text_size);
        Widget::update_layout(true);
        inherit_parent_content_clip_rect();

        rebuild_text_buffers(text_size);

        if (!is_fixed())
        {
            if (auto_width) text_size.x = _layout_result.size.x;
            if (auto_height) text_size.y = _layout_result.size.y;
            set_size(text_size);
            set_required_size(
                {_layout_result.size.x + margin.x + margin.z, _layout_result.size.y + margin.y + margin.w});
        }

        detail::get_context().screen_cursor = {cursor.x, content_pos.y + size().y + margin.w};
    }

    void Text::translate(const amal::vec2 &delta)
    {
        if (delta.x == 0.0f && delta.y == 0.0f) return;
        Widget::translate(delta);
        for (auto &instance : _instances)
            instance.rect.offset += delta;
        if (!_instances.empty()) _instances_gpu_dirty = true;
    }

    void Text::rebuild_clip_rects()
    {
        inherit_parent_content_clip_rect();
        _draw_ids.clear();
    }

    void Text::draw(DrawCtx &ctx)
    {
        auto *image_stream = get_primary_image_stream();
        if (!image_stream || _instances.empty()) return;

        const u16 current_clip = clip_id();
        const f32 current_z = get_z_order();
        const amal::vec4 current_tint = _render_config.tint_color;
        const bool draw_state_changed = (_applied_clip_id != current_clip) || (_applied_z_order != current_z) ||
                                        (_applied_tint_color != current_tint);
        const bool instances_changed = _instances_gpu_dirty;
        if (draw_state_changed)
        {
            for (auto &instance : _instances)
            {
                instance.clip_id = current_clip;
                instance.z_order = current_z;
                instance.tint_color = current_tint;
            }
        }

        if (ctx.emit == &emit_draw_record)
        {
            _draw_ids.resize(_instances.size());
            push_textures_batch_to_stream(image_stream, _instances.data(), static_cast<u32>(_instances.size()),
                                          _draw_ids.data());
            _applied_clip_id = current_clip;
            _applied_z_order = current_z;
            _applied_tint_color = current_tint;
            _instances_gpu_dirty = false;
            return;
        }

        assert(_draw_ids.size() == _instances.size() && "Text draw ids are out of sync with layout instances");
        if (!draw_state_changed && !instances_changed) return;
        update_textures_batch_in_stream(image_stream, _draw_ids.data(), _instances.data(),
                                        static_cast<u32>(_instances.size()));
        _applied_clip_id = current_clip;
        _applied_z_order = current_z;
        _applied_tint_color = current_tint;
        _instances_gpu_dirty = false;
    }

    void Text::set_text(const acul::string &text)
    {
        if (_text == text) return;
        _text = text;
        mark_layout_dirty();
    }

    void Text::set_multiline(bool value)
    {
        const auto next = value ? detail::TextWrapMode::word : detail::TextWrapMode::none;
        if (_layout_config.wrap == next) return;
        _layout_config.wrap = next;
        mark_layout_dirty();
    }

    void Text::set_overflow_mode(detail::TextOverflowMode value)
    {
        if (_layout_config.overflow == value) return;
        _layout_config.overflow = value;
        mark_layout_dirty();
    }

    void Text::set_horizontal_align(detail::TextHorizontalAlign value)
    {
        if (_render_config.horizontal_align == value) return;
        _render_config.horizontal_align = value;
        mark_layout_dirty();
    }

    void Text::set_vertical_align(detail::TextVerticalAlign value)
    {
        if (_render_config.vertical_align == value) return;
        _render_config.vertical_align = value;
        mark_layout_dirty();
    }

    void Text::set_max_lines(u32 value)
    {
        if (_layout_config.max_lines == value) return;
        _layout_config.max_lines = value;
        mark_layout_dirty();
    }

    void Text::set_color(const amal::vec4 &color)
    {
        if (_render_config.tint_color == color) return;
        _render_config.tint_color = color;
        _use_style_text_color = false;
        auto &ctx = detail::get_context();
        ctx.dirty_flags |= DirtyFlagBits::redraw;
        detail::mark_host_refresh_request();
    }

    bool Text::rebuild_text_buffers(const amal::vec2 &bounds_size)
    {
        _instances.clear();
        _layout_result.clear();
        _instances_gpu_dirty = true;

        auto *font = get_theme()->get_style(_style.id).font();
        if (!font || _text.empty() || _layout_config.size_px == 0) return false;

        auto render_config = _render_config;
        render_config.bounds = {position(), bounds_size};
        render_config.z_order = get_z_order();
        render_config.clip_id = clip_id();

        return multiline() ? detail::build_multiline_instances(*font, _text, _layout_config, render_config, _instances,
                                                               &_layout_result)
                           : detail::build_single_line_instances(*font, _text, _layout_config, render_config,
                                                                 _instances, &_layout_result);
    }

    void Text::mark_layout_dirty()
    {
        auto &ctx = detail::get_context();
        ctx.dirty_flags |= DirtyFlagBits::layout;
        detail::mark_host_refresh_request();
        _draw_ids.clear();
    }
} // namespace auik::v2
