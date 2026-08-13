#include <acul/string/utils.hpp>
#include <auik/auik.hpp>
#include <auik/widgets/drag.hpp>
#include "../core/session_stream_utils.hpp"

namespace auik
{
    static f64 round_float_noise(f64 value)
    {
        if (!std::isfinite(value)) return value;
        const f64 scale = 100000.0;
        value = std::round(value * scale) / scale;
        return value == 0.0 ? 0.0 : value;
    }

    static acul::string format_display_float(f64 value)
    {
        acul::string out = acul::format("%.5f", value);
        while (!out.empty() && out.back() == '0') out.pop_back();
        if (!out.empty() && out.back() == '.') out.pop_back();
        return out.empty() || out == "-0" ? acul::string("0") : out;
    }

    template <typename T>
    static T clamp_value(T value, T min_value, T max_value)
    {
        if (min_value > max_value) std::swap(min_value, max_value);
        return amal::clamp(value, min_value, max_value);
    }

    template <typename T>
    static T cast_drag_value(f64 value)
    {
        if constexpr (std::is_integral_v<T>) return static_cast<T>(std::llround(value));
        else return static_cast<T>(value);
    }

    template <typename T>
    static T cast_relative_stepped_drag_value(f64 origin, f64 step_count, f64 speed)
    {
        if constexpr (std::is_integral_v<T>) return cast_drag_value<T>(origin + step_count * speed);
        else
        {
            const f64 step = amal::abs(speed);
            f64 value = origin + step_count * speed;
            if (step > 0.0 && std::isfinite(step))
            {
                const f64 offset = origin - amal::trunc(origin / step) * step;
                const f64 snapped = offset + amal::round((value - offset) / step) * step;
                if (amal::abs(snapped) < step * 1e-6) value = 0.0;
                else value = snapped;
            }
            return cast_drag_value<T>(value);
        }
    }

    namespace detail
    {
        enum DragInteractionFlagBits : u8
        {
            drag_interaction_drag_active = 1u << 0u,
            drag_interaction_select_all_on_next_focus = 1u << 1u,
            drag_interaction_select_all_on_release = 1u << 2u,
            drag_interaction_text_edit_mode = 1u << 3u,
            drag_interaction_edit_text_on_release = 1u << 4u,
        };

        static inline bool has_drag_interaction_flag(u8 flags, DragInteractionFlagBits bit)
        {
            return (flags & bit) != 0u;
        }

        static inline void set_drag_interaction_flag(u8 &flags, DragInteractionFlagBits bit) { flags |= bit; }

        static inline void clear_drag_interaction_flag(u8 &flags, DragInteractionFlagBits bit)
        {
            flags &= static_cast<u8>(~bit);
        }

        static inline void clear_drag_interaction_flags(u8 &flags, u8 bits) { flags &= static_cast<u8>(~bits); }

        template <typename T>
        static inline TextFlags drag_text_flags()
        {
            if constexpr (std::is_integral_v<T>)
                return TextFlagBits::chars_decimal | TextFlagBits::chars_no_blank | TextFlagBits::chars_ascii;
            else return TextFlagBits::chars_scientific | TextFlagBits::chars_ascii;
        }

        template <typename T>
        Draggable<T>::Draggable(u32 id, u32 tag_id, T value, T min_value, T max_value, f32 speed,
                                amal::vec2 inline_size, WidgetFlags flags)
            : Textbox(id, "", inline_size, flags, AUIK_STYLE_TAG_TEXTBOX, drag_text_flags<T>()),
              _value(value),
              _min_value(min_value),
              _max_value(max_value),
              _speed(speed),
              _last_value(value),
              _presented_value(value)
        {
            set_rect_tag_id(tag_id);
            set_drag_interaction_flag(_interaction_flags, drag_interaction_select_all_on_next_focus);
            sync_text_from_value();
        }

        template <typename T>
        void Draggable<T>::set_value(T value)
        {
            set_value_internal(value, false);
        }

        template <typename T>
        void Draggable<T>::set_value_internal(T value, bool manual_change, bool sync_model)
        {
            value = clamp_value(value, _min_value, _max_value);
            const T prev_value = this->value();
            if (prev_value == value && !manual_change) return;
            _last_value = prev_value;
            _value = value;
            _manual_change = manual_change;
            if (sync_model && _value_model_binding) set_model_binding_value<T>(*_value_model_binding, value);
            const bool prevented = mark_changed();
            sync_text_from_value();
            // A model-bound draggable reads its initial value before it is attached to the widget tree. Preserve the
            // value and text presentation, but defer layout/draw work until Textbox::update_style() has resolved both
            // the textbox and child Text styles.
            if (!prevented && _style.id != Theme::STYLE_ID_INVALID) apply_render_update(true);
        }

        template <typename T>
        void Draggable<T>::set_model_binding(ModelBinding *binding)
        {
            if (_value_model_binding) _value_model_binding->on_field_change = nullptr;
            _value_model_binding = binding;
            if (!_value_model_binding) return;

            _value_model_binding->on_field_change = [this](ModelRecordID, ModelFieldID) {
                T value{};
                if (read_model_binding_value(*_value_model_binding, value)) set_value_internal(value, false, false);
            };
            attach_model_binding(*_value_model_binding);
            T value{};
            if (read_model_binding_value(*_value_model_binding, value)) set_value_internal(value, false, false);
        }

        template <typename T>
        void Draggable<T>::set_postfix(StringView value)
        {
            _postfix = value.is_translated ? translate_string(value) : acul::string(value.str ? value.str : "");
            // Numeric-only filtering would make the displayed postfix impossible to enter manually. Once a
            // draggable has a postfix, keep the input ASCII-only and let the integration validate or convert the
            // suffix from input_postfix() on commit.
            text_flags = _postfix.empty() ? drag_text_flags<T>() : TextFlagBits::chars_ascii;
            sync_text_presentation_from_value();
        }

        template <typename T>
        acul::string Draggable<T>::input_postfix() const
        {
            const acul::string text = acul::trim(Textbox::value());
            if (text.empty()) return {};

            const char *ptr = text.c_str();
            f32 parsed = 0.0f;
            if (!acul::stof(ptr, parsed)) return {};
            return acul::trim(acul::string(ptr));
        }

        template <typename T>
        void Draggable<T>::set_limits(T min_value, T max_value)
        {
            _min_value = min_value;
            _max_value = max_value;
            set_value(value());
        }

        template <typename T>
        void Draggable<T>::update_layout(bool min_size_known)
        {
            if (value() != _presented_value) sync_text_from_value();
            Textbox::update_layout(min_size_known);
        }

        template <typename T>
        void Draggable<T>::on_focus(bool focused)
        {
            if (!focused && _pending_text_commit)
            {
                if (!commit_text_value()) sync_text_from_value();
                _pending_text_commit = false;
            }
            Textbox::on_focus(focused);
            if (focused && has_drag_interaction_flag(_interaction_flags, drag_interaction_select_all_on_next_focus))
            {
                clear_drag_interaction_flag(_interaction_flags, drag_interaction_select_all_on_next_focus);
                set_drag_interaction_flag(_interaction_flags, drag_interaction_select_all_on_release);
            }
            if (!focused)
            {
                _interaction_flags = drag_interaction_select_all_on_next_focus;
            }
        }

        template <typename T>
        void Draggable<T>::on_hover(HoverState state)
        {
            auto &ctx = detail::get_context();
            detail::CursorID::enum_type cursor = detail::CursorID::arrow;
            if (state != HoverState::leave)
                cursor = has_drag_interaction_flag(_interaction_flags, drag_interaction_text_edit_mode)
                             ? detail::CursorID::ibeam
                             : detail::CursorID::resize_ew;
            detail::set_window_cursor(cursor, ctx.window_ctx);
        }

        template <typename T>
        void Draggable<T>::on_click(MouseKey key, KeyPressState state, u32 click_count)
        {
            if (key != MouseKey::left)
            {
                if (has_drag_interaction_flag(_interaction_flags, drag_interaction_text_edit_mode))
                    Textbox::on_click(key, state, click_count);
                return;
            }

            if (key == MouseKey::left && state == KeyPressState::release &&
                has_drag_interaction_flag(_interaction_flags, drag_interaction_select_all_on_release))
            {
                clear_drag_interaction_flag(_interaction_flags, drag_interaction_select_all_on_release);
                if (!has_drag_interaction_flag(_interaction_flags, drag_interaction_drag_active))
                {
                    set_drag_interaction_flag(_interaction_flags, drag_interaction_text_edit_mode);
                    select_all();
                    add_render_command<detail::ClickEventTraits>(this, [this]() { apply_render_update(false); });
                }
                return;
            }
            if (key == MouseKey::left && state == KeyPressState::press &&
                has_drag_interaction_flag(_interaction_flags, drag_interaction_select_all_on_release))
            {
                return;
            }

            if (has_drag_interaction_flag(_interaction_flags, drag_interaction_text_edit_mode))
            {
                Textbox::on_click(key, state, click_count);
                return;
            }

            if (state == KeyPressState::press)
            {
                set_drag_interaction_flag(_interaction_flags, drag_interaction_edit_text_on_release);
                return;
            }
            if (state == KeyPressState::release &&
                has_drag_interaction_flag(_interaction_flags, drag_interaction_edit_text_on_release))
            {
                clear_drag_interaction_flag(_interaction_flags, drag_interaction_edit_text_on_release);
                if (has_drag_interaction_flag(_interaction_flags, drag_interaction_drag_active)) return;

                set_drag_interaction_flag(_interaction_flags, drag_interaction_text_edit_mode);
                if (click_count > 1) select_all();
                else collapse_cursor_at_point(get_mouse_pos());
                add_render_command<detail::ClickEventTraits>(this, [this]() { apply_render_update(false); });
            }
        }

        template <typename T>
        bool Draggable<T>::allows_unbounded_drag() const
        {
            return !(has_drag_interaction_flag(_interaction_flags, drag_interaction_text_edit_mode) &&
                     !has_drag_interaction_flag(_interaction_flags, drag_interaction_select_all_on_release));
        }

        template <typename T>
        void Draggable<T>::on_drag(const amal::vec2 &delta, KeyPressState state)
        {
            if (has_drag_interaction_flag(_interaction_flags, drag_interaction_text_edit_mode) &&
                !has_drag_interaction_flag(_interaction_flags, drag_interaction_select_all_on_release))
            {
                Textbox::on_drag(delta, state);
                return;
            }

            if (state == KeyPressState::press)
            {
                clear_drag_interaction_flag(_interaction_flags, drag_interaction_drag_active);
                _drag_delta_steps = 0.0;
                return;
            }
            if (state == KeyPressState::release)
            {
                if (has_drag_interaction_flag(_interaction_flags, drag_interaction_drag_active))
                    clear_drag_interaction_flag(_interaction_flags, drag_interaction_text_edit_mode);
                clear_drag_interaction_flag(_interaction_flags, drag_interaction_drag_active);
                return;
            }

            clear_drag_interaction_flags(_interaction_flags, drag_interaction_text_edit_mode |
                                                                 drag_interaction_select_all_on_release |
                                                                 drag_interaction_edit_text_on_release);
            if (!has_drag_interaction_flag(_interaction_flags, drag_interaction_drag_active))
            {
                set_drag_interaction_flag(_interaction_flags, drag_interaction_drag_active);
                _drag_origin_value = static_cast<f64>(value());
                _drag_delta_steps = 0.0;
                _drag_value = _drag_origin_value;
            }
            _drag_delta_steps += static_cast<f64>(delta.x);
            const T next_value =
                clamp_value(cast_relative_stepped_drag_value<T>(_drag_origin_value, _drag_delta_steps, _speed),
                            _min_value, _max_value);
            _drag_value = static_cast<f64>(next_value);
            set_value(next_value);
        }

        template <typename T>
        void Draggable<T>::on_key(Key key, KeyPressState state, KeyMode mods)
        {
            if (state == KeyPressState::release) return;
            if (key == Key::enter || key == Key::kp_enter)
            {
                if (!commit_text_value()) sync_text_from_value();
                _pending_text_commit = false;
                add_render_command<KeyEventTraits>(this, [this]() { apply_render_update(true); });
                focus_widget(focus_parent() ? focus_parent() : parent());
                return;
            }
            if (key == Key::up || key == Key::down)
            {
                step_value(key == Key::up ? 1.0 : -1.0);
                add_render_command<KeyEventTraits>(this, [this]() { apply_render_update(true); });
                return;
            }
            Textbox::on_key(key, state, mods);
            _pending_text_commit = true;
        }

        template <typename T>
        void Draggable<T>::on_char_input(u32 char_code, u32 count)
        {
            Textbox::on_char_input(char_code, count);
            _pending_text_commit = true;
        }

        template <typename T>
        void Draggable<T>::sync_text_from_value()
        {
            _presented_value = value();
            Textbox::set_value(format_value(value()));
            Textbox::sync_value();
            _pending_text_commit = false;
            _manual_change = false;
        }

        template <typename T>
        void Draggable<T>::sync_text_presentation_from_value()
        {
            _presented_value = value();
            Textbox::_value = format_value(value());
            sync_text_presentation();
            _pending_text_commit = false;
        }

        template <typename T>
        bool Draggable<T>::commit_text_value()
        {
            T parsed{};
            if (!parse_text_value(parsed)) return false;
            set_value_internal(parsed, true);
            return true;
        }

        template <typename T>
        void Draggable<T>::step_value(f64 delta)
        {
            const T next_value = cast_relative_stepped_drag_value<T>(static_cast<f64>(value()), delta, _speed);
            set_value(clamp_value(next_value, _min_value, _max_value));
        }

        template <typename T>
        acul::string Draggable<T>::format_value(T value) const
        {
            f64 display_value = static_cast<f64>(value);
            if constexpr (!std::is_integral_v<T>) display_value = round_float_noise(display_value);

            acul::string out;
            if constexpr (std::is_integral_v<T>) out = acul::to_string(static_cast<int>(std::llround(display_value)));
            else out = format_display_float(display_value);

            out += _postfix;
            return out;
        }

        template <typename T>
        bool Draggable<T>::parse_text_value(T &out) const
        {
            const acul::string text = acul::trim(Textbox::value());
            if (text.empty()) return false;

            const char *ptr = text.c_str();
            f32 parsed = 0.0f;
            if (!acul::stof(ptr, parsed)) return false;
            out = clamp_value(cast_drag_value<T>(parsed), _min_value, _max_value);
            return true;
        }

        template <typename T>
        bool Draggable<T>::should_draw_caret() const
        {
            return has_drag_interaction_flag(_interaction_flags, drag_interaction_text_edit_mode);
        }

        template class Draggable<int>;
        template class Draggable<f32>;
        template class Draggable<f64>;
    } // namespace detail

    DragInt::DragInt(u32 id, int value, int min_value, int max_value, f32 speed, amal::vec2 inline_size,
                     WidgetFlags flags)
        : Draggable(id, AUIK_TAG_DRAG_INT, value, min_value, max_value, speed, inline_size, flags)
    {
    }

    DragInt::DragInt(u32 id, ModelBinding *binding, int min_value, int max_value, f32 speed, amal::vec2 inline_size,
                     WidgetFlags flags)
        : Draggable(id, AUIK_TAG_DRAG_INT, 0, min_value, max_value, speed, inline_size, flags)
    {
        set_model_binding(binding);
    }

    DragFloat::DragFloat(u32 id, f32 value, f32 min_value, f32 max_value, f32 speed, amal::vec2 inline_size,
                         WidgetFlags flags)
        : Draggable(id, AUIK_TAG_DRAG_FLOAT, value, min_value, max_value, speed, inline_size, flags)
    {
    }

    DragFloat::DragFloat(u32 id, ModelBinding *binding, f32 min_value, f32 max_value, f32 speed, amal::vec2 inline_size,
                         WidgetFlags flags)
        : Draggable(id, AUIK_TAG_DRAG_FLOAT, 0, min_value, max_value, speed, inline_size, flags)
    {
        set_model_binding(binding);
    }

    DragDouble::DragDouble(u32 id, f64 value, f64 min_value, f64 max_value, f32 speed, amal::vec2 inline_size,
                           WidgetFlags flags)
        : Draggable(id, AUIK_TAG_DRAG_DOUBLE, value, min_value, max_value, speed, inline_size, flags)
    {
    }

    DragDouble::DragDouble(u32 id, ModelBinding *binding, f64 min_value, f64 max_value, f32 speed,
                           amal::vec2 inline_size, WidgetFlags flags)
        : Draggable(id, AUIK_TAG_DRAG_DOUBLE, 0.0, min_value, max_value, speed, inline_size, flags)
    {
        set_model_binding(binding);
    }

    namespace
    {
        template <typename T>
        void apply_drag_common_data(detail::Draggable<T> *widget, const detail::WidgetCommonData &common, u32 style_tag,
                                    u32 rect_tag)
        {
            widget->set_style_tag(style_tag);
            widget->set_rect_tag_id(rect_tag);
            detail::apply_widget_common_data(widget, common);
        }

        void write_drag_int(acul::bin_stream &stream, umbf::Block *block)
        {
            auto *widget = static_cast<DragInt *>(block);
            detail::write_widget_common_data(stream, *widget);
            stream.write(widget->value())
                .write(widget->min_value())
                .write(widget->max_value())
                .write(widget->speed())
                .write(static_cast<u32>(widget->text_flags))
                .write(widget->style_tag());
        }

        umbf::Block *read_drag_int(acul::bin_stream &stream)
        {
            const auto common = detail::read_widget_common_data(stream);
            int value = 0;
            int min_value = std::numeric_limits<int>::lowest();
            int max_value = std::numeric_limits<int>::max();
            f32 speed = 1.0f;
            u32 text_flags = 0u;
            u32 style_tag = AUIK_STYLE_TAG_TEXTBOX;
            stream.read(value).read(min_value).read(max_value).read(speed).read(text_flags).read(style_tag);

            auto *widget = acul::alloc<DragInt>(common.id, value, min_value, max_value, speed, common.inline_size,
                                                WidgetFlags(common.widget_flags));
            widget->text_flags = TextFlags(text_flags);
            apply_drag_common_data(widget, common, style_tag, AUIK_TAG_DRAG_INT);
            return widget;
        }

        void write_drag_float(acul::bin_stream &stream, umbf::Block *block)
        {
            auto *widget = static_cast<DragFloat *>(block);
            detail::write_widget_common_data(stream, *widget);
            stream.write(widget->value())
                .write(widget->min_value())
                .write(widget->max_value())
                .write(widget->speed())
                .write(static_cast<u32>(widget->text_flags))
                .write(widget->style_tag());
        }

        umbf::Block *read_drag_float(acul::bin_stream &stream)
        {
            const auto common = detail::read_widget_common_data(stream);
            f32 value = 0.0f;
            f32 min_value = std::numeric_limits<f32>::lowest();
            f32 max_value = std::numeric_limits<f32>::max();
            f32 speed = 1.0f;
            u32 text_flags = 0u;
            u32 style_tag = AUIK_STYLE_TAG_TEXTBOX;
            stream.read(value).read(min_value).read(max_value).read(speed).read(text_flags).read(style_tag);

            auto *widget = acul::alloc<DragFloat>(common.id, value, min_value, max_value, speed, common.inline_size,
                                                  WidgetFlags(common.widget_flags));
            widget->text_flags = TextFlags(text_flags);
            apply_drag_common_data(widget, common, style_tag, AUIK_TAG_DRAG_FLOAT);
            return widget;
        }

        void write_drag_double(acul::bin_stream &stream, umbf::Block *block)
        {
            auto *widget = static_cast<DragDouble *>(block);
            detail::write_widget_common_data(stream, *widget);
            stream.write(widget->value())
                .write(widget->min_value())
                .write(widget->max_value())
                .write(widget->speed())
                .write(static_cast<u32>(widget->text_flags))
                .write(widget->style_tag());
        }

        umbf::Block *read_drag_double(acul::bin_stream &stream)
        {
            const auto common = detail::read_widget_common_data(stream);
            f64 value = 0.0;
            f64 min_value = std::numeric_limits<f64>::lowest();
            f64 max_value = std::numeric_limits<f64>::max();
            f32 speed = 1.0f;
            u32 text_flags = 0u;
            u32 style_tag = AUIK_STYLE_TAG_TEXTBOX;
            stream.read(value).read(min_value).read(max_value).read(speed).read(text_flags).read(style_tag);

            auto *widget = acul::alloc<DragDouble>(common.id, value, min_value, max_value, speed, common.inline_size,
                                                   WidgetFlags(common.widget_flags));
            widget->text_flags = TextFlags(text_flags);
            apply_drag_common_data(widget, common, style_tag, AUIK_TAG_DRAG_DOUBLE);
            return widget;
        }
    } // namespace

    namespace streams
    {
        AUIK_EXPORT const umbf::streams::Stream drag_int{read_drag_int, write_drag_int};
        AUIK_EXPORT const umbf::streams::Stream drag_float{read_drag_float, write_drag_float};
        AUIK_EXPORT const umbf::streams::Stream drag_double{read_drag_double, write_drag_double};
    } // namespace streams
} // namespace auik
