#pragma once

#include <limits>
#include "textbox.hpp"

#define AUIK_TAG_DRAG_INT    0x000EB95Cu
#define AUIK_TAG_DRAG_FLOAT  0x8054A4DAu
#define AUIK_TAG_DRAG_DOUBLE 0x73F0FEACu

namespace auik
{
    namespace detail
    {
        template <typename T>
        class Draggable : public Textbox
        {
        public:
            AUIK_EXPORT Draggable(u32 id, u32 tag_id, T value, T min_value, T max_value, f32 speed,
                                  amal::vec2 inline_size, WidgetFlags flags);

            T value() const { return _value; }
            AUIK_EXPORT void set_value(T value);
            T last_value() const { return _last_value; }
            bool is_manual_change() const { return _manual_change; }
            AUIK_EXPORT void set_model_binding(ModelBinding *binding);
            ModelBinding *model_binding() const { return _value_model_binding; }
            AUIK_EXPORT void set_postfix(StringView value);
            const acul::string &postfix() const { return _postfix; }
            AUIK_EXPORT acul::string input_postfix() const;
            void set_speed(f32 value) { _speed = value; }
            f32 speed() const { return _speed; }
            AUIK_EXPORT void set_limits(T min_value, T max_value);
            T min_value() const { return _min_value; }
            T max_value() const { return _max_value; }
            AUIK_EXPORT void update_layout(bool min_size_known) override;
            AUIK_EXPORT void on_focus(bool focused) override;
            AUIK_EXPORT void on_hover(HoverState state) override;
            AUIK_EXPORT void on_click(MouseKey key, KeyPressState state, u32 click_count) override;
            AUIK_EXPORT void on_drag(const amal::vec2 &delta, KeyPressState state) override;
            AUIK_EXPORT void on_key(Key key, KeyPressState state, KeyMode mods) override;
            AUIK_EXPORT void on_char_input(u32 char_code, u32 count) override;
            AUIK_EXPORT bool allows_unbounded_drag() const override;

        protected:
            AUIK_EXPORT void sync_text_from_value();
            AUIK_EXPORT bool commit_text_value();
            AUIK_EXPORT void step_value(f64 delta);
            AUIK_EXPORT acul::string format_value(T value) const;
            AUIK_EXPORT void sync_text_presentation_from_value();
            AUIK_EXPORT bool parse_text_value(T &out) const;
            AUIK_EXPORT bool should_draw_caret() const override;
            AUIK_EXPORT void set_value_internal(T value, bool manual_change, bool sync_model = true);

            T _value = T(0);
            ModelBinding *_value_model_binding = nullptr;
            T _min_value{};
            T _max_value{};
            f32 _speed = 1.0f;
            acul::string _postfix;
            f64 _drag_origin_value = 0.0;
            f64 _drag_delta_steps = 0.0;
            f64 _drag_value = 0.0;
            T _last_value{};
            T _presented_value{};
            u8 _interaction_flags = 0u;
            bool _pending_text_commit = false;
            bool _manual_change = false;
        };
    } // namespace detail

    class DragInt final : public detail::Draggable<int>
    {
    public:
        AUIK_EXPORT DragInt(u32 id, int value, int min_value, int max_value, f32 speed, amal::vec2 inline_size,
                            WidgetFlags flags);
        AUIK_EXPORT DragInt(u32 id, ModelBinding *binding, int min_value, int max_value, f32 speed,
                            amal::vec2 inline_size, WidgetFlags flags);

        virtual u32 signature() const override { return AUIK_TAG_DRAG_INT; }
    };

    class DragFloat final : public detail::Draggable<f32>
    {
    public:
        AUIK_EXPORT DragFloat(u32 id, f32 value, f32 min_value, f32 max_value, f32 speed, amal::vec2 inline_size,
                              WidgetFlags flags);
        AUIK_EXPORT DragFloat(u32 id, ModelBinding *binding, f32 min_value, f32 max_value, f32 speed,
                              amal::vec2 inline_size, WidgetFlags flags);

        virtual u32 signature() const override { return AUIK_TAG_DRAG_FLOAT; }
    };

    class DragDouble final : public detail::Draggable<f64>
    {
    public:
        AUIK_EXPORT DragDouble(u32 id, f64 value, f64 min_value, f64 max_value, f32 speed, amal::vec2 inline_size,
                               WidgetFlags flags);
        AUIK_EXPORT DragDouble(u32 id, ModelBinding *binding, f64 min_value, f64 max_value, f32 speed,
                               amal::vec2 inline_size, WidgetFlags flags);

        virtual u32 signature() const override { return AUIK_TAG_DRAG_DOUBLE; }
    };

    inline DragInt *make_drag_int(u32 id, int value, int min_value = std::numeric_limits<int>::lowest(),
                                  int max_value = std::numeric_limits<int>::max(), f32 speed = 1.0f,
                                  amal::vec2 inline_size = AUIK_SIZE_INHERIT)
    {
        return acul::alloc<DragInt>(id, value, min_value, max_value, speed, inline_size,
                                    WidgetFlagBits::visible | WidgetFlagBits::attachable |
                                        WidgetFlagBits::configurable);
    }

    inline DragInt *make_drag_int(u32 id, ModelBinding *binding, int min_value = std::numeric_limits<int>::lowest(),
                                  int max_value = std::numeric_limits<int>::max(), f32 speed = 1.0f,
                                  amal::vec2 inline_size = AUIK_SIZE_INHERIT)
    {
        return acul::alloc<DragInt>(id, binding, min_value, max_value, speed, inline_size,
                                    WidgetFlagBits::visible | WidgetFlagBits::attachable |
                                        WidgetFlagBits::configurable);
    }

    inline DragFloat *make_drag_float(u32 id, f32 value, f32 min_value = std::numeric_limits<f32>::lowest(),
                                      f32 max_value = std::numeric_limits<f32>::max(), f32 speed = 1.0f,
                                      amal::vec2 inline_size = AUIK_SIZE_INHERIT)
    {
        return acul::alloc<DragFloat>(id, value, min_value, max_value, speed, inline_size,
                                      WidgetFlagBits::visible | WidgetFlagBits::attachable |
                                          WidgetFlagBits::configurable);
    }

    inline DragFloat *make_drag_float(u32 id, ModelBinding *binding, f32 min_value = std::numeric_limits<f32>::lowest(),
                                      f32 max_value = std::numeric_limits<f32>::max(), f32 speed = 1.0f,
                                      amal::vec2 inline_size = AUIK_SIZE_INHERIT)
    {
        return acul::alloc<DragFloat>(id, binding, min_value, max_value, speed, inline_size,
                                      WidgetFlagBits::visible | WidgetFlagBits::attachable |
                                          WidgetFlagBits::configurable);
    }

    inline DragDouble *make_drag_double(u32 id, f64 value, f64 min_value = std::numeric_limits<f64>::lowest(),
                                        f64 max_value = std::numeric_limits<f64>::max(), f32 speed = 1.0f,
                                        amal::vec2 inline_size = AUIK_SIZE_INHERIT)
    {
        return acul::alloc<DragDouble>(id, value, min_value, max_value, speed, inline_size,
                                       WidgetFlagBits::visible | WidgetFlagBits::attachable |
                                           WidgetFlagBits::configurable);
    }

    inline DragDouble *make_drag_double(u32 id, ModelBinding *binding,
                                        f64 min_value = std::numeric_limits<f64>::lowest(),
                                        f64 max_value = std::numeric_limits<f64>::max(), f32 speed = 1.0f,
                                        amal::vec2 inline_size = AUIK_SIZE_INHERIT)
    {
        return acul::alloc<DragDouble>(id, binding, min_value, max_value, speed, inline_size,
                                       WidgetFlagBits::visible | WidgetFlagBits::attachable |
                                           WidgetFlagBits::configurable);
    }

    namespace streams
    {
        extern AUIK_EXPORT const umbf::streams::Stream drag_int;
        extern AUIK_EXPORT const umbf::streams::Stream drag_float;
        extern AUIK_EXPORT const umbf::streams::Stream drag_double;
    } // namespace streams
} // namespace auik
