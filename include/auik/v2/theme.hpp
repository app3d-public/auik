#pragma once

#include <acul/any.hpp>
#include <acul/enum.hpp>
#include <acul/hash/hashmap.hpp>
#include <acul/hash/utils.hpp>
#include <acul/scalars.hpp>
#include <acul/vector.hpp>
#include <amal/color.hpp>
#include <amal/vector.hpp>
#include <auik/v2/widget_tags.hpp>

#define AUIK_THEME_FONT_REGULAR 0
#define AUIK_THEME_FONT_BOLD    1

namespace auik::v2
{
    class Font;

    namespace detail
    {
        inline constexpr u32 pack_rgba8(u8 r, u8 g, u8 b, u8 a)
        {
            // Match GLSL unpackUnorm4x8(): x=LSB -> r, y -> g, z -> b, w=MSB -> a.
            return static_cast<u32>(r) | (static_cast<u32>(g) << 8u) | (static_cast<u32>(b) << 16u) |
                   (static_cast<u32>(a) << 24u);
        }

        inline u32 pack_rgba8(const amal::vec4 &color)
        {
            const auto to_u8 = [](f32 v) -> u8 {
                const f32 clamped = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
                return static_cast<u8>(clamped * 255.0f + 0.5f);
            };
            return pack_rgba8(to_u8(color.x), to_u8(color.y), to_u8(color.z), to_u8(color.w));
        }

        struct StylePropertiesBits
        {
            enum enum_type : u16
            {
                none = 0x0,
                padding = 0x1,
                margin = 0x2,
                background_color = 0x4,
                text_color = 0x8,
                border_color = 0x10,
                border_radius = 0x20,
                border_thickness = 0x40,
                corner_mask = 0x80,
                text_size = 0x100,
                font = 0x200,
                inline_spacing = 0x400
            };

            using flag_bitmask = std::true_type;
        };

        using StylePropertyFlags = acul::flags<StylePropertiesBits>;

        constexpr StylePropertyFlags g_style_visible_draw_mask =
            StylePropertiesBits::border_color | StylePropertiesBits::background_color;
    } // namespace detail

    enum class StyleState : u8
    {
        normal,
        hover,
        active,
        focus,
        max
    };
    using StyleID = u32;

    class Style final
    {
    public:
        [[nodiscard]] const amal::vec4 &padding() const { return _padding; }
        Style &padding(const amal::vec4 &value)
        {
            _padding = value;
            _mask |= detail::StylePropertiesBits::padding;
            return *this;
        }
        Style &padding(const amal::vec2 &value)
        {
            _padding = {value.x, value.y, value.x, value.y};
            _mask |= detail::StylePropertiesBits::padding;
            return *this;
        }

        [[nodiscard]] const amal::vec4 &margin() const { return _margin; }
        Style &margin(const amal::vec4 &value)
        {
            _margin = value;
            _mask |= detail::StylePropertiesBits::margin;
            return *this;
        }
        Style &margin(const amal::vec2 &value)
        {
            _margin = {value.x, value.y, value.x, value.y};
            _mask |= detail::StylePropertiesBits::margin;
            return *this;
        }

        [[nodiscard]] u32 background_color() const { return _background_color; }
        Style &background_color(const amal::vec4 &value)
        {
            _background_color = detail::pack_rgba8(value);
            _mask |= detail::StylePropertiesBits::background_color;
            return *this;
        }
        Style &background_color(u32 value)
        {
            _background_color = value;
            _mask |= detail::StylePropertiesBits::background_color;
            return *this;
        }

        [[nodiscard]] u32 text_color() const { return _text_color; }
        Style &text_color(const amal::vec4 &value)
        {
            _text_color = detail::pack_rgba8(value);
            _mask |= detail::StylePropertiesBits::text_color;
            return *this;
        }
        Style &text_color(u32 value)
        {
            _text_color = value;
            _mask |= detail::StylePropertiesBits::text_color;
            return *this;
        }

        [[nodiscard]] f32 text_size() const { return _text_size; }
        Style &text_size(f32 value)
        {
            _text_size = value;
            _mask |= detail::StylePropertiesBits::text_size;
            return *this;
        }

        [[nodiscard]] Font *font() const { return _font; }
        Style &font(Font *value)
        {
            _font = value;
            _mask |= detail::StylePropertiesBits::font;
            return *this;
        }

        [[nodiscard]] f32 inline_spacing() const { return _inline_spacing; }
        Style &inline_spacing(f32 value)
        {
            _inline_spacing = value;
            _mask |= detail::StylePropertiesBits::inline_spacing;
            return *this;
        }

        [[nodiscard]] u32 border_color() const { return _border_color; }
        Style &border_color(const amal::vec4 &value)
        {
            _border_color = detail::pack_rgba8(value);
            _mask |= detail::StylePropertiesBits::border_color;
            return *this;
        }
        Style &border_color(u32 value)
        {
            _border_color = value;
            _mask |= detail::StylePropertiesBits::border_color;
            return *this;
        }

        [[nodiscard]] f32 border_radius() const { return _border_radius; }
        Style &border_radius(f32 value)
        {
            _border_radius = value;
            _mask |= detail::StylePropertiesBits::border_radius;
            if (value <= 0.0f)
            {
                _corner_mask = 0u;
                _mask |= detail::StylePropertiesBits::corner_mask;
            }
            else if (!(_mask & detail::StylePropertiesBits::corner_mask))
            {
                // Apply default rounding to all corners only when corner_mask was not set explicitly.
                _corner_mask = 0xFu;
                _mask |= detail::StylePropertiesBits::corner_mask;
            }
            return *this;
        }

        [[nodiscard]] f32 border_thickness() const { return _border_thickness; }
        Style &border_thickness(f32 value)
        {
            _border_thickness = value;
            _mask |= detail::StylePropertiesBits::border_thickness;
            return *this;
        }
        [[nodiscard]] bool has_visible_border() const { return _border_thickness > 0.0f && _border_color != 0u; }

        [[nodiscard]] u32 corner_mask() const { return _corner_mask; }
        Style &corner_mask(u32 value)
        {
            _corner_mask = value;
            _mask |= detail::StylePropertiesBits::corner_mask;
            return *this;
        }

        [[nodiscard]] detail::StylePropertyFlags mask() const { return _mask; }

    private:
        amal::vec4 _padding{0.0f};
        amal::vec4 _margin{0.0f};
        u32 _background_color{0};
        u32 _text_color{0};
        u32 _border_color{0};
        f32 _border_radius{0.0f};
        f32 _border_thickness{0.0f};
        f32 _text_size{12.5f};
        f32 _inline_spacing{0.0f};
        Font *_font{nullptr};
        u32 _corner_mask{0};
        detail::StylePropertyFlags _mask{0};
    };

    inline Style make_style() { return {}; }

    class Theme final
    {
    public:
        static constexpr StyleID STYLE_ID_INVALID = static_cast<StyleID>(-1);

        Theme() = default;

        StyleID add_style(u32 key, const Style &style, StyleState state = StyleState::normal)
        {
            return add_desc(key, style, state);
        }

        StyleID get(u32 key, StyleState state = StyleState::normal) const
        {
            auto it = _style_options.find(make_theme_key(key, state));
            return it == _style_options.end() ? STYLE_ID_INVALID : it->second;
        }

        const Style *get_desc_style(u32 key, StyleState state = StyleState::normal) const
        {
            const StyleID id = get(key, state);
            if (id == STYLE_ID_INVALID) return nullptr;
            assert(id < _style_options_pool.size());
            return &_style_options_pool[id];
        }

        const Style &get_style(StyleID id) const
        {
            assert(id != STYLE_ID_INVALID);
            assert(id < _resolved_pool.size());
            return _resolved_pool[id];
        }

        APPLIB_API StyleID get_resolved_style(u32 type, u32 id, u32 parent, StyleState state = StyleState::normal);
        inline bool has_state_style(u32 type, u32 id, u32 parent, StyleState state) const
        {
            return has_style_desc(id, state) || has_style_desc(type, state) || has_style_desc(parent, state) ||
                   has_style_desc(AUIK_STYLE_TAG_GLOBAL, state);
        }

        template <typename T>
        void set_var(u32 key, const T &value)
        {
            _var_store[key] = value;
            clear_resolved_cache();
        }

        template <typename T>
        T get_var(u32 key) const
        {
            const auto it = _var_store.find(key);
            if (it == _var_store.end()) return T{};
            return it->second.get<T>();
        }

    private:
        acul::hashmap<u64, StyleID> _style_options;
        acul::hashmap<u64, StyleID> _resolved;
        acul::vector<Style> _style_options_pool;
        acul::vector<Style> _resolved_pool;
        acul::hashmap<u32, acul::any> _var_store;

        APPLIB_API StyleID add_desc(u32 key, const Style &style, StyleState state);

        static u64 make_theme_key(u32 key, StyleState state)
        {
            size_t seed = 0;
            acul::hash_combine(seed, key);
            acul::hash_combine(seed, static_cast<u8>(state));
            return static_cast<u64>(seed);
        }

        void clear_resolved_cache()
        {
            _resolved.clear();
            _resolved_pool.clear();
        }

        inline bool has_style_desc(u32 key, StyleState state) const
        {
            const Style *desc = get_desc_style(key, state);
            return desc && static_cast<u16>(desc->mask()) != 0;
        }
    };

    struct StyleSelector
    {
        StyleID id = Theme::STYLE_ID_INVALID;
        u32 tag_id = 0;
    };
} // namespace auik::v2
