#pragma once

#include <auik/widgets/widget.hpp>

namespace auik::detail
{
    struct WidgetCommonData
    {
        u32 id = 0u;
        u32 widget_flags = 0u;
        amal::rect bounds{};
        amal::vec2 requested_size{};
    };

    struct LocalizedStringData
    {
        acul::string text;
        bool translated = false;
    };

    inline void write_localized_string(acul::bin_stream &stream, const acul::string &text, bool translated)
    {
        stream.write(text).write(translated);
    }

    inline LocalizedStringData read_localized_string(acul::bin_stream &stream)
    {
        LocalizedStringData out{};
        stream.read(out.text).read(out.translated);
        return out;
    }

    inline void write_widget_common_data(acul::bin_stream &stream, const Widget &widget)
    {
        stream.write(widget.id())
            .write(static_cast<u32>(widget.widget_flags))
            .write(widget.bounds().offset)
            .write(widget.bounds().size)
            .write(widget.requested_size());
    }

    inline WidgetCommonData read_widget_common_data(acul::bin_stream &stream)
    {
        WidgetCommonData out{};
        stream.read(out.id)
            .read(out.widget_flags)
            .read(out.bounds.offset)
            .read(out.bounds.size)
            .read(out.requested_size);
        return out;
    }

    inline void apply_widget_common_data(Widget *widget, const WidgetCommonData &common)
    {
        widget->widget_flags = common.widget_flags;
        widget->set_size(common.requested_size);
        widget->set_position(common.bounds.offset);
        widget->set_layout_size(common.bounds.size);
    }
} // namespace auik::detail
