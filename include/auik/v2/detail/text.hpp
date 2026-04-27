#pragma once

#include <amal/rect.hpp>
#include <auik/v2/auik.hpp>
#include <auik/v2/pipelines.hpp>

namespace auik::v2::detail
{
    bool is_utf8_trail(unsigned char ch);
    int next_utf8_index(const acul::string &text, int idx);
    int prev_utf8_index(const acul::string &text, int idx);
    acul::string encode_utf8_codepoint(u32 char_code, TextFlags flags);
    u32 decode_utf8_codepoint(const acul::string &text, size_t &idx);
    acul::string filter_text_input(const acul::string &input, TextFlags flags, bool allow_newline);

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

    enum class TextHorizontalAlign : u8
    {
        left,
        center,
        right
    };

    enum class TextVerticalAlign : u8
    {
        top,
        center,
        bottom
    };

    struct TextLayoutConfig
    {
        u32 size_px = 0;
        f32 max_width = 0.0f;
        u32 max_lines = 0;
        TextOverflowMode overflow = TextOverflowMode::ellipsis;
        TextWrapMode wrap = TextWrapMode::none;
        bool trim_trailing_spaces = true;
    };

    struct TextRenderConfig
    {
        amal::rect bounds{};
        u32 tint_color = detail::pack_rgba8(255, 255, 255, 255);
        f32 z_order = 0.0f;
        u16 clip_id = 0xFFFFu;
        TextHorizontalAlign horizontal_align = TextHorizontalAlign::left;
        TextVerticalAlign vertical_align = TextVerticalAlign::top;
        bool fallback_question_mark = true;
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

    bool layout_single_line(Font &font, const acul::string &utf8_text, const TextLayoutConfig &config,
                            TextLayoutResult &out);
    bool layout_multiline(Font &font, const acul::string &utf8_text, const TextLayoutConfig &config,
                          TextLayoutResult &out);
    bool build_single_line_instances(Font &font, const acul::string &utf8_text, const TextLayoutConfig &layout_config,
                                     const TextRenderConfig &render_config, acul::vector<TexturesInstanceData> &out,
                                     TextLayoutResult *layout_result = nullptr);
    bool build_multiline_instances(Font &font, const acul::string &utf8_text, const TextLayoutConfig &layout_config,
                                   const TextRenderConfig &render_config, acul::vector<TexturesInstanceData> &out,
                                   TextLayoutResult *layout_result = nullptr);
} // namespace auik::v2::detail
