#include <acul/string/utils.hpp>
#include <auik/auik.hpp>
#include <auik/widgets/drag.hpp>

namespace auik
{
    static const DragUnit *find_unit(const DragUnitResolver *resolver, const acul::string &name)
    {
        if (!resolver || name.empty()) return nullptr;
        for (u32 i = 0; i < resolver->unit_count; ++i)
        {
            const DragUnit &unit = resolver->units[i];
            if (unit.name && name == unit.name) return &unit;
        }
        return nullptr;
    }

    static const DragUnit *find_default_unit(const DragUnitResolver *resolver, const char *preferred)
    {
        if (!resolver) return nullptr;
        if (auto *unit = find_unit(resolver, preferred ? acul::string(preferred) : acul::string{})) return unit;
        return find_unit(resolver, resolver->default_unit ? acul::string(resolver->default_unit) : acul::string{});
    }

    static bool only_trailing_spaces(const char *str)
    {
        while (*str)
        {
            if (!isspace(static_cast<unsigned char>(*str))) return false;
            ++str;
        }
        return true;
    }

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

    static f64 resolve_base_speed(const DragUnitResolver *resolver, const char *default_unit, f32 speed)
    {
        const DragUnit *unit = find_default_unit(resolver, default_unit);
        return unit ? static_cast<f64>(speed) * unit->to_base : static_cast<f64>(speed);
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
        static inline TextFlags drag_text_flags(const DragUnitResolver *resolver)
        {
            if (resolver) return TextFlagBits::chars_ascii;
            if constexpr (std::is_integral_v<T>)
                return TextFlagBits::chars_decimal | TextFlagBits::chars_no_blank | TextFlagBits::chars_ascii;
            else return TextFlagBits::chars_scientific | TextFlagBits::chars_ascii;
        }

        template <typename T>
        Draggable<T>::Draggable(u32 id, T *value, T min_value, T max_value, f32 speed, amal::vec2 size,
                                WidgetFlags flags, Widget *parent, const DragUnitResolver *unit_resolver,
                                const char *default_unit)
            : TextBox(id, "", size, flags, parent, AUIK_TAG_TEXTBOX, drag_text_flags<T>(unit_resolver)),
              _value(value),
              _fallback_value(value ? *value : T{}),
              _min_value(min_value),
              _max_value(max_value),
              _speed(speed),
              _unit_resolver(unit_resolver),
              _default_unit(default_unit),
              _last_value(value ? *value : T{})
        {
            set_drag_interaction_flag(_interaction_flags, drag_interaction_select_all_on_next_focus);
            sync_text_from_value();
        }

        template <typename T>
        void Draggable<T>::set_value(T value)
        {
            value = clamp_value(value, _min_value, _max_value);
            const T prev_value = this->value();
            if (prev_value == value) return;
            if (_value) *_value = value;
            else _fallback_value = value;
            _last_value = value;
            const bool prevented = mark_changed();
            sync_text_from_value();
            if (!prevented) apply_render_update(true);
        }

        template <typename T>
        void Draggable<T>::set_unit_resolver(const DragUnitResolver *resolver, const char *default_unit)
        {
            _unit_resolver = resolver;
            _default_unit = default_unit;
            text_flags = drag_text_flags<T>(resolver);
            sync_text_from_value();
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
            if (value() != _last_value) sync_text_from_value();
            TextBox::update_layout(min_size_known);
        }

        template <typename T>
        void Draggable<T>::on_focus(bool focused)
        {
            if (!focused && _pending_text_commit)
            {
                if (!commit_text_value()) sync_text_from_value();
                _pending_text_commit = false;
            }
            TextBox::on_focus(focused);
            if (focused && has_drag_interaction_flag(_interaction_flags, drag_interaction_select_all_on_next_focus))
            {
                clear_drag_interaction_flag(_interaction_flags, drag_interaction_select_all_on_next_focus);
                set_drag_interaction_flag(_interaction_flags, drag_interaction_select_all_on_release);
            }
            if (!focused) { _interaction_flags = drag_interaction_select_all_on_next_focus; }
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
                    TextBox::on_click(key, state, click_count);
                return;
            }

            if (key == MouseKey::left && state == KeyPressState::release &&
                has_drag_interaction_flag(_interaction_flags, drag_interaction_select_all_on_release))
            {
                clear_drag_interaction_flag(_interaction_flags, drag_interaction_select_all_on_release);
                if (!has_drag_interaction_flag(_interaction_flags, drag_interaction_drag_active))
                {
                    set_drag_interaction_flag(_interaction_flags, drag_interaction_text_edit_mode);
                    select_all_text();
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
                TextBox::on_click(key, state, click_count);
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
                if (click_count > 1) select_all_text();
                else collapse_cursor_at_point(get_mouse_pos());
                add_render_command<detail::ClickEventTraits>(this, [this]() { apply_render_update(false); });
            }
        }

        template <typename T>
        void Draggable<T>::on_drag(const amal::vec2 &delta, KeyPressState state)
        {
            if (has_drag_interaction_flag(_interaction_flags, drag_interaction_text_edit_mode) &&
                !has_drag_interaction_flag(_interaction_flags, drag_interaction_select_all_on_release))
            {
                TextBox::on_drag(delta, state);
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
            const f64 base_speed = resolve_base_speed(_unit_resolver, _default_unit, _speed);
            const T next_value =
                clamp_value(cast_relative_stepped_drag_value<T>(_drag_origin_value, _drag_delta_steps, base_speed),
                            _min_value, _max_value);
            _drag_value = static_cast<f64>(next_value);
            set_value(next_value);
            add_render_command<DragEventTraits>(this, [this]() { apply_render_update(true); });
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
            TextBox::on_key(key, state, mods);
            _pending_text_commit = true;
        }

        template <typename T>
        void Draggable<T>::on_char_input(u32 char_code, u32 count)
        {
            if constexpr (std::is_integral_v<T>)
            {
                if (!_unit_resolver)
                {
                    const bool digit = char_code >= '0' && char_code <= '9';
                    const bool sign = char_code == '+' || char_code == '-';
                    if (!digit && !sign) return;
                }
            }
            TextBox::on_char_input(char_code, count);
            _pending_text_commit = true;
        }

        template <typename T>
        void Draggable<T>::sync_text_from_value()
        {
            _last_value = value();
            TextBox::set_value_internal(format_value(value()));
            _pending_text_commit = false;
        }

        template <typename T>
        bool Draggable<T>::commit_text_value()
        {
            T parsed{};
            if (!parse_text_value(parsed)) return false;
            set_value(parsed);
            return true;
        }

        template <typename T>
        void Draggable<T>::step_value(f64 delta)
        {
            const f64 base_speed = resolve_base_speed(_unit_resolver, _default_unit, _speed);
            const T next_value = cast_relative_stepped_drag_value<T>(static_cast<f64>(value()), delta, base_speed);
            set_value(clamp_value(next_value, _min_value, _max_value));
        }

        template <typename T>
        acul::string Draggable<T>::format_value(T value) const
        {
            const auto *unit = find_default_unit(_unit_resolver, _default_unit);
            f64 display_value = static_cast<f64>(value);
            if (unit && unit->to_base != 0.0) display_value /= unit->to_base;
            if constexpr (!std::is_integral_v<T>) display_value = round_float_noise(display_value);

            acul::string out;
            if constexpr (std::is_integral_v<T>) out = acul::to_string(static_cast<int>(std::llround(display_value)));
            else out = format_display_float(display_value);

            if (unit && unit->name)
            {
                out += " ";
                out += unit->name;
            }
            return out;
        }

        template <typename T>
        bool Draggable<T>::parse_text_value(T &out) const
        {
            const acul::string text = acul::trim(TextBox::value());
            if (text.empty()) return false;

            const char *ptr = text.c_str();
            f32 parsed = 0.0f;
            if (!acul::stof(ptr, parsed)) return false;

            while (isspace(static_cast<unsigned char>(*ptr))) ++ptr;
            const char *unit_begin = ptr;
            while (*ptr && !isspace(static_cast<unsigned char>(*ptr))) ++ptr;
            acul::string unit_name(unit_begin, ptr - unit_begin);
            if (!only_trailing_spaces(ptr)) return false;

            const DragUnit *unit = nullptr;
            if (_unit_resolver)
            {
                unit = unit_name.empty() ? find_default_unit(_unit_resolver, _default_unit)
                                         : find_unit(_unit_resolver, unit_name);
                if (!unit) return false;
            }
            else if (!unit_name.empty()) return false;

            f64 base_value = static_cast<f64>(parsed);
            if (unit) base_value *= unit->to_base;
            out = clamp_value(cast_drag_value<T>(base_value), _min_value, _max_value);
            return true;
        }

        template <typename T>
        const char *Draggable<T>::resolve_default_unit() const
        {
            if (_default_unit) return _default_unit;
            return _unit_resolver ? _unit_resolver->default_unit : nullptr;
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

    DragInt::DragInt(u32 id, int *value, int min_value, int max_value, f32 speed, amal::vec2 size, WidgetFlags flags,
                     Widget *parent, const DragUnitResolver *unit_resolver, const char *default_unit)
        : Draggable(id, value, min_value, max_value, speed, size, flags, parent, unit_resolver, default_unit)
    {
    }

    DragFloat::DragFloat(u32 id, f32 *value, f32 min_value, f32 max_value, f32 speed, amal::vec2 size,
                         WidgetFlags flags, Widget *parent, const DragUnitResolver *unit_resolver,
                         const char *default_unit)
        : Draggable(id, value, min_value, max_value, speed, size, flags, parent, unit_resolver, default_unit)
    {
    }

    DragDouble::DragDouble(u32 id, f64 *value, f64 min_value, f64 max_value, f32 speed, amal::vec2 size,
                           WidgetFlags flags, Widget *parent, const DragUnitResolver *unit_resolver,
                           const char *default_unit)
        : Draggable(id, value, min_value, max_value, speed, size, flags, parent, unit_resolver, default_unit)
    {
    }
} // namespace auik
