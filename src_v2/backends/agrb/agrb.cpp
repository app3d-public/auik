#include <auik/v2/detail/gpu_dispatch.hpp>

namespace auik::v2
{
    namespace detail
    {
        void init_quads_pipeline_calls(QuadsGPUDispatch &dispatch);
    }

    APPLIB_API void init_agrb_dispatcher() { detail::init_quads_pipeline_calls(detail::g_gpu_dispatch.quads); }
} // namespace auik::v2