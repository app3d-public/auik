#include <auik/auik.hpp>
#include <auik/pipelines.hpp>
#include <auik/widgets/text_button.hpp>
#include "../core/session_stream_utils.hpp"

namespace auik
{
    StyleUpdateFlags TextButton::update_style()
    {
        const u32 parent_id = parent() ? parent()->id() : 0u;
        const auto flags = resolve_style_selector(_style, id(), parent_id, style_state());
        const auto &style = get_theme()->get_style(_style.id);
        _text->update_style();
        if (_resolved_text_color != style.text_color())
        {
            _resolved_text_color = style.text_color();
            _text_draw_dirty = true;
        }
        return flags;
    }

    void TextButton::update_layout_min_size()
    {
        auto *theme = get_theme();
        const auto &style = theme->get_style(_style.id);
        const amal::vec4 margin = style.margin();
        const amal::vec4 padding = style.padding();

        _text->update_layout_min_size();
        const amal::vec2 text_size = _text->required_size();

        amal::vec2 min_size = {is_size_concrete(requested_size().x) ? requested_size().x : 0.0f,
                               is_size_concrete(requested_size().y) ? requested_size().y : 0.0f};
        const f32 content_min_width = text_size.x + padding.x + padding.z;
        const f32 content_min_height = amal::max(style.text_size(), text_size.y) + padding.y + padding.w;
        if (!is_width_fixed() || fill_width())
        {
            min_size.x = content_min_width;
            min_size.y = content_min_height;
        }
        else
        {
            if (min_size.x <= 0.0f) min_size.x = amal::max(120.0f, content_min_width);
            if (min_size.y <= 0.0f) min_size.y = content_min_height;
        }
        if (min_size.x > 0.0f) min_size.x = amal::max(min_size.x, content_min_width);
        set_required_size({min_size.x + margin.x + margin.z, min_size.y + margin.y + margin.w});
    }

    void TextButton::update_layout(bool min_size_known)
    {
        if (!min_size_known) update_layout_min_size();
        auto *theme = get_theme();
        const auto &style = theme->get_style(_style.id);
        const amal::vec2 layout_origin = position();
        const amal::vec4 margin = style.margin();

        const amal::vec2 min_required = required_size();
        const amal::vec2 min_button = {amal::max(min_required.x - margin.x - margin.z, 0.0f),
                                       amal::max(min_required.y - margin.y - margin.w, 0.0f)};
        amal::vec2 button_size = size();
        if (fill_width() || is_width_fixed()) button_size.x = amal::max(button_size.x, min_button.x);
        else button_size.x = min_button.x;

        if (fill_height() || is_height_fixed()) button_size.y = amal::max(button_size.y, min_button.y);
        else button_size.y = min_button.y;

        const amal::vec2 pos = {layout_origin.x + margin.x, layout_origin.y + margin.y};
        set_position(pos);
        set_layout_size(button_size);
        Widget::update_layout(true);
        assert(parent() && "TextButton must have parent");
        set_clip_id(parent()->content_clip_id());

        const amal::vec4 padding = style.padding();
        const amal::vec2 content_pos = {pos.x + padding.x, pos.y + padding.y};
        const amal::vec2 content_size = {amal::max(button_size.x - padding.x - padding.z, 0.0f),
                                         amal::max(button_size.y - padding.y - padding.w, 0.0f)};
        _text->set_position(content_pos);
        _text->set_layout_size(content_size);
        _text->update_layout(true);
    }

    void TextButton::translate(const amal::vec2 &delta)
    {
        if (delta.x == 0.0f && delta.y == 0.0f) return;
        Widget::translate(delta);
        _text->translate(delta);
    }

    void TextButton::rebuild_clip_rects()
    {
        assert(parent() && "TextButton must have parent");
        set_clip_id(parent()->content_clip_id());
        invalidate_hit_rect(_bg);
        _text->set_clip_id(clip_id());
        _text->rebuild_clip_rects();
        _text_draw_dirty = true;
    }

    void TextButton::update_depth(const amal::vec2 &depth_range)
    {
        Widget::update_depth(depth_range);
        amal::vec2 text_range{};
        assign_next_depth(this->depth_range(), text_range);
        _text->update_depth(text_range);
    }

    void TextButton::back_hit_depth()
    {
        Widget::back_hit_depth();
        _text->back_hit_depth();
    }

    void TextButton::restore_hit_depth()
    {
        Widget::restore_hit_depth();
        _text->restore_hit_depth();
    }

    void TextButton::draw(DrawCtx &ctx)
    {
        auto *theme = get_theme();
        auto *quads_stream = get_primary_quads_stream();

        QuadsInstanceData bg_data{};
        bg_data.rect = bounds();
        bg_data.z_order = get_z_order();
        const bool bg_visible = fill_quads_instance_by_style(theme->get_style(_style.id), clip_id(), bg_data);
        emit_quads_instance(ctx, quads_stream, _bg, bg_data, get_rect(), bg_visible, can_emit_hit(ctx));

        _text->draw_local(ctx);
        _text_draw_dirty = false;
    }

    namespace
    {
        void write_text_button(acul::bin_stream &stream, umbf::Block *block)
        {
            auto *widget = static_cast<TextButton *>(block);
            detail::write_widget_common_data(stream, *widget);
            const bool translated = widget->is_translated_text();
            const char *literal = translated ? widget->translated_text_literal() : nullptr;
            detail::write_localized_string(stream, translated ? acul::string(literal ? literal : "") : widget->text(),
                                           translated);
            stream.write(widget->style_tag());
        }

        umbf::Block *read_text_button(acul::bin_stream &stream)
        {
            const auto common = detail::read_widget_common_data(stream);
            const auto text = detail::read_localized_string(stream);
            u32 style_tag = AUIK_STYLE_TAG_TEXT_BUTTON;
            stream.read(style_tag);
            TextButton *text_button = acul::alloc<TextButton>(common.id, StringView{text.text.c_str(), text.translated},
                                                              common.requested_size,
                                                              WidgetFlags(common.widget_flags), EventFlagBits::none,
                                                              style_tag);
            detail::apply_widget_common_data(text_button, common);
            return text_button;
        }
    } // namespace

    namespace streams
    {
        AUIK_EXPORT const umbf::streams::Stream text_button{read_text_button, write_text_button};
    } // namespace streams

} // namespace auik
