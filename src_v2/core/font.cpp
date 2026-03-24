#include <algorithm>
#include <cctype>
#include <cstring>
#include <utility>
#include <acul/io/fs/path.hpp>
#include <auik/v2/auik.hpp>
#include <auik/v2/detail/atlas.hpp>
#include <auik/v2/detail/context.hpp>
#include <auik/v2/widgets/image.hpp>
#include <fontconfig/fontconfig.h>
#include <freetype/freetype.h>

#ifdef _WIN32
#include <windows.h>
#endif

namespace auik::v2
{
    namespace
    {
        static FT_Int32 to_ft_load_flags(FontLoadFlags flags)
        {
            return static_cast<FT_Int32>(static_cast<FontLoadFlags::mask_t>(flags));
        }

        static FT_Render_Mode to_ft_render_mode(FontRenderMode mode)
        {
            return static_cast<FT_Render_Mode>(static_cast<u32>(mode));
        }

        static bool decode_utf8_codepoint(const acul::string &text, size_t &offset, u32 &out_codepoint)
        {
            if (offset >= text.size()) return false;
            const auto *src = reinterpret_cast<const u8 *>(text.data());
            const u8 lead = src[offset];
            if (lead < 0x80u)
            {
                out_codepoint = lead;
                ++offset;
                return true;
            }

            auto read_tail = [&](size_t i) -> u8 { return (i < text.size()) ? src[i] : 0xFFu; };
            if ((lead & 0xE0u) == 0xC0u)
            {
                const u8 b1 = read_tail(offset + 1);
                if ((b1 & 0xC0u) != 0x80u) return false;
                out_codepoint = ((lead & 0x1Fu) << 6) | (b1 & 0x3Fu);
                offset += 2;
                return true;
            }
            if ((lead & 0xF0u) == 0xE0u)
            {
                const u8 b1 = read_tail(offset + 1);
                const u8 b2 = read_tail(offset + 2);
                if ((b1 & 0xC0u) != 0x80u || (b2 & 0xC0u) != 0x80u) return false;
                out_codepoint = ((lead & 0x0Fu) << 12) | ((b1 & 0x3Fu) << 6) | (b2 & 0x3Fu);
                offset += 3;
                return true;
            }
            if ((lead & 0xF8u) == 0xF0u)
            {
                const u8 b1 = read_tail(offset + 1);
                const u8 b2 = read_tail(offset + 2);
                const u8 b3 = read_tail(offset + 3);
                if ((b1 & 0xC0u) != 0x80u || (b2 & 0xC0u) != 0x80u || (b3 & 0xC0u) != 0x80u) return false;
                out_codepoint = ((lead & 0x07u) << 18) | ((b1 & 0x3Fu) << 12) | ((b2 & 0x3Fu) << 6) | (b3 & 0x3Fu);
                offset += 4;
                return true;
            }
            return false;
        }

        static bool make_gray_image_from_bitmap(const FT_Bitmap &bitmap, umbf::Image2D &dst)
        {
            dst.width = bitmap.width;
            dst.height = bitmap.rows;
            dst.format = {umbf::ImageFormat::Type::uint, 1};
            dst.channels = {"r", "g", "b", "a"};
            const size_t size = static_cast<size_t>(dst.width) * dst.height * 4;
            auto *pixels = acul::mem_allocator<u8>::allocate(size);
            if (!pixels) return false;

            const int pitch = bitmap.pitch >= 0 ? bitmap.pitch : -bitmap.pitch;
            const u8 *src = bitmap.buffer;
            for (u32 y = 0; y < dst.height; ++y)
            {
                const u8 *src_row = (bitmap.pitch >= 0) ? (src + y * pitch) : (src + (dst.height - 1 - y) * pitch);
                u8 *dst_row = pixels + static_cast<size_t>(y) * dst.width * 4;
                for (u32 x = 0; x < dst.width; ++x)
                {
                    const u8 alpha = src_row[x];
                    dst_row[x * 4 + 0] = 0xFF;
                    dst_row[x * 4 + 1] = 0xFF;
                    dst_row[x * 4 + 2] = 0xFF;
                    dst_row[x * 4 + 3] = alpha;
                }
            }
            dst.pixels = pixels;
            return true;
        }

        static bool make_mono_image_from_bitmap(const FT_Bitmap &bitmap, umbf::Image2D &dst)
        {
            dst.width = bitmap.width;
            dst.height = bitmap.rows;
            dst.format = {umbf::ImageFormat::Type::uint, 1};
            dst.channels = {"r", "g", "b", "a"};
            const size_t size = static_cast<size_t>(dst.width) * dst.height * 4;
            auto *pixels = acul::mem_allocator<u8>::allocate(size);
            if (!pixels) return false;

            const int pitch = bitmap.pitch >= 0 ? bitmap.pitch : -bitmap.pitch;
            const u8 *src = bitmap.buffer;
            for (u32 y = 0; y < dst.height; ++y)
            {
                const u8 *src_row = (bitmap.pitch >= 0) ? (src + y * pitch) : (src + (dst.height - 1 - y) * pitch);
                u8 *dst_row = pixels + static_cast<size_t>(y) * dst.width * 4;
                for (u32 x = 0; x < dst.width; ++x)
                {
                    const u8 bits = src_row[x / 8];
                    const u8 alpha = (bits & (0x80u >> (x & 7u))) ? 0xFFu : 0x00u;
                    dst_row[x * 4 + 0] = 0xFF;
                    dst_row[x * 4 + 1] = 0xFF;
                    dst_row[x * 4 + 2] = 0xFF;
                    dst_row[x * 4 + 3] = alpha;
                }
            }
            dst.pixels = pixels;
            return true;
        }

        static bool make_bgra_image_from_bitmap(const FT_Bitmap &bitmap, umbf::Image2D &dst)
        {
            dst.width = bitmap.width;
            dst.height = bitmap.rows;
            dst.format = {umbf::ImageFormat::Type::uint, 1};
            dst.channels = {"r", "g", "b", "a"};
            const size_t size = static_cast<size_t>(dst.width) * dst.height * 4;
            auto *pixels = acul::mem_allocator<u8>::allocate(size);
            if (!pixels) return false;

            const int pitch = bitmap.pitch >= 0 ? bitmap.pitch : -bitmap.pitch;
            const u8 *src = bitmap.buffer;
            for (u32 y = 0; y < dst.height; ++y)
            {
                const u8 *src_row = (bitmap.pitch >= 0) ? (src + y * pitch) : (src + (dst.height - 1 - y) * pitch);
                u8 *dst_row = pixels + static_cast<size_t>(y) * dst.width * 4;
                for (u32 x = 0; x < dst.width; ++x)
                {
                    const u8 *bgra = src_row + x * 4;
                    u8 *rgba = dst_row + x * 4;
                    rgba[0] = bgra[2];
                    rgba[1] = bgra[1];
                    rgba[2] = bgra[0];
                    rgba[3] = bgra[3];
                }
            }
            dst.pixels = pixels;
            return true;
        }

        static bool make_glyph_image(const FT_Bitmap &bitmap, umbf::Image2D &dst, bool &colored)
        {
            colored = bitmap.pixel_mode == FT_PIXEL_MODE_BGRA;
            switch (bitmap.pixel_mode)
            {
                case FT_PIXEL_MODE_GRAY:
                    return make_gray_image_from_bitmap(bitmap, dst);
                case FT_PIXEL_MODE_MONO:
                    return make_mono_image_from_bitmap(bitmap, dst);
                case FT_PIXEL_MODE_BGRA:
                    return make_bgra_image_from_bitmap(bitmap, dst);
                default:
                    return false;
            }
        }

        struct PreparedGlyph
        {
            Glyph glyph{};
            umbf::Image2D image{};
        };
    } // namespace

    Font::Font(const FontInfo &info, int face_index) { load(info, face_index); }
    Font::Font(const acul::string &path, int face_index) { load(path, face_index); }
    Font::~Font() { clear(); }

    Font::Font(Font &&other) noexcept
        : _info(std::move(other._info)), _face(other._face), _face_index(other._face_index),
          _glyphs(std::move(other._glyphs))
    {
        other._face = nullptr;
        other._face_index = 0;
    }

    Font &Font::operator=(Font &&other) noexcept
    {
        if (this == &other) return *this;

        clear();
        _info = std::move(other._info);
        _face = other._face;
        _face_index = other._face_index;
        _glyphs = std::move(other._glyphs);

        other._face = nullptr;
        other._face_index = 0;
        return *this;
    }

    bool Font::load(const FontInfo &info, int face_index)
    {
        _info = info;
        return load(info.path, face_index);
    }

    bool Font::load(const acul::string &path, int face_index)
    {
        clear();
        if (path.empty()) return false;
        if (!detail::g_context || !detail::g_context->ft_library) return false;

        _face_index = face_index;
        auto error = FT_New_Face(detail::g_context->ft_library, path.c_str(), face_index, &_face);
        if (error)
        {
            clear();
            return false;
        }

        _info.path = path;
        return true;
    }

    void Font::clear()
    {
        _glyphs.clear();
        if (_face)
        {
            FT_Done_Face(_face);
            _face = nullptr;
        }
        _face_index = 0;
    }

    bool Font::set_pixel_size(u32 size_px)
    {
        if (!_face || size_px == 0) return false;
        if (FT_Set_Pixel_Sizes(_face, 0, size_px) != 0) return false;
        _glyphs.clear();
        return true;
    }

    void Font::set_load_flags(FontLoadFlags flags)
    {
        _load_flags = flags;
        _glyphs.clear();
    }

    void Font::add_load_flags(FontLoadFlags flags)
    {
        _load_flags |= flags;
        _glyphs.clear();
    }

    void Font::remove_load_flags(FontLoadFlags flags)
    {
        _load_flags &= ~flags;
        _glyphs.clear();
    }

    void Font::set_render_mode(FontRenderMode mode)
    {
        _render_mode = mode;
        _glyphs.clear();
    }

    bool Font::load_glyph(u32 codepoint)
    {
        acul::vector<u32> codepoints;
        codepoints.push_back(codepoint);
        return load_glyphs(codepoints);
    }

    bool Font::load_glyphs(const acul::vector<u32> &codepoints)
    {
        if (!_face || codepoints.empty()) return false;

        acul::vector<PreparedGlyph> prepared;
        acul::vector<umbf::Image2D> images;
        acul::vector<size_t> image_to_glyph;
        bool loaded_any = false;

        auto release_prepared = [&]() {
            for (auto &item : prepared)
            {
                if (item.image.pixels) acul::release(item.image.pixels);
                item.image = {};
            }
        };

        auto is_pending = [&](u32 codepoint) {
            for (const auto &item : prepared)
                if (item.glyph.codepoint == codepoint) return true;
            return false;
        };

        for (u32 codepoint : codepoints)
        {
            if (codepoint > 0x3FFFFFFFu) continue;
            if (_glyphs.find(codepoint) != _glyphs.end() || is_pending(codepoint)) continue;

            const FT_UInt glyph_index = FT_Get_Char_Index(_face, codepoint);
            if (glyph_index == 0) continue;

            auto error = FT_Load_Glyph(_face, glyph_index, to_ft_load_flags(_load_flags));
            if (error) continue;

            FT_GlyphSlot slot = _face->glyph;
            if (slot->format != FT_GLYPH_FORMAT_BITMAP)
            {
                error = FT_Render_Glyph(slot, to_ft_render_mode(_render_mode));
                if (error) continue;
            }

            PreparedGlyph item;
            item.glyph.codepoint = codepoint;
            item.glyph.offset = {slot->bitmap_left, slot->bitmap_top};
            item.glyph.size = {static_cast<i32>(slot->bitmap.width), static_cast<i32>(slot->bitmap.rows)};
            item.glyph.advance_x = static_cast<f32>(slot->advance.x) / 64.0f;
            item.glyph.empty = (item.glyph.size.x == 0 || item.glyph.size.y == 0) ? 1u : 0u;

            if (!item.glyph.empty)
            {
                bool is_colored = false;
                if (!make_glyph_image(slot->bitmap, item.image, is_colored))
                {
                    release_prepared();
                    return false;
                }
                item.glyph.colored = is_colored ? 1u : 0u;
                images.push_back(item.image);
                image_to_glyph.push_back(prepared.size());
            }

            prepared.push_back(std::move(item));
        }

        if (prepared.empty()) return false;

        if (!images.empty())
        {
            acul::vector<detail::AtlasAllocation> allocations;
            if (!detail::allocate_atlas_regions(images, allocations) || allocations.size() != images.size())
            {
                release_prepared();
                return false;
            }

            for (size_t i = 0; i < allocations.size(); ++i)
            {
                auto &glyph = prepared[image_to_glyph[i]].glyph;
                const auto &allocation = allocations[i];
                glyph.atlas_id = allocation.atlas_id;
                glyph.texture_id = allocation.texture_id;
                glyph.pixel_rect = allocation.pixel_rect;
                glyph.uv_rect = allocation.uv_rect;
            }
        }

        for (auto &item : prepared)
        {
            const u32 codepoint = item.glyph.codepoint;
            _glyphs[codepoint] = item.glyph;
            loaded_any = true;
        }

        release_prepared();
        return loaded_any;
    }

    bool Font::load_glyphs(const acul::string &utf8_text)
    {
        acul::vector<u32> codepoints;
        size_t offset = 0;
        while (offset < utf8_text.size())
        {
            u32 codepoint = 0;
            const size_t prev = offset;
            if (!decode_utf8_codepoint(utf8_text, offset, codepoint))
            {
                offset = prev + 1;
                continue;
            }
            codepoints.push_back(codepoint);
        }
        return load_glyphs(codepoints);
    }

    Glyph *Font::find_glyph(u32 codepoint)
    {
        auto it = _glyphs.find(codepoint);
        return it != _glyphs.end() ? &it->second : nullptr;
    }

    const Glyph *Font::find_glyph(u32 codepoint) const
    {
        auto it = _glyphs.find(codepoint);
        return it != _glyphs.end() ? &it->second : nullptr;
    }

    bool load_fonts(FontRegistry &fonts, const acul::vector<acul::string> &search_dirs)
    {
        if (!detail::g_context || !detail::g_context->ft_library) return false;
        const acul::vector<acul::string> supported_extensions = {".ttf", ".ttc", ".cff", ".otf", ".otc",
                                                                 ".pcf", ".fnt", ".bdf", ".pfr"};
        FcConfig *config = FcInitLoadConfigAndFonts();
        FcPattern *pat = FcPatternCreate();
        FcObjectSet *os = FcObjectSetBuild(FC_FAMILY, FC_WEIGHT, FC_SLANT, FC_FILE, FC_FULLNAME, (char *)0);
        if (!search_dirs.empty())
            for (auto &path : search_dirs) FcConfigAppFontAddFile(config, (const FcChar8 *)path.c_str());
        FcFontSet *fs = FcFontList(config, pat, os);
        for (int i = 0; fs && i < fs->nfont; ++i)
        {
            FcPattern *font = fs->fonts[i];

            FcChar8 *file, *family, *fullname;
            int slant, weight;
            if (FcPatternGetString(font, FC_FILE, 0, &file) == FcResultMatch &&
                FcPatternGetString(font, FC_FAMILY, 0, &family) == FcResultMatch &&
                FcPatternGetString(font, FC_FULLNAME, 0, &fullname) == FcResultMatch &&
                FcPatternGetInteger(font, FC_WEIGHT, 0, &weight) == FcResultMatch &&
                FcPatternGetInteger(font, FC_SLANT, 0, &slant) == FcResultMatch)
            {
                acul::string ext = acul::fs::get_extension(reinterpret_cast<char *>(file));
                std::transform(ext.begin(), ext.end(), ext.begin(),
                               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                if (std::find(supported_extensions.begin(), supported_extensions.end(), ext) !=
                    supported_extensions.end())
                    fonts.emplace((char *)family, {(char *)file, (char *)fullname, weight, slant});
            }
        }
        if (fs) FcFontSetDestroy(fs);
        FcObjectSetDestroy(os);
        FcPatternDestroy(pat);
        FcConfigDestroy(config);
        FcFini();
        return true;
    }

    bool load_material_icons(const FontRegistry &fonts, f32 dpi)
    {
        FontInfo *font_info = get_font_info_by_family(fonts, "Material Symbols Outlined");
        if (!font_info) return false;

        Font font;
        const u32 size = round_font_px(18.0f * dpi);
        const acul::vector<u32> codepoints = {0xE5CCu, 0xE5CFu, 0xE5CAu, 0xE8B6u, 0xEF4Fu, 0xE5D2u};
        const acul::vector<u32> ids = {AUIK_ICON_CHEVRON_RIGHT, AUIK_ICON_CHEVRON_DOWN, AUIK_ICON_CHECKMARK,
                                       AUIK_ICON_SEARCH, AUIK_ICON_FILTER, AUIK_ICON_MENU};

        if (!font.load(font_info->path) || !font.set_pixel_size(size)) return false;
        if (!font.load_glyphs(codepoints)) return false;

        for (u32 i = 0; i < codepoints.size(); ++i)
        {
            auto *glyph = font.find_glyph(codepoints[i]);
            if (!glyph) return false;

            const u32 id = ids[i];
            auto *image = make_image(id, glyph->texture_id,
                                     {static_cast<f32>(glyph->size.x), static_cast<f32>(glyph->size.y)}, glyph->uv_rect);
            if (image) cache_image(id, image);
        }

        return true;
    }

#ifdef _WIN32
    namespace
    {
        static bool is_win_11_or_greater()
        {
            OSVERSIONINFOEX osvi{};
            osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
            GetVersionEx((OSVERSIONINFO *)&osvi);
            return osvi.dwBuildNumber >= 22000;
        }
    } // namespace

    bool load_win32_icons(const FontRegistry &fonts, f32 dpi)
    {
        FontInfo *font_info = nullptr;
        if (is_win_11_or_greater()) font_info = get_font_info_by_family(fonts, "Segoe Fluent Icons");
        if (!font_info) font_info = get_font_info_by_family(fonts, "Segoe MDL2 Assets");
        if (!font_info) return false;

        struct
        {
            Font font;
            u32 size;
            acul::vector<u32> codepoints;
            acul::vector<u32> ids;
        } specs[3] = {
            {
                .size = round_font_px(10.0f * dpi),
                .codepoints = {0xE921u, 0xE922u, 0xE923u, 0xE8BBu},
                .ids = {AUIK_ICON_MINIMIZE, AUIK_ICON_MAXIMIZE, AUIK_ICON_RESTORE, AUIK_ICON_CLOSE}
            },
            {.size = round_font_px(11.5f * dpi), .codepoints = {0xE700u}, .ids = {AUIK_ICON_MENU}},
            {
                .size = round_font_px(pt_to_px(11.0f, dpi)),
                .codepoints = {0xE76Cu, 0xE70Du, 0xE73Eu, 0xE721u, 0xE71Cu},
                .ids = {AUIK_ICON_CHEVRON_RIGHT, AUIK_ICON_CHEVRON_DOWN, AUIK_ICON_CHECKMARK, AUIK_ICON_SEARCH,
                        AUIK_ICON_FILTER}
            },
        };

        for (u32 i = 0; i < 3; ++i)
        {
            auto &font = specs[i].font;
            if (!font.load(font_info->path) || !font.set_pixel_size(specs[i].size)) return false;
            if (!font.load_glyphs(specs[i].codepoints)) return false;
            for (u32 code = 0; code < specs[i].codepoints.size(); ++code)
            {
                auto *glyph = font.find_glyph(specs[i].codepoints[code]);
                if (!glyph) return false;
                u32 id = specs[i].ids[code];
                auto *image =
                    make_image(id, glyph->texture_id, {static_cast<f32>(glyph->size.x), static_cast<f32>(glyph->size.y)},
                               glyph->uv_rect);
                if (image) cache_image(id, image);
            }
        }
        return true;
    }
#endif
} // namespace auik::v2
