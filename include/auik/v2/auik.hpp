#pragma once

#include <acul/disposal_queue.hpp>
#include <acul/event.hpp>
#include "detail/context.hpp"

namespace auik::v2
{
    struct CreateInfo
    {
        acul::events::dispatcher *ed = nullptr;
        acul::disposal_queue *disposal_queue = nullptr;
        void *gpu_backend = nullptr;
        u32 frames_in_flight = 0;
        acul::point2D<i32> window_size{0, 0};

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

        CreateInfo &set_gpu_backend(void *gpu_backend)
        {
            this->gpu_backend = gpu_backend;
            return *this;
        }

        CreateInfo &set_frames_in_flight(u32 frames_in_flight)
        {
            this->frames_in_flight = frames_in_flight;
            return *this;
        }

        CreateInfo &set_window_size(acul::point2D<i32> window_size)
        {
            this->window_size = window_size;
            return *this;
        }
    };

    APPLIB_API void init_library(const CreateInfo &create_info);
    APPLIB_API void destroy_library();
} // namespace auik::v2
