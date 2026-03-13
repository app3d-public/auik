#pragma once

#include <acul/enum.hpp>
#include <acul/scalars.hpp>

namespace auik::v2
{
    struct PendingMaskBits
    {
        enum enum_type : u8
        {
            none = 0x0,
            disallow = 0x1,
            resize = 0x2,
            mouse_move = 0x4
        };
        using flag_bitmask = std::true_type;
    };
    using PendingMask = acul::flags<PendingMaskBits>;

    struct PendingFilter
    {
        PendingMask mask = PendingMaskBits::none;

        inline void sync(f64 now)
        {
            if (_min_interval <= 0.0)
            {
                mask &= ~PendingMaskBits::disallow;
                return;
            }
            if (_last_sync_time >= 0.0 && (now - _last_sync_time) < _min_interval)
            {
                mask |= PendingMaskBits::disallow;
                return;
            }
            mask &= ~PendingMaskBits::disallow;
            _last_sync_time = now;
        }

        inline bool allow() const { return !(mask & PendingMaskBits::disallow); }

        inline bool has(PendingMaskBits::enum_type bit) const { return (mask & bit) != 0; }

        inline void set(PendingMaskBits::enum_type bit) { mask |= bit; }

        inline void reset() { mask = PendingMaskBits::none; }

        inline void set_frame_rate(f64 frame_rate) { _min_interval = 1.0 / frame_rate; }

    private:
        f64 _last_sync_time = -1.0;
        f64 _min_interval = 60.0;
    };
} // namespace auik::v2
