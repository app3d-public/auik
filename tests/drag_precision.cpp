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
        bool parse(const char *text, T &value)
        {
            this->Textbox::set_value(text);
            return this->parse_text_value(value);
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
    {
        auik::Theme large, normal, padded;
        large.add_style(AUIK_STYLE_TAG_TEXTBOX, auik::make_style().text_size(15.f).padding({10, 8, 10, 8}));
        normal.add_style(AUIK_STYLE_TAG_TEXTBOX, auik::make_style().text_size(12.f).padding({8, 6, 8, 6}));
        ctx.theme = &large;
        auto *rescaled = auik::make_drag_float(9810, 10.f);
        auto measure = [](auik::DragFloat *field) {
            field->invalidate_style();
            field->update_style_invalidated();
            field->invalidate_layout_measure();
            field->update_layout_min_size();
            return field->required_size().y;
        };
        const float previous = measure(rescaled);
        rescaled->set_layout_size({120.f, previous});
        ctx.theme = &normal;
        auto *fresh = auik::make_drag_float(9811, 10.f);
        const float expected = measure(fresh);
        assert(previous > expected);
        assert(measure(rescaled) == expected);
        padded.add_style(AUIK_STYLE_TAG_TEXTBOX,
                         auik::make_style().height(22.f).text_size(12.f).padding({6, 15, 6, 15}));
        ctx.theme = &padded;
        assert(measure(rescaled) == 42.f);
        acul::release(fresh);
        acul::release(rescaled);
        ctx.theme = nullptr;
    }
    check_precision<f32>();
    check_precision<f64>();
    {
        FormattingDrag<f64> drag;
        f64 value;
        assert(drag.parse("10*2", value) && value == 20);
        assert(drag.parse("-(1 + 2*3) / .5", value) && value == -14);
        assert(drag.parse("1e-3", value) && value == 0.001);
        assert(!drag.parse("3 / 0", value));
        assert(!drag.parse("10 mm", value));
        assert(!drag.parse("(10*2", value));
        assert(!drag.parse("10*2 mm", value));
    }
    ctx.io.shortcut_model_db = nullptr;
    auik::detail::g_context = nullptr;
}
