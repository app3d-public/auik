#include <auik/detail/depth.hpp>
#include <auik/pipelines.hpp>
#include <auik/widgets/detail/draw_cull.hpp>
#include <auik/widgets/shortcut.hpp>
#include "../core/session_stream_utils.hpp"

namespace auik
{
    ShortcutLabel::ShortcutLabel(u32 id, const acul::vector<StringView> &parts)
        : Widget(id, WidgetFlagBits::visible | WidgetFlagBits::attachable | WidgetFlagBits::cache_snapshot,
                 EventFlagBits::none, {{0.0f, 0.0f}, AUIK_SIZE_FIT}, AUIK_TAG_SHORTCUT)
    {
        set_parts(parts);
    }

    ShortcutLabel::~ShortcutLabel() { clear_parts(); }

    acul::vector<acul::string> ShortcutLabel::parts() const
    {
        acul::vector<acul::string> out;
        out.reserve((_parts.size() + 1u) / 2u);
        for (size_t part_i = 0u; part_i < _parts.size(); part_i += 2u)
            if (_parts[part_i]) out.push_back(_parts[part_i]->text());
        return out;
    }

    void ShortcutLabel::clear_parts()
    {
        for (auto *part : _parts)
        {
            if (!part) continue;
            if (part->is_attached()) part->on_detach();
            acul::release(part);
        }
        _parts.clear();
        _key_rects.clear();
        _key_draw_ids.clear();
    }

    void ShortcutLabel::set_parts(const acul::vector<StringView> &parts)
    {
        const bool attached = is_attached();
        if (attached) update_draw_commands(DrawReasonBits::invalidate);
        clear_parts();
        _parts.reserve(parts.size());
        _key_rects.resize(parts.size());
        _key_draw_ids.resize(parts.size());
        auto add_label = [this, attached](StringView value, bool key) {
            auto *label = acul::alloc<Text>(AUIK_TAG_TEXT, value, amal::vec2{AUIK_SIZE_X_FIT, AUIK_SIZE_Y_FIT},
                                            WidgetFlagBits::visible);
            label->set_style_tag(key ? AUIK_STYLE_TAG_SHORTCUT_KEY_TEXT : AUIK_STYLE_TAG_NO_PAD);
            label->set_parent(this);
            label->set_focus_parent(this);
            if (attached)
            {
                label->on_attach();
                label->update_style_invalidated();
            }
            _parts.push_back(label);
        };
        for (size_t index = 0u; index < parts.size(); ++index)
        {
            if (index != 0u) add_label("+", false);
            add_label(parts[index], true);
        }
        if (attached)
        {
            update_depth(depth_range());
        }
    }

    StyleUpdateFlags ShortcutLabel::update_style()
    {
        StyleUpdateFlags out = resolve_style_selector(_style, id(), parent() ? parent()->id() : 0u, style_state());
        out |= resolve_style_selector(_key_style, AUIK_STYLE_TAG_SHORTCUT_KEY, id(), StyleState::normal);
        apply_style_layout(get_theme()->get_style(_style.id));
        for (auto *part : _parts)
            if (part) out |= part->update_style_invalidated();
        return out;
    }

    void ShortcutLabel::update_layout_min_size_force()
    {
        if (_style.id == Theme::STYLE_ID_INVALID || _key_style.id == Theme::STYLE_ID_INVALID) update_style();
        const auto &style = get_theme()->get_style(_style.id);
        const auto &key_style = get_theme()->get_style(_key_style.id);
        const amal::vec4 margin = style.margin();
        const amal::vec4 padding = style.padding();
        const amal::vec4 key_margin = key_style.margin();
        const amal::vec4 key_padding = key_style.padding();
        f32 width = 0.0f;
        f32 height = 0.0f;
        for (size_t i = 0u; i < _parts.size(); ++i)
        {
            auto *part = _parts[i];
            if (!part) continue;
            part->update_layout_min_size();
            const amal::vec2 part_size = part->required_size();
            const bool is_key = (i % 2u) == 0u;
            if (is_key)
            {
                width += key_margin.x + key_padding.x + part_size.x + key_padding.z + key_margin.z;
                height = amal::max(height, key_margin.y + key_padding.y + part_size.y + key_padding.w + key_margin.w);
            }
            else
            {
                width += part_size.x;
                height = amal::max(height, part_size.y);
            }
            if (i + 1u < _parts.size()) width += style.inline_spacing();
        }
        set_required_size({margin.x + padding.x + width + padding.z + margin.z,
                           margin.y + padding.y + height + padding.w + margin.w});
    }

    void ShortcutLabel::update_layout(bool min_size_known)
    {
        if (layout_measure_required(min_size_known)) update_layout_min_size_force();
        const auto &style = get_theme()->get_style(_style.id);
        const auto &key_style = get_theme()->get_style(_key_style.id);
        const amal::vec4 margin = style.margin();
        const amal::vec4 padding = style.padding();
        const amal::vec4 key_margin = key_style.margin();
        const amal::vec4 key_padding = key_style.padding();
        const amal::vec2 layout_origin = position();
        set_position({layout_origin.x + margin.x, layout_origin.y + margin.y});
        set_layout_size({amal::max(required_size().x - margin.x - margin.z, 0.0f),
                         amal::max(required_size().y - margin.y - margin.w, 0.0f)});
        Widget::update_layout(true);
        set_clip_id(parent() ? parent()->content_clip_id() : clip_id());

        f32 cursor_x = position().x + padding.x;
        const f32 content_y = position().y + padding.y;
        const f32 content_h = amal::max(size().y - padding.y - padding.w, 0.0f);
        for (size_t i = 0u; i < _parts.size(); ++i)
        {
            auto *part = _parts[i];
            if (!part) continue;
            const amal::vec2 part_size = part->required_size();
            const bool is_key = (i % 2u) == 0u;
            if (!is_key)
            {
                part->set_position({cursor_x, content_y + amal::max((content_h - part_size.y) * 0.5f, 0.0f)});
                part->set_layout_size(part_size);
                part->set_clip_id(clip_id());
                part->update_layout(true);
                cursor_x += part_size.x;
                if (i + 1u < _parts.size()) cursor_x += style.inline_spacing();
                continue;
            }
            const amal::vec2 key_size = {key_padding.x + part_size.x + key_padding.z,
                                         key_padding.y + part_size.y + key_padding.w};
            const f32 key_y = content_y + amal::max((content_h - key_size.y) * 0.5f, 0.0f);
            const amal::rect key_bounds{{cursor_x + key_margin.x, key_y}, key_size};
            const size_t key_index = i / 2u;
            _key_rects[key_index] = detail::make_rect_data(id(), AUIK_STYLE_TAG_SHORTCUT_KEY, key_bounds, clip_id(),
                                                           get_rect().depth, 0u, static_cast<u32>(key_index));
            part->set_position({key_bounds.offset.x + key_padding.x, key_bounds.offset.y + key_padding.y});
            part->set_layout_size(part_size);
            part->set_clip_id(clip_id());
            part->update_layout(true);
            cursor_x += key_margin.x + key_size.x + key_margin.z;
            if (i + 1u < _parts.size()) cursor_x += style.inline_spacing();
        }
    }

    void ShortcutLabel::translate(const amal::vec2 &delta)
    {
        Widget::translate(delta);
        for (auto *part : _parts)
            if (part) part->translate(delta);
        for (auto &rect : _key_rects) rect.bounds.offset += delta;
    }

    void ShortcutLabel::rebuild_clip_rects()
    {
        set_clip_id(parent() ? parent()->content_clip_id() : clip_id());
        for (auto *part : _parts)
        {
            if (!part) continue;
            part->set_clip_id(clip_id());
            part->rebuild_clip_rects();
        }
        for (auto &rect : _key_rects) rect.clip_id = clip_id();
    }

    void ShortcutLabel::reset_draw_records()
    {
        for (auto &draw_id : _key_draw_ids) draw_id = {};
        for (auto *part : _parts)
            if (part) part->reset_draw_records();
    }

    void ShortcutLabel::invalidate_style()
    {
        Widget::invalidate_style();
        for (auto *part : _parts)
            if (part) part->invalidate_style();
    }

    bool ShortcutLabel::update_locale()
    {
        bool changed = Widget::update_locale();
        for (auto *part : _parts)
            if (part) changed |= part->update_locale();
        return changed;
    }

    void ShortcutLabel::add_state_flags_inherit(WidgetStateFlags flags)
    {
        Widget::add_state_flags_inherit(flags);
        if ((flags & WidgetStateFlagBits::visible) && !is_visible()) flags &= ~WidgetStateFlagBits::visible;
        for (auto *part : _parts)
            if (part) part->add_state_flags_inherit(flags);
    }

    void ShortcutLabel::remove_state_flags_inherit(WidgetStateFlags flags)
    {
        Widget::remove_state_flags_inherit(flags);
        for (auto *part : _parts)
            if (part) part->remove_state_flags_inherit(flags);
    }

    u32 ShortcutLabel::get_depth_requirement() const
    {
        u32 requirement = 1u;
        for (auto *part : _parts)
            if (part) requirement = amal::max(requirement, 1u + part->get_depth_requirement());
        return requirement;
    }

    void ShortcutLabel::update_depth(const amal::vec2 &depth_range)
    {
        Widget::update_depth(depth_range);
        const auto child_range = detail::depth_work_range(depth_range);
        for (auto *part : _parts)
            if (part) part->update_depth(child_range);
        for (auto &rect : _key_rects) rect.depth = get_rect().depth;
    }

    void ShortcutLabel::draw(DrawCtx &ctx)
    {
        if (!is_visible() && !(ctx.reason & DrawReasonBits::invalidate)) return;
        auto *quads_stream = get_primary_quads_stream();
        const auto &key_style = get_theme()->get_style(_key_style.id);
        for (size_t i = 0u; i < _key_rects.size(); ++i)
        {
            QuadsInstanceData data{};
            data.rect = _key_rects[i].bounds;
            data.z_order = get_z_order();
            const bool visible = fill_quads_instance_by_style(key_style, clip_id(), data);
            emit_quads_instance(ctx, quads_stream, _key_draw_ids[i], data, _key_rects[i], visible, false);
        }
        if (ctx.reason & DrawReasonBits::invalidate)
        {
            for (auto *part : _parts)
                if (part) part->draw_local(ctx);
            return;
        }
        const amal::vec4 clip = get_content_clip_rect();
        for (auto *part : _parts)
        {
            if (!part) continue;
            DrawCtx part_ctx = ctx;
            detail::draw_child_in_clip(part, part_ctx, clip);
        }
    }

    void ShortcutLabel::on_attach()
    {
        Widget::on_attach();
        for (auto *part : _parts)
            if (part && !part->is_attached()) part->on_attach();
    }

    void ShortcutLabel::on_detach()
    {
        for (auto *part : _parts)
            if (part && part->is_attached()) part->on_detach();
        Widget::on_detach();
    }

    ShortcutInput::ShortcutInput(u32 id, const acul::vector<StringView> &parts)
        : DrawBlock(id, WidgetFlagBits::visible | WidgetFlagBits::attachable, AUIK_TAG_SHORTCUT_INPUT)
    {
        set_style_tag(AUIK_STYLE_TAG_SHORTCUT_INPUT);
        set_draw_block_flags(DrawBlockFlagBits::none);
        _label = make_shortcut_label(AUIK_TAG_SHORTCUT, parts);
        _label->unset_cache_snapshot();
        _caret = make_text(AUIK_TAG_TEXT, "|");
        _caret->unset_cache_snapshot();
        _caret->set_style_tag(AUIK_STYLE_TAG_SHORTCUT_INPUT_CARET);
        auto *content = make_block();
        content->unset_cache_snapshot();
        content->set_size(AUIK_SIZE_FIT);
        content->add_child(_label, make_layout_flags(ChildLayout::inline_, HAlign::left, VAlign::center));
        content->add_child(_caret, make_layout_flags(ChildLayout::inline_, HAlign::left, VAlign::center));
        add_child(content, make_layout_flags(ChildLayout::block, HAlign::center, VAlign::center));
        set_parts(parts);
    }

    void ShortcutInput::set_parts(const acul::vector<StringView> &parts)
    {
        _empty = parts.empty();
        _label->set_parts(parts);
        if (_capturing && _empty) _caret->set_visible();
        else _caret->unset_visible();
        if (_caret->is_attached()) _caret->sync_widget_flags();
    }

    void ShortcutInput::set_capturing(bool value)
    {
        _capturing = value;
        if (_capturing && _empty) _caret->set_visible();
        else _caret->unset_visible();
        if (is_attached()) sync_widget_flags();
    }

    namespace
    {
        void write_shortcut_label(acul::bin_stream &stream, umbf::Block *block)
        {
            const auto *label = static_cast<ShortcutLabel *>(block);
            detail::write_widget_common_data(stream, *label);
            const auto parts = label->parts();
            stream.write(static_cast<u32>(parts.size()));
            for (const auto &part : parts) stream.write(part);
        }

        umbf::Block *read_shortcut_label(acul::bin_stream &stream)
        {
            const auto common = detail::read_widget_common_data(stream);
            u32 part_count = 0u;
            stream.read(part_count);
            acul::vector<acul::string> parts;
            parts.reserve(part_count);
            for (u32 part_i = 0u; part_i < part_count; ++part_i)
            {
                acul::string part;
                stream.read(part);
                parts.push_back(std::move(part));
            }
            acul::vector<StringView> views;
            views.reserve(parts.size());
            for (const auto &part : parts) views.emplace_back(part);
            auto *label = acul::alloc<ShortcutLabel>(common.id, views);
            detail::apply_widget_common_data(label, common);
            return label;
        }
    } // namespace

    namespace streams
    {
        extern AUIK_EXPORT const umbf::registry::BlockStream shortcut_label{read_shortcut_label, write_shortcut_label};
    }
} // namespace auik
