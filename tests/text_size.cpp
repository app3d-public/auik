#include <auik/auik.hpp>
#include <auik/widgets/text.hpp>
#include <cassert>
#include <cstdio>

void test_text_size()
{
    auik::detail::Context ctx{};
    auik::Viewport viewport{};
    ctx.main_viewport = &viewport;
    auik::detail::g_context = &ctx;

    auik::Style style;
    style.width(120.0f).height(18.0f);

    auto *inherited = auik::make_text(9500, "label");
    assert(auik::is_size_inherit(inherited->inline_size().x));
    assert(auik::is_size_inherit(inherited->inline_size().y));
    inherited->apply_style_layout(style);
    assert(inherited->requested_size() == (amal::vec2{120.0f, 18.0f}));

    auto *explicit_size = auik::make_text(9501, "label", {42.0f, AUIK_SIZE_Y_INHERIT});
    explicit_size->apply_style_layout(style);
    assert(explicit_size->requested_size() == (amal::vec2{42.0f, 18.0f}));

    style.width(160.0f);
    inherited->apply_style_layout(style);
    explicit_size->apply_style_layout(style);
    assert(inherited->requested_size().x == 160.0f);
    assert(explicit_size->requested_size().x == 42.0f);

    auto *clone = inherited->clone(9502);
    assert(auik::is_size_inherit(clone->inline_size().x));
    clone->apply_style_layout(style.width(96.0f));
    assert(clone->requested_size().x == 96.0f);

    acul::release(clone);
    acul::release(explicit_size);
    acul::release(inherited);
    auik::detail::g_context = nullptr;
    puts("PASS: text inherits style size unless inline size is explicit");
}
