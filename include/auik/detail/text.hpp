#pragma once

#include <amal/rect.hpp>
#include <auik/auik.hpp>
#include <auik/pipelines.hpp>

namespace auik
{
    enum class TextOverflowMode : u8
    {
        clip,
        ellipsis
    };

    enum class TextWrapMode : u8
    {
        none,
        word
    };

    enum class TextLayoutWidthMode : u8
    {
        // Measure the full natural text width. This is for dynamic labels that should grow their required width and let
        // a parent container/window decide whether scrolling is needed.
        natural,
        // Measure the full natural text width, but treat the widget bounds as a viewport. The owner can move the text
        // with an external x offset and clip it to the viewport; single-line Textbox uses this for horizontal caret scroll.
        viewport,
        // Constrain layout to max_width/widget bounds. This is for fixed text, wrapping, and ellipsis/truncation.
        bounds
    };

    struct TextLayoutFlagBits
    {
        enum enum_type
        {
            none = 0x0,
            ellipsis = 0x1,
            wrap_word = 0x2,
            width_viewport = 0x4,
            width_bounds = 0x8,
            trim_trailing_spaces = 0x10
        };

        using flag_bitmask = std::true_type;
    };

    using TextLayoutFlags = acul::flags<TextLayoutFlagBits>;

    constexpr inline TextLayoutFlags default_text_layout_flags()
    { return TextLayoutFlagBits::ellipsis | TextLayoutFlagBits::width_bounds | TextLayoutFlagBits::trim_trailing_spaces; }

    inline TextLayoutFlags make_text_layout_flags(TextOverflowMode overflow = TextOverflowMode::ellipsis,
                                                  TextWrapMode wrap = TextWrapMode::none,
                                                  TextLayoutWidthMode width_mode = TextLayoutWidthMode::bounds,
                                                  bool trim_trailing_spaces = true)
    {
        TextLayoutFlags flags = TextLayoutFlagBits::none;
        if (overflow == TextOverflowMode::ellipsis) flags |= TextLayoutFlagBits::ellipsis;
        if (wrap == TextWrapMode::word) flags |= TextLayoutFlagBits::wrap_word;
        if (width_mode == TextLayoutWidthMode::viewport) flags |= TextLayoutFlagBits::width_viewport;
        else if (width_mode == TextLayoutWidthMode::bounds) flags |= TextLayoutFlagBits::width_bounds;
        if (trim_trailing_spaces) flags |= TextLayoutFlagBits::trim_trailing_spaces;
        return flags;
    }

    constexpr inline TextOverflowMode text_overflow_mode(TextLayoutFlags flags)
    { return flags & TextLayoutFlagBits::ellipsis ? TextOverflowMode::ellipsis : TextOverflowMode::clip; }

    constexpr inline TextWrapMode text_wrap_mode(TextLayoutFlags flags)
    { return flags & TextLayoutFlagBits::wrap_word ? TextWrapMode::word : TextWrapMode::none; }

    constexpr inline TextLayoutWidthMode text_width_mode(TextLayoutFlags flags)
    {
        if (flags & TextLayoutFlagBits::width_bounds) return TextLayoutWidthMode::bounds;
        if (flags & TextLayoutFlagBits::width_viewport) return TextLayoutWidthMode::viewport;
        return TextLayoutWidthMode::natural;
    }
} // namespace auik

namespace auik::detail
{
    bool is_utf8_trail(unsigned char ch);
    int next_utf8_index(const acul::string &text, int idx);
    int prev_utf8_index(const acul::string &text, int idx);
    acul::string encode_utf8_codepoint(u32 char_code, TextFlags flags);
    u32 decode_utf8_codepoint(const acul::string &text, size_t &idx);
    acul::string filter_text_input(const acul::string &input, TextFlags flags, bool allow_newline);

    struct TextLayoutConfig
    {
        f32 max_width = 0.0f;
        u32 max_lines = 0;
        TextLayoutFlags flags = default_text_layout_flags();
    };

    struct TextRenderConfig
    {
        amal::vec2 origin{0.0f, 0.0f};
        amal::vec2 size{0.0f, 0.0f};
        f32 z_order = 0.0f;
        u16 clip_id = 0xFFFFu;
    };

    struct ShapedGlyph
    {
        const Glyph *glyph = nullptr;
        u32 glyph_index = 0;
        u32 cluster = 0;
        amal::vec2 pen{0.0f, 0.0f};
        amal::rect rect{};
        amal::vec2 advance{0.0f, 0.0f};
        amal::vec2 offset{0.0f, 0.0f};

        bool visible() const { return glyph && glyph->visible() && !amal::is_rect_empty(rect); }
    };

    struct TextLine
    {
        u32 glyph_offset = 0;
        u32 glyph_count = 0;
        size_t text_start = 0;
        size_t text_end = 0;
        f32 width = 0.0f;
    };

    struct TextLayoutResult
    {
        acul::vector<ShapedGlyph> glyphs;
        acul::vector<TextLine> lines;
        amal::vec2 size{0.0f, 0.0f};
        f32 ascender = 0.0f;
        f32 descender = 0.0f;
        f32 line_height = 0.0f;
        bool truncated = false;

        void clear()
        {
            glyphs.clear();
            lines.clear();
            size = {0.0f, 0.0f};
            ascender = 0.0f;
            descender = 0.0f;
            line_height = 0.0f;
            truncated = false;
        }
    };

    struct TextFontAccess
    {
        static FT_Face face(Font &font, u32 size_px);
        static Glyph *find_glyph_by_index(Font &font, u32 size_px, u32 glyph_index);
        static const Glyph *find_glyph_by_index(const Font &font, u32 size_px, u32 glyph_index);
        static bool load_glyph_indices(Font &font, u32 size_px, const acul::vector<u32> &glyph_indices);
        static f32 ascender(Font &font, u32 size_px);
        static f32 descender(Font &font, u32 size_px);
        static f32 line_height(Font &font, u32 size_px);
    };

    bool layout_single_line(Font &font, u32 size_px, const acul::string &utf8_text, const TextLayoutConfig &config,
                            TextLayoutResult &out);
    bool layout_multiline(Font &font, u32 size_px, const acul::string &utf8_text, const TextLayoutConfig &config,
                          TextLayoutResult &out);
    bool build_text_instances_from_layout(Font &font, u32 size_px, const TextLayoutResult &layout,
                                          const TextRenderConfig &render_config, u32 tint_color,
                                          acul::vector<TexturesInstanceData> &out);
    bool build_single_line_instances(Font &font, u32 size_px, const acul::string &utf8_text,
                                     const TextLayoutConfig &layout_config, const TextRenderConfig &render_config,
                                     u32 tint_color, acul::vector<TexturesInstanceData> &out,
                                     TextLayoutResult *layout_result = nullptr);
    bool build_multiline_instances(Font &font, u32 size_px, const acul::string &utf8_text,
                                   const TextLayoutConfig &layout_config, const TextRenderConfig &render_config,
                                   u32 tint_color, acul::vector<TexturesInstanceData> &out,
                                   TextLayoutResult *layout_result = nullptr);
} // namespace auik::detail
