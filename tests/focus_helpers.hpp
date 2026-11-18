#pragma once
#include <auik/auik.hpp>

namespace auik_test
{
    inline bool is_in_focus_chain(const auik::Widget *widget)
    {
        if (!widget) return false;
        const auto &ctx = auik::detail::get_context();
        const auto it = ctx.id_map.find(ctx.focus_id);
        if (it == ctx.id_map.end()) return false;
        for (auto *node = it->second; node; node = node->focus_parent())
            if (node == widget) return true;
        return false;
    }
}
