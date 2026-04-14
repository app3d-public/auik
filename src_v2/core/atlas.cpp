#include <auik/v2/detail/atlas.hpp>
#include <auik/v2/detail/context.hpp>

namespace auik::v2::detail
{
    namespace
    {
        static inline amal::rect make_uv_rect(const amal::irect &rect, const umbf::Image2D &image)
        {
            const f32 inv_width = 1.0f / static_cast<f32>(image.width);
            const f32 inv_height = 1.0f / static_cast<f32>(image.height);
            return {{rect.offset.x * inv_width, rect.offset.y * inv_height},
                    {rect.size.x * inv_width, rect.size.y * inv_height}};
        }

        static void destroy_page(AtlasPage &page, GPUContext *gpu_ctx)
        {
            destroy_atlas_texture(gpu_ctx, &page.texture);
            acul::release(page.surface.pixels);
            page.surface.pixels = nullptr;
            page.surface.width = 0;
            page.surface.height = 0;
            page.surface.channels.clear();
            page.rects.clear();
            page.texture = {};
        }

        static bool init_surface(umbf::Image2D &surface, const amal::ivec2 &size, umbf::ImageFormat format, u32 channel_count)
        {
            surface.width = static_cast<u32>(size.x);
            surface.height = static_cast<u32>(size.y);
            surface.format = format;
            surface.channels.clear();
            static constexpr const char *channel_names[] = {"r", "g", "b", "a"};
            for (u32 i = 0; i < channel_count && i < 4u; ++i) surface.channels.push_back(channel_names[i]);
            auto clear_pixel = umbf::utils::make_clear_pixel(surface.format, surface.channels.size());
            umbf::utils::fill_color_pixels(clear_pixel.get(), surface);
            return surface.pixels != nullptr;
        }

        static void copy_full_surface(const umbf::Image2D &src, umbf::Image2D &dst)
        {
            const size_t row_bytes =
                static_cast<size_t>(src.width) * src.channels.size() * src.format.bytes_per_channel;
            const size_t dst_row_bytes =
                static_cast<size_t>(dst.width) * dst.channels.size() * dst.format.bytes_per_channel;
            const auto *src_pixels = static_cast<const std::byte *>(src.pixels);
            auto *dst_pixels = static_cast<std::byte *>(dst.pixels);
            for (u32 y = 0; y < src.height; ++y)
                memcpy(dst_pixels + y * dst_row_bytes, src_pixels + y * row_bytes, row_bytes);
        }

        static bool is_atlas_compatible_image(const umbf::Image2D &image)
        {
            return image.pixels != nullptr && image.width > 0 && image.height > 0 &&
                   image.format.type == umbf::ImageFormat::Type::uint && image.format.bytes_per_channel == 1 &&
                   (image.channels.size() == 1 || image.channels.size() == 4);
        }

        static bool create_page_texture(AtlasPage &page, GPUContext *gpu_ctx)
        {
            return create_atlas_texture(gpu_ctx, &page.texture, page.surface);
        }

        static bool upload_full_page(AtlasPage &page, GPUContext *gpu_ctx)
        {
            return upload_atlas_texture(gpu_ctx, &page.texture, page.surface, page.surface.width, page.surface.height, 0,
                                        0);
        }

        static bool can_fit_after_growth(const AtlasPage &page, const amal::ivec2 &new_size, amal::irect &probe_rect,
                                         umbf::utils::SkylinePacker &grown_packer, i32 padding)
        {
            grown_packer.reset(new_size, padding);
            for (const auto &locked : page.rects)
                if (!grown_packer.add_locked(locked)) return false;
            return grown_packer.pack_rect(probe_rect, umbf::utils::SkylineHeuristic::bottom_left);
        }

        static bool grow_page(AtlasPage &page, const amal::irect &incoming_rect, const amal::ivec2 &max_size,
                              i32 padding, GPUContext *gpu_ctx)
        {
            amal::ivec2 current_size{static_cast<i32>(page.surface.width), static_cast<i32>(page.surface.height)};
            amal::ivec2 next_size = current_size;

            while (next_size.x < max_size.x || next_size.y < max_size.y)
            {
                if (next_size.x < max_size.x) next_size.x = amal::min(next_size.x * 2, max_size.x);
                if (next_size.y < max_size.y) next_size.y = amal::min(next_size.y * 2, max_size.y);

                amal::irect probe = incoming_rect;
                umbf::utils::SkylinePacker grown_packer;
                if (!can_fit_after_growth(page, next_size, probe, grown_packer, padding)) continue;

                umbf::Image2D grown_surface;
                if (!init_surface(grown_surface, next_size, page.surface.format,
                                  static_cast<u32>(page.surface.channels.size())))
                    return false;
                copy_full_surface(page.surface, grown_surface);

                destroy_atlas_texture(gpu_ctx, &page.texture);

                acul::release(page.surface.pixels);
                page.surface = grown_surface;
                page.packer = std::move(grown_packer);

                if (!create_page_texture(page, gpu_ctx)) return false;
                return true;
            }

            return false;
        }

        static AtlasPage *find_page(AtlasState &state, u32 atlas_id)
        {
            for (auto *page : state.pages)
            {
                if (!page || page->atlas_id != atlas_id) continue;
                return page;
            }
            return nullptr;
        }

        static AtlasPage *create_page(AtlasState &state, const amal::ivec2 &required_size, GPUContext *gpu_ctx,
                                      umbf::ImageFormat format, u32 channel_count)
        {
            amal::ivec2 page_size = state.initial_size;
            while ((page_size.x < required_size.x || page_size.y < required_size.y) &&
                   (page_size.x < state.max_size.x || page_size.y < state.max_size.y))
            {
                if (page_size.x < required_size.x) page_size.x = amal::min(page_size.x * 2, state.max_size.x);
                if (page_size.y < required_size.y) page_size.y = amal::min(page_size.y * 2, state.max_size.y);
            }

            if (page_size.x < required_size.x || page_size.y < required_size.y) return nullptr;

            auto *page = acul::alloc<AtlasPage>();
            page->atlas_id = state.next_atlas_id++;
            page->packer.reset(page_size, state.padding);
            if (!init_surface(page->surface, page_size, format, channel_count) || !create_page_texture(*page, gpu_ctx))
            {
                destroy_page(*page, gpu_ctx);
                acul::release(page);
                return nullptr;
            }

            state.pages.push_back(page);
            return page;
        }
    } // namespace

    bool init_atlas_state(AtlasState &state)
    {
        state.pages.clear();
        state.initial_size = {256, 256};
        state.max_size = {2048, 2048};
        state.padding = 1;
        state.next_atlas_id = 0;
        return true;
    }

    void destroy_atlas_state(AtlasState &state)
    {
        if (!g_context || !g_context->gpu_ctx) return;
        for (auto *page : state.pages)
        {
            if (!page) continue;
            destroy_page(*page, g_context->gpu_ctx);
            acul::release(page);
        }
        state.pages.clear();
        state.next_atlas_id = 0;
    }

    bool allocate_atlas_region(const umbf::Image2D &image, AtlasAllocation &out)
    {
        acul::vector<umbf::Image2D> images;
        images.push_back(image);
        acul::vector<AtlasAllocation> allocations;
        if (!allocate_atlas_regions(images, allocations) || allocations.empty()) return false;
        out = allocations.front();
        return true;
    }

    static inline void mark_page_touched(AtlasPage *page, acul::vector<AtlasPage *> &touched_pages)
    {
        for (auto *it : touched_pages)
            if (it == page) return;
        touched_pages.push_back(page);
    }

    bool allocate_atlas_regions(const acul::vector<umbf::Image2D> &images, acul::vector<AtlasAllocation> &out)
    {
        out.clear();
        if (images.empty()) return true;
        assert (g_context && g_context->gpu_ctx);

        auto &state = g_context->atlas_state;
        auto *gpu_ctx = g_context->gpu_ctx;
        out.resize(images.size());
        acul::vector<AtlasPage *> touched_pages;

        for (size_t i = 0; i < images.size(); ++i)
        {
            const auto &source = images[i];
            assert(is_atlas_compatible_image(source) && "auik atlas expects atlas-native R8 images");
            if (!is_atlas_compatible_image(source))
            {
                out.clear();
                return false;
            }
            const u32 source_channels = static_cast<u32>(source.channels.size());
            const amal::ivec2 required_size{static_cast<i32>(source.width), static_cast<i32>(source.height)};
            AtlasPage *target_page = nullptr;
            amal::irect rect{{0, 0}, required_size};

            for (auto *page : state.pages)
            {
                if (!page) continue;
                if (page->surface.format != source.format) continue;
                if (page->surface.channels.size() != source.channels.size()) continue;

                rect = {{0, 0}, required_size};
                if (!page->packer.pack_rect(rect, umbf::utils::SkylineHeuristic::bottom_left))
                {
                    if (!grow_page(*page, rect, state.max_size, state.padding, gpu_ctx)) continue;
                    if (!page->packer.pack_rect(rect, umbf::utils::SkylineHeuristic::bottom_left))
                    {
                        out.clear();
                        return false;
                    }
                }

                target_page = page;
                break;
            }

            if (!target_page)
            {
                target_page = create_page(state, required_size, gpu_ctx, source.format, source_channels);
                if (!target_page)
                {
                    out.clear();
                    return false;
                }

                rect = {{0, 0}, required_size};
                if (!target_page->packer.pack_rect(rect, umbf::utils::SkylineHeuristic::bottom_left))
                {
                    out.clear();
                    return false;
                }
            }

            umbf::utils::copy_pixels_to_area(source, target_page->surface, rect);
            target_page->rects.push_back(rect);
            mark_page_touched(target_page, touched_pages);

            out[i].atlas_id = target_page->atlas_id;
            out[i].texture_id = target_page->texture.texture_id;
            out[i].pixel_rect = rect;
            out[i].uv_rect = make_uv_rect(rect, target_page->surface);
        }

        for (auto *page : touched_pages)
        {
            if (!page || !upload_full_page(*page, gpu_ctx))
            {
                out.clear();
                return false;
            }
        }
        return true;
    }

    TextureID get_atlas_texture(u32 atlas_id)
    {
        if (!g_context) return AUIK_INVALID_TEXTURE_ID;
        auto *page = find_page(g_context->atlas_state, atlas_id);
        return page ? page->texture.texture_id : AUIK_INVALID_TEXTURE_ID;
    }
} // namespace auik::v2::detail
