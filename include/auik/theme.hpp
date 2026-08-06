#pragma once

#include <acul/any.hpp>
#include <acul/enum.hpp>
#include <acul/hash/hashmap.hpp>
#include <acul/hash/utils.hpp>
#include <acul/scalars.hpp>
#include <acul/vector.hpp>
#include <amal/color.hpp>
#include <amal/vector.hpp>
#include <auik/symbol_export.h>
#include <auik/widget_tags.hpp>

#define AUIK_THEME_FONT_REGULAR 0
#define AUIK_THEME_FONT_BOLD    1

#define AUIK_SIZE_X_MIN_FIT         0x0
#define AUIK_SIZE_Y_MIN_FIT         AUIK_SIZE_X_MIN_FIT
#define AUIK_SIZE_MIN_FIT           {AUIK_SIZE_X_MIN_FIT, AUIK_SIZE_Y_MIN_FIT}
#define AUIK_SIZE_X_MIN_FIT_REQUIRE 0xFFFF01p0f
#define AUIK_SIZE_Y_MIN_FIT_REQUIRE AUIK_SIZE_X_MIN_FIT_REQUIRE
#define AUIK_SIZE_MIN_FIT_REQUIRE   {AUIK_SIZE_X_MIN_FIT_REQUIRE, AUIK_SIZE_Y_MIN_FIT_REQUIRE}
#define AUIK_SIZE_X_FILL            0xFFFF00p0f
#define AUIK_SIZE_Y_FILL            AUIK_SIZE_X_FILL
#define AUIK_SIZE_FILL              {AUIK_SIZE_X_FILL, AUIK_SIZE_Y_FILL}
#define AUIK_SIZE_X_INHERIT         0xFFFF02p0f
#define AUIK_SIZE_Y_INHERIT         AUIK_SIZE_X_INHERIT
#define AUIK_SIZE_INHERIT           {AUIK_SIZE_X_INHERIT, AUIK_SIZE_Y_INHERIT}
#define AUIK_SIZE_X_FIT             AUIK_SIZE_X_MIN_FIT_REQUIRE
#define AUIK_SIZE_Y_FIT             AUIK_SIZE_Y_MIN_FIT_REQUIRE
#define AUIK_SIZE_FIT               AUIK_SIZE_MIN_FIT_REQUIRE
#define AUIK_SIZE_AUTO              AUIK_SIZE_FIT
#define AUIK_POS_IGNORE             {AUIK_SIZE_X_FIT, AUIK_SIZE_Y_FIT}

#define AUIK_STYLE_EXTRA_ALIGN 0x2E0F75C4u
#define AUIK_STYLE_EXTRA_TEXT  0x7674E155u

namespace auik
{
    class Font;
    enum class TextOverflowMode : u8;
    enum class TextWrapMode : u8;

    enum class HAlign : u8
    {
        left,
        center,
        right,
        none = 0xFFu
    };

    enum class VAlign : u8
    {
        none,
        top,
        center,
        bottom
    };

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
                inline_spacing = 0x400,
                width = 0x800,
                height = 0x1000,
                min_width = 0x2000,
                min_height = 0x4000,
                extra = 0x8000
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

    struct StyleExtra
    {
        u32 id = 0u;
        void *data = nullptr;
        StyleExtra *next = nullptr;
    };

    struct StyleExtraAlign
    {
        u32 flags = 0u;
    };

    struct StyleExtraText
    {
        TextWrapMode wrap = static_cast<TextWrapMode>(0u);
        TextOverflowMode overflow = static_cast<TextOverflowMode>(1u);
    };

    class Style final
    {
    public:
        Style() = default;
        Style(const Style &other) { *this = other; }
        Style(Style &&other) noexcept { *this = std::move(other); }

        Style &operator=(const Style &other)
        {
            if (this == &other) return *this;
            destroy_extra();
            _padding = other._padding;
            _margin = other._margin;
            _background_color = other._background_color;
            _text_color = other._text_color;
            _border_color = other._border_color;
            _border_radius = other._border_radius;
            _border_thickness = other._border_thickness;
            _text_size = other._text_size;
            _inline_spacing = other._inline_spacing;
            _size = other._size;
            _min_width = other._min_width;
            _min_height = other._min_height;
            _font = other._font;
            _corner_mask = other._corner_mask;
            _mask = other._mask;
            clone_extra(other);
            _mask = other._mask;
            return *this;
        }

        Style &operator=(Style &&other) noexcept
        {
            if (this == &other) return *this;
            destroy_extra();
            _padding = other._padding;
            _margin = other._margin;
            _background_color = other._background_color;
            _text_color = other._text_color;
            _border_color = other._border_color;
            _border_radius = other._border_radius;
            _border_thickness = other._border_thickness;
            _text_size = other._text_size;
            _inline_spacing = other._inline_spacing;
            _size = other._size;
            _min_width = other._min_width;
            _min_height = other._min_height;
            _font = other._font;
            _corner_mask = other._corner_mask;
            _extra = other._extra;
            _mask = other._mask;
            other._extra = nullptr;
            other._mask &= ~detail::StylePropertyFlags(detail::StylePropertiesBits::extra);
            return *this;
        }

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

        [[nodiscard]] f32 width() const { return _size.x; }
        Style &width(f32 value)
        {
            _size.x = value;
            _mask |= detail::StylePropertiesBits::width;
            return *this;
        }

        [[nodiscard]] f32 height() const { return _size.y; }
        Style &height(f32 value)
        {
            _size.y = value;
            _mask |= detail::StylePropertiesBits::height;
            return *this;
        }

        [[nodiscard]] const amal::vec2 &size() const { return _size; }
        Style &size(const amal::vec2 &value)
        {
            _size = value;
            _mask |= detail::StylePropertiesBits::width | detail::StylePropertiesBits::height;
            return *this;
        }

        [[nodiscard]] f32 min_width() const { return _min_width; }
        Style &min_width(f32 value)
        {
            _min_width = value;
            _mask |= detail::StylePropertiesBits::min_width;
            return *this;
        }

        [[nodiscard]] f32 min_height() const { return _min_height; }
        Style &min_height(f32 value)
        {
            _min_height = value;
            _mask |= detail::StylePropertiesBits::min_height;
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

        [[nodiscard]] const StyleExtra *extra() const { return _extra; }

        [[nodiscard]] const StyleExtra *extra(u32 id) const
        {
            for (auto *node = extra(); node; node = node->next)
                if (node->id == id) return node;
            return nullptr;
        }

        [[nodiscard]] const StyleExtraAlign *align_settings() const
        {
            const auto *node = extra(AUIK_STYLE_EXTRA_ALIGN);
            return node ? static_cast<const StyleExtraAlign *>(node->data) : nullptr;
        }

        [[nodiscard]] const StyleExtraText *text_settings() const
        {
            const auto *node = extra(AUIK_STYLE_EXTRA_TEXT);
            return node ? static_cast<const StyleExtraText *>(node->data) : nullptr;
        }

        Style &align_extra(const StyleExtraAlign &value)
        {
            auto *data = upsert_extra_data<StyleExtraAlign>(AUIK_STYLE_EXTRA_ALIGN);
            *data = value;
            _mask |= detail::StylePropertiesBits::extra;
            return *this;
        }

        Style &text_extra(const StyleExtraText &value)
        {
            auto *data = upsert_extra_data<StyleExtraText>(AUIK_STYLE_EXTRA_TEXT);
            *data = value;
            _mask |= detail::StylePropertiesBits::extra;
            return *this;
        }

        void destroy_extra()
        {
            StyleExtra *node = _extra;
            while (node)
            {
                StyleExtra *next = node->next;
                release_extra_data(*node);
                acul::release(node);
                node = next;
            }
            _extra = nullptr;
            _mask &= ~detail::StylePropertyFlags(detail::StylePropertiesBits::extra);
        }

        [[nodiscard]] detail::StylePropertyFlags mask() const { return _mask; }

    private:
        StyleExtra *mutable_extra(u32 id)
        {
            for (StyleExtra *it = _extra; it; it = it->next)
                if (it->id == id) return it;
            return nullptr;
        }

        void append_extra_node(StyleExtra *node)
        {
            if (!_extra)
            {
                _extra = node;
                return;
            }
            StyleExtra *tail = _extra;
            while (tail->next) tail = tail->next;
            tail->next = node;
        }

        template <typename T>
        T *upsert_extra_data(u32 id)
        {
            if (auto *node = mutable_extra(id)) return static_cast<T *>(node->data);
            auto *data = acul::alloc<T>(T{});
            auto *node = acul::alloc<StyleExtra>(StyleExtra{id, data, nullptr});
            append_extra_node(node);
            return data;
        }

        static void release_extra_data(StyleExtra &node)
        {
            if (!node.data) return;
            if (node.id == AUIK_STYLE_EXTRA_ALIGN)
                acul::release(static_cast<StyleExtraAlign *>(node.data));
            else if (node.id == AUIK_STYLE_EXTRA_TEXT)
                acul::release(static_cast<StyleExtraText *>(node.data));
        }

        void clone_extra(const Style &other)
        {
            for (const StyleExtra *node = other.extra(); node; node = node->next)
            {
                if (node->id == AUIK_STYLE_EXTRA_ALIGN && node->data)
                    align_extra(*static_cast<const StyleExtraAlign *>(node->data));
                else if (node->id == AUIK_STYLE_EXTRA_TEXT && node->data)
                    text_extra(*static_cast<const StyleExtraText *>(node->data));
            }
        }

        amal::vec4 _padding{0.0f};
        amal::vec4 _margin{0.0f};
        u32 _background_color{0};
        u32 _text_color{0};
        u32 _border_color{0};
        f32 _border_radius{0.0f};
        f32 _border_thickness{0.0f};
        f32 _text_size{12.5f};
        f32 _inline_spacing{0.0f};
        amal::vec2 _size{AUIK_SIZE_X_FILL, AUIK_SIZE_Y_FIT};
        f32 _min_width{0.0f};
        f32 _min_height{0.0f};
        Font *_font{nullptr};
        u32 _corner_mask{0};
        StyleExtra *_extra = nullptr;
        detail::StylePropertyFlags _mask{0};
    };

    inline Style make_style() { return {}; }

    class Theme final
    {
    public:
        static constexpr StyleID STYLE_ID_INVALID = static_cast<StyleID>(-1);

        Theme() = default;
        ~Theme() { destroy(); }

        StyleID add_style(u32 key, const Style &style, StyleState state = StyleState::normal)
        { return add_desc(key, style, state); }

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
            return _style_options_pool[id];
        }

        const Style &get_style(StyleID id) const
        {
            assert(id != STYLE_ID_INVALID);
            assert(id < _resolved_pool.size());
            return *_resolved_pool[id];
        }

        AUIK_EXPORT StyleID get_resolved_style(u32 type, u32 id, u32 parent, StyleState state = StyleState::normal);
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
        acul::vector<Style *> _style_options_pool;
        acul::vector<Style *> _resolved_pool;
        acul::hashmap<u32, acul::any> _var_store;

        AUIK_EXPORT StyleID add_desc(u32 key, const Style &style, StyleState state);

        void destroy()
        {
            clear_resolved_cache();
            for (Style *style : _style_options_pool)
            {
                if (!style) continue;
                style->destroy_extra();
                acul::release(style);
            }
            _style_options_pool.clear();
            _style_options.clear();
            _var_store.clear();
        }

        static u64 make_theme_key(u32 key, StyleState state)
        {
            size_t seed = 0;
            acul::hash_combine(seed, key);
            acul::hash_combine(seed, static_cast<u8>(state));
            return static_cast<u64>(seed);
        }

        void clear_resolved_cache()
        {
            for (Style *style : _resolved_pool)
            {
                if (!style) continue;
                style->destroy_extra();
                acul::release(style);
            }
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
} // namespace auik
