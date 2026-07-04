#pragma once

#include "../animation.hpp"
#include "../post_effects.hpp"
#include "detail/table_base.hpp"
#include "table.hpp"
#include "widget.hpp"

#define AUIK_TAG_TREE             0xC2970B5Eu
#define AUIK_TAG_TABLE_TREE       0x933619CCu
#define AUIK_TAG_TABLE_TREE_ARROW 0x0FB27EE1u

#define AUIK_TABLE_TREE_FLAG_ALTERNATING_ROWS        0x1u
#define AUIK_TABLE_TREE_FLAG_COLUMN_RESIZABLE        0x2u
#define AUIK_TABLE_TREE_FLAG_RESIZE_INDICATOR_ACTIVE 0x4u
#define AUIK_TABLE_TREE_FLAG_COLUMN_SIZE_OVERRIDES   0x8u
#define AUIK_TREE_PARENT_FIELD 2u

namespace auik
{
    class Tree : public Widget
    {
    public:
        static AUIK_EXPORT constexpr size_t invalid_node = static_cast<size_t>(-1);

        struct Node
        {
            Widget *label = nullptr;
            size_t parent = invalid_node;
            bool expanded = true;
        };

        AUIK_EXPORT explicit Tree(u32 id, amal::vec2 inline_size, WidgetFlags flags, u32 style_tag_id);
        AUIK_EXPORT ~Tree() override;

        AUIK_EXPORT void clear();
        AUIK_EXPORT void set_model_binding(ModelBinding *binding);
        size_t add_node(Widget *label, size_t parent = invalid_node) { return add_node(label, Row{}, parent); }
        AUIK_EXPORT void set_node_expanded(size_t node, bool expanded, bool animate = true);
        AUIK_EXPORT bool node_expanded(size_t node) const;
        AUIK_EXPORT bool node_has_children(size_t node) const;

        AUIK_EXPORT void set_alternating_rows(bool value);
        bool alternating_rows() const
        { return detail::has_table_flag(_tree_flags, AUIK_TABLE_TREE_FLAG_ALTERNATING_ROWS); }
        AUIK_EXPORT void set_alternating_row_style_tag(u32 tag_id);
        u32 alternating_row_style_tag() const { return _alternating_row_style.tag_id; }

        u32 tree_flags() const { return _tree_flags; }

        AUIK_EXPORT void set_style_tag(u32 tag_id);
        u32 style_tag() const { return _style.tag_id; }
        AUIK_EXPORT void set_cell_style_tag(u32 tag_id);
        u32 cell_style_tag() const { return _cell_style.tag_id; }
        AUIK_EXPORT void set_line_style_tag(u32 tag_id);
        u32 line_style_tag() const { return _line_style.tag_id; }
        AUIK_EXPORT void set_collapse_icon_style_tag(u32 tag_id);
        u32 collapse_icon_style_tag() const { return _collapse_icon_style.tag_id; }

        const acul::vector<Node> &nodes() const { return _nodes; }
        size_t visible_row_count() const { return _visible_nodes.size(); }
        f32 indent_width() const { return _indent_width; }
        void set_indent_width(f32 value) { _indent_width = value; }
        AUIK_EXPORT bool is_row_hovered(size_t visible_row) const;
        AUIK_EXPORT bool is_arrow_hovered(size_t node) const;

        AUIK_EXPORT StyleUpdateFlags update_style() override;
        AUIK_EXPORT void update_layout_min_size() override;
        AUIK_EXPORT void update_layout(bool min_size_known) override;
        AUIK_EXPORT void translate(const amal::vec2 &delta) override;
        AUIK_EXPORT void rebuild_clip_rects() override;
        AUIK_EXPORT void reset_draw_records() override;
        AUIK_EXPORT void update_depth(const amal::vec2 &depth_range) override;
        AUIK_EXPORT void back_hit_depth() override;
        AUIK_EXPORT void restore_hit_depth() override;
        AUIK_EXPORT void draw(DrawCtx &ctx) override;
        AUIK_EXPORT void on_click(MouseKey key, KeyPressState state, u32 click_count) override;
        AUIK_EXPORT void on_hover(HoverState state) override;
        AUIK_EXPORT void on_drag(const amal::vec2 &delta, KeyPressState state) override;
        u16 content_clip_id() const override { return clip_id(); }
        AUIK_EXPORT amal::vec4 get_content_clip_rect() const override;
        AUIK_EXPORT void on_attach() override;
        AUIK_EXPORT void on_detach() override;
        virtual u32 signature() const override { return AUIK_TAG_TREE; }

    protected:
        using Row = acul::vector<Widget *>;
        virtual bool supports_columns() const { return false; }
        AUIK_EXPORT size_t add_node(Widget *label, Row cells, size_t parent);
        AUIK_EXPORT void set_default_column_settings(TableColumnSettings settings);
        const TableColumnSettings &default_column_settings() const { return _default_column_settings; }
        AUIK_EXPORT void set_column_settings(acul::vector<TableColumnSettings> settings);
        AUIK_EXPORT void set_column_settings(size_t column, TableColumnSettings settings);
        AUIK_EXPORT void clear_column_settings();
        const acul::vector<TableColumnSettings> &column_settings() const { return _column_settings; }
        AUIK_EXPORT void set_column_resizable(bool value);
        bool column_resizable() const
        { return detail::has_table_flag(_tree_flags, AUIK_TABLE_TREE_FLAG_COLUMN_RESIZABLE); }
        AUIK_EXPORT bool is_resize_border_hovered(size_t element_id) const;
        const acul::vector<acul::point2D<f32>> &size_overrides() const { return _size_overrides; }
        AUIK_EXPORT void set_size_overrides(acul::vector<acul::point2D<f32>> values, bool column_overrides);
        AUIK_EXPORT void set_model_binding(ModelBinding *binding, acul::vector<ModelFieldID> field_ids);
        const Row &node_cells_impl(size_t node) const
        {
            static const Row empty;
            return node < _node_cells.size() ? _node_cells[node] : empty;
        }
        size_t column_count_impl() const { return _column_count; }

    private:
        struct ArrowVisual
        {
            detail::RectData rect{};
            DrawDataID hit_draw{};
            DrawDataID icon_draw{};
            size_t node = invalid_node;
            f32 icon_center_x = 0.0f;
            f32 icon_center_y = 0.0f;
        };

        struct ArrowAnimation
        {
            size_t node = invalid_node;
            DrawDataID draw{};
            AnimationState state;
        };

        void rebuild_visible_nodes();
        void rebuild_cells();
        void clear_cells(bool invalidate_draw = true);
        void clear_nodes();
        void invalidate_visual_draw_records();
        void clear_tree_line_draw_records();
        void draw_tree_lines(DrawCtx &ctx, DrawStream *stream);
        size_t node_depth(size_t node) const;
        bool node_is_last_sibling(size_t node) const;
        size_t node_ancestor_at_depth(size_t node, size_t depth) const;
        size_t resolve_column_count() const;
        u32 cell_element_id(size_t visible_row, size_t column) const;
        Widget *cell_widget(size_t visible_row, size_t column) const;
        const TableColumnSettings &settings_for_column(size_t column) const;
        void update_column_widths(f32 inner_width);
        void resize_visuals();
        void update_cell_clip_rects();
        void sync_cell_parents();
        void invalidate_layout();
        void update_own_layout();
        StyleUpdateFlags update_resize_indicator();
        void rebuild_from_model_binding();

        void ensure_arrow_resources();
        ArrowAnimation *find_arrow_animation(size_t node);
        const ArrowAnimation *find_arrow_animation(size_t node) const;
        void start_arrow_animation(size_t node, bool opening);
        void clear_arrow_animation_draw(ArrowAnimation &animation);
        void release_arrow_animations();
        void draw_arrow(DrawCtx &ctx, ArrowVisual &visual);

        acul::vector<Node> _nodes;
        acul::vector<Row> _node_cells;
        acul::vector<size_t> _visible_nodes;
        acul::vector<acul::vector<Widget *>> _cells;
        acul::vector<acul::point2D<detail::TableTrackMetrics>> _layout_metrics;
        acul::vector<detail::TableCellVisual> _row_visuals;
        acul::vector<detail::TableCellVisual> _cell_visuals;
        acul::vector<detail::TableCellVisual> _alt_row_visuals;
        acul::vector<detail::TableCellVisual> _resize_border_hit_visuals;
        detail::TableCellVisual _resize_indicator_visual;
        acul::vector<QuadsInstanceData> _tree_line_data;
        acul::vector<DrawDataID> _tree_line_draws;
        acul::vector<ArrowVisual> _arrow_visuals;
        acul::vector<ArrowAnimation> _arrow_animations;
        DrawDataID _bg{};
        StyleSelector _style;
        StyleSelector _cell_style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_TREE_CELL};
        StyleSelector _alternating_row_style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_TREE_ROW_ALT};
        StyleSelector _line_style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_TREE_LINE};
        StyleSelector _collapse_icon_style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_TREE_COLLAPSE_ICON};
        StyleSelector _resize_border_style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_TABLE_TREE_RESIZE_BORDER};
        acul::vector<TableColumnSettings> _column_settings;
        acul::vector<acul::point2D<f32>> _size_overrides;
        acul::vector<acul::point2D<f32>> _resize_size_basis;
        TableColumnSettings _default_column_settings{};
        TextureID _arrow_texture{};
        amal::rect _arrow_uv_rect{{0.0f, 0.0f}, {1.0f, 1.0f}};
        amal::vec2 _arrow_size{0.0f, 0.0f};
        u32 _tree_flags = 0u;
        size_t _resizing_column = static_cast<size_t>(-1);
        amal::vec2 _resize_drag_accum{0.0f, 0.0f};
        size_t _column_count = 0u;
        f32 _indent_width = 16.0f;
        ModelBinding *_model_binding = nullptr;
    };

    class TableTree final : public Tree
    {
    public:
        using Row = acul::vector<Widget *>;

        explicit TableTree(u32 id, amal::vec2 inline_size, WidgetFlags flags, u32 style_tag_id)
            : Tree(id, inline_size, flags, style_tag_id)
        {
        }

        size_t add_node(Widget *label, Row cells = {}, size_t parent = invalid_node)
        { return Tree::add_node(label, std::move(cells), parent); }
        const Row &node_cells(size_t node) const { return Tree::node_cells_impl(node); }
        void set_model_binding(ModelBinding *binding, acul::vector<ModelFieldID> field_ids)
        { Tree::set_model_binding(binding, std::move(field_ids)); }

        void set_default_column_settings(TableColumnSettings settings) { Tree::set_default_column_settings(settings); }
        const TableColumnSettings &default_column_settings() const { return Tree::default_column_settings(); }
        void set_column_settings(acul::vector<TableColumnSettings> settings)
        { Tree::set_column_settings(std::move(settings)); }
        void set_column_settings(size_t column, TableColumnSettings settings)
        { Tree::set_column_settings(column, settings); }
        void clear_column_settings() { Tree::clear_column_settings(); }
        const acul::vector<TableColumnSettings> &column_settings() const { return Tree::column_settings(); }
        void set_column_resizable(bool value) { Tree::set_column_resizable(value); }
        bool column_resizable() const { return Tree::column_resizable(); }
        size_t column_count() const { return Tree::column_count_impl(); }
        bool is_resize_border_hovered(size_t element_id) const { return Tree::is_resize_border_hovered(element_id); }
        const acul::vector<acul::point2D<f32>> &size_overrides() const { return Tree::size_overrides(); }
        void set_size_overrides(acul::vector<acul::point2D<f32>> values, bool column_overrides)
        { Tree::set_size_overrides(std::move(values), column_overrides); }

        virtual u32 signature() const override { return AUIK_TAG_TABLE_TREE; }

    private:
        virtual bool supports_columns() const override { return true; }
    };

    inline Tree *make_tree(u32 id, amal::vec2 inline_size = AUIK_SIZE_INHERIT)
    {
        constexpr WidgetFlags widget_flags = WidgetFlagBits::visible | WidgetFlagBits::attachable |
                                             WidgetFlagBits::configurable | WidgetFlagBits::hittable;
        return acul::alloc<Tree>(id, inline_size, widget_flags, AUIK_STYLE_TAG_TREE);
    }

    inline TableTree *make_table_tree(u32 id, amal::vec2 inline_size = AUIK_SIZE_INHERIT)
    {
        constexpr WidgetFlags widget_flags = WidgetFlagBits::visible | WidgetFlagBits::attachable |
                                             WidgetFlagBits::configurable | WidgetFlagBits::hittable;
        return acul::alloc<TableTree>(id, inline_size, widget_flags, AUIK_STYLE_TAG_TREE);
    }

    template <class T>
    struct ModelTreeNode
    {
        T label{};
        acul::vector<ModelTreeNode<T>> children;

        ModelTreeNode() = default;
        explicit ModelTreeNode(T node_label) : label(std::move(node_label)) {}
        ModelTreeNode(T node_label, acul::vector<ModelTreeNode<T>> node_children)
            : label(std::move(node_label)), children(std::move(node_children))
        {
        }
    };

    template <class Node, class MakeLabelField>
    inline Model *make_tree_model(ModelDB *db, ModelID model_id, const acul::vector<Node> &nodes,
                                  ModelFieldID label_field_id, ModelFieldID parent_field_id,
                                  MakeLabelField &&make_label_field)
    {
        if (!db) return nullptr;
        if (model_id == 0u) model_id = make_generated_model_id();
        if (find_model(db, model_id)) return nullptr;
        Model model{};
        model.make_record_id_cb = make_generated_model_record_id;

        auto append_node = [&](auto &&self, const Node &node, ModelRecordID parent_id) -> void {
            ModelRecord record{};
            auto *label_field = make_label_field(label_field_id, node.label);
            auto *parent_field = acul::alloc<ModelValueField<ModelRecordID>>(parent_field_id, parent_id);
            add_model_field(record, *label_field);
            add_model_field(record, *parent_field);
            auto &stored = model.add_record(std::move(record));
            const ModelRecordID record_id = stored.id;
            for (const auto &child : node.children) self(self, child, record_id);
        };

        for (const auto &node : nodes) append_node(append_node, node, AUIK_MODEL_RECORD_ID_INVALID);

        if (!register_model(db, model_id, std::move(model), destroy_model_fields))
        {
            destroy_model_fields(&model);
            return nullptr;
        }
        return find_model(db, model_id);
    }

    template <class T = acul::string>
    inline Model *make_tree_value_model(ModelDB *db, ModelID model_id, const acul::vector<ModelTreeNode<T>> &nodes,
                                        ModelFieldID label_field_id = 1u,
                                        ModelFieldID parent_field_id = AUIK_TREE_PARENT_FIELD)
    {
        return make_tree_model(db, model_id, nodes, label_field_id, parent_field_id,
                               [](ModelFieldID field_id, const T &label) -> ModelField * {
                                   return acul::alloc<ModelValueField<T>>(field_id, label);
                               });
    }

    namespace streams
    {
        extern AUIK_EXPORT const umbf::streams::Stream tree;
        extern AUIK_EXPORT const umbf::streams::Stream table_tree;
    } // namespace streams
} // namespace auik
