#include <acul/memory/alloc.hpp>
#include <auik/v2/theme.hpp>
#include <auik/v2/window.hpp>

namespace auik::v2
{
    using namespace detail;
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
    }

    StyleID Theme::get_resolved_style(u32 type, u32 id, u32 parent, StyleState state)
    {
        Style out{};
        detail::StylePropertyFlags need_inh = detail::g_inheritable_mask;
        detail::StylePropertyFlags need_non_inh = detail::g_non_inheritable_mask;
        const bool use_normal_fallback = state != StyleState::normal;
        size_t resolve_seed = 0;

        const auto apply_from_desc = [&](const Style *desc) {
            if (!desc) return;

            const auto take_non_inh = desc->mask() & need_non_inh;
            if (static_cast<u8>(take_non_inh) != 0)
            {
                apply_desc_masked(out, *desc, take_non_inh);
                need_non_inh &= ~take_non_inh;
            }

            const auto take_inh = desc->mask() & need_inh;
            if (static_cast<u8>(take_inh) != 0)
            {
                apply_desc_masked(out, *desc, take_inh);
                need_inh &= ~take_inh;
            }
        };

        const auto apply_inheritable_only_desc = [&](const Style *desc) {
            if (!desc) return;
            const auto take_inh = desc->mask() & need_inh;
            if (static_cast<u8>(take_inh) != 0)
            {
                apply_desc_masked(out, *desc, take_inh);
                need_inh &= ~take_inh;
            }
        };

        const auto get_desc = [&](u32 key, StyleState source_state) -> const Style * {
            const StyleID style_id = get(key, source_state);
            if (style_id == STYLE_ID_INVALID) return nullptr;
            return &_style_options_pool[style_id];
        };

        const auto apply_from_key = [&](u32 key, bool inheritable_only, u8 source_id) {
            if (key == 0 && inheritable_only) return;

            auto consume_cache_key = [&](const Style *desc, StyleState source_state) {
                if (!desc) return;
                const auto prev_non_inh = need_non_inh;
                const auto prev_inh = need_inh;
                if (inheritable_only)
                    apply_inheritable_only_desc(desc);
                else
                    apply_from_desc(desc);

                const auto used_non_inh = prev_non_inh & ~need_non_inh;
                const auto used_inh = prev_inh & ~need_inh;
                const auto used = used_non_inh | used_inh;
                if (static_cast<u8>(used) == 0) return;

                acul::hash_combine(resolve_seed, source_id);
                acul::hash_combine(resolve_seed, key);
                acul::hash_combine(resolve_seed, static_cast<u8>(source_state));
                acul::hash_combine(resolve_seed, static_cast<u8>(used));
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
        apply_from_key(AUIK_STYLE_ID_GLOBAL, false, 4);

        const u64 cache_key = static_cast<u64>(resolve_seed);
        auto it_cache = _resolved.find(cache_key);
        if (it_cache != _resolved.end()) return it_cache->second;

        const StyleID resolved_id = static_cast<StyleID>(_resolved_pool.size());
        _resolved_pool.push_back(out);
        _resolved.emplace(cache_key, resolved_id);
        return resolved_id;
    }

    StyleID Theme::add_desc(u32 key, const Style &style, StyleState state)
    {
        const u64 full_key = make_theme_key(key, state);
        auto it = _style_options.find(full_key);
        if (it != _style_options.end())
        {
            _style_options_pool[it->second] = style;
            clear_resolved_cache();
            return it->second;
        }
        const StyleID id = static_cast<StyleID>(_style_options_pool.size());
        _style_options_pool.push_back(style);
        _style_options.emplace(full_key, id);
        clear_resolved_cache();
        return id;
    }

    Theme *create_default_theme()
    {
        auto *theme = acul::alloc<Theme>();

        // Base palette
        const auto c_surface = color_rgba8(41, 41, 43, 255);       // ~0.16
        const auto c_surface_light = color_rgba8(74, 74, 75, 255); // ~0.29
        const auto c_hover = color_rgba8(97, 97, 97, 255);         // ~0.38
        const auto c_active = color_rgba8(107, 107, 108, 255);     // ~0.42

        // Global settings.
        theme->add_style(AUIK_STYLE_ID_GLOBAL, make_style().text_color({1.0f}).margin(amal::vec2{8.0f, 8.0f}));

        // Window body.
        theme->add_style(AUIK_STYLE_ID_WINDOW_TYPE, make_style()
                                                        .padding(amal::vec2{10.0f, 8.0f})
                                                        .background_color(c_surface)
                                                        .border_color({0.2f, 0.2f, 0.2f, 1.0f})
                                                        .border_radius(4.0f)
                                                        .border_thickness(1.0f));
        theme->add_style(AUIK_STYLE_ID_WINDOW_TYPE, make_style().background_color(c_hover), StyleState::hover);
        theme->add_style(AUIK_STYLE_ID_WINDOW_TYPE, make_style().background_color(c_active), StyleState::active);

        // Window header.
        theme->add_style(
            AUIK_STYLE_ID_WINDOW_HEADER_TYPE,
            make_style().padding(amal::vec2{10.0f, 8.0f}).background_color(c_surface_light).border_radius(4.0f));
        theme->add_style(AUIK_STYLE_ID_WINDOW_HEADER_TYPE, make_style().background_color(c_hover), StyleState::hover);
        theme->add_style(AUIK_STYLE_ID_WINDOW_HEADER_TYPE, make_style().background_color(c_active), StyleState::active);

        return theme;
    }
} // namespace auik::v2
