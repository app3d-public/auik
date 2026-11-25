#include <auik/theme.hpp>
#include <stdio.h>
#include <stdlib.h>

namespace
{
    void check(bool condition, const char *message)
    {
        if (condition) return;
        fprintf(stderr, "%s\n", message);
        abort();
    }
}

void test_style_fallback()
{
    constexpr u32 style_tag = 0x4C10u;
    constexpr u32 text_color = 0xFF102030u;
    auik::Theme theme;
    theme.add_style(style_tag, auik::Style{}.text_color(text_color));

    const auto id = theme.get_resolved_style(style_tag, 0u, 0u);
    check(theme.get_style(id).text_color() == text_color, "Resolved style lost its text color");

    theme.set_var(0x4C11u, 1.f);
#ifdef NDEBUG
    const auto &empty = theme.get_style(id);
    check(!empty.mask() && !empty.font() && !empty.extra(), "Stale style ID must return an empty style");
    check(&empty == &theme.get_style(auik::Theme::STYLE_ID_INVALID), "Invalid ID must return the same empty style");
#endif

    const auto fresh_id = theme.get_resolved_style(style_tag, 0u, 0u);
    check(theme.get_style(fresh_id).text_color() == text_color, "Style resolution failed after clearing the cache");
}
