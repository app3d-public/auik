#pragma once

#include <agrb/descriptors.hpp>
#include <agrb/framebuffer.hpp>
#include <agrb/pipeline.hpp>
#include <agrb/vector.hpp>
#include <amal/vector.hpp>
#include <auik/v2/detail/events.hpp>
#include <auik/v2/detail/fwd.hpp>

namespace auik::v2::detail
{
    struct AgrbContext;

    class GPUPicker final : public agrb::framebuffer
    {
    public:
        GPUPicker(agrb::device &device)
        {
            acul::vector<vk::Format> depth_candidates(
                {vk::Format::eX8D24UnormPack32, vk::Format::eD32Sfloat, vk::Format::eD16Unorm});
            _depth_format = device.find_supported_format(depth_candidates, vk::ImageTiling::eOptimal,
                                                         vk::FormatFeatureFlagBits::eDepthStencilAttachment);
        }

        bool prepare(AgrbContext *context);

        void destroy(agrb::device &device);
        bool construct_pipeline(agrb::device &device, DrawPipeline &pipeline);
        bool configure_pipeline(AgrbContext *ctx, agrb::graphics_pipeline_batch::artifact &, DrawPipeline &);

        void render(AgrbContext *ctx);
        void pick(AgrbContext *ctx, u32 read_frame_id);
        u32 push_hover_rect(const RectData &rect);
        void update_hover_rect(u32 id, const RectData &rect);
        void clear_hover_rects();

    private:
        struct PickValue
        {
            u32 widget_id = 0;
            u32 tag_id = 0;
        };

        DrawPipeline *_pipeline = nullptr;
        vk::Format _depth_format = vk::Format::eUndefined;
        agrb::vector<RectData> _rects;
        acul::shared_ptr<agrb::descriptor_set_layout> _descriptor_set_layout = nullptr;
        acul::vector<vk::DescriptorSet> _descriptor_sets;
        vk::Buffer _descriptor_buffer_instances = nullptr;
        vk::Buffer _descriptor_buffer_clip_rects = nullptr;
        acul::vector<vk::CommandBuffer> _command_buffers;
        acul::vector<agrb::managed_buffer> _readback_buffers;
        acul::vector<vk::Fence> _submit_fences;

        void create_render_pass(agrb::device &device);
        bool create_attachments(agrb::device &device);
        bool create_descriptor_resources(agrb::device &device);
        bool create_readback_resources(agrb::device &device);
        bool update_descriptors(AgrbContext *ctx);
    };

    void update_hover_id_impl(GPUContext *);
} // namespace auik::v2::detail
