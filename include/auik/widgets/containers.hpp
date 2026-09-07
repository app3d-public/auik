#pragma once

#include "detail/popup_trigger.hpp"
#include "detail/scrollbar.hpp"
#include "scroll.hpp"
#include "text.hpp"

#define AUIK_TAG_BLOCK                   0x237AFC8Eu
#define AUIK_TAG_DRAW_BLOCK              0xB2F15B07u
#define AUIK_TAG_DUMMY                   0xD5A4C970u
#define AUIK_TAG_COLLAPSE_HEADER         0x565C9C5Eu
#define AUIK_TAG_COLLAPSE_HEADER_STATE   0xC03CA5C4u
#define AUIK_TAG_COLLAPSE_HEADER_TRIGGER 0x4ABB6689u
#define AUIK_TAG_WIDGET_STACK            0xE9A58F31u
#define AUIK_TAG_WIDGET_REF              0x7C5F435Au

namespace auik
{
    namespace detail
    {
        amal::vec2 compute_children_layout_required_size(const acul::vector<Widget *> &children,
                                                         const acul::vector<ChildLayoutFlags> &layouts,
                                                         f32 inline_spacing_x, f32 wrap_width = 0.0f,
                                                         bool refresh_min_size = true);
        AUIK_EXPORT void layout_child_widgets(Widget *layout_owner, const acul::vector<Widget *> &children,
                                              const acul::vector<ChildLayoutFlags> &layouts,
                                              const amal::rect &content_rect, f32 inline_spacing_x);

        struct CollapseHeaderStateData final : public WidgetStateData
        {
            bool expanded = true;

            u32 signature() const noexcept override { return AUIK_TAG_COLLAPSE_HEADER_STATE; }
        };

    } // namespace detail

    class Block : public Widget, public umbf::Block
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
        AUIK_EXPORT void add_children(const acul::vector<Widget *> &new_children,
                                      const acul::vector<ChildLayoutFlags> &layouts = {});
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
        AUIK_EXPORT void invalidate_style() override;
        AUIK_EXPORT bool update_locale() override;
        AUIK_EXPORT void add_state_flags_inherit(WidgetStateFlags flags) override;
        AUIK_EXPORT void remove_state_flags_inherit(WidgetStateFlags flags) override;
        AUIK_EXPORT u32 get_depth_requirement() const override;
        AUIK_EXPORT void update_depth(const amal::vec2 &depth_range) override;
        AUIK_EXPORT void back_hit_depth() override;
        AUIK_EXPORT void restore_hit_depth() override;
        AUIK_EXPORT void draw(DrawCtx &ctx) override;
        u16 content_clip_id() const override { return parent() ? parent()->content_clip_id() : clip_id(); }
        u32 signature() const noexcept override { return AUIK_TAG_BLOCK; }
        umbf::Block *as_snapshot_block() noexcept override { return this; }
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
        void set_content_layout(ChildLayoutFlags value)
        {
            if (_content_layout == value) return;
            _content_layout = value;
            refresh_child_layouts();
        }
        acul::vector<ChildLayoutFlags> _explicit_child_layouts;
        acul::vector<ChildLayoutFlags> _child_layouts;
        ChildLayoutFlags _content_layout = default_child_layout_flags();

    private:
        AUIK_EXPORT void add_child_to_layer(Widget *child, ChildLayoutFlags layout, DepthZone layer);
        bool request_children_update();
        amal::vec2 _explicit_size = AUIK_SIZE_FIT;
        f32 _inline_spacing = 0.0f;
    };

    struct DrawBlockFlagBits
    {
        enum enum_type
        {
            none = 0x0,
            clip_ignores_padding_x = 0x1,
            clip_ignores_padding_y = 0x2
        };

        using flag_bitmask = std::true_type;
    };

    using DrawBlockFlags = acul::flags<DrawBlockFlagBits>;

    namespace detail
    {
        struct BlockScrollData : ScrollData
        {
            StyleID style_id = Theme::STYLE_ID_INVALID;
            Scrollbar *scrollbar_x = nullptr;
            Scrollbar *scrollbar_y = nullptr;
            amal::vec4 content_padding{0.0f};
            bool clip_rect_overridden = false;
            amal::vec4 clip_rect_override{0.0f};
            u16 content_clip_id = 0xFFFFu;
            DrawBlockFlags flags = DrawBlockFlagBits::none;
            u32 scrollbar_track_style_tag = AUIK_STYLE_TAG_SCROLLBAR_TRACK_INTERNAL;
            u32 scrollbar_thumb_style_tag = AUIK_STYLE_TAG_SCROLLBAR_THUMB_INTERNAL;
        };

        class ScrollableBlock : public Block
        {
        public:
            AUIK_EXPORT explicit ScrollableBlock(u32 id, WidgetFlags widget_flags, u32 tag_id);
            AUIK_EXPORT ~ScrollableBlock() override;

            AUIK_EXPORT StyleUpdateFlags update_style() override;
            AUIK_EXPORT void update_layout_min_size_force() override;
            AUIK_EXPORT void update_layout(bool min_size_known) override;
            AUIK_EXPORT void translate(const amal::vec2 &delta) override;
            AUIK_EXPORT void reset_clip_rect_records() override;
            AUIK_EXPORT void rebuild_clip_rects() override;
            AUIK_EXPORT void reset_draw_records() override;
            AUIK_EXPORT void invalidate_style() override;
            AUIK_EXPORT void add_state_flags_inherit(WidgetStateFlags flags) override;
            AUIK_EXPORT void remove_state_flags_inherit(WidgetStateFlags flags) override;
            AUIK_EXPORT u32 get_depth_requirement() const override;
            AUIK_EXPORT void update_depth(const amal::vec2 &depth_range) override;
            AUIK_EXPORT void back_hit_depth() override;
            AUIK_EXPORT void restore_hit_depth() override;
            AUIK_EXPORT void draw(DrawCtx &ctx) override;
            AUIK_EXPORT void on_scroll(const amal::vec2 &delta) override;
            AUIK_EXPORT void on_click(MouseKey key, KeyPressState state, u32 click_count) override;
            AUIK_EXPORT void on_drag(const amal::vec2 &delta, KeyPressState state) override;

            AUIK_EXPORT void set_scrollbar_style_tag(u32 track_tag_id);
            AUIK_EXPORT void set_scrollbar_style_tags(u32 track_tag_id, u32 thumb_tag_id);
            void set_content_padding(const amal::vec4 &value) { _scroll.content_padding = value; }
            amal::vec4 content_padding() const { return _scroll.content_padding; }
            void set_draw_block_flags(DrawBlockFlags flags) { _scroll.flags = flags; }
            DrawBlockFlags draw_block_flags() const { return _scroll.flags; }
            u32 scrollbar_track_style_tag() const { return _scroll.scrollbar_track_style_tag; }
            u32 scrollbar_thumb_style_tag() const { return _scroll.scrollbar_thumb_style_tag; }
            AUIK_EXPORT void override_content_clip_rect(const amal::vec4 &rect);
            bool has_visible_scrollbar_x() const { return _scroll.scrollbar_x && _scroll.scrollbar_x->is_visible(); }
            bool has_visible_scrollbar_y() const { return _scroll.scrollbar_y && _scroll.scrollbar_y->is_visible(); }
            void reset_scroll_offset() { _scroll.content_offset = {0.0f, 0.0f}; }
            AUIK_EXPORT void set_scroll_offset(const amal::vec2 &value);
            const amal::vec2 &scroll_offset() const { return _scroll.content_offset; }
            ScrollData *scroll_data() { return &_scroll; }
            const ScrollData *scroll_data() const { return &_scroll; }
            u16 content_clip_id() const override { return _scroll.content_clip_id; }
            amal::vec4 get_content_clip_rect() const override
            {
                if (_scroll.content_clip_id != 0xFFFFu) return get_clip_rect(_scroll.content_clip_id);
                if (clip_id() != 0xFFFFu) return get_clip_rect(clip_id());
                return parent() ? parent()->get_content_clip_rect() : get_main_viewport_rect();
            }

        protected:
            amal::vec2 scroll_content_size() const { return _scroll.content_size; }
            amal::vec2 scroll_view_size() const { return _scroll.view_size; }
            amal::vec2 content_offset() const { return _scroll.content_offset; }
            void set_scroll_style_id(StyleID style_id) { _scroll.style_id = style_id; }
            StyleID scroll_style_id() const { return _scroll.style_id; }

            AUIK_EXPORT void update_scroll_layout_min_size(const amal::vec4 &margin, const amal::vec4 &padding,
                                                           const amal::vec2 &style_min);
            AUIK_EXPORT void update_scroll_layout(bool min_size_known, const amal::vec4 &margin,
                                                  const amal::vec4 &padding, f32 inline_spacing);
            AUIK_EXPORT void ensure_scrollbars();
            AUIK_EXPORT void update_scroll_clip(const amal::vec2 &content_pos, const amal::vec2 &view_size);
            AUIK_EXPORT void rebuild_scroll_clip_rect();
            AUIK_EXPORT void rebuild_scroll_clip_rect(const amal::vec4 &padding);
            AUIK_EXPORT void request_scroll_layout_update(DrawReasonFlags reason);
            AUIK_EXPORT StyleExtraOverflow scroll_overflow() const;
            AUIK_EXPORT static void scroll_to_callback(ScrollData &data, amal::axis axis);

            BlockScrollData _scroll{};
        };
    } // namespace detail

    class DrawBlock : public detail::ScrollableBlock
    {
    public:
        AUIK_EXPORT explicit DrawBlock(u32 id, WidgetFlags widget_flags, u32 tag_id);
        ~DrawBlock() override = default;

        AUIK_EXPORT StyleUpdateFlags update_style() override;
        AUIK_EXPORT void update_layout_min_size_force() override;
        AUIK_EXPORT void update_layout(bool min_size_known) override;
        AUIK_EXPORT void translate(const amal::vec2 &delta) override;
        AUIK_EXPORT void reset_clip_rect_records() override;
        AUIK_EXPORT void rebuild_clip_rects() override;
        AUIK_EXPORT void reset_draw_records() override;
        AUIK_EXPORT void update_depth(const amal::vec2 &depth_range) override;
        AUIK_EXPORT void draw(DrawCtx &ctx) override;
        u32 signature() const noexcept override { return AUIK_TAG_DRAW_BLOCK; }
        AUIK_EXPORT void set_style_tag(u32 tag_id);
        u32 style_tag() const { return _style_tag_id; }

    protected:
        AUIK_EXPORT f32 resolved_inline_spacing() const override;
        AUIK_EXPORT const Style *draw_style() const;
        AUIK_EXPORT amal::vec4 draw_margin() const;
        AUIK_EXPORT amal::vec4 draw_padding() const;
        AUIK_EXPORT void sync_draw_bounds();

        detail::RectData _bg_rect{};
        DrawDataID _bg_draw_id{};
        u32 _style_tag_id = 0u;
        u32 _border_mask_clear = 0u;
        StyleSelector _style{Theme::STYLE_ID_INVALID, 0u};

        friend class Table;
    };

    class WidgetRef final : public Widget, public umbf::Block
    {
    public:
        AUIK_EXPORT explicit WidgetRef(Widget *target = nullptr,
                                       WidgetFlags widget_flags = WidgetFlagBits::visible |
                                                                  WidgetFlagBits::cache_snapshot);
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
        AUIK_EXPORT void invalidate_style() override;
        AUIK_EXPORT bool update_locale() override;
        AUIK_EXPORT u32 get_depth_requirement() const override;
        AUIK_EXPORT void update_depth(const amal::vec2 &depth_range) override;
        AUIK_EXPORT void back_hit_depth() override;
        AUIK_EXPORT void restore_hit_depth() override;
        AUIK_EXPORT void draw(DrawCtx &ctx) override;
        u32 signature() const noexcept override { return AUIK_TAG_WIDGET_REF; }
        umbf::Block *as_snapshot_block() noexcept override { return this; }
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

    class WidgetStack final : public Widget, public umbf::Block
    {
    public:
        AUIK_EXPORT explicit WidgetStack(WidgetFlags widget_flags = WidgetFlagBits::visible |
                                                                    WidgetFlagBits::attachable |
                                                                    WidgetFlagBits::cache_snapshot);
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

        AUIK_EXPORT StyleUpdateFlags update_style() override;
        AUIK_EXPORT bool accepts_child_style_update(const Widget *child) const override;
        AUIK_EXPORT void update_layout_min_size_force() override;
        AUIK_EXPORT void update_layout(bool min_size_known) override;
        AUIK_EXPORT void translate(const amal::vec2 &delta) override;
        AUIK_EXPORT void reset_clip_rect_records() override;
        AUIK_EXPORT void rebuild_clip_rects() override;
        AUIK_EXPORT void reset_draw_records() override;
        AUIK_EXPORT void invalidate_style() override;
        AUIK_EXPORT bool update_locale() override;
        AUIK_EXPORT void add_state_flags_inherit(WidgetStateFlags flags) override;
        AUIK_EXPORT void remove_state_flags_inherit(WidgetStateFlags flags) override;
        AUIK_EXPORT u32 get_depth_requirement() const override;
        AUIK_EXPORT void update_depth(const amal::vec2 &depth_range) override;
        AUIK_EXPORT void back_hit_depth() override;
        AUIK_EXPORT void restore_hit_depth() override;
        AUIK_EXPORT void draw(DrawCtx &ctx) override;
        AUIK_EXPORT void on_attach() override;
        AUIK_EXPORT void on_detach() override;
        u32 signature() const noexcept override { return AUIK_TAG_WIDGET_STACK; }
        umbf::Block *as_snapshot_block() noexcept override { return this; }
        AUIK_EXPORT amal::vec2 requested_size() const override;
        u16 content_clip_id() const override { return parent() ? parent()->content_clip_id() : clip_id(); }
        amal::vec4 get_content_clip_rect() const override
        {
            return parent() ? parent()->get_content_clip_rect() : get_clip_rect(content_clip_id());
        }

    private:
        void set_child_ref_active(Widget *child, bool active);
        void add_layer_child(Widget *child, DepthZone layer);
        void update_layer_layout(DepthZone layer);

        acul::vector<Widget *> _children;
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
        AUIK_EXPORT void invalidate_style() override;
        AUIK_EXPORT bool update_locale() override;
        AUIK_EXPORT void update_depth(const amal::vec2 &depth_range) override;
        AUIK_EXPORT void back_hit_depth() override;
        AUIK_EXPORT void restore_hit_depth() override;
        AUIK_EXPORT void draw(DrawCtx &ctx) override;
        AUIK_EXPORT void on_click(MouseKey key, KeyPressState state, u32 click_count) override;
        AUIK_EXPORT void on_attach() override;
        AUIK_EXPORT void on_detach() override;
        u32 signature() const noexcept override { return AUIK_TAG_COLLAPSE_HEADER; }

    protected:
        AUIK_EXPORT void on_change(ChangeEvent &event) override;
        AUIK_EXPORT amal::vec2 compute_content_min_size() override;
        AUIK_EXPORT void layout_children(const amal::rect &content_rect) override;

    private:
        void sync_expanded_state();
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

    class Dummy final : public Widget, public umbf::Block
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
        u32 signature() const noexcept override { return AUIK_TAG_DUMMY; }
        umbf::Block *as_snapshot_block() noexcept override { return this; }
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
        return acul::alloc<CollapseHeader>(id, label, expanded,
                                           WidgetFlagBits::visible | WidgetFlagBits::attachable |
                                               WidgetFlagBits::hittable | WidgetFlagBits::cache_snapshot,
                                           AUIK_STYLE_TAG_COLLAPSE_HEADER);
    }

    namespace streams
    {
        extern AUIK_EXPORT const umbf::registry::BlockStream block;
        extern AUIK_EXPORT const umbf::registry::BlockStream draw_block;
        extern AUIK_EXPORT const umbf::registry::BlockStream widget_stack;
        extern AUIK_EXPORT const umbf::registry::BlockStream widget_ref;
        extern AUIK_EXPORT const umbf::registry::BlockStream collapse_header;
        extern AUIK_EXPORT const umbf::registry::BlockStream collapse_header_state;
        extern AUIK_EXPORT const umbf::registry::BlockStream dummy;
    } // namespace streams
} // namespace auik
