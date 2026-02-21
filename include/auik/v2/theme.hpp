#pragma once

#include <acul/any.hpp>
#include <acul/enum.hpp>
#include <acul/hash/hashmap.hpp>
#include <acul/hash/utils.hpp>
#include <acul/scalars.hpp>
#include <acul/vector.hpp>
#include <amal/vector.hpp>

#define AUIK_STYLE_ID_GLOBAL 0x00000000u

namespace auik::v2
{
    namespace detail
    {
        struct StylePropertiesBits
        {
            enum enum_type : u8
            {
                none = 0x0,
                padding = 0x1,
                margin = 0x2,
                background_color = 0x4,
                text_color = 0x8,
                border_color = 0x10,
                border_radius = 0x20,
                border_thickness = 0x40,
                corner_mask = 0x80
            };

            using flag_bitmask = std::true_type;
        };

        using StylePropertyFlags = acul::flags<StylePropertiesBits>;

        static constexpr StylePropertyFlags g_inheritable_mask = StylePropertiesBits::text_color;
        static constexpr StylePropertyFlags g_all_mask = acul::flag_traits<StylePropertiesBits>::all_flags;
        static constexpr StylePropertyFlags g_non_inheritable_mask = g_all_mask & ~g_inheritable_mask;

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

        [[nodiscard]] const amal::vec4 &background_color() const { return _background_color; }
        Style &background_color(const amal::vec4 &value)
        {
            _background_color = value;
            _mask |= detail::StylePropertiesBits::background_color;
            return *this;
        }

        [[nodiscard]] const amal::vec4 &text_color() const { return _text_color; }
        Style &text_color(const amal::vec4 &value)
        {
            _text_color = value;
            _mask |= detail::StylePropertiesBits::text_color;
            return *this;
        }

        [[nodiscard]] const amal::vec4 &border_color() const { return _border_color; }
        Style &border_color(const amal::vec4 &value)
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
            return *this;
        }

        [[nodiscard]] f32 border_thickness() const { return _border_thickness; }
        Style &border_thickness(f32 value)
        {
            _border_thickness = value;
            _mask |= detail::StylePropertiesBits::border_thickness;
            return *this;
        }

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
        amal::vec4 _background_color{0.0f};
        amal::vec4 _text_color{1.0f};
        amal::vec4 _border_color{0.0f};
        f32 _border_radius{0.0f};
        f32 _border_thickness{0.0f};
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

        const Style &get_style(StyleID id) const
        {
            assert(id != STYLE_ID_INVALID);
            assert(id < _resolved_pool.size());
            return _resolved_pool[id];
        }

        APPLIB_API StyleID get_resolved_style(u32 type, u32 id, u32 parent, StyleState state = StyleState::normal);

        template <typename T>
        void set_var(u32 key, const T &value)
        {
            _var_store[key] = value;
        }

        template <typename T>
        T &get_var(u32 key)
        {
            return _var_store[key].get<T>();
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
    };

    APPLIB_API Theme *create_default_theme();

    inline constexpr amal::vec4 color_rgba8(u8 r, u8 g, u8 b, u8 a)
    {
        return amal::vec4{static_cast<f32>(r) / 255.0f, static_cast<f32>(g) / 255.0f, static_cast<f32>(b) / 255.0f,
                          static_cast<f32>(a) / 255.0f};
    }
} // namespace auik::v2
