#include <auik/theme.hpp>

namespace auik
{
    using namespace detail;
    constexpr StylePropertyFlags g_style_inheritable_mask = StylePropertiesBits::text_color |
                                                            StylePropertiesBits::text_size | StylePropertiesBits::font |
                                                            StylePropertiesBits::inline_spacing;
    constexpr StylePropertyFlags g_style_all_mask = acul::flag_traits<StylePropertiesBits>::all_flags;
    constexpr StylePropertyFlags g_style_non_inheritable_mask = g_style_all_mask & ~g_style_inheritable_mask;

    struct ResolvedStyleDesc
    {
        const Style *style = nullptr;
        StylePropertyFlags take = StylePropertiesBits::none;
    };

    static inline void apply_desc_masked(Style &out, const Style &d, StylePropertyFlags take)
    {
        if (take & StylePropertiesBits::padding) out.padding(d.padding());
        if (take & StylePropertiesBits::margin) out.margin(d.margin());
        if (take & StylePropertiesBits::background_color) out.background_color(d.background_color());
        if (take & StylePropertiesBits::text_color) out.text_color(d.text_color());
        if (take & StylePropertiesBits::border_color) out.border_color(d.border_color());
        if (take & StylePropertiesBits::border_radius) out.border_radius(d.border_radius());
        if (take & StylePropertiesBits::border_thickness) out.border_thickness(d.border_thickness());
        if (take & StylePropertiesBits::corner_mask) out.corner_mask(d.corner_mask());
        if (take & StylePropertiesBits::text_size) out.text_size(d.text_size());
        if (take & StylePropertiesBits::font) out.font(d.font());
        if (take & StylePropertiesBits::inline_spacing) out.inline_spacing(d.inline_spacing());
        if (take & StylePropertiesBits::width) out.width(d.width());
        if (take & StylePropertiesBits::height) out.height(d.height());
        if (take & StylePropertiesBits::min_width) out.min_width(d.min_width());
        if (take & StylePropertiesBits::min_height) out.min_height(d.min_height());
        if (take & StylePropertiesBits::extra)
        {
            for (const StyleExtra *extra = d.extra(); extra; extra = extra->next)
            {
                if (extra->id == AUIK_STYLE_EXTRA_ALIGN)
                    out.align_extra(*static_cast<const StyleExtraAlign *>(extra->data));
                else if (extra->id == AUIK_STYLE_EXTRA_TEXT)
                    out.text_extra(*static_cast<const StyleExtraText *>(extra->data));
            }
        }
    }

    StyleID Theme::get_resolved_style(u32 type, u32 id, u32 parent, StyleState state)
    {
        detail::StylePropertyFlags need_inh = g_style_inheritable_mask;
        detail::StylePropertyFlags need_non_inh = g_style_non_inheritable_mask;
        const bool use_normal_fallback = state != StyleState::normal;
        size_t resolve_seed = 0;
        ResolvedStyleDesc chain[8u]{};
        u32 chain_count = 0u;

        const auto push_chain = [&](const Style *desc, StylePropertyFlags take) {
            if (!desc || static_cast<u16>(take) == 0u) return;
            assert(chain_count < 8u && "resolved style chain overflow");
            chain[chain_count++] = {desc, take};
        };

        const auto collect_from_desc = [&](const Style *desc) {
            if (!desc) return;

            const auto take_non_inh = desc->mask() & need_non_inh;
            const auto take_inh = desc->mask() & need_inh;
            push_chain(desc, take_non_inh | take_inh);
            if (static_cast<u16>(take_non_inh) != 0) need_non_inh &= ~take_non_inh;
            if (static_cast<u16>(take_inh) != 0) need_inh &= ~take_inh;
        };

        const auto collect_inheritable_only_desc = [&](const Style *desc) {
            if (!desc) return;
            const auto take_inh = desc->mask() & need_inh;
            if (static_cast<u16>(take_inh) != 0)
            {
                push_chain(desc, take_inh);
                need_inh &= ~take_inh;
            }
        };

        const auto get_desc = [&](u32 key, StyleState source_state) -> const Style * {
            const StyleID style_id = get(key, source_state);
            if (style_id == STYLE_ID_INVALID) return nullptr;
            return _style_options_pool[style_id];
        };

        const auto apply_from_key = [&](u32 key, bool inheritable_only, u8 source_id) {
            if (key == 0 && inheritable_only) return;

            auto consume_cache_key = [&](const Style *desc, StyleState source_state) {
                if (!desc) return;
                const auto prev_non_inh = need_non_inh;
                const auto prev_inh = need_inh;
                if (inheritable_only) collect_inheritable_only_desc(desc);
                else collect_from_desc(desc);

                const auto used_non_inh = prev_non_inh & ~need_non_inh;
                const auto used_inh = prev_inh & ~need_inh;
                const auto used = used_non_inh | used_inh;
                if (static_cast<u16>(used) == 0) return;

                acul::hash_combine(resolve_seed, source_id);
                acul::hash_combine(resolve_seed, key);
                acul::hash_combine(resolve_seed, static_cast<u8>(source_state));
                acul::hash_combine(resolve_seed, static_cast<u16>(used));
            };

            const Style *desc_state = get_desc(key, state);
            consume_cache_key(desc_state, state);

            if (!use_normal_fallback) return;
            const Style *desc_normal = get_desc(key, StyleState::normal);
            consume_cache_key(desc_normal, StyleState::normal);
        };

        // Non-inheritable: id -> type -> global
        // Inheritable: id -> type -> parent -> global
        apply_from_key(id, false, 1);
        apply_from_key(type, false, 2);
        apply_from_key(parent, true, 3);
        apply_from_key(AUIK_STYLE_TAG_GLOBAL, false, 4);

        const u64 cache_key = static_cast<u64>(resolve_seed);
        auto it_cache = _resolved.find(cache_key);
        if (it_cache != _resolved.end()) return it_cache->second;

        const StyleID resolved_id = static_cast<StyleID>(_resolved_pool.size());
        auto *resolved = acul::alloc<Style>();
        for (u32 i = 0u; i < chain_count; ++i) apply_desc_masked(*resolved, *chain[i].style, chain[i].take);
        _resolved_pool.push_back(resolved);
        _resolved.emplace(cache_key, resolved_id);
        return resolved_id;
    }

    StyleID Theme::add_desc(u32 key, const Style &style, StyleState state)
    {
        const u64 full_key = make_theme_key(key, state);
        auto it = _style_options.find(full_key);
        if (it != _style_options.end())
        {
            *_style_options_pool[it->second] = style;
            clear_resolved_cache();
            return it->second;
        }
        const StyleID id = static_cast<StyleID>(_style_options_pool.size());
        _style_options_pool.push_back(acul::alloc<Style>(style));
        _style_options.emplace(full_key, id);
        clear_resolved_cache();
        return id;
    }
} // namespace auik
