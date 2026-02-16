#pragma once

#include <acul/disposal_queue.hpp>
#include <acul/enum.hpp>
#include <acul/event.hpp>
#include <vulkan/vulkan.h>

namespace auik::v2
{
    struct WidgetFlagBits
    {
        enum enum_type
        {
            none = 0x0,
            visible = 0x1,
            active = 0x2,
            hovered = 0x4,
            configurable = 0x8
        };
        using flag_bitmask = std::true_type;
    };

    using WidgetFlags = acul::flags<WidgetFlagBits>;

    class Widget
    {
    public:
        WidgetFlags widget_flags;

        Widget(u32 id, WidgetFlags flags, Widget *parent = nullptr) : widget_flags(flags), _id(id), _parent(parent) {}

        u32 id() const { return _id; }

        virtual void render() {}

    protected:
        u32 _id;
        Widget *_parent = nullptr;
    };

    struct DrawPipeline
    {
        VkPipeline handle;
        VkPipelineLayout layout;
    };

    struct DrawStream
    {
        // Callbacks
        void (*push_data_to_stream)(DrawStream *, void *) = nullptr;
        void (*clear_stream)(DrawStream *, VkCommandBuffer) = nullptr;
        void (*render)(DrawStream *, VkCommandBuffer) = nullptr;
        void (*next_call)(DrawStream *, VkCommandBuffer) = nullptr;
        void (*destroy)(DrawStream *) = nullptr;

        // Data
        void **stream_instances = nullptr;
        DrawPipeline *pipeline;
        u32 *sizes;
        u32 write_id = 0;
    };

    inline void push_data_to_stream(DrawStream *stream, void *data) { stream->push_data_to_stream(stream, data); }

    inline void clear_stream(DrawStream *stream)
    {
        stream->clear_stream(stream, nullptr);
        stream->next_call = stream->render;
    }

    inline void render_stream(DrawStream *stream, VkCommandBuffer cmd)
    {
        stream->render(stream, cmd);
        stream->next_call = stream->clear_stream;
    }

    inline void next_stream_call(DrawStream *stream, VkCommandBuffer cmd) { stream->next_call(stream, cmd); }

    struct DrawLayer
    {
        u32 subpass;
        DrawStream *streams;
        u32 stream_count = 0;
    };

    namespace detail
    {
        extern APPLIB_API struct Context
        {
            DrawLayer *layers = nullptr;
            u32 layer_count = 0;
            acul::events::dispatcher *ed = nullptr;
            acul::disposal_queue *disposal_queue = nullptr;
            acul::vector<Widget *> widget_tree;
            void *gpu_backend = nullptr;
            acul::point2D<u32> window_size;
            u32 frame_id = 0;
            u32 frames_in_flight = 0;
        } *g_context;
    }; // namespace detail

    inline detail::Context &get_context()
    {
        assert(detail::g_context && "auik context is not initialized");
        return *detail::g_context;
    }

    inline void next_frame_id()
    {
        auto &ctx = get_context();
        ctx.frame_id = (ctx.frame_id + 1) % ctx.frames_in_flight;
    }

    struct CreateInfo
    {
        acul::events::dispatcher *ed = nullptr;
        acul::disposal_queue *disposal_queue = nullptr;
        u32 layer_count = 0;
        DrawLayer *layers = nullptr;
        void *gpu_backend = nullptr;

        CreateInfo &set_ed(acul::events::dispatcher *ed)
        {
            this->ed = ed;
            return *this;
        }

        CreateInfo &set_disposal_queue(acul::disposal_queue *disposal_queue)
        {
            this->disposal_queue = disposal_queue;
            return *this;
        }

        CreateInfo &set_layers(DrawLayer *layers, u32 layer_count)
        {
            this->layers = layers;
            this->layer_count = layer_count;
            return *this;
        }

        CreateInfo &set_gpu_backend(void *gpu_backend)
        {
            this->gpu_backend = gpu_backend;
            return *this;
        }
    };

    inline void init_library(const CreateInfo &create_info)
    {
        detail::g_context = acul::alloc<detail::Context>();
        detail::g_context->ed = create_info.ed;
        detail::g_context->disposal_queue = create_info.disposal_queue;
        detail::g_context->layer_count = create_info.layer_count;
        detail::g_context->layers = create_info.layers;
    }

    inline void render_layer(const DrawLayer &layer, VkCommandBuffer cmd, u32 frame_id)
    {
        for (u32 stream_id = 0; stream_id < layer.stream_count; stream_id++)
        {
            auto &stream = layer.streams[stream_id];
            if (stream.sizes[frame_id] > 0) next_stream_call(&stream, cmd);
        }
    }

    inline void render_all_layers(VkCommandBuffer cmd)
    {
        auto &ctx = get_context();
        for (u32 layer_id = 0; layer_id < ctx.layer_count; layer_id++)
            render_layer(ctx.layers[layer_id], cmd, ctx.frame_id);
        next_frame_id(); // todo: replace to gpu update flag check
    };
} // namespace auik::v2