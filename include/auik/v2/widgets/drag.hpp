#pragma once

#include <limits>
#include "textbox.hpp"

namespace auik::v2
{
    struct DragUnit
    {
        const char *name = nullptr;
        f64 to_base = 1.0;
    };

    struct DragUnitResolver
    {
        const DragUnit *units = nullptr;
        u32 unit_count = 0;
        const char *default_unit = nullptr;
    };

    namespace detail
    {
        template <typename T>
        class APPLIB_API Draggable : public TextBox
        {
        public:
            Draggable(u32 id, T *value, T min_value, T max_value, f32 speed, amal::vec2 size, WidgetFlags flags,
                      Widget *parent = nullptr, const DragUnitResolver *unit_resolver = nullptr,
                      const char *default_unit = nullptr);

            T value() const { return _value ? *_value : _fallback_value; }
            void set_value(T value);
            void set_unit_resolver(const DragUnitResolver *resolver, const char *default_unit = nullptr);
            const DragUnitResolver *unit_resolver() const { return _unit_resolver; }
            const char *default_unit() const { return _default_unit; }
            void set_speed(f32 value) { _speed = value; }
            f32 speed() const { return _speed; }
            void set_limits(T min_value, T max_value);
            T min_value() const { return _min_value; }
            T max_value() const { return _max_value; }

            void update_layout(bool min_size_known) override;
            void on_focus(bool focused) override;
            void on_hover(HoverState state) override;
            void on_click(MouseKey key, KeyPressState state, u32 click_count) override;
            void on_drag(const amal::vec2 &delta, KeyPressState state) override;
            void on_key(Key key, KeyPressState state, KeyMode mods) override;
            void on_char_input(u32 char_code, u32 count) override;

        protected:
            void sync_text_from_value();
            bool commit_text_value();
            void step_value(f64 delta);
            acul::string format_value(T value) const;
            bool parse_text_value(T &out) const;
            const char *resolve_default_unit() const;
            bool should_draw_caret() const override;

            T *_value = nullptr;
            T _fallback_value{};
            T _min_value{};
            T _max_value{};
            f32 _speed = 1.0f;
            const DragUnitResolver *_unit_resolver = nullptr;
            const char *_default_unit = nullptr;
            f64 _drag_origin_value = 0.0;
            f64 _drag_delta_steps = 0.0;
            f64 _drag_value = 0.0;
            T _last_value{};
            u8 _interaction_flags = 0u;
        };
    } // namespace detail

    class APPLIB_API DragInt final : public detail::Draggable<int>
    {
    public:
        DragInt(u32 id, int *value, int min_value = std::numeric_limits<int>::lowest(),
                int max_value = std::numeric_limits<int>::max(), f32 speed = 1.0f,
                amal::vec2 size = {0.0f, 0.0f}, WidgetFlags flags = get_default_textbox_flags(),
                Widget *parent = nullptr, const DragUnitResolver *unit_resolver = nullptr,
                const char *default_unit = nullptr);
    };

    class APPLIB_API DragFloat final : public detail::Draggable<f32>
    {
    public:
        DragFloat(u32 id, f32 *value, f32 min_value = std::numeric_limits<f32>::lowest(),
                  f32 max_value = std::numeric_limits<f32>::max(), f32 speed = 1.0f,
                  amal::vec2 size = {0.0f, 0.0f}, WidgetFlags flags = get_default_textbox_flags(),
                  Widget *parent = nullptr, const DragUnitResolver *unit_resolver = nullptr,
                  const char *default_unit = nullptr);
    };

    class APPLIB_API DragDouble final : public detail::Draggable<f64>
    {
    public:
        DragDouble(u32 id, f64 *value, f64 min_value = std::numeric_limits<f64>::lowest(),
                   f64 max_value = std::numeric_limits<f64>::max(), f32 speed = 1.0f,
                   amal::vec2 size = {0.0f, 0.0f}, WidgetFlags flags = get_default_textbox_flags(),
                   Widget *parent = nullptr, const DragUnitResolver *unit_resolver = nullptr,
                   const char *default_unit = nullptr);
    };

    inline DragInt *make_drag_int(u32 id, int *value, int min_value = std::numeric_limits<int>::lowest(),
                                  int max_value = std::numeric_limits<int>::max(), f32 speed = 1.0f,
                                  const DragUnitResolver *unit_resolver = nullptr, const char *default_unit = nullptr)
    {
        return acul::alloc<DragInt>(id, value, min_value, max_value, speed, amal::vec2{0.0f, 0.0f},
                                    WidgetFlagBits::visible | WidgetFlagBits::attachable |
                                        WidgetFlagBits::configurable,
                                    nullptr, unit_resolver, default_unit);
    }

    inline DragFloat *make_drag_float(u32 id, f32 *value, f32 min_value = std::numeric_limits<f32>::lowest(),
                                      f32 max_value = std::numeric_limits<f32>::max(), f32 speed = 1.0f,
                                      const DragUnitResolver *unit_resolver = nullptr,
                                      const char *default_unit = nullptr)
    {
        return acul::alloc<DragFloat>(id, value, min_value, max_value, speed, amal::vec2{0.0f, 0.0f},
                                      WidgetFlagBits::visible | WidgetFlagBits::attachable |
                                          WidgetFlagBits::configurable,
                                      nullptr, unit_resolver, default_unit);
    }

    inline DragDouble *make_drag_double(u32 id, f64 *value, f64 min_value = std::numeric_limits<f64>::lowest(),
                                        f64 max_value = std::numeric_limits<f64>::max(), f32 speed = 1.0f,
                                        const DragUnitResolver *unit_resolver = nullptr,
                                        const char *default_unit = nullptr)
    {
        return acul::alloc<DragDouble>(id, value, min_value, max_value, speed, amal::vec2{0.0f, 0.0f},
                                       WidgetFlagBits::visible | WidgetFlagBits::attachable |
                                           WidgetFlagBits::configurable,
                                       nullptr, unit_resolver, default_unit);
    }

    inline DragInt *make_fixed_drag_int(u32 id, int *value, amal::vec2 size = {120.0f, 0.0f},
                                        int min_value = std::numeric_limits<int>::lowest(),
                                        int max_value = std::numeric_limits<int>::max(), f32 speed = 1.0f,
                                        const DragUnitResolver *unit_resolver = nullptr,
                                        const char *default_unit = nullptr)
    {
        return acul::alloc<DragInt>(id, value, min_value, max_value, speed, size,
                                    WidgetFlagBits::visible | WidgetFlagBits::attachable |
                                        WidgetFlagBits::configurable | WidgetFlagBits::fixed,
                                    nullptr, unit_resolver, default_unit);
    }

    inline DragFloat *make_fixed_drag_float(u32 id, f32 *value, amal::vec2 size = {120.0f, 0.0f},
                                            f32 min_value = std::numeric_limits<f32>::lowest(),
                                            f32 max_value = std::numeric_limits<f32>::max(), f32 speed = 1.0f,
                                            const DragUnitResolver *unit_resolver = nullptr,
                                            const char *default_unit = nullptr)
    {
        return acul::alloc<DragFloat>(id, value, min_value, max_value, speed, size,
                                      WidgetFlagBits::visible | WidgetFlagBits::attachable |
                                          WidgetFlagBits::configurable | WidgetFlagBits::fixed,
                                      nullptr, unit_resolver, default_unit);
    }

    inline DragDouble *make_fixed_drag_double(u32 id, f64 *value, amal::vec2 size = {120.0f, 0.0f},
                                              f64 min_value = std::numeric_limits<f64>::lowest(),
                                              f64 max_value = std::numeric_limits<f64>::max(), f32 speed = 1.0f,
                                              const DragUnitResolver *unit_resolver = nullptr,
                                              const char *default_unit = nullptr)
    {
        return acul::alloc<DragDouble>(id, value, min_value, max_value, speed, size,
                                       WidgetFlagBits::visible | WidgetFlagBits::attachable |
                                           WidgetFlagBits::configurable | WidgetFlagBits::fixed,
                                       nullptr, unit_resolver, default_unit);
    }
} // namespace auik::v2
