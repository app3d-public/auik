#pragma once

#include <acul/vector.hpp>
#include "../model.hpp"
#include "containers.hpp"
#include "widget.hpp"

#define AUIK_TAG_COLUMN 0x6D89BE20u

namespace auik
{
    class Column : public Widget
    {
    public:
        using ColumnChildren = acul::vector<Widget *>;
        using ColumnItems = acul::vector<ColumnChildren>;

        AUIK_EXPORT explicit Column(u32 id, ColumnItems columns, amal::vec2 size, WidgetFlags flags);
        AUIK_EXPORT ~Column() override;

        AUIK_EXPORT void clear_columns();
        AUIK_EXPORT void set_model_binding(ModelBinding *binding, acul::vector<ModelFieldID> field_ids = {});
        AUIK_EXPORT void set_columns(ColumnItems columns);
        AUIK_EXPORT void add_column(ColumnChildren children = {});
        AUIK_EXPORT void add_child(size_t column_index, Widget *child,
                                   ChildLayoutFlags layout = default_child_layout_flags());
        AUIK_EXPORT void set_child_layout(size_t column_index, size_t row_index, ChildLayoutFlags layout);

        size_t column_count() const { return _columns.size(); }
        AUIK_EXPORT const ColumnChildren &column_children(size_t index) const;
        AUIK_EXPORT ColumnChildren &column_children(size_t index);
        AUIK_EXPORT const acul::vector<ChildLayoutFlags> &column_layouts(size_t index) const;
        AUIK_EXPORT ::auik::Block *&operator[](size_t index);
        AUIK_EXPORT const ::auik::Block *operator[](size_t index) const;

        AUIK_EXPORT void set_style_tag(u32 tag_id);
        u32 style_tag() const { return _style.tag_id; }

        AUIK_EXPORT StyleUpdateFlags update_style() override;
        AUIK_EXPORT void update_layout_min_size() override;
        AUIK_EXPORT void update_layout(bool min_size_known) override;
        AUIK_EXPORT void translate(const amal::vec2 &delta) override;
        AUIK_EXPORT void reset_clip_rect_records() override;
        AUIK_EXPORT void rebuild_clip_rects() override;
        AUIK_EXPORT void reset_draw_records() override;
        AUIK_EXPORT void update_depth(const amal::vec2 &depth_range) override;
        AUIK_EXPORT void back_hit_depth() override;
        AUIK_EXPORT void restore_hit_depth() override;
        AUIK_EXPORT void draw(DrawCtx &ctx) override;
        u16 content_clip_id() const override { return clip_id(); }
        AUIK_EXPORT amal::vec4 get_content_clip_rect() const override;
        AUIK_EXPORT void on_attach() override;
        AUIK_EXPORT void on_detach() override;
        u32 signature() const override { return AUIK_TAG_COLUMN; }

    private:
        void add_slot(ColumnChildren children);
        void update_column_clip_rects();
        void rebuild_from_model_binding();

        acul::vector<::auik::Block *> _columns;
        acul::vector<f32> _column_widths;
        StyleSelector _style;
        ModelBinding *_model_binding = nullptr;
    };

    inline Block *make_column_block(u32 owner_id)
    { return acul::alloc<Block>(owner_id, WidgetFlagBits::visible, AUIK_TAG_BLOCK); }

    inline Block *make_column_block() { return make_column_block(AUIK_TAG_COLUMN); }


    inline Column *make_column(u32 id, Column::ColumnItems columns = {},
                               amal::vec2 size = {AUIK_SIZE_X_FILL, AUIK_SIZE_Y_FIT})
    {
        constexpr WidgetFlags widget_flags = WidgetFlagBits::visible | WidgetFlagBits::attachable |
                                             WidgetFlagBits::configurable;
        return acul::alloc<Column>(id, std::move(columns), size, widget_flags);
    }

    inline Column *make_column(u32 id, size_t column_count, amal::vec2 size = {AUIK_SIZE_X_FILL, AUIK_SIZE_Y_FIT})
    {
        Column::ColumnItems columns;
        columns.resize(column_count);
        return make_column(id, std::move(columns), size);
    }

    namespace streams
    {
        extern AUIK_EXPORT const umbf::streams::Stream column;
    }
} // namespace auik
