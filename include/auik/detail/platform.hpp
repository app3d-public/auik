#pragma once

#ifdef _WIN32
    #include <windows.h>
#endif

namespace auik::detail
{
    inline bool is_win_11_or_greater()
    {
#ifdef _WIN32
        OSVERSIONINFOEX osvi{};
        osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
        GetVersionEx(reinterpret_cast<OSVERSIONINFO *>(&osvi));
        return osvi.dwBuildNumber >= 22000;
#else
        return false;
#endif
    }
} // namespace auik::detail
