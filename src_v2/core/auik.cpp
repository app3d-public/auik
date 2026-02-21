#include <auik/v2/auik.hpp>
#include <auik/v2/detail/context.hpp>
#include <auik/v2/detail/gpu_dispatch.hpp>

namespace auik::v2
{
    namespace detail
    {
        Context *g_context = nullptr;
        GPUDispatch g_gpu_dispatch;
    }
    void init_library(const CreateInfo &create_info)
    {
        if (detail::g_context) destroy_library();
        detail::g_context = acul::alloc<detail::Context>();
        auto &ctx = detail::get_context();
        ctx.ed = create_info.ed;
        ctx.disposal_queue = create_info.disposal_queue;
        ctx.gpu_backend = create_info.gpu_backend;
        ctx.frames_in_flight = create_info.frames_in_flight;
        ctx.window_size = create_info.window_size;
    }

    void destroy_library()
    {
        if (!detail::g_context) return;
        acul::release(detail::g_context);
        detail::g_context = nullptr;
    }
} // namespace auik::v2
