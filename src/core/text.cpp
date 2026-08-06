#include <auik/auik.hpp>
#include <auik/detail/text.hpp>
#include <harfbuzz/hb-ft.h>

namespace auik
{
    static acul::string encode_utf8(u32 cp)
    {
        acul::string out;
        if (cp <= 0x7Fu) out.push_back(static_cast<char>(cp));
        else if (cp <= 0x7FFu)
        {
            out.push_back(static_cast<char>(0xC0u | ((cp >> 6) & 0x1Fu)));
            out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
        }
        else if (cp <= 0xFFFFu)
        {
            out.push_back(static_cast<char>(0xE0u | ((cp >> 12) & 0x0Fu)));
            out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
            out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
        }
        else if (cp <= 0x10FFFFu)
        {
            out.push_back(static_cast<char>(0xF0u | ((cp >> 18) & 0x07u)));
            out.push_back(static_cast<char>(0x80u | ((cp >> 12) & 0x3Fu)));
            out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
            out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
        }
        return out;
    }

    bool add_char_to_string(acul::string &dst, u32 char_code, TextFlags flags)
    {
        u32 c = char_code;
        if (c == 0) return false;
        if (!(flags & TextFlagBits::allow_tab_input) && c == '\t') return false;
        if ((flags & TextFlagBits::chars_no_blank) && (c == ' ' || c == '\t')) return false;
        if ((flags & TextFlagBits::chars_ascii) && c >= 128) return false;

        const bool is_decimal = flags & TextFlagBits::chars_decimal;
        const bool is_scientific = flags & TextFlagBits::chars_scientific;
        const bool is_hex = flags & TextFlagBits::chars_hexadecimal;

        if (is_decimal || is_scientific)
        {
            const bool is_digit = c >= '0' && c <= '9';
            const bool is_common_op = c == '+' || c == '-' || c == '*' || c == '/' || c == '.';
            const bool is_science_op = (c == 'e' || c == 'E');
            if (!is_digit && !is_common_op && !(is_scientific && is_science_op)) return false;
        }
        else if (is_hex)
        {
            const bool is_digit = c >= '0' && c <= '9';
            const bool is_lower_hex = c >= 'a' && c <= 'f';
            const bool is_upper_hex = c >= 'A' && c <= 'F';
            const bool is_hex_prefix = c == 'x' || c == 'X';
            if (!is_digit && !is_lower_hex && !is_upper_hex && !is_hex_prefix) return false;
        }

        if ((flags & TextFlagBits::chars_uppercase) && c >= 'a' && c <= 'z') c = c - 'a' + 'A';
        dst += encode_utf8(c);
        return true;
    }

    namespace detail
    {
        bool is_utf8_trail(unsigned char ch) { return (ch & 0xC0u) == 0x80u; }

        int next_utf8_index(const acul::string &text, int idx)
        {
            const int len = static_cast<int>(text.size());
            if (idx >= len) return len;
            ++idx;
            while (idx < len && is_utf8_trail(static_cast<unsigned char>(text[idx]))) ++idx;
            return idx;
        }

        int prev_utf8_index(const acul::string &text, int idx)
        {
            if (idx <= 0) return 0;
            --idx;
            while (idx > 0 && is_utf8_trail(static_cast<unsigned char>(text[idx]))) --idx;
            return idx;
        }

        acul::string encode_utf8_codepoint(u32 char_code, TextFlags flags)
        {
            acul::string out;
            add_char_to_string(out, char_code, flags);
            return out;
        }

        u32 decode_utf8_codepoint(const acul::string &text, size_t &idx)
        {
            const auto len = text.size();
            const u8 lead = static_cast<u8>(text[idx++]);
            if (lead < 0x80u) return lead;

            u32 codepoint = 0;
            u32 trail_count = 0;
            if ((lead & 0xE0u) == 0xC0u)
            {
                codepoint = lead & 0x1Fu;
                trail_count = 1;
            }
            else if ((lead & 0xF0u) == 0xE0u)
            {
                codepoint = lead & 0x0Fu;
                trail_count = 2;
            }
            else if ((lead & 0xF8u) == 0xF0u)
            {
                codepoint = lead & 0x07u;
                trail_count = 3;
            }
            else return 0xFFFDu;

            for (u32 i = 0; i < trail_count; ++i)
            {
                if (idx >= len) return 0xFFFDu;
                const u8 ch = static_cast<u8>(text[idx]);
                if ((ch & 0xC0u) != 0x80u) return 0xFFFDu;
                ++idx;
                codepoint = (codepoint << 6u) | (ch & 0x3Fu);
            }
            return codepoint;
        }

        acul::string filter_text_input(const acul::string &input, TextFlags flags, bool allow_newline)
        {
            acul::string out;
            for (size_t i = 0; i < input.size();)
            {
                const u32 codepoint = decode_utf8_codepoint(input, i);
                if (codepoint == '\r') continue;
                if (codepoint == '\n' && !allow_newline) continue;
                add_char_to_string(out, codepoint, flags);
            }
            return out;
        }

        namespace
        {
            constexpr f32 g_hb_scale = 64.0f;
            constexpr char g_ellipsis_utf8[] = "\xE2\x80\xA6";

            struct ShapedRun
            {
                size_t text_start = 0;
                size_t text_end = 0;
                f32 width = 0.0f;
                acul::vector<hb_glyph_info_t> infos;
                acul::vector<hb_glyph_position_t> positions;
            };

            struct Token
            {
                size_t start = 0;
                size_t end = 0;
                bool whitespace = false;
            };

            static bool is_inline_space(char c) { return c == ' ' || c == '\t' || c == '\r'; }

            static size_t trim_trailing_spaces(const acul::string &text, size_t start, size_t end)
            {
                while (end > start && is_inline_space(text[end - 1])) --end;
                return end;
            }

            static size_t skip_leading_spaces(const acul::string &text, size_t start, size_t end)
            {
                while (start < end && is_inline_space(text[start])) ++start;
                return start;
            }

            static Token next_token(const acul::string &text, size_t start, size_t end)
            {
                Token token{};
                token.start = start;
                token.end = start;
                token.whitespace = false;
                if (start >= end) return token;

                token.whitespace = is_inline_space(text[start]);
                while (token.end < end && is_inline_space(text[token.end]) == token.whitespace) ++token.end;
                return token;
            }

            static bool shape_range(Font &font, u32 size_px, const acul::string &text, size_t start, size_t end,
                                    ShapedRun &out)
            {
                out = {};
                out.text_start = start;
                out.text_end = end;
                if (start >= end) return true;

                auto *face = TextFontAccess::face(font, size_px);
                if (!face) return false;

                hb_buffer_t *buffer = hb_buffer_create();
                if (!buffer) return false;

                hb_buffer_add_utf8(buffer, text.c_str(), static_cast<int>(text.size()),
                                   static_cast<unsigned int>(start), static_cast<int>(end - start));
                hb_buffer_guess_segment_properties(buffer);

                hb_font_t *hb_font = hb_ft_font_create_referenced(face);
                if (!hb_font)
                {
                    hb_buffer_destroy(buffer);
                    return false;
                }

                hb_shape(hb_font, buffer, nullptr, 0);

                unsigned int glyph_count = 0;
                auto *infos = hb_buffer_get_glyph_infos(buffer, &glyph_count);
                auto *positions = hb_buffer_get_glyph_positions(buffer, &glyph_count);

                acul::vector<u32> glyph_indices;
                for (unsigned int i = 0; i < glyph_count; ++i)
                {
                    glyph_indices.push_back(infos[i].codepoint);
                    out.infos.push_back(infos[i]);
                    out.positions.push_back(positions[i]);
                    out.width += static_cast<f32>(positions[i].x_advance) / g_hb_scale;
                }

                if (!glyph_indices.empty()) TextFontAccess::load_glyph_indices(font, size_px, glyph_indices);

                hb_font_destroy(hb_font);
                hb_buffer_destroy(buffer);
                return true;
            }

            static size_t trim_run_to_width(const ShapedRun &run, f32 max_width, f32 &out_width)
            {
                out_width = 0.0f;
                if (run.infos.empty()) return run.text_start;

                size_t first_cluster_end = run.text_end;
                size_t last_fit_end = run.text_start;
                f32 width = 0.0f;

                for (size_t i = 0; i < run.infos.size();)
                {
                    const u32 cluster = run.infos[i].cluster;
                    size_t j = i;
                    while (j < run.infos.size() && run.infos[j].cluster == cluster)
                    {
                        width += static_cast<f32>(run.positions[j].x_advance) / g_hb_scale;
                        ++j;
                    }

                    const size_t cluster_end = (j < run.infos.size()) ? run.infos[j].cluster : run.text_end;
                    if (i == 0) first_cluster_end = cluster_end;
                    if (width <= max_width)
                    {
                        last_fit_end = cluster_end;
                        out_width = width;
                    }
                    else break;

                    i = j;
                }

                if (last_fit_end == run.text_start)
                {
                    out_width = width;
                    return first_cluster_end;
                }
                return last_fit_end;
            }

            static f32 append_run_to_line(TextLayoutResult &out, Font &font, u32 size_px, const ShapedRun &run,
                                          f32 line_y, f32 pen_x, TextLine &line)
            {
                for (size_t i = 0; i < run.infos.size(); ++i)
                {
                    const u32 glyph_index = run.infos[i].codepoint;
                    const auto *glyph = TextFontAccess::find_glyph_by_index(font, size_px, glyph_index);
                    const f32 x_offset = static_cast<f32>(run.positions[i].x_offset) / g_hb_scale;
                    const f32 y_offset = static_cast<f32>(run.positions[i].y_offset) / g_hb_scale;
                    const f32 x_advance = static_cast<f32>(run.positions[i].x_advance) / g_hb_scale;
                    const f32 y_advance = static_cast<f32>(run.positions[i].y_advance) / g_hb_scale;

                    ShapedGlyph shaped{};
                    shaped.glyph = glyph;
                    shaped.glyph_index = glyph_index;
                    shaped.cluster = run.infos[i].cluster;
                    shaped.pen = {pen_x + x_offset, line_y + out.ascender - y_offset};
                    shaped.advance = {x_advance, y_advance};
                    shaped.offset = {x_offset, y_offset};

                    if (glyph)
                    {
                        shaped.rect.offset = {shaped.pen.x + static_cast<f32>(glyph->offset.x),
                                              shaped.pen.y - static_cast<f32>(glyph->offset.y)};
                        shaped.rect.size = {static_cast<f32>(glyph->size.x), static_cast<f32>(glyph->size.y)};
                    }

                    out.glyphs.push_back(shaped);
                    pen_x += x_advance;
                }

                line.width = pen_x;
                return pen_x;
            }

            static void finalize_line(TextLayoutResult &out, TextLine &line, f32 line_y)
            {
                out.lines.push_back(line);
                out.size.x = amal::max(out.size.x, line.width);
                out.size.y = amal::max(out.size.y, line_y + out.line_height);
            }

            static bool append_instances_from_layout(Font &font, u32 size_px, const TextLayoutResult &layout,
                                                     const TextRenderConfig &render_config, u32 tint_color,
                                                     acul::vector<TexturesInstanceData> &out)
            {
                out.clear();

                const f32 base_x = render_config.origin.x;
                const f32 base_y = render_config.origin.y;

                for (const auto &line : layout.lines)
                {
                    for (u32 i = 0; i < line.glyph_count; ++i)
                    {
                        const auto &shaped = layout.glyphs[line.glyph_offset + i];
                        const Glyph *glyph = shaped.glyph;
                        amal::vec2 glyph_offset = shaped.rect.offset;

                        if (glyph && glyph->empty) continue;

                        if (!glyph)
                        {
                            if (!font.find_glyph(size_px, '?')) font.load_glyph(size_px, '?');
                            glyph = font.find_glyph(size_px, '?');
                            if (glyph)
                            {
                                glyph_offset = {shaped.pen.x + static_cast<f32>(glyph->offset.x),
                                                shaped.pen.y - static_cast<f32>(glyph->offset.y)};
                            }
                        }

                        if (!glyph || !glyph->visible()) continue;

                        TexturesInstanceData instance{};
                        instance.rect.offset = {amal::round(base_x + glyph_offset.x),
                                                amal::round(base_y + glyph_offset.y)};
                        instance.rect.size = {static_cast<f32>(glyph->size.x), static_cast<f32>(glyph->size.y)};
                        instance.tint_color = tint_color;
                        instance.uv_rect = glyph->uv_rect;
                        instance.z_order = render_config.z_order;
                        instance.texture_id = static_cast<u16>(glyph->texture_id.bind_slot);
                        instance.clip_id = render_config.clip_id;
                        instance.flags = AUIK_TEXTURE_INSTANCE_TINT_BIT;
                        out.push_back(instance);
                    }
                }

                return true;
            }

            static bool append_shaped_line(TextLayoutResult &out, Font &font, u32 size_px, const acul::string &text,
                                           size_t start, size_t end, f32 line_y)
            {
                ShapedRun run;
                if (!shape_range(font, size_px, text, start, end, run)) return false;

                TextLine line{};
                line.glyph_offset = static_cast<u32>(out.glyphs.size());
                line.text_start = start;
                line.text_end = end;
                append_run_to_line(out, font, size_px, run, line_y, 0.0f, line);
                line.glyph_count = static_cast<u32>(out.glyphs.size()) - line.glyph_offset;
                finalize_line(out, line, line_y);
                return true;
            }

            static bool append_ellipsized_line(TextLayoutResult &out, Font &font, u32 size_px, const acul::string &text,
                                               size_t start, size_t end, f32 line_y, f32 max_width)
            {
                const acul::string ellipsis(g_ellipsis_utf8);
                ShapedRun ellipsis_run;
                if (!shape_range(font, size_px, ellipsis, 0, ellipsis.size(), ellipsis_run)) return false;

                TextLine line{};
                line.glyph_offset = static_cast<u32>(out.glyphs.size());
                line.text_start = start;
                line.text_end = end;

                if (max_width <= 0.0f || ellipsis_run.width >= max_width)
                {
                    append_run_to_line(out, font, size_px, ellipsis_run, line_y, 0.0f, line);
                    line.glyph_count = static_cast<u32>(out.glyphs.size()) - line.glyph_offset;
                    finalize_line(out, line, line_y);
                    return true;
                }

                const f32 available_width = max_width - ellipsis_run.width;
                ShapedRun full_run;
                if (!shape_range(font, size_px, text, start, end, full_run)) return false;

                f32 visible_width = 0.0f;
                const size_t trim_end = trim_run_to_width(full_run, available_width, visible_width);
                if (trim_end > start)
                {
                    ShapedRun visible_run;
                    if (!shape_range(font, size_px, text, start, trim_end, visible_run)) return false;
                    append_run_to_line(out, font, size_px, visible_run, line_y, 0.0f, line);
                }

                append_run_to_line(out, font, size_px, ellipsis_run, line_y, line.width, line);
                line.glyph_count = static_cast<u32>(out.glyphs.size()) - line.glyph_offset;
                finalize_line(out, line, line_y);
                return true;
            }

            static bool layout_wrapped_span(TextLayoutResult &out, Font &font, u32 size_px, const acul::string &text,
                                            size_t start, size_t end, const TextLayoutConfig &config, f32 &line_y,
                                            bool &truncated)
            {
                size_t cursor = start;
                while (cursor < end)
                {
                    if (config.flags & TextLayoutFlagBits::trim_trailing_spaces)
                    {
                        cursor = skip_leading_spaces(text, cursor, end);
                        if (cursor >= end) break;
                    }

                    if (config.max_lines != 0 && out.lines.size() + 1 >= config.max_lines)
                    {
                        if (!append_ellipsized_line(out, font, size_px, text, cursor, end, line_y, config.max_width))
                            return false;
                        truncated = true;
                        return true;
                    }

                    if (config.max_width <= 0.0f)
                    {
                        if (!append_shaped_line(out, font, size_px, text, cursor, end, line_y)) return false;
                        line_y += out.line_height;
                        return true;
                    }

                    size_t probe = cursor;
                    size_t last_break = cursor;
                    while (probe < end)
                    {
                        Token token = next_token(text, probe, end);
                        if (token.start == token.end) break;

                        const size_t candidate_end = token.end;
                        ShapedRun candidate_run;
                        if (!shape_range(font, size_px, text, cursor, candidate_end, candidate_run)) return false;
                        if (candidate_run.width <= config.max_width)
                        {
                            probe = candidate_end;
                            if (token.whitespace) last_break = candidate_end;
                            continue;
                        }

                        size_t line_end = last_break > cursor ? last_break : cursor;
                        if (line_end > cursor)
                        {
                            const size_t untrimmed_end = line_end;
                            if (config.flags & TextLayoutFlagBits::trim_trailing_spaces)
                                line_end = trim_trailing_spaces(text, cursor, line_end);
                            if (line_end == cursor) line_end = untrimmed_end;
                            if (!append_shaped_line(out, font, size_px, text, cursor, line_end, line_y)) return false;
                            line_y += out.line_height;
                            cursor = skip_leading_spaces(text, last_break, end);
                            goto next_line;
                        }

                        {
                            f32 cluster_width = 0.0f;
                            const size_t cluster_end =
                                trim_run_to_width(candidate_run, config.max_width, cluster_width);
                            if (!append_shaped_line(out, font, size_px, text, cursor, cluster_end, line_y))
                                return false;
                            line_y += out.line_height;
                            cursor = cluster_end;
                            goto next_line;
                        }
                    }

                    if (probe > cursor)
                    {
                        size_t line_end = probe;
                        if (config.flags & TextLayoutFlagBits::trim_trailing_spaces)
                            line_end = trim_trailing_spaces(text, cursor, probe);
                        if (line_end == cursor) line_end = probe;
                        if (!append_shaped_line(out, font, size_px, text, cursor, line_end, line_y)) return false;
                        line_y += out.line_height;
                        cursor = probe;
                    }

                next_line:
                    continue;
                }

                return true;
            }

            static void append_empty_line(TextLayoutResult &out, size_t cursor, f32 &line_y)
            {
                TextLine empty_line{};
                empty_line.text_start = cursor;
                empty_line.text_end = cursor;
                empty_line.glyph_offset = static_cast<u32>(out.glyphs.size());
                finalize_line(out, empty_line, line_y);
                line_y += out.line_height;
            }
        } // namespace

        bool build_text_instances_from_layout(Font &font, u32 size_px, const TextLayoutResult &layout,
                                              const TextRenderConfig &render_config, u32 tint_color,
                                              acul::vector<TexturesInstanceData> &out)
        {
            return append_instances_from_layout(font, size_px, layout, render_config, tint_color, out);
        }

        bool layout_single_line(Font &font, u32 size_px, const acul::string &utf8_text, const TextLayoutConfig &config,
                                TextLayoutResult &out)
        {
            out.clear();
            if (size_px == 0) return false;
            out.ascender = TextFontAccess::ascender(font, size_px);
            out.descender = TextFontAccess::descender(font, size_px);
            out.line_height = TextFontAccess::line_height(font, size_px);

            if (utf8_text.empty()) return true;

            ShapedRun run;
            if (!shape_range(font, size_px, utf8_text, 0, utf8_text.size(), run)) return false;

            if (config.max_width <= 0.0f || run.width <= config.max_width ||
                text_overflow_mode(config.flags) == TextOverflowMode::clip)
            {
                if (config.max_width > 0.0f && run.width > config.max_width &&
                    text_overflow_mode(config.flags) == TextOverflowMode::clip)
                {
                    f32 trimmed_width = 0.0f;
                    const size_t trim_end = trim_run_to_width(run, config.max_width, trimmed_width);
                    return append_shaped_line(out, font, size_px, utf8_text, 0, trim_end, 0.0f);
                }
                return append_shaped_line(out, font, size_px, utf8_text, 0, utf8_text.size(), 0.0f);
            }

            out.truncated = true;
            return append_ellipsized_line(out, font, size_px, utf8_text, 0, utf8_text.size(), 0.0f, config.max_width);
        }

        bool layout_multiline(Font &font, u32 size_px, const acul::string &utf8_text, const TextLayoutConfig &config,
                              TextLayoutResult &out)
        {
            out.clear();
            if (size_px == 0) return false;
            out.ascender = TextFontAccess::ascender(font, size_px);
            out.descender = TextFontAccess::descender(font, size_px);
            out.line_height = TextFontAccess::line_height(font, size_px);

            f32 line_y = 0.0f;
            size_t cursor = 0;
            while (cursor <= utf8_text.size())
            {
                const size_t line_end = utf8_text.find('\n', cursor);
                const bool has_newline = line_end != acul::string::npos;
                const size_t span_end = has_newline ? line_end : utf8_text.size();

                if (config.max_lines != 0 && out.lines.size() >= config.max_lines)
                {
                    out.truncated = true;
                    break;
                }

                if (span_end == cursor) append_empty_line(out, cursor, line_y);
                else if (text_wrap_mode(config.flags) == TextWrapMode::word)
                {
                    bool truncated = false;
                    if (!layout_wrapped_span(out, font, size_px, utf8_text, cursor, span_end, config, line_y,
                                             truncated))
                        return false;
                    if (truncated)
                    {
                        out.truncated = true;
                        break;
                    }
                }
                else
                {
                    if (config.max_lines != 0 && out.lines.size() + 1 >= config.max_lines &&
                        (span_end < utf8_text.size() || span_end - cursor > 0))
                    {
                        if (!append_ellipsized_line(out, font, size_px, utf8_text, cursor, span_end, line_y,
                                                    config.max_width))
                            return false;
                        out.truncated = span_end < utf8_text.size();
                        break;
                    }

                    if (!append_shaped_line(out, font, size_px, utf8_text, cursor, span_end, line_y)) return false;
                    line_y += out.line_height;
                }

                if (!has_newline) break;
                cursor = span_end + 1;
            }

            return true;
        }

        bool build_single_line_instances(Font &font, u32 size_px, const acul::string &utf8_text,
                                         const TextLayoutConfig &layout_config, const TextRenderConfig &render_config,
                                         u32 tint_color, acul::vector<TexturesInstanceData> &out,
                                         TextLayoutResult *layout_result)
        {
            TextLayoutResult local_layout;
            auto &layout = layout_result ? *layout_result : local_layout;
            auto effective_layout = layout_config;
            if (size_px == 0) return false;
            if (text_width_mode(effective_layout.flags) == TextLayoutWidthMode::bounds &&
                effective_layout.max_width <= 0.0f)
                effective_layout.max_width = render_config.size.x;
            if (!layout_single_line(font, size_px, utf8_text, effective_layout, layout)) return false;
            return append_instances_from_layout(font, size_px, layout, render_config, tint_color, out);
        }

        bool build_multiline_instances(Font &font, u32 size_px, const acul::string &utf8_text,
                                       const TextLayoutConfig &layout_config, const TextRenderConfig &render_config,
                                       u32 tint_color, acul::vector<TexturesInstanceData> &out,
                                       TextLayoutResult *layout_result)
        {
            TextLayoutResult local_layout;
            auto &layout = layout_result ? *layout_result : local_layout;
            auto effective_layout = layout_config;
            if (size_px == 0) return false;
            if (text_width_mode(effective_layout.flags) == TextLayoutWidthMode::bounds &&
                effective_layout.max_width <= 0.0f)
                effective_layout.max_width = render_config.size.x;
            if (!layout_multiline(font, size_px, utf8_text, effective_layout, layout)) return false;
            return append_instances_from_layout(font, size_px, layout, render_config, tint_color, out);
        }
    } // namespace detail
} // namespace auik
