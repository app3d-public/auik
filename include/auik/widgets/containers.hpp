#pragma once

#include "detail/popup_trigger.hpp"
#include "detail/scrollbar.hpp"
#include "text.hpp"

#define AUIK_TAG_BLOCK                   0x237AFC8Eu
#define AUIK_TAG_DRAW_BLOCK              0xB2F15B07u
#define AUIK_TAG_DUMMY                   0xD5A4C970u
#define AUIK_TAG_COLLAPSE_HEADER         0x565C9C5Eu
#define AUIK_TAG_COLLAPSE_HEADER_TRIGGER 0x4ABB6689u
#define AUIK_TAG_WIDGET_STACK            0xE9A58F31u
#define AUIK_TAG_WIDGET_REF              0x7C5F435Au

namespace auik
{
    enum class BlockChildLayer : u8
    {
        background,
        work,
        foreground
    };

    namespace detail
    {
        amal::vec2 compute_children_layout_required_size(const acul::vector<Widget *> &children,
                                                         const acul::vector<ChildLayoutFlags> &layouts,
                                                         f32 inline_spacing_x, f32 wrap_width = 0.0f,
                                                         bool refresh_min_size = true);
        AUIK_EXPORT void layout_child_widgets(Widget *layout_owner, const acul::vector<Widget *> &children,
                                              const acul::vector<ChildLayoutFlags> &layouts,
                                              const amal::rect &content_rect, f32 inline_spacing_x);
    } // namespace detail

    class Block : public Widget
    {
    public:
        acul::vector<Widget *> children;

        explicit Block(u32 id, WidgetFlags widget_flags, u32 tag_id)
            : Widget(id, widget_flags, EventFlagBits::none, {{0.0f, 0.0f}, {0.0f, 0.0f}}, tag_id)
        {
        }

        ~Block() { clear_children(); }

        void clear_children() { erase_children(0u, children.size()); }
        AUIK_EXPORT void erase_children(size_t first, size_t count);
        void erase_children_from(size_t first)
        {
            if (first < children.size()) erase_children(first, children.size() - first);
        }
        AUIK_EXPORT void add_child(Widget *child, ChildLayoutFlags layout = default_child_layout_flags());
        AUIK_EXPORT void add_child_to_background(Widget *child, ChildLayoutFlags layout = default_child_layout_flags());
        AUIK_EXPORT void add_child_to_foreground(Widget *child, ChildLayoutFlags layout = default_child_layout_flags());
        AUIK_EXPORT void set_child_layout(size_t index, ChildLayoutFlags layout);
        AUIK_EXPORT void set_width(f32 value);
        AUIK_EXPORT void set_height(f32 value);
        AUIK_EXPORT void set_size(const amal::vec2 &value);
        void set_inline_spacing(f32 value) { _inline_spacing = value; }
        f32 inline_spacing() const { return _inline_spacing; }
        const acul::vector<ChildLayoutFlags> &child_layouts() const { return _child_layouts; }
        amal::vec2 explicit_size() const { return _explicit_size; }

        AUIK_EXPORT StyleUpdateFlags update_style() override;
        AUIK_EXPORT void update_layout_min_size_force() override;
        AUIK_EXPORT void update_layout(bool min_size_known) override;
        AUIK_EXPORT void translate(const amal::vec2 &delta) override;
        AUIK_EXPORT void reset_clip_rect_records() override;
        AUIK_EXPORT void rebuild_clip_rects() override;
        AUIK_EXPORT void reset_draw_records() override;
        AUIK_EXPORT u32 get_depth_requirement() const override;
        AUIK_EXPORT void update_depth(const amal::vec2 &depth_range) override;
        AUIK_EXPORT void back_hit_depth() override;
        AUIK_EXPORT void restore_hit_depth() override;
        AUIK_EXPORT void draw(DrawCtx &ctx) override;
        u16 content_clip_id() const override { return parent() ? parent()->content_clip_id() : clip_id(); }
        u32 signature() const override { return AUIK_TAG_BLOCK; }
        amal::vec4 get_content_clip_rect() const override
        {
            return parent() ? parent()->get_content_clip_rect() : get_clip_rect(content_clip_id());
        }
        AUIK_EXPORT void on_attach() override;
        AUIK_EXPORT void on_detach() override;
        AUIK_EXPORT void on_change(ChangeEvent &event) override;

    protected:
        AUIK_EXPORT virtual f32 resolved_inline_spacing() const;
        AUIK_EXPORT virtual amal::vec2 compute_content_min_size();
        AUIK_EXPORT virtual void layout_children(const amal::rect &content_rect);
        AUIK_EXPORT void update_layout_min_size_with(const amal::vec4 &margin, const amal::vec4 &padding);
        AUIK_EXPORT void update_layout_with(bool min_size_known, const amal::vec4 &margin, const amal::vec4 &padding);
        AUIK_EXPORT bool has_explicit_width() const;
        AUIK_EXPORT bool has_explicit_height() const;
        AUIK_EXPORT f32 resolved_explicit_width() const;
        AUIK_EXPORT f32 resolved_explicit_height() const;
        AUIK_EXPORT void refresh_child_layout(size_t index);
        AUIK_EXPORT void refresh_child_layouts();
        acul::vector<ChildLayoutFlags> _explicit_child_layouts;
        acul::vector<ChildLayoutFlags> _child_layouts;
        acul::vector<BlockChildLayer> _child_layers;

    private:
        AUIK_EXPORT void add_child_to_layer(Widget *child, ChildLayoutFlags layout, BlockChildLayer layer);
        amal::vec2 _explicit_size = AUIK_SIZE_FIT;
        f32 _inline_spacing = 0.0f;
    };

    struct DrawBlockFlagBits
    {
        enum enum_type
        {
            none = 0x0,
            clip_ignores_padding_x = 0x1,
            clip_ignores_padding_y = 0x2,
            scrollbar_x = 0x4,
            scrollbar_y = 0x8
        };

        using flag_bitmask = std::true_type;
    };

    using DrawBlockFlags = acul::flags<DrawBlockFlagBits>;

    class DrawBlock : public Block
    {
    public:
        AUIK_EXPORT explicit DrawBlock(u32 id, WidgetFlags widget_flags, u32 tag_id);
        AUIK_EXPORT ~DrawBlock() override;

        AUIK_EXPORT StyleUpdateFlags update_style() override;
        AUIK_EXPORT void update_layout_min_size_force() override;
        AUIK_EXPORT void update_layout(bool min_size_known) override;
        AUIK_EXPORT void translate(const amal::vec2 &delta) override;
        AUIK_EXPORT void reset_clip_rect_records() override;
        AUIK_EXPORT void rebuild_clip_rects() override;
        AUIK_EXPORT void reset_draw_records() override;
        AUIK_EXPORT u32 get_depth_requirement() const override;
        AUIK_EXPORT void update_depth(const amal::vec2 &depth_range) override;
        AUIK_EXPORT void back_hit_depth() override;
        AUIK_EXPORT void restore_hit_depth() override;
        AUIK_EXPORT void draw(DrawCtx &ctx) override;
        u32 signature() const override { return AUIK_TAG_DRAW_BLOCK; }
        AUIK_EXPORT void on_scroll(const amal::vec2 &delta) override;
        AUIK_EXPORT void on_click(MouseKey key, KeyPressState state, u32 click_count) override;
        AUIK_EXPORT void on_drag(const amal::vec2 &delta, KeyPressState state) override;
        AUIK_EXPORT void set_scrollbars_enabled(bool x, bool y);
        AUIK_EXPORT void set_scrollbar_style_tag(u32 track_tag_id);
        AUIK_EXPORT void set_scrollbar_style_tags(u32 track_tag_id, u32 thumb_tag_id);
        void set_content_padding(const amal::vec4 &value) { _content_padding = value; }
        amal::vec4 content_padding() const { return _content_padding; }
        void set_draw_block_flags(DrawBlockFlags flags) { _draw_flags = flags; }
        DrawBlockFlags draw_block_flags() const { return _draw_flags; }
        AUIK_EXPORT void set_style_tag(u32 tag_id);
        u32 style_tag() const { return _style_tag_id; }
        u32 scrollbar_track_style_tag() const { return _scrollbar_track_style_tag; }
        u32 scrollbar_thumb_style_tag() const { return _scrollbar_thumb_style_tag; }
        AUIK_EXPORT void override_content_clip_rect(const amal::vec4 &rect);
        bool has_visible_scrollbar_x() const { return _scrollbar_x && _scrollbar_x->is_visible(); }
        bool has_visible_scrollbar_y() const { return _scrollbar_y && _scrollbar_y->is_visible(); }
        void reset_scroll_offset() { _content_offset = {0.0f, 0.0f}; }
        u16 content_clip_id() const override { return _content_clip_id; }
        amal::vec4 get_content_clip_rect() const override
        {
            if (_content_clip_id != 0xFFFFu) return get_clip_rect(_content_clip_id);
            if (clip_id() != 0xFFFFu) return get_clip_rect(clip_id());
            return parent() ? parent()->get_content_clip_rect() : get_main_viewport_rect();
        }

    protected:
        amal::vec2 scroll_content_size() const { return _scroll_content_size; }
        amal::vec2 scroll_view_size() const { return _scroll_view_size; }
        amal::vec2 content_offset() const { return _content_offset; }

    protected:
        AUIK_EXPORT void ensure_scrollbars();
        AUIK_EXPORT void update_scroll_clip(const amal::vec2 &content_pos, const amal::vec2 &view_size);
        AUIK_EXPORT void rebuild_scroll_clip_rect();
        AUIK_EXPORT void request_scroll_layout_update(DrawReasonFlags reason);
        AUIK_EXPORT f32 resolved_inline_spacing() const override;
        AUIK_EXPORT const Style *draw_style() const;
        AUIK_EXPORT amal::vec4 draw_margin() const;
        AUIK_EXPORT amal::vec4 draw_padding() const;
        AUIK_EXPORT void sync_draw_bounds();

        detail::Scrollbar *_scrollbar_x = nullptr;
        detail::Scrollbar *_scrollbar_y = nullptr;
        detail::RectData _bg_rect{};
        DrawDataID _bg_draw_id{};
        u32 _style_tag_id = 0u;
        StyleSelector _style{Theme::STYLE_ID_INVALID, 0u};
        amal::vec4 _content_padding{0.0f, 0.0f, 0.0f, 0.0f};
        amal::vec2 _content_offset{0.0f, 0.0f};
        amal::vec2 _scroll_content_size{0.0f, 0.0f};
        amal::vec2 _scroll_view_size{0.0f, 0.0f};
        bool _content_clip_rect_overridden = false;
        amal::vec4 _content_clip_rect_override{0.0f, 0.0f, 0.0f, 0.0f};
        u16 _content_clip_id = 0xFFFFu;
        DrawBlockFlags _draw_flags = DrawBlockFlagBits::scrollbar_x | DrawBlockFlagBits::scrollbar_y;
        u32 _scrollbar_track_style_tag = AUIK_STYLE_TAG_SCROLLBAR_TRACK_INTERNAL;
        u32 _scrollbar_thumb_style_tag = AUIK_STYLE_TAG_SCROLLBAR_THUMB_INTERNAL;
    };

    class WidgetRef final : public Widget
    {
    public:
        AUIK_EXPORT explicit WidgetRef(Widget *target = nullptr,
                                       WidgetFlags widget_flags = WidgetFlagBits::visible |
                                                                  WidgetFlagBits::configurable);
        AUIK_EXPORT ~WidgetRef() override;

        AUIK_EXPORT void set_target(Widget *target);
        Widget *target() const { return _target; }
        AUIK_EXPORT void set_ref_active(bool active);
        bool ref_active() const { return _ref_active; }

        AUIK_EXPORT StyleUpdateFlags update_style() override;
        AUIK_EXPORT void update_layout_min_size_force() override;
        AUIK_EXPORT void update_layout(bool min_size_known) override;
        AUIK_EXPORT void translate(const amal::vec2 &delta) override;
        AUIK_EXPORT void reset_clip_rect_records() override;
        AUIK_EXPORT void rebuild_clip_rects() override;
        AUIK_EXPORT void reset_draw_records() override;
        AUIK_EXPORT u32 get_depth_requirement() const override;
        AUIK_EXPORT void update_depth(const amal::vec2 &depth_range) override;
        AUIK_EXPORT void back_hit_depth() override;
        AUIK_EXPORT void restore_hit_depth() override;
        AUIK_EXPORT void draw(DrawCtx &ctx) override;
        u32 signature() const override { return AUIK_TAG_WIDGET_REF; }
        AUIK_EXPORT amal::vec2 requested_size() const override;
        u16 content_clip_id() const override { return clip_id(); }
        amal::vec4 get_content_clip_rect() const override
        {
            return clip_id() != 0xFFFFu ? get_clip_rect(clip_id()) : get_main_viewport_rect();
        }

    private:
        void save_target_layout();
        void restore_target_layout();
        void apply_target_layout();

        Widget *_target = nullptr;
        Widget *_saved_parent = nullptr;
        bool _ref_active = false;
        bool _saved_layout_valid = false;
        amal::vec2 _saved_position{0.0f, 0.0f};
        amal::vec2 _saved_size{0.0f, 0.0f};
    };

    class WidgetStack final : public Widget
    {
    public:
        AUIK_EXPORT explicit WidgetStack(WidgetFlags widget_flags = WidgetFlagBits::visible |
                                                                    WidgetFlagBits::attachable |
                                                                    WidgetFlagBits::configurable);
        AUIK_EXPORT ~WidgetStack() override;

        AUIK_EXPORT void clear_children();
        AUIK_EXPORT void add_child(Widget *child);
        AUIK_EXPORT void add_child_to_background(Widget *child);
        AUIK_EXPORT void add_child_to_foreground(Widget *child);
        AUIK_EXPORT void set_active_index(size_t index);
        size_t active_index() const { return _active_index; }
        Widget *active_child() const;
        AUIK_EXPORT bool is_active_child(const Widget *child) const;
        size_t child_count() const { return _children.size(); }
        const acul::vector<Widget *> &children() const { return _children; }
        const acul::vector<BlockChildLayer> &child_layers() const { return _child_layers; }

        AUIK_EXPORT StyleUpdateFlags update_style() override;
        AUIK_EXPORT bool accepts_child_style_update(const Widget *child) const override;
        AUIK_EXPORT void update_layout_min_size_force() override;
        AUIK_EXPORT void update_layout(bool min_size_known) override;
        AUIK_EXPORT void translate(const amal::vec2 &delta) override;
        AUIK_EXPORT void reset_clip_rect_records() override;
        AUIK_EXPORT void rebuild_clip_rects() override;
        AUIK_EXPORT void reset_draw_records() override;
        AUIK_EXPORT u32 get_depth_requirement() const override;
        AUIK_EXPORT void update_depth(const amal::vec2 &depth_range) override;
        AUIK_EXPORT void back_hit_depth() override;
        AUIK_EXPORT void restore_hit_depth() override;
        AUIK_EXPORT void draw(DrawCtx &ctx) override;
        AUIK_EXPORT void on_attach() override;
        AUIK_EXPORT void on_detach() override;
        u32 signature() const override { return AUIK_TAG_WIDGET_STACK; }
        AUIK_EXPORT amal::vec2 requested_size() const override;
        u16 content_clip_id() const override { return parent() ? parent()->content_clip_id() : clip_id(); }
        amal::vec4 get_content_clip_rect() const override
        {
            return parent() ? parent()->get_content_clip_rect() : get_clip_rect(content_clip_id());
        }

    private:
        void set_child_ref_active(Widget *child, bool active);
        void add_layer_child(Widget *child, BlockChildLayer layer);
        void update_layer_layout(BlockChildLayer layer);

        acul::vector<Widget *> _children;
        acul::vector<BlockChildLayer> _child_layers;
        size_t _active_index = 0u;
    };

    class CollapseHeader final : public Block
    {
    public:
        AUIK_EXPORT explicit CollapseHeader(u32 id, StringView label, bool expanded, WidgetFlags widget_flags,
                                            u32 style_tag_id);
        AUIK_EXPORT ~CollapseHeader() override;

        AUIK_EXPORT void set_label(StringView value);
        AUIK_EXPORT const acul::string &label() const;
        bool is_translated_label() const { return _label && _label->is_translated_text(); }
        const char *label_literal() const { return _label ? _label->translated_text_literal() : nullptr; }
        AUIK_EXPORT void set_expanded(bool value);
        bool expanded() const { return _expanded; }
        void toggle() { set_expanded(!_expanded); }
        AUIK_EXPORT void set_style_tag(u32 tag_id);
        u32 style_tag() const { return _style.tag_id; }
        AUIK_EXPORT void set_closed_style_tag(u32 tag_id);
        u32 closed_style_tag() const { return _closed_style_tag; }
        AUIK_EXPORT void set_content_style_tag(u32 tag_id);
        u32 content_style_tag() const { return _content_style.tag_id; }
        AUIK_EXPORT void set_trigger_style_tag(u32 tag_id);
        u32 trigger_style_tag() const { return _trigger_style_tag; }

        AUIK_EXPORT StyleUpdateFlags update_style() override;
        AUIK_EXPORT void update_layout_min_size_force() override;
        AUIK_EXPORT void update_layout(bool min_size_known) override;
        AUIK_EXPORT void translate(const amal::vec2 &delta) override;
        AUIK_EXPORT void rebuild_clip_rects() override;
        AUIK_EXPORT void reset_draw_records() override;
        AUIK_EXPORT void update_depth(const amal::vec2 &depth_range) override;
        AUIK_EXPORT void back_hit_depth() override;
        AUIK_EXPORT void restore_hit_depth() override;
        AUIK_EXPORT void draw(DrawCtx &ctx) override;
        AUIK_EXPORT void on_click(MouseKey key, KeyPressState state, u32 click_count) override;
        AUIK_EXPORT void on_attach() override;
        AUIK_EXPORT void on_detach() override;
        u32 signature() const override { return AUIK_TAG_COLLAPSE_HEADER; }

    protected:
        AUIK_EXPORT amal::vec2 compute_content_min_size() override;
        AUIK_EXPORT void layout_children(const amal::rect &content_rect) override;

    private:
        u32 current_header_style_tag() const;
        void invalidate_layout();

        Text *_label = nullptr;
        detail::PopupTrigger *_trigger = nullptr;
        DrawDataID _header_bg{};
        DrawDataID _content_bg{};
        detail::RectData _header_rect{};
        detail::RectData _content_rect{};
        StyleSelector _style;
        StyleSelector _content_style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_COLLAPSE_HEADER_CONTENT};
        u32 _closed_style_tag = AUIK_STYLE_TAG_COLLAPSE_HEADER_CLOSED;
        u32 _trigger_style_tag = AUIK_STYLE_TAG_COLLAPSE_HEADER_TRIGGER;
        bool _expanded = true;
    };

    class Dummy final : public Widget
    {
    public:
        explicit Dummy(u32 id, amal::vec2 size, WidgetFlags widget_flags)
            : Widget(id, widget_flags, EventFlagBits::none, {{0.0f, 0.0f}, size}, AUIK_TAG_DUMMY), _style({})
        {
        }

        AUIK_EXPORT StyleUpdateFlags update_style() override;
        AUIK_EXPORT void update_layout_min_size_force() override;
        AUIK_EXPORT void update_layout(bool min_size_known) override;
        void draw(DrawCtx &) override {}
        u32 signature() const override { return AUIK_TAG_DUMMY; }
        u32 style_tag() const { return _style_tag_id; }
        void set_style_tag(u32 tag_id)
        {
            _style_tag_id = tag_id;
            _style = {Theme::STYLE_ID_INVALID, tag_id};
        }

    private:
        u32 _style_tag_id = 0u;
        StyleSelector _style;
    };

    inline Block *make_block(u32 id = AUIK_TAG_BLOCK)
    {
        return acul::alloc<Block>(id, WidgetFlagBits::visible | WidgetFlagBits::attachable, AUIK_TAG_BLOCK);
    }

    inline DrawBlock *make_draw_block(u32 id)
    {
        return acul::alloc<DrawBlock>(
            id, WidgetFlagBits::visible | WidgetFlagBits::attachable | WidgetFlagBits::hittable, AUIK_TAG_DRAW_BLOCK);
    }

    inline DrawBlock *make_draw_block() { return make_draw_block(AUIK_TAG_DRAW_BLOCK); }

    inline Dummy *make_dummy(amal::vec2 size = AUIK_SIZE_AUTO)
    {
        return acul::alloc<Dummy>(AUIK_TAG_DUMMY, size, WidgetFlagBits::visible);
    }

    inline WidgetStack *make_widget_stack() { return acul::alloc<WidgetStack>(); }

    inline WidgetRef *make_widget_ref(Widget *target = nullptr) { return acul::alloc<WidgetRef>(target); }

    inline CollapseHeader *make_collapse_header(u32 id, StringView label, bool expanded = true)
    {
        return acul::alloc<CollapseHeader>(
            id, label, expanded, WidgetFlagBits::visible | WidgetFlagBits::attachable | WidgetFlagBits::hittable,
            AUIK_STYLE_TAG_COLLAPSE_HEADER);
    }

    namespace streams
    {
        extern AUIK_EXPORT const umbf::streams::Stream block;
        extern AUIK_EXPORT const umbf::streams::Stream draw_block;
        extern AUIK_EXPORT const umbf::streams::Stream widget_stack;
        extern AUIK_EXPORT const umbf::streams::Stream widget_ref;
        extern AUIK_EXPORT const umbf::streams::Stream collapse_header;
        extern AUIK_EXPORT const umbf::streams::Stream dummy;
    } // namespace streams
} // namespace auik
