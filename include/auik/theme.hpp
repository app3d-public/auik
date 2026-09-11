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
#include <cstddef>
#include <cstring>
#include <type_traits>


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
#define AUIK_POS_UNDEFINED_VALUE    0xFFFF03p0f
#define AUIK_POS_UNDEFINED          {AUIK_POS_UNDEFINED_VALUE, AUIK_POS_UNDEFINED_VALUE}

#define AUIK_STYLE_EXTRA_ALIGN        0x2E0F75C4u
#define AUIK_STYLE_EXTRA_TEXT         0x7674E155u
#define AUIK_STYLE_EXTRA_OVERFLOW     0xD49D84A3u
#define AUIK_STYLE_EXTRA_ASPECT_RATIO 0x73A24D11u

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
            enum enum_type : u32
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
                extra = 0x8000,
                max_width = 0x10000,
                max_height = 0x20000,
                border_mask = 0x40000
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
        StyleExtra *root = nullptr;
        StyleExtra *next = nullptr;
        u32 id = 0u;
        u32 size = 0u;
    };

    enum class AspectRatioMode : u8
    {
        initial,
        preserve
    };

    enum class OverflowMode : u8
    {
        visible,
        hidden,
        auto_,
        scroll
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

    struct StyleExtraOverflow
    {
        OverflowMode x = OverflowMode::visible;
        OverflowMode y = OverflowMode::visible;
    };

    struct StyleExtraAspectRatio
    {
        AspectRatioMode mode = AspectRatioMode::initial;
    };

    class Style final
    {
    public:
        Style() = default;
        explicit Style(size_t extra_storage_size) { create_extra_storage(extra_storage_size); }
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
            _border_mask = other._border_mask;
            _text_size = other._text_size;
            _inline_spacing = other._inline_spacing;
            _size = other._size;
            _min_width = other._min_width;
            _min_height = other._min_height;
            _max_width = other._max_width;
            _max_height = other._max_height;
            _font = other._font;
            _corner_mask = other._corner_mask;
            clone_extra(other);
            _mask = other._mask;
            _disabled_mask = other._disabled_mask;
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
            _border_mask = other._border_mask;
            _text_size = other._text_size;
            _inline_spacing = other._inline_spacing;
            _size = other._size;
            _min_width = other._min_width;
            _min_height = other._min_height;
            _max_width = other._max_width;
            _max_height = other._max_height;
            _font = other._font;
            _corner_mask = other._corner_mask;
            _extra = other._extra;
            _mask = other._mask;
            _disabled_mask = other._disabled_mask;
            other._extra = nullptr;
            other._mask &= ~detail::StylePropertyFlags(detail::StylePropertiesBits::extra);
            return *this;
        }

        [[nodiscard]] const amal::vec4 &padding() const { return _padding; }
        Style &padding(const amal::vec4 &value)
        {
            _padding = value;
            enable(detail::StylePropertiesBits::padding);
            return *this;
        }
        Style &padding(const amal::vec2 &value)
        {
            _padding = {value.x, value.y, value.x, value.y};
            enable(detail::StylePropertiesBits::padding);
            return *this;
        }

        [[nodiscard]] const amal::vec4 &margin() const { return _margin; }
        Style &margin(const amal::vec4 &value)
        {
            _margin = value;
            enable(detail::StylePropertiesBits::margin);
            return *this;
        }
        Style &margin(const amal::vec2 &value)
        {
            _margin = {value.x, value.y, value.x, value.y};
            enable(detail::StylePropertiesBits::margin);
            return *this;
        }

        [[nodiscard]] u32 background_color() const { return _background_color; }
        Style &background_color(const amal::vec4 &value)
        {
            _background_color = detail::pack_rgba8(value);
            enable(detail::StylePropertiesBits::background_color);
            return *this;
        }
        Style &background_color(u32 value)
        {
            _background_color = value;
            enable(detail::StylePropertiesBits::background_color);
            return *this;
        }

        [[nodiscard]] u32 text_color() const { return _text_color; }
        Style &text_color(const amal::vec4 &value)
        {
            _text_color = detail::pack_rgba8(value);
            enable(detail::StylePropertiesBits::text_color);
            return *this;
        }
        Style &text_color(u32 value)
        {
            _text_color = value;
            enable(detail::StylePropertiesBits::text_color);
            return *this;
        }

        [[nodiscard]] f32 text_size() const { return _text_size; }
        Style &text_size(f32 value)
        {
            _text_size = value;
            enable(detail::StylePropertiesBits::text_size);
            return *this;
        }

        [[nodiscard]] Font *font() const { return _font; }
        Style &font(Font *value)
        {
            _font = value;
            enable(detail::StylePropertiesBits::font);
            return *this;
        }

        [[nodiscard]] f32 inline_spacing() const { return _inline_spacing; }
        Style &inline_spacing(f32 value)
        {
            _inline_spacing = value;
            enable(detail::StylePropertiesBits::inline_spacing);
            return *this;
        }

        [[nodiscard]] f32 width() const { return _size.x; }
        Style &width(f32 value)
        {
            _size.x = value;
            enable(detail::StylePropertiesBits::width);
            return *this;
        }

        [[nodiscard]] f32 height() const { return _size.y; }
        Style &height(f32 value)
        {
            _size.y = value;
            enable(detail::StylePropertiesBits::height);
            return *this;
        }

        [[nodiscard]] const amal::vec2 &size() const { return _size; }
        Style &size(const amal::vec2 &value)
        {
            _size = value;
            enable(detail::StylePropertiesBits::width | detail::StylePropertiesBits::height);
            return *this;
        }

        [[nodiscard]] f32 min_width() const { return _min_width; }
        Style &min_width(f32 value)
        {
            _min_width = value;
            enable(detail::StylePropertiesBits::min_width);
            return *this;
        }

        [[nodiscard]] f32 min_height() const { return _min_height; }
        Style &min_height(f32 value)
        {
            _min_height = value;
            enable(detail::StylePropertiesBits::min_height);
            return *this;
        }

        [[nodiscard]] f32 max_width() const { return _max_width; }
        Style &max_width(f32 value)
        {
            _max_width = value;
            enable(detail::StylePropertiesBits::max_width);
            return *this;
        }

        [[nodiscard]] f32 max_height() const { return _max_height; }
        Style &max_height(f32 value)
        {
            _max_height = value;
            enable(detail::StylePropertiesBits::max_height);
            return *this;
        }

        [[nodiscard]] u32 border_color() const { return _border_color; }
        Style &border_color(const amal::vec4 &value)
        {
            _border_color = detail::pack_rgba8(value);
            enable(detail::StylePropertiesBits::border_color);
            return *this;
        }
        Style &border_color(u32 value)
        {
            _border_color = value;
            enable(detail::StylePropertiesBits::border_color);
            return *this;
        }

        [[nodiscard]] f32 border_radius() const { return _border_radius; }
        Style &border_radius(f32 value)
        {
            _border_radius = value;
            enable(detail::StylePropertiesBits::border_radius);
            if (value <= 0.0f)
            {
                _corner_mask = 0u;
                enable(detail::StylePropertiesBits::corner_mask);
            }
            else if (!(_mask & detail::StylePropertiesBits::corner_mask))
            {
                // Apply default rounding to all corners only when corner_mask was not set explicitly.
                _corner_mask = 0xFu;
                enable(detail::StylePropertiesBits::corner_mask);
            }
            return *this;
        }

        [[nodiscard]] f32 border_thickness() const { return _border_thickness; }
        Style &border_thickness(f32 value)
        {
            _border_thickness = value;
            enable(detail::StylePropertiesBits::border_thickness);
            return *this;
        }
        [[nodiscard]] bool has_visible_border() const { return _border_thickness > 0.0f && _border_color != 0u; }

        [[nodiscard]] u32 border_mask() const { return _border_mask; }
        Style &border_mask(u32 value)
        {
            _border_mask = value & 0xFu;
            enable(detail::StylePropertiesBits::border_mask);
            return *this;
        }

        [[nodiscard]] u32 corner_mask() const { return _corner_mask; }
        Style &corner_mask(u32 value)
        {
            _corner_mask = value;
            enable(detail::StylePropertiesBits::corner_mask);
            return *this;
        }

        [[nodiscard]] const StyleExtra *extra() const { return _extra ? _extra->next : nullptr; }

        [[nodiscard]] static const void *extra_data(const StyleExtra *node)
        {
            return node ? reinterpret_cast<const u8 *>(node) + extra_header_size() : nullptr;
        }

        [[nodiscard]] const StyleExtra *extra(u32 id) const
        {
            for (auto *node = extra(); node; node = node->next)
                if (node->id == id) return node;
            return nullptr;
        }

        [[nodiscard]] const StyleExtraAlign *align_settings() const
        {
            const auto *node = extra(AUIK_STYLE_EXTRA_ALIGN);
            return node ? static_cast<const StyleExtraAlign *>(extra_data(node)) : nullptr;
        }

        [[nodiscard]] const StyleExtraText *text_settings() const
        {
            const auto *node = extra(AUIK_STYLE_EXTRA_TEXT);
            return node ? static_cast<const StyleExtraText *>(extra_data(node)) : nullptr;
        }

        [[nodiscard]] const StyleExtraOverflow *overflow_settings() const
        {
            const auto *node = extra(AUIK_STYLE_EXTRA_OVERFLOW);
            return node ? static_cast<const StyleExtraOverflow *>(extra_data(node)) : nullptr;
        }

        [[nodiscard]] const StyleExtraAspectRatio *aspect_ratio_settings() const
        {
            const auto *node = extra(AUIK_STYLE_EXTRA_ASPECT_RATIO);
            return node ? static_cast<const StyleExtraAspectRatio *>(extra_data(node)) : nullptr;
        }

        Style &align_extra(const StyleExtraAlign &value)
        {
            if (set_extra(AUIK_STYLE_EXTRA_ALIGN, value)) enable(detail::StylePropertiesBits::extra);
            return *this;
        }

        Style &text_extra(const StyleExtraText &value)
        {
            if (set_extra(AUIK_STYLE_EXTRA_TEXT, value)) enable(detail::StylePropertiesBits::extra);
            return *this;
        }

        Style &overflow_extra(const StyleExtraOverflow &value)
        {
            if (set_extra(AUIK_STYLE_EXTRA_OVERFLOW, value)) enable(detail::StylePropertiesBits::extra);
            return *this;
        }

        Style &aspect_ratio_extra(const StyleExtraAspectRatio &value)
        {
            if (set_extra(AUIK_STYLE_EXTRA_ASPECT_RATIO, value)) enable(detail::StylePropertiesBits::extra);
            return *this;
        }

        Style &copy_extra(const StyleExtra &extra, const void *data)
        {
            if (this->extra(extra.id)) return *this;
            if (write_extra(extra.id, data, extra.size)) enable(detail::StylePropertiesBits::extra);
            return *this;
        }

        Style &disable(detail::StylePropertyFlags properties)
        {
            _mask &= ~properties;
            _disabled_mask |= properties;
            return *this;
        }

        void destroy_extra()
        {
            if (_extra) acul::release(_extra);
            _extra = nullptr;
            _mask &= ~detail::StylePropertyFlags(detail::StylePropertiesBits::extra);
            _disabled_mask &= ~detail::StylePropertyFlags(detail::StylePropertiesBits::extra);
        }

        [[nodiscard]] detail::StylePropertyFlags mask() const { return _mask; }
        [[nodiscard]] detail::StylePropertyFlags disabled_mask() const { return _disabled_mask; }

        static constexpr size_t extra_storage_size(size_t data_size)
        {
            return extra_header_size() + align_extra_size(data_size);
        }

    private:
        void enable(detail::StylePropertyFlags properties)
        {
            _disabled_mask &= ~properties;
            _mask |= properties;
        }

        static constexpr size_t align_extra_size(size_t value)
        {
            constexpr size_t alignment = alignof(std::max_align_t);
            return (value + alignment - 1u) & ~(alignment - 1u);
        }

        static constexpr size_t extra_header_size() { return align_extra_size(sizeof(StyleExtra)); }
        void create_extra_storage(size_t storage_size)
        {
            if (storage_size == 0u) return;
            assert(storage_size <= UINT32_MAX && "style extra storage is too large");
            const size_t allocation_size = extra_header_size() + storage_size;
            const size_t node_count = (allocation_size + sizeof(StyleExtra) - 1u) / sizeof(StyleExtra);
            _extra = acul::mem_allocator<StyleExtra>::allocate(node_count);
            assert(_extra && "failed to allocate style extra storage");
            if (_extra) *_extra = StyleExtra{_extra, nullptr, 0u, static_cast<u32>(storage_size)};
        }

        StyleExtra *mutable_extra(u32 id)
        {
            return const_cast<StyleExtra *>(static_cast<const Style *>(this)->extra(id));
        }

        bool write_extra(u32 id, const void *data, size_t data_size)
        {
            assert(_extra && "style extra storage was not declared");
            if (!_extra) return false;
            const size_t record_size = extra_storage_size(data_size);
            StyleExtra *tail = _extra;
            while (tail->next) tail = tail->next;
            auto *storage = reinterpret_cast<u8 *>(_extra) + extra_header_size();
            auto *node_ptr = tail == _extra ? storage : reinterpret_cast<u8 *>(tail) + extra_storage_size(tail->size);
            const size_t used = static_cast<size_t>(node_ptr - storage);
            assert(used + record_size <= _extra->size && "style extra storage was not declared");
            if (used + record_size > _extra->size) return false;
            auto *node = reinterpret_cast<StyleExtra *>(node_ptr);
            *node = StyleExtra{_extra, nullptr, id, static_cast<u32>(data_size)};
            tail->next = node;
            auto *dst = node_ptr + extra_header_size();
            std::memset(dst, 0, align_extra_size(data_size));
            if (data && data_size) std::memcpy(dst, data, data_size);
            return true;
        }

        void clone_extra(const Style &other)
        {
            if (!other._extra) return;
            create_extra_storage(other._extra->size);
            for (const StyleExtra *node = other.extra(); node; node = node->next)
                write_extra(node->id, extra_data(node), node->size);
        }

        template <typename T>
        bool set_extra(u32 id, const T &value)
        {
            static_assert(std::is_trivially_copyable_v<T>);
            if (auto *node = mutable_extra(id))
            {
                assert(node->size == sizeof(T) && "style extra type size mismatch");
                if (node->size != sizeof(T)) return false;
                std::memcpy(reinterpret_cast<u8 *>(node) + extra_header_size(), &value, sizeof(T));
                return true;
            }
            return write_extra(id, &value, sizeof(T));
        }

        amal::vec4 _padding{0.0f};
        amal::vec4 _margin{0.0f};
        u32 _background_color{0};
        u32 _text_color{0};
        u32 _border_color{0};
        f32 _border_radius{0.0f};
        f32 _border_thickness{0.0f};
        u32 _border_mask{0xFu};
        f32 _text_size{12.5f};
        f32 _inline_spacing{0.0f};
        amal::vec2 _size{AUIK_SIZE_X_FILL, AUIK_SIZE_Y_FIT};
        f32 _min_width{0.0f};
        f32 _min_height{0.0f};
        f32 _max_width{0.0f};
        f32 _max_height{0.0f};
        Font *_font{nullptr};
        u32 _corner_mask{0};
        StyleExtra *_extra = nullptr;
        detail::StylePropertyFlags _mask{0};
        detail::StylePropertyFlags _disabled_mask{0};
    };

    inline Style make_style(size_t extra_storage_size = 0u) { return Style(extra_storage_size); }

    class Theme final
    {
    public:
        static constexpr StyleID STYLE_ID_INVALID = static_cast<StyleID>(-1);

        Theme() = default;
        ~Theme() { destroy(); }

        StyleID add_style(u32 key, const Style &style, StyleState state = StyleState::normal)
        {
            return add_desc(key, Style(style), state);
        }

        // Transfers ownership of extra data to the theme without cloning it.
        StyleID add_style(u32 key, Style &&style, StyleState state = StyleState::normal)
        {
            return add_desc(key, std::move(style), state);
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
        AUIK_EXPORT StyleID add_desc(u32 key, Style &&style, StyleState state);
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
            return desc && static_cast<u32>(desc->mask()) != 0;
        }
    };

    struct StyleSelector
    {
        StyleID id = Theme::STYLE_ID_INVALID;
        u32 tag_id = 0;
    };
} // namespace auik
