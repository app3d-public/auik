#pragma once

#include <agrb/device.hpp>
#include "../detail/context.hpp"
#ifdef AUIK_BACKEND_AGRB_ADOPTED
    #include <agrb/device_adapter.hpp>
#endif

namespace auik::v2
{
    inline void *init_agrb_backend(agrb::device &device)
    {
        detail::get_context().gpu_backend = &device;
        return &device;
    }

    inline agrb::device &get_agrb_device(void *gpu_backend)
    {
        auto *device = static_cast<agrb::device *>(gpu_backend);
        assert(device && "gpu backend is not initialized");
        return *device;
    }

    struct DrawPipeline
    {
        vk::Pipeline handle;
        vk::PipelineLayout layout;
    };

    APPLIB_API void init_agrb_dispatcher();

#ifdef AUIK_BACKEND_AGRB_ADOPTED
    static agrb::adopted_device_create_info &
    setup_agrb_adopted_device_create_info(agrb::adopted_device_create_info &create_info)
    {
        create_info.set_create_command_pools(true)
            .set_graphics_command_buffers(5, 10)
            .set_create_fence_pool(true)
            .set_fence_pool_size(1);
        return create_info;
    }

    inline bool initialize_agrb_adopted_device(agrb::device &device, agrb::adopted_device_create_info &create_info)
    {
        setup_agrb_adopted_device_create_info(create_info);
        agrb::initialize_adopted_device(device, create_info);
        if (!device.allocator) return agrb::create_adopted_allocator(device);
        return true;
    }
#endif
} // namespace auik::v2
