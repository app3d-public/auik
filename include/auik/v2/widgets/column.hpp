#pragma once

#include <acul/vector.hpp>
#include "widget.hpp"

namespace auik::v2
{
    constexpr inline WidgetFlags get_default_column_flags() { return get_default_widget_flags(); }

    class APPLIB_API Column : public Widget
    {
    public:
        using ColumnChildren = acul::vector<Widget *>;
        using ColumnItems = acul::vector<ColumnChildren>;

        explicit Column(u32 id, ColumnItems columns = {}, amal::vec2 size = {0.0f, 0.0f},
                        WidgetFlags flags = get_default_column_flags(), Widget *parent = nullptr,
                        u32 style_tag_id = AUIK_STYLE_TAG_COLUMN);
        ~Column() override;

        void clear_columns();
        void set_columns(ColumnItems columns);
        void add_column(ColumnChildren children = {});
        void add_child(size_t column_index, Widget *child, ChildLayoutFlags layout = default_child_layout_flags());
        void set_child_layout(size_t column_index, size_t row_index, ChildLayoutFlags layout);

        size_t column_count() const { return _columns.size(); }
        const ColumnChildren &column_children(size_t index) const;
        ColumnChildren &column_children(size_t index);

        void set_style_tag(u32 tag_id);
        u32 style_tag() const { return _style.tag_id; }

        StyleUpdateFlags update_style() override;
        void update_layout_min_size() override;
        void update_layout(bool min_size_known) override;
        void translate(const amal::vec2 &delta) override;
        void reset_clip_rect_records() override;
        void rebuild_clip_rects() override;
        void reset_draw_records() override;
        void update_depth(const amal::vec2 &depth_range) override;
        void back_hit_depth() override;
        void restore_hit_depth() override;
        void draw(DrawCtx &ctx) override;
        u16 content_clip_id() const override { return clip_id(); }
        amal::vec4 get_content_clip_rect() const override;
        void on_attach() override;
        void on_detach() override;

    private:
        class Slot;

        void add_slot(ColumnChildren children);
        void update_column_clip_rects();

        acul::vector<Slot *> _columns;
        acul::vector<f32> _column_widths;
        acul::vector<f32> _row_heights;
        StyleSelector _style;
    };

    inline Column *make_column(u32 id, Column::ColumnItems columns = {}, Widget *parent = nullptr)
    {
        return acul::alloc<Column>(id, std::move(columns), amal::vec2{0.0f, 0.0f}, get_default_column_flags(), parent,
                                   AUIK_STYLE_TAG_COLUMN);
    }

    inline Column *make_fixed_column(u32 id, Column::ColumnItems columns = {}, amal::vec2 size = {0.0f, 0.0f},
                                     Widget *parent = nullptr)
    {
        return acul::alloc<Column>(id, std::move(columns), size, get_default_column_flags() | WidgetFlagBits::fixed_layout,
                                   parent, AUIK_STYLE_TAG_COLUMN);
    }
} // namespace auik::v2
