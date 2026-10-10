#include <auik/animation.hpp>
#include <auik/auik.hpp>
#include <cassert>
#include <cmath>

namespace
{
    struct AnimationWidget : auik::Widget
    {
        AnimationWidget() : Widget(9700, auik::WidgetFlagBits::visible) {}
        auik::StyleUpdateFlags update_style() override { return auik::StyleUpdateFlagBits::none; }
        void draw(auik::DrawCtx &) override {}
    };

    bool close(f32 a, f32 b) { return std::abs(a - b) < 0.001f; }
}

void test_bounds_animation()
{
    auik::detail::Context ctx{};
    auik::detail::WindowContext window{};
    auik::Viewport viewport{};
    ctx.window_ctx = &window;
    ctx.main_viewport = &viewport;
    auik::detail::g_context = &ctx;
    {
        AnimationWidget widget;
        widget.set_position({10, 20});
        widget.set_layout_size({100, 3});
        auik::animate_move(widget, {110, 20}, 1.0);
        auik::animate_scale_to(widget, {200, 3}, 1.0);
        assert(ctx.delayed_tasks.size() == 2);
        window.time = 0.5;
        auik::dispatch_delayed_tasks(window.time);
        assert(close(widget.position().x, 85));
        assert(close(widget.size().x, 175));
        assert(widget.size().y == 3);

        // Retarget move from its visible position without cancelling scale.
        auik::animate_move(widget, {185, 20}, 1.0);
        window.time = 1.0;
        auik::dispatch_delayed_tasks(window.time);
        assert(close(widget.position().x, 160));
        assert(close(widget.size().x, 200));
        window.time = 1.5;
        auik::dispatch_delayed_tasks(window.time);
        assert(close(widget.position().x, 185));
        assert(ctx.delayed_tasks.empty());

        auik::animate_move(widget, {300, 20}, 1.0);
        auik::animate_scale_to(widget, {400, 3}, 1.0);
        auik::cancel_bounds_animations(widget);
        assert(ctx.delayed_tasks.empty());
        assert(close(widget.position().x, 185));
        assert(close(widget.size().x, 200));
        auik::animate_move(widget, {50, 20}, 0.0);
        auik::animate_scale_to(widget, {70, 3}, 0.0);
        assert(widget.position().x == 50 && widget.size().x == 70);

        // Destruction owns cancellation of both independent scheduled tasks.
        auik::animate_move(widget, {80, 20}, 1.0);
        auik::animate_scale_to(widget, {90, 3}, 1.0);
    }
    assert(ctx.delayed_tasks.empty());
    auik::detail::g_context = nullptr;
}
