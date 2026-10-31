#include <auik/auik.hpp>
#include <auik/widgets/drag.hpp>
#include <cassert>

namespace
{
    template <typename T>
    struct FormattingDrag : auik::detail::Draggable<T>
    {
        FormattingDrag() : auik::detail::Draggable<T>(9800, AUIK_TAG_DRAG_FLOAT, T(0), T(-1000), T(1000),
                                                     1.0f, {}, auik::WidgetFlagBits::visible) {}
        acul::string format(T value, u8 precision)
        {
            this->_display_precision = precision;
            return this->format_value(value);
        }
    };

    template <typename T>
    void check_precision()
    {
        FormattingDrag<T> drag;
        assert(drag.format(T(100), 0) == "100");
        assert(drag.format(T(-100), 0) == "-100");
        assert(drag.format(T(10), 0) == "10");
        assert(drag.format(T(0), 0) == "0");
        assert(drag.format(T(100), 2) == "100");
        assert(drag.format(T(10.5), 2) == "10.5");
        assert(drag.format(T(1.25), 2) == "1.25");
    }
}

void test_drag_precision()
{
    auik::detail::Context ctx{};
    auik::detail::WindowContext window{};
    auik::Viewport viewport{};
    ctx.window_ctx = &window;
    ctx.main_viewport = &viewport;
    auik::detail::g_context = &ctx;
    auik::ModelDB shortcuts;
    assert(auik::setup_shortcut_model(&shortcuts));
    check_precision<f32>();
    check_precision<f64>();
    ctx.io.shortcut_model_db = nullptr;
    auik::detail::g_context = nullptr;
}
