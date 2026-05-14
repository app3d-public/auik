#include <acul/io/fs/path.hpp>
#include <auik/v2/auik.hpp>
#include <auik/v2/detail/atlas.hpp>
#include <auik/v2/detail/context.hpp>
#include <auik/v2/detail/text.hpp>
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
            dst.channels = {"r"};
            const size_t size = static_cast<size_t>(dst.width) * dst.height;
            auto *pixels = acul::mem_allocator<u8>::allocate(size);
            if (!pixels) return false;

            const int pitch = bitmap.pitch >= 0 ? bitmap.pitch : -bitmap.pitch;
            const u8 *src = bitmap.buffer;
            for (u32 y = 0; y < dst.height; ++y)
            {
                const u8 *src_row = (bitmap.pitch >= 0) ? (src + y * pitch) : (src + (dst.height - 1 - y) * pitch);
                u8 *dst_row = pixels + static_cast<size_t>(y) * dst.width;
                for (u32 x = 0; x < dst.width; ++x) dst_row[x] = src_row[x];
            }
            dst.pixels = pixels;
            return true;
        }

        static bool make_mono_image_from_bitmap(const FT_Bitmap &bitmap, umbf::Image2D &dst)
        {
            dst.width = bitmap.width;
            dst.height = bitmap.rows;
            dst.format = {umbf::ImageFormat::Type::uint, 1};
            dst.channels = {"r"};
            const size_t size = static_cast<size_t>(dst.width) * dst.height;
            auto *pixels = acul::mem_allocator<u8>::allocate(size);
            if (!pixels) return false;

            const int pitch = bitmap.pitch >= 0 ? bitmap.pitch : -bitmap.pitch;
            const u8 *src = bitmap.buffer;
            for (u32 y = 0; y < dst.height; ++y)
            {
                const u8 *src_row = (bitmap.pitch >= 0) ? (src + y * pitch) : (src + (dst.height - 1 - y) * pitch);
                u8 *dst_row = pixels + static_cast<size_t>(y) * dst.width;
                for (u32 x = 0; x < dst.width; ++x)
                {
                    const u8 bits = src_row[x / 8];
                    dst_row[x] = (bits & (0x80u >> (x & 7u))) ? 0xFFu : 0x00u;
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
            dst.channels = {"r"};
            const size_t size = static_cast<size_t>(dst.width) * dst.height;
            auto *pixels = acul::mem_allocator<u8>::allocate(size);
            if (!pixels) return false;

            const int pitch = bitmap.pitch >= 0 ? bitmap.pitch : -bitmap.pitch;
            const u8 *src = bitmap.buffer;
            for (u32 y = 0; y < dst.height; ++y)
            {
                const u8 *src_row = (bitmap.pitch >= 0) ? (src + y * pitch) : (src + (dst.height - 1 - y) * pitch);
                u8 *dst_row = pixels + static_cast<size_t>(y) * dst.width;
                for (u32 x = 0; x < dst.width; ++x)
                {
                    const u8 *bgra = src_row + x * 4;
                    dst_row[x] = bgra[3];
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

        static bool make_six_dots_icon_image(f32 dpi, umbf::Image2D &dst)
        {
            constexpr f32 supersample = 3.0f;
            const f32 s = dpi * supersample;
            const f32 dot_radius = amal::max(1.15f * s, 1.0f);
            const f32 fringe = amal::max(0.85f * supersample, 1.0f);
            const u32 dot_step = amal::max(round_font_px(3.4f * s), 3u);
            const u32 pad_x = amal::max(round_font_px(2.0f * s), 2u);
            const u32 pad_y = amal::max(round_font_px(1.5f * s), 2u);
            dst.width = amal::max(round_font_px(7.0f * s), 12u);
            dst.height = pad_y * 2u + dot_step * 5u + round_font_px(dot_radius * 2.0f);
            dst.format = {umbf::ImageFormat::Type::uint, 1};
            dst.channels = {"r"};

            auto *pixels = acul::mem_allocator<u8>::allocate(static_cast<size_t>(dst.width) * dst.height);
            if (!pixels) return false;
            memset(pixels, 0, static_cast<size_t>(dst.width) * dst.height);

            const f32 cx = static_cast<f32>(pad_x) + dot_radius;
            for (u32 dot = 0; dot < 6u; ++dot)
            {
                const f32 cy = static_cast<f32>(pad_y) + dot_radius + static_cast<f32>(dot * dot_step);
                for (u32 y = 0; y < dst.height; ++y)
                {
                    for (u32 x = 0; x < dst.width; ++x)
                    {
                        const f32 dx = static_cast<f32>(x) + 0.5f - cx;
                        const f32 dy = static_cast<f32>(y) + 0.5f - cy;
                        const f32 dist = std::sqrt(dx * dx + dy * dy);
                        const f32 coverage = amal::clamp((dot_radius + fringe - dist) / fringe, 0.0f, 1.0f);
                        u8 &dst_pixel = pixels[static_cast<size_t>(y) * dst.width + x];
                        dst_pixel = amal::max(dst_pixel, static_cast<u8>(coverage * 255.0f + 0.5f));
                    }
                }
            }

            dst.pixels = pixels;
            return true;
        }

        static bool cache_six_dots_icon(f32 dpi)
        {
            if (get_cached_image(AUIK_ICON_SIX_DOTS)) return true;

            umbf::Image2D image{};
            if (!make_six_dots_icon_image(dpi, image)) return false;

            detail::AtlasAllocation allocation{};
            const bool allocated = detail::allocate_atlas_region(image, allocation) && allocation.valid();
            if (allocated)
            {
                auto *icon =
                    make_image(AUIK_ICON_SIX_DOTS, allocation.texture_id,
                               {static_cast<f32>(image.width), static_cast<f32>(image.height)}, allocation.uv_rect);
                if (icon)
                {
                    icon->set_coverage_mode(true);
                    cache_image(AUIK_ICON_SIX_DOTS, icon);
                }
            }
            acul::release(image.pixels);
            return allocated && get_cached_image(AUIK_ICON_SIX_DOTS) != nullptr;
        }

        struct PreparedGlyph
        {
            u32 size_px = 0;
            u32 glyph_index = 0;
            Glyph glyph{};
            umbf::Image2D image{};
        };

        static bool prepare_glyph(Font &font, u32 size_px, u32 glyph_index, u32 codepoint, PreparedGlyph &item)
        {
            auto *face = detail::TextFontAccess::face(font, size_px);
            if (!face || glyph_index == 0) return false;

            auto error = FT_Load_Glyph(face, glyph_index, to_ft_load_flags(font.load_flags()));
            if (error) return false;

            FT_GlyphSlot slot = face->glyph;
            if (slot->format != FT_GLYPH_FORMAT_BITMAP)
            {
                error = FT_Render_Glyph(slot, to_ft_render_mode(font.render_mode()));
                if (error) return false;
            }

            item.size_px = size_px;
            item.glyph_index = glyph_index;
            item.glyph.codepoint = codepoint;
            item.glyph.offset = {slot->bitmap_left, slot->bitmap_top};
            item.glyph.size = {static_cast<i32>(slot->bitmap.width), static_cast<i32>(slot->bitmap.rows)};
            item.glyph.advance_x = static_cast<f32>(slot->advance.x) / 64.0f;
            item.glyph.empty = (item.glyph.size.x == 0 || item.glyph.size.y == 0) ? 1u : 0u;

            bool is_colored = false;
            if (!item.glyph.empty)
            {
                if (!make_glyph_image(slot->bitmap, item.image, is_colored)) return false;
                item.glyph.colored = is_colored ? 1u : 0u;
            }

            return true;
        }

        static bool finalize_prepared_glyphs(acul::vector<PreparedGlyph> &prepared)
        {
            if (prepared.empty()) return false;

            acul::vector<umbf::Image2D> images;
            acul::vector<size_t> image_to_glyph;
            images.reserve(prepared.size());
            image_to_glyph.reserve(prepared.size());

            auto release_prepared = [&]() {
                for (auto &item : prepared)
                {
                    if (item.image.pixels) acul::release(item.image.pixels);
                    item.image = {};
                }
            };

            for (size_t i = 0; i < prepared.size(); ++i)
            {
                if (prepared[i].glyph.empty) continue;
                images.push_back(prepared[i].image);
                image_to_glyph.push_back(i);
            }

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

            release_prepared();
            return true;
        }
    } // namespace

    Font::Font(const FontInfo &info, int face_index) { load(info, face_index); }
    Font::Font(const acul::string &path, int face_index) { load(path, face_index); }
    Font::~Font() { clear(); }

    Font::Font(Font &&other) noexcept
        : _info(std::move(other._info)),
          _face(other._face),
          _face_index(other._face_index),
          _active_size_px(other._active_size_px),
          _glyphs(std::move(other._glyphs))
    {
        other._face = nullptr;
        other._face_index = 0;
        other._active_size_px = 0;
    }

    Font &Font::operator=(Font &&other) noexcept
    {
        if (this == &other) return *this;

        clear();
        _info = std::move(other._info);
        _face = other._face;
        _face_index = other._face_index;
        _active_size_px = other._active_size_px;
        _glyphs = std::move(other._glyphs);

        other._face = nullptr;
        other._face_index = 0;
        other._active_size_px = 0;
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
        _active_size_px = 0;
        _face_index = 0;
    }

    bool Font::ensure_size_px(u32 size_px)
    {
        if (!_face || size_px == 0) return false;
        if (_active_size_px == size_px) return true;
        if (FT_Set_Pixel_Sizes(_face, 0, size_px) != 0) return false;
        _active_size_px = size_px;
        return true;
    }

    GlyphCache *Font::find_cache(u32 size_px)
    {
        auto it = _glyphs.find(size_px);
        return it != _glyphs.end() ? &it->second : nullptr;
    }

    const GlyphCache *Font::find_cache(u32 size_px) const
    {
        auto it = _glyphs.find(size_px);
        return it != _glyphs.end() ? &it->second : nullptr;
    }

    GlyphCache &Font::ensure_cache(u32 size_px) { return _glyphs[size_px]; }

    void Font::set_load_flags(FontLoadFlags flags)
    {
        _load_flags = flags;
        _glyphs.clear();
        _active_size_px = 0;
    }

    void Font::add_load_flags(FontLoadFlags flags)
    {
        _load_flags |= flags;
        _glyphs.clear();
        _active_size_px = 0;
    }

    void Font::remove_load_flags(FontLoadFlags flags)
    {
        _load_flags &= ~flags;
        _glyphs.clear();
        _active_size_px = 0;
    }

    void Font::set_render_mode(FontRenderMode mode)
    {
        _render_mode = mode;
        _glyphs.clear();
        _active_size_px = 0;
    }

    size_t Font::glyph_count() const
    {
        size_t total = 0;
        for (const auto &cache : _glyphs) total += cache.second.size();
        return total;
    }

    bool Font::load_glyph(u32 size_px, u32 codepoint)
    {
        acul::vector<u32> codepoints;
        codepoints.push_back(codepoint);
        return load_glyphs(size_px, codepoints);
    }

    bool Font::load_glyphs(u32 size_px, const acul::vector<u32> &codepoints)
    {
        if (!_face || size_px == 0 || codepoints.empty()) return false;

        acul::vector<PreparedGlyph> prepared;
        bool loaded_any = false;
        auto &cache = ensure_cache(size_px);

        auto release_prepared = [&]() {
            for (auto &item : prepared)
            {
                if (item.image.pixels) acul::release(item.image.pixels);
                item.image = {};
            }
        };

        auto is_pending = [&](u32 codepoint) {
            for (const auto &item : prepared)
                if (item.glyph_index == codepoint) return true;
            return false;
        };

        for (u32 codepoint : codepoints)
        {
            if (codepoint > 0x3FFFFFFFu) continue;

            const FT_UInt glyph_index = FT_Get_Char_Index(_face, codepoint);
            if (glyph_index == 0) continue;
            if (cache.find(glyph_index) != cache.end() || is_pending(glyph_index)) continue;

            PreparedGlyph item;
            if (!prepare_glyph(*this, size_px, glyph_index, codepoint, item))
            {
                release_prepared();
                return false;
            }

            prepared.push_back(std::move(item));
        }

        if (prepared.empty()) return false;

        if (!finalize_prepared_glyphs(prepared)) return false;

        for (auto &item : prepared)
        {
            cache[item.glyph_index] = item.glyph;
            loaded_any = true;
        }

        release_prepared();
        return loaded_any;
    }

    bool Font::load_glyphs(u32 size_px, const acul::string &utf8_text)
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
        return load_glyphs(size_px, codepoints);
    }

    Glyph *Font::find_glyph(u32 size_px, u32 codepoint)
    {
        if (!_face) return nullptr;
        const u32 glyph_index = FT_Get_Char_Index(_face, codepoint);
        if (glyph_index == 0) return nullptr;
        auto *cache = find_cache(size_px);
        if (!cache) return nullptr;
        auto it = cache->find(glyph_index);
        return it != cache->end() ? &it->second : nullptr;
    }

    const Glyph *Font::find_glyph(u32 size_px, u32 codepoint) const
    {
        if (!_face) return nullptr;
        const u32 glyph_index = FT_Get_Char_Index(_face, codepoint);
        if (glyph_index == 0) return nullptr;
        auto *cache = find_cache(size_px);
        if (!cache) return nullptr;
        auto it = cache->find(glyph_index);
        return it != cache->end() ? &it->second : nullptr;
    }

    FT_Face detail::TextFontAccess::face(Font &font, u32 size_px)
    {
        if (!font.ensure_size_px(size_px)) return nullptr;
        return font._face;
    }

    Glyph *detail::TextFontAccess::find_glyph_by_index(Font &font, u32 size_px, u32 glyph_index)
    {
        auto *cache = font.find_cache(size_px);
        if (!cache) return nullptr;
        auto it = cache->find(glyph_index);
        return it != cache->end() ? &it->second : nullptr;
    }

    const Glyph *detail::TextFontAccess::find_glyph_by_index(const Font &font, u32 size_px, u32 glyph_index)
    {
        auto *cache = font.find_cache(size_px);
        if (!cache) return nullptr;
        auto it = cache->find(glyph_index);
        return it != cache->end() ? &it->second : nullptr;
    }

    bool detail::TextFontAccess::load_glyph_indices(Font &font, u32 size_px, const acul::vector<u32> &glyph_indices)
    {
        if (!font._face || size_px == 0 || glyph_indices.empty()) return false;

        acul::vector<PreparedGlyph> prepared;
        prepared.reserve(glyph_indices.size());
        auto &cache = font.ensure_cache(size_px);

        auto is_pending = [&](u32 glyph_index) {
            for (const auto &item : prepared)
                if (item.glyph_index == glyph_index) return true;
            return false;
        };

        bool loaded_any = false;
        for (u32 glyph_index : glyph_indices)
        {
            if (glyph_index == 0 || cache.find(glyph_index) != cache.end() || is_pending(glyph_index)) continue;

            PreparedGlyph item;
            if (!prepare_glyph(font, size_px, glyph_index, 0, item))
            {
                for (auto &prepared_item : prepared)
                {
                    if (prepared_item.image.pixels) acul::release(prepared_item.image.pixels);
                    prepared_item.image = {};
                }
                return false;
            }
            prepared.push_back(std::move(item));
        }

        if (prepared.empty()) return false;
        if (!finalize_prepared_glyphs(prepared)) return false;
        for (auto &item : prepared) cache[item.glyph_index] = item.glyph;
        loaded_any = true;
        return loaded_any;
    }

    f32 detail::TextFontAccess::ascender(Font &font, u32 size_px)
    {
        if (!font.ensure_size_px(size_px) || !font._face->size) return 0.0f;
        return static_cast<f32>(font._face->size->metrics.ascender) / 64.0f;
    }

    f32 detail::TextFontAccess::descender(Font &font, u32 size_px)
    {
        if (!font.ensure_size_px(size_px) || !font._face->size) return 0.0f;
        return static_cast<f32>(font._face->size->metrics.descender) / 64.0f;
    }

    f32 detail::TextFontAccess::line_height(Font &font, u32 size_px)
    {
        if (!font.ensure_size_px(size_px) || !font._face->size) return 0.0f;
        return static_cast<f32>(font._face->size->metrics.height) / 64.0f;
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
        const acul::vector<u32> codepoints = {0xE5CCu, 0xE5C5u, 0xE5CEu, 0xE5CAu, 0xE8B6u, 0xEF4Fu, 0xE5D2u, 0xE5CDu};
        const acul::vector<u32> ids = {AUIK_ICON_CHEVRON_RIGHT, AUIK_ICON_CHEVRON_DOWN, AUIK_ICON_CHEVRON_UP,
                                       AUIK_ICON_CHECKMARK,     AUIK_ICON_SEARCH,       AUIK_ICON_FILTER,
                                       AUIK_ICON_MENU,          AUIK_ICON_CLOSE};

        if (!font.load(font_info->path)) return false;
        if (!font.load_glyphs(size, codepoints)) return false;

        for (u32 i = 0; i < codepoints.size(); ++i)
        {
            auto *glyph = font.find_glyph(size, codepoints[i]);
            if (!glyph) return false;

            const u32 id = ids[i];
            auto *image =
                make_image(id, glyph->texture_id, {static_cast<f32>(glyph->size.x), static_cast<f32>(glyph->size.y)},
                           glyph->uv_rect);
            if (image)
            {
                image->set_coverage_mode(true);
                cache_image(id, image);
            }
        }

        return cache_six_dots_icon(dpi);
    }

#ifdef _WIN32
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
            {.size = round_font_px(11.5f * dpi), .codepoints = {0xE700u}, .ids = {AUIK_ICON_MENU}},
            {.size = round_font_px(pt_to_px(8.5f, dpi)), .codepoints = {0xE711u}, .ids = {AUIK_ICON_CLOSE}},
            {.size = round_font_px(pt_to_px(9.5f, dpi)),
             .codepoints = {0xE76Cu, 0xE70Du, 0xE70Eu, 0xE73Eu, 0xE721u, 0xE71Cu},
             .ids = {AUIK_ICON_CHEVRON_RIGHT, AUIK_ICON_CHEVRON_DOWN, AUIK_ICON_CHEVRON_UP, AUIK_ICON_CHECKMARK,
                     AUIK_ICON_SEARCH, AUIK_ICON_FILTER}},
        };

        for (u32 i = 0; i < 3; ++i)
        {
            auto &font = specs[i].font;
            if (!font.load(font_info->path)) return false;
            font.set_load_flags(FontLoadFlagBits::target_light | FontLoadFlagBits::no_bitmap);
            font.set_render_mode(FontRenderMode::light);
            if (!font.load_glyphs(specs[i].size, specs[i].codepoints)) return false;
            for (u32 code = 0; code < specs[i].codepoints.size(); ++code)
            {
                auto *glyph = font.find_glyph(specs[i].size, specs[i].codepoints[code]);
                if (!glyph) return false;
                u32 id = specs[i].ids[code];
                auto *image =
                    make_image(id, glyph->texture_id,
                               {static_cast<f32>(glyph->size.x), static_cast<f32>(glyph->size.y)}, glyph->uv_rect);
                if (image)
                {
                    image->set_coverage_mode(true);
                    cache_image(id, image);
                }
            }
        }
        return cache_six_dots_icon(dpi);
    }
#endif
} // namespace auik::v2
