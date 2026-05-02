#include <acul/memory/alloc.hpp>
#include <auik/v2/theme.hpp>
#include <auik/v2/widgets/checkbox.hpp>
#include <auik/v2/widgets/color_picker.hpp>
#include <auik/v2/widgets/combobox.hpp>
#include <auik/v2/widgets/containers.hpp>
#include <auik/v2/widgets/image_button.hpp>
#include <auik/v2/widgets/radio_button.hpp>
#include <auik/v2/widgets/rubber_band.hpp>
#include <auik/v2/widgets/slider.hpp>
#include <auik/v2/widgets/switch_button.hpp>
#include <auik/v2/widgets/tabbar.hpp>
#include <auik/v2/widgets/text_button.hpp>
#include <auik/v2/widgets/textbox.hpp>
#include <auik/v2/widgets/tooltip.hpp>
#include <auik/v2/widgets/window.hpp>

namespace auik::v2
{
    using namespace detail;
    constexpr StylePropertyFlags g_style_inheritable_mask = StylePropertiesBits::text_color |
                                                            StylePropertiesBits::text_size | StylePropertiesBits::font |
                                                            StylePropertiesBits::inline_spacing;
    constexpr StylePropertyFlags g_style_all_mask = acul::flag_traits<StylePropertiesBits>::all_flags;
    constexpr StylePropertyFlags g_style_non_inheritable_mask = g_style_all_mask & ~g_style_inheritable_mask;
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
    }

    StyleID Theme::get_resolved_style(u32 type, u32 id, u32 parent, StyleState state)
    {
        Style out{};
        detail::StylePropertyFlags need_inh = g_style_inheritable_mask;
        detail::StylePropertyFlags need_non_inh = g_style_non_inheritable_mask;
        const bool use_normal_fallback = state != StyleState::normal;
        size_t resolve_seed = 0;

        const auto apply_from_desc = [&](const Style *desc) {
            if (!desc) return;

            const auto take_non_inh = desc->mask() & need_non_inh;
            if (static_cast<u16>(take_non_inh) != 0)
            {
                apply_desc_masked(out, *desc, take_non_inh);
                need_non_inh &= ~take_non_inh;
            }

            const auto take_inh = desc->mask() & need_inh;
            if (static_cast<u16>(take_inh) != 0)
            {
                apply_desc_masked(out, *desc, take_inh);
                need_inh &= ~take_inh;
            }
        };

        const auto apply_inheritable_only_desc = [&](const Style *desc) {
            if (!desc) return;
            const auto take_inh = desc->mask() & need_inh;
            if (static_cast<u16>(take_inh) != 0)
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
                if (inheritable_only) apply_inheritable_only_desc(desc);
                else apply_from_desc(desc);

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
        apply_from_key(AUIK_TAG_GLOBAL, false, 4);

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

    Theme *create_default_theme(Font *default_font, f32 dpi)
    {
        auto *theme = acul::alloc<Theme>();

        // Base palette
        constexpr auto c_surface = amal::rgba8_to_vec4(41, 41, 43, 255);
        constexpr auto c_surface_light = amal::rgba8_to_vec4(74, 74, 75, 255);
        constexpr auto c_hover = amal::rgba8_to_vec4(90, 90, 90, 255);
        constexpr auto c_active = amal::rgba8_to_vec4(31, 31, 31, 255);
        constexpr auto c_border = amal::rgba8_to_vec4(51, 51, 51, 255);
        constexpr auto c_ascent = amal::rgba8_to_vec4(72, 114, 255, 255);
        const amal::vec4 c_white = {0.9f, 0.9f, 0.9f, 1.0f};
        const amal::vec4 c_semi_transparent = {0.0f, 0.0f, 0.0f, 0.5f};
        const amal::vec2 empty_vec2{0.0f};

        // Global settings.
        theme->add_style(AUIK_TAG_GLOBAL, make_style()
                                              .text_color(c_white)
                                              .text_size(12.5f * dpi)
                                              .font(default_font)
                                              .margin(amal::vec2{0.0f, 5.0f})
                                              .inline_spacing(4.0f));
        theme->add_style(AUIK_TAG_CARET,
                         make_style().padding(amal::vec2{1.0f, 0.0f}).background_color({0.9f, 0.9f, 0.9f, 1.0f}));
        theme->add_style(AUIK_TAG_NO_PAD, make_style().margin(empty_vec2).padding(empty_vec2));
        theme->add_style(AUIK_TAG_PLACEHOLDER, make_style()
                                                   .margin(empty_vec2)
                                                   .padding(empty_vec2)
                                                   .text_size(12.5f * dpi)
                                                   .text_color({0.9f, 0.9f, 0.9f, 0.5f}));
        theme->add_style(AUIK_TAG_SELECTION, make_style().background_color(amal::rgba8_to_vec4(72, 114, 255, 120)));
        theme->add_style(AUIK_TAG_RUBBER_BAND, make_style()
                                                   .background_color(amal::rgba8_to_vec4(72, 114, 255, 45))
                                                   .border_color(amal::rgba8_to_vec4(72, 114, 255, 210))
                                                   .border_thickness(1.0f));
        theme->add_style(AUIK_TAG_TEXT_DRAG_ICON, make_style().background_color({0.8f, 0.8f, 0.8f, 1.0f}));
        theme->set_var(AUIK_VAR_COLOR_PICKER_SIZE, 185.0f);

        // Window body.
        theme->add_style(AUIK_TAG_WINDOW, make_style()
                                              .padding(amal::vec2{10.0f, 8.0f})
                                              .background_color(c_surface)
                                              .border_color(c_border)
                                              .border_radius(6.0f)
                                              .border_thickness(1.0f));
        // Window header.
        theme->add_style(AUIK_TAG_WINDOW_HEADER, make_style()
                                                     .padding(amal::vec2{10.0f, 8.0f})
                                                     .background_color(c_active)
                                                     .border_radius(6.0f)
                                                     .corner_mask(0x3u));
        theme->add_style(AUIK_TAG_WINDOW_HEADER, make_style().background_color(c_surface_light), StyleState::focus);

        theme->add_style(AUIK_TAG_TITLEBAR, make_style().background_color(c_active));
        theme->add_style(AUIK_TAG_TITLEBAR_ICON,
                         make_style().margin(amal::vec4{8.0f, 0.0f, 8.0f, 0.0f}).background_color(c_surface));

        theme->add_style(AUIK_TAG_TEXT_BUTTON, make_style()
                                                   .padding(amal::vec2{10.0f, 4.0f})
                                                   .background_color(c_surface_light)
                                                   .border_radius(4.0f)
                                                   .border_color(c_border)
                                                   .border_thickness(1.0f));
        theme->add_style(AUIK_TAG_TEXT_BUTTON, make_style().background_color(c_hover), StyleState::hover);
        theme->add_style(AUIK_TAG_TEXT_BUTTON, make_style().background_color(c_surface_light), StyleState::active);

        theme->add_style(AUIK_TAG_IMAGE_BUTTON,
                         make_style().padding(amal::vec2{6.0f}).background_color(c_surface_light).border_radius(4.0f));
        theme->add_style(AUIK_TAG_IMAGE_BUTTON, make_style().background_color(c_hover), StyleState::hover);
        theme->add_style(AUIK_TAG_IMAGE_BUTTON, make_style().background_color(c_surface_light), StyleState::active);

        theme->add_style(AUIK_TAG_TEXTBOX, make_style()
                                               .margin(amal::vec2{0.0f, 5.0f})
                                               .padding(amal::vec2{8.0f, 4.0f})
                                               .background_color(c_surface_light)
                                               .border_radius(4.0f));

        theme->add_style(AUIK_TAG_CHECKBOX,
                         make_style().padding(amal::vec2{3.0f}).background_color(c_surface_light).border_radius(3.0f));
        theme->add_style(AUIK_TAG_CHECKBOX, make_style().background_color(c_hover), StyleState::hover);
        theme->add_style(AUIK_TAG_CHECKBOX, make_style().background_color(c_surface_light), StyleState::active);

        theme->add_style(
            AUIK_TAG_RADIO_BUTTON,
            make_style().padding(amal::vec2{10.0f}).background_color(c_surface_light).border_radius(10.0f));
        theme->add_style(AUIK_TAG_RADIO_BUTTON, make_style().background_color(c_hover), StyleState::hover);
        theme->add_style(AUIK_TAG_RADIO_BUTTON, make_style().background_color(c_surface_light), StyleState::active);
        theme->add_style(
            AUIK_TAG_RADIO_BUTTON_INDICATOR,
            make_style().margin(empty_vec2).padding(amal::vec2{5.0f}).background_color(c_white).border_radius(5.0f));

        theme->add_style(AUIK_TAG_SWITCH_BUTTON,
                         make_style().padding(amal::vec2{2.0f}).background_color(c_surface_light).border_radius(10.0f));
        theme->add_style(AUIK_TAG_SWITCH_BUTTON, make_style().background_color(c_hover), StyleState::hover);
        theme->add_style(AUIK_TAG_SWITCH_BUTTON, make_style().background_color(c_surface_light), StyleState::active);
        theme->add_style(AUIK_TAG_SWITCH_BUTTON_ON,
                         make_style().padding(amal::vec2{2.0f}).background_color(c_ascent).border_radius(10.0f));
        theme->add_style(AUIK_TAG_SWITCH_BUTTON_GRAB,
                         make_style().padding(amal::vec2{8.0f}).background_color(c_white).border_radius(8.0f));

        theme->add_style(
            AUIK_TAG_COMBO_BOX,
            make_style().padding(amal::vec2{10.0f, 4.0f}).background_color(c_surface_light).border_radius(4.0f));
        theme->add_style(AUIK_TAG_COMBO_BOX, make_style().background_color(c_hover), StyleState::hover);
        theme->add_style(AUIK_TAG_COMBO_BOX, make_style().corner_mask(0x3u), StyleState::focus);
        theme->add_style(AUIK_TAG_COMBO_BOX_POPUP, make_style()
                                                       .margin(empty_vec2)
                                                       .padding(amal::vec2{0.0f, 2.0f})
                                                       .background_color(c_active)
                                                       .border_radius(4.0f)
                                                       .corner_mask(0xCu));
        theme->add_style(
            AUIK_TAG_COMBO_BOX_ITEM,
            make_style().margin(amal::vec2{4.0f, 2.0f}).padding(amal::vec2{6.0f, 4.0f}).border_radius(3.0f));
        theme->add_style(AUIK_TAG_COMBO_BOX_ITEM, make_style().background_color(c_hover), StyleState::hover);
        theme->add_style(AUIK_TAG_COMBO_BOX_ITEM, make_style().background_color(c_ascent), StyleState::focus);

        theme->add_style(AUIK_TAG_TAB_BAR_ITEM, make_style()
                                                    .margin(empty_vec2)
                                                    .background_color(c_surface_light)
                                                    .padding(amal::vec2{10.0f, 5.0f})
                                                    .border_radius(4.0f));
        theme->add_style(AUIK_TAG_TAB_BAR_ITEM, make_style().background_color(c_hover), StyleState::hover);
        theme->add_style(AUIK_TAG_TAB_BAR_ITEM, make_style().background_color(c_ascent), StyleState::focus);
        theme->add_style(AUIK_TAG_TABBAR_POPUP_BTN,
                         make_style().margin(empty_vec2).padding(amal::vec2{8.0f, 5.0f}).border_radius(4.0f));
        theme->add_style(AUIK_TAG_TABBAR_POPUP_BTN, make_style().background_color(c_hover), StyleState::hover);
        theme->add_style(AUIK_TAG_TAB_BAR_POPUP, make_style()
                                                     .margin(empty_vec2)
                                                     .padding(amal::vec2{0.0f, 2.0f})
                                                     .background_color(c_active)
                                                     .border_radius(4.0f));
        theme->add_style(AUIK_TAG_CLOSE_BUTTON,
                         make_style().margin(amal::vec2{4.0f, 0.0f}).padding(amal::vec2{8.0f}).border_radius(4.0f));
        theme->add_style(AUIK_TAG_CLOSE_BUTTON, make_style().background_color({1.0f, 1.0f, 1.0f, 0.15f}),
                         StyleState::hover);
        theme->add_style(AUIK_TAG_CLOSE_BUTTON, make_style().background_color(c_surface_light), StyleState::active);

        theme->add_style(
            AUIK_TAG_SLIDER,
            make_style().background_color(c_surface_light).border_radius(2.5f).padding(amal::vec2{0.0f, 4.0f}));
        theme->add_style(AUIK_TAG_SLIDER, make_style().background_color(c_ascent), StyleState::active);
        theme->add_style(AUIK_TAG_SLIDER_GRAB, make_style()
                                                   .margin(amal::vec2{0.0f, 4.0f})
                                                   .padding(amal::vec2{6.0f})
                                                   .background_color(c_white)
                                                   .border_radius(6.0f));
        theme->add_style(AUIK_TAG_SLIDER_GRAB,
                         make_style()
                             .margin(amal::vec2{-4.0f, 0.0f})
                             .padding(amal::vec2{10.0f})
                             .border_color({1.0f, 1.0f, 1.0f, 0.25f})
                             .border_radius(10.0f)
                             .border_thickness(4.0f),
                         StyleState::hover);
        theme->add_style(AUIK_TAG_SLIDER_GRAB,
                         make_style()
                             .margin(amal::vec2{-4.0f, 0.0f})
                             .padding(amal::vec2{10.0f})
                             .border_color({1.0f, 1.0f, 1.0f, 0.35f})
                             .border_radius(10.0f)
                             .border_thickness(4.0f),
                         StyleState::active);
        theme->add_style(AUIK_TAG_GRADIENT_SLIDER_GRAB, make_style()
                                                            .padding(amal::vec2{5.0f})
                                                            .background_color(c_white)
                                                            .border_color(c_semi_transparent)
                                                            .border_thickness(1.5f)
                                                            .border_radius(5.0f));
        theme->add_style(AUIK_TAG_GRADIENT_SLIDER_GRAB_BORDER, make_style()
                                                                   .padding(amal::vec2{8.0f})
                                                                   .background_color(c_white)
                                                                   .border_thickness(1.5f)
                                                                   .border_color(c_semi_transparent)
                                                                   .border_radius(8.0f));
        theme->add_style(
            AUIK_TAG_GRADIENT_SLIDER,
            make_style().background_color(c_surface_light).border_radius(2.0f).padding(amal::vec2{0.0f, 4.0f}));

        theme->add_style(AUIK_TAG_TOOLTIP, make_style()
                                               .margin(empty_vec2)
                                               .padding(amal::vec2{10.0f, 6.0f})
                                               .background_color(c_active)
                                               .border_color(c_border)
                                               .border_radius(3.0f)
                                               .text_color({0.92f, 0.92f, 0.92f, 1.0f}));

        theme->add_style(AUIK_TAG_SCROLLBAR_TRACK,
                         make_style().background_color(c_active).margin(empty_vec2).padding(amal::vec2{2.0f}));
        theme->add_style(AUIK_TAG_SCROLLBAR_THUMB, make_style()
                                                       .background_color(amal::vec4{0.35f, 0.35f, 0.35f, 1.0f})
                                                       .margin(empty_vec2)
                                                       .padding(amal::vec2{4.0f, 0.0f})
                                                       .border_radius(2.0f));
        auto scroll_thumb_style = make_style().background_color(amal::vec4{0.5f, 0.5f, 0.5f, 1.0f});
        theme->add_style(AUIK_TAG_SCROLLBAR_THUMB, scroll_thumb_style, StyleState::hover);
        theme->add_style(AUIK_TAG_SCROLLBAR_THUMB, scroll_thumb_style, StyleState::active);

        return theme;
    }
} // namespace auik::v2
