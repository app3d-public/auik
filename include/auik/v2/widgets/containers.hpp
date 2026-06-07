#pragma once

#include "detail/popup_trigger.hpp"
#include "detail/scrollbar.hpp"
#include "text.hpp"

#define AUIK_TAG_BLOCK                   0x237AFC8Eu
#define AUIK_TAG_SCROLL_BLOCK            0xB2F15B07u
#define AUIK_TAG_DUMMY                   0xD5A4C970u
#define AUIK_TAG_COLLAPSE_HEADER         0x565C9C5Eu
#define AUIK_TAG_COLLAPSE_HEADER_TRIGGER 0x4ABB6689u

namespace auik::v2
{
    namespace detail
    {
        APPLIB_API amal::vec2 compute_children_layout_required_size(const acul::vector<Widget *> &children,
                                                                    const acul::vector<ChildLayoutFlags> &layouts,
                                                                    f32 inline_spacing_x, f32 wrap_width = 0.0f,
                                                                    bool refresh_min_size = true);
        APPLIB_API void layout_child_widgets(const acul::vector<Widget *> &children,
                                             const acul::vector<ChildLayoutFlags> &layouts,
                                             const amal::vec2 &content_pos, f32 available_width, f32 inline_spacing_x);
    } // namespace detail

    constexpr inline WidgetFlags get_default_block_flags()
    {
        return WidgetFlagBits::visible | WidgetFlagBits::attachable;
    }

    constexpr inline WidgetFlags get_default_dummy_flags()
    {
        return WidgetFlagBits::visible | WidgetFlagBits::fixed_layout;
    }

    class APPLIB_API Block : public Widget
    {
    public:
        acul::vector<Widget *> children;

        explicit Block(u32 id, WidgetFlags widget_flags = get_default_block_flags(), Widget *parent = nullptr,
                       u32 tag_id = AUIK_TAG_BLOCK)
            : Widget(id, widget_flags, EventFlagBits::none, parent, {{0.0f, 0.0f}, {0.0f, 0.0f}}, tag_id)
        {
        }

        ~Block() override { clear_children(); }

        void clear_children();
        void add_child(Widget *child, ChildLayoutFlags layout = default_child_layout_flags());
        void set_width(f32 value);
        void set_height(f32 value);
        void set_size(const amal::vec2 &value);
        void set_kv_width(u32 key);
        void set_kv_height(u32 key);
        void set_kv_size(u32 key);
        void set_kv_size(u32 width_key, u32 height_key);

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
        u16 content_clip_id() const override { return parent() ? parent()->content_clip_id() : clip_id(); }
        amal::vec4 get_content_clip_rect() const override
        {
            return parent() ? parent()->get_content_clip_rect() : get_clip_rect(content_clip_id());
        }
        void on_attach() override;
        void on_detach() override;

    protected:
        virtual f32 resolved_inline_spacing() const;
        virtual amal::vec2 compute_content_min_size();
        virtual void layout_children(const amal::vec2 &content_pos, const amal::vec2 &content_size);
        void update_layout_min_size_with(const amal::vec4 &margin, const amal::vec4 &padding);
        void update_layout_with(bool min_size_known, const amal::vec4 &margin, const amal::vec4 &padding);
        void add_child_with_flags(Widget *child, ChildLayoutFlags layout);
        bool has_explicit_width() const;
        bool has_explicit_height() const;
        f32 resolved_explicit_width() const;
        f32 resolved_explicit_height() const;
        void update_size_fixed_flag();
        acul::vector<ChildLayoutFlags> _child_layouts;

    private:
        amal::vec2 _explicit_size = AUIK_SIZE_IGNORE;
        amal::uvec2 _size_key{0u, 0u};
        u32 _size_vec_key = 0u;
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

    class APPLIB_API DrawBlock : public Block
    {
    public:
        explicit DrawBlock(u32 id, WidgetFlags widget_flags = get_default_block_flags() | WidgetFlagBits::hittable,
                           Widget *parent = nullptr, u32 tag_id = AUIK_TAG_SCROLL_BLOCK, u32 style_tag_id = 0u);
        ~DrawBlock() override;

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
        void on_scroll(const amal::vec2 &delta) override;
        void on_click(MouseKey key, KeyPressState state, u32 click_count) override;
        void on_drag(const amal::vec2 &delta, KeyPressState state) override;
        void set_scrollbars_enabled(bool x, bool y);
        void set_scrollbar_style_tag(u32 track_tag_id);
        void set_scrollbar_style_tags(u32 track_tag_id, u32 thumb_tag_id);
        void set_content_padding(const amal::vec4 &value) { _content_padding = value; }
        void set_draw_block_flags(DrawBlockFlags flags) { _draw_flags = flags; }
        DrawBlockFlags draw_block_flags() const { return _draw_flags; }
        void set_style_tag(u32 tag_id);
        void override_content_clip_rect(const amal::vec4 &rect);
        void clear_content_clip_rect_override();
        bool has_visible_scrollbar_x() const { return _scrollbar_x && _scrollbar_x->is_visible(); }
        bool has_visible_scrollbar_y() const { return _scrollbar_y && _scrollbar_y->is_visible(); }
        void reset_scroll_offset() { _content_offset = {0.0f, 0.0f}; }
        u16 content_clip_id() const override { return _content_clip_id; }
        amal::vec4 get_content_clip_rect() const override
        {
            if (_content_clip_id != 0xFFFFu) return get_clip_rect(_content_clip_id);
            if (clip_id() != 0xFFFFu) return get_clip_rect(clip_id());
            return parent() ? parent()->get_content_clip_rect() : get_main_viewport();
        }

    protected:
        amal::vec2 scroll_content_size() const { return _scroll_content_size; }
        amal::vec2 scroll_view_size() const { return _scroll_view_size; }
        amal::vec2 content_offset() const { return _content_offset; }

    protected:
        void ensure_scrollbars();
        void update_scroll_clip(const amal::vec2 &content_pos, const amal::vec2 &view_size);
        void rebuild_scroll_clip_rect();
        void request_scroll_layout_update(DrawReasonFlags reason);
        f32 resolved_inline_spacing() const override;
        const Style *draw_style() const;
        amal::vec4 draw_margin() const;
        amal::vec4 draw_padding() const;

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
        u32 _scrollbar_track_style_tag = AUIK_TAG_SCROLLBAR_TRACK_INTERNAL;
        u32 _scrollbar_thumb_style_tag = AUIK_TAG_SCROLLBAR_THUMB_INTERNAL;
    };

    class APPLIB_API CollapseHeader final : public Block
    {
    public:
        explicit CollapseHeader(u32 id, acul::string label, bool expanded = true,
                                WidgetFlags widget_flags = get_default_block_flags() | WidgetFlagBits::hittable,
                                Widget *parent = nullptr, u32 style_tag_id = AUIK_STYLE_TAG_COLLAPSE_HEADER);
        ~CollapseHeader() override;

        void set_label(acul::string value);
        const acul::string &label() const;
        void set_expanded(bool value);
        bool expanded() const { return _expanded; }
        void toggle() { set_expanded(!_expanded); }
        void set_style_tag(u32 tag_id);
        u32 style_tag() const { return _style.tag_id; }
        void set_closed_style_tag(u32 tag_id);
        u32 closed_style_tag() const { return _closed_style_tag; }
        void set_content_style_tag(u32 tag_id);
        u32 content_style_tag() const { return _content_style.tag_id; }
        void set_trigger_style_tag(u32 tag_id);
        u32 trigger_style_tag() const { return _trigger_style_tag; }

        StyleUpdateFlags update_style() override;
        void update_layout_min_size() override;
        void update_layout(bool min_size_known) override;
        void translate(const amal::vec2 &delta) override;
        void rebuild_clip_rects() override;
        void reset_draw_records() override;
        void update_depth(const amal::vec2 &depth_range) override;
        void back_hit_depth() override;
        void restore_hit_depth() override;
        void draw(DrawCtx &ctx) override;
        void on_click(MouseKey key, KeyPressState state, u32 click_count) override;
        void on_attach() override;
        void on_detach() override;

    protected:
        amal::vec2 compute_content_min_size() override;
        void layout_children(const amal::vec2 &content_pos, const amal::vec2 &content_size) override;

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

    class APPLIB_API Dummy final : public Widget
    {
    public:
        explicit Dummy(u32 id, amal::vec2 size = {0.0f, 0.0f}, WidgetFlags widget_flags = get_default_dummy_flags(),
                       Widget *parent = nullptr, u32 style_tag_id = 0u)
            : Widget(id, widget_flags, EventFlagBits::none, parent, {{0.0f, 0.0f}, size}, AUIK_TAG_DUMMY),
              _style_tag_id(style_tag_id),
              _style({Theme::STYLE_ID_INVALID, style_tag_id})
        {
        }

        StyleUpdateFlags update_style() override;
        void update_layout_min_size() override;
        void update_layout(bool min_size_known) override;
        void draw(DrawCtx &) override {}

    private:
        u32 _style_tag_id = 0u;
        StyleSelector _style;
    };

    inline Block *make_block(Widget *parent = nullptr)
    {
        return acul::alloc<Block>(AUIK_TAG_BLOCK, get_default_block_flags(), parent, AUIK_TAG_BLOCK);
    }

    inline DrawBlock *make_draw_block(Widget *parent = nullptr, u32 style_tag_id = 0u)
    {
        return acul::alloc<DrawBlock>(AUIK_TAG_SCROLL_BLOCK, get_default_block_flags() | WidgetFlagBits::hittable,
                                      parent, AUIK_TAG_SCROLL_BLOCK, style_tag_id);
    }

    inline Dummy *make_dummy(amal::vec2 size = {0.0f, 0.0f}, Widget *parent = nullptr, u32 style_tag_id = 0u)
    {
        return acul::alloc<Dummy>(AUIK_TAG_DUMMY, size, get_default_dummy_flags(), parent, style_tag_id);
    }

    inline CollapseHeader *make_collapse_header(u32 id, const acul::string &label, bool expanded = true,
                                                Widget *parent = nullptr)
    {
        return acul::alloc<CollapseHeader>(id, label, expanded, get_default_block_flags() | WidgetFlagBits::hittable,
                                           parent, AUIK_STYLE_TAG_COLLAPSE_HEADER);
    }
} // namespace auik::v2
