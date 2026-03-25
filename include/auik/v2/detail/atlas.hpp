#pragma once

#include <umbf/utils.hpp>
#include "fwd.hpp"
#include "gpu_context.hpp"

namespace auik::v2::detail
{
    struct AtlasAllocation
    {
        u32 atlas_id = AUIK_INVALID_DRAW_DATA_ID;
        TextureID texture_id = AUIK_INVALID_TEXTURE_ID;
        amal::irect pixel_rect{};
        amal::rect uv_rect{};

        bool valid() const
        {
            return atlas_id != AUIK_INVALID_DRAW_DATA_ID && texture_id.handle != 0 && !amal::is_rect_empty(pixel_rect);
        }
    };

    struct AtlasPage
    {
        u32 atlas_id = AUIK_INVALID_DRAW_DATA_ID;
        AtlasTextureResource texture{};
        umbf::Image2D surface{};
        umbf::utils::SkylinePacker packer{};
        acul::vector<amal::irect> rects;
    };

    struct AtlasState
    {
        acul::vector<AtlasPage *> pages;
        amal::ivec2 initial_size{256, 256};
        amal::ivec2 max_size{2048, 2048};
        i32 padding = 1;
        u32 next_atlas_id = 0;
    };

    bool init_atlas_state(AtlasState &state);
    void destroy_atlas_state(AtlasState &state);
    bool allocate_atlas_region(const umbf::Image2D &image, AtlasAllocation &out);
    bool allocate_atlas_regions(const acul::vector<umbf::Image2D> &images, acul::vector<AtlasAllocation> &out);
    TextureID get_atlas_texture(u32 atlas_id);
} // namespace auik::v2::detail
