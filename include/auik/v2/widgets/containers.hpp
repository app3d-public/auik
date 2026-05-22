#pragma once

#include <acul/memory/alloc.hpp>
#include <acul/string/string.hpp>
#include <acul/vector.hpp>
#include "text.hpp"
#include "widget.hpp"

#define AUIK_TAG_GROUP                    0x2F4D3A13u
#define AUIK_TAG_BLOCK                    0x237AFC8Eu
#define AUIK_TAG_INLINE_BLOCK             0xB2F15B07u
#define AUIK_TAG_DUMMY                    0xD5A4C970u
#define AUIK_TAG_LABEL_WIDGET             0xDB9EBBC9u
#define AUIK_DEFAULT_LABEL_WIDGET_LABEL_W 100.0f

namespace auik::v2
{
    constexpr inline WidgetFlags get_default_group_flags()
    {
        return WidgetFlagBits::visible | WidgetFlagBits::attachable;
    }

    constexpr inline WidgetFlags get_default_block_flags() { return get_default_group_flags(); }
    constexpr inline WidgetFlags get_default_dummy_flags() { return WidgetFlagBits::visible | WidgetFlagBits::fixed; }

    class APPLIB_API Group : public Widget
    {
    public:
        acul::vector<Widget *> children;

        explicit Group(u32 id, WidgetFlags widget_flags = get_default_group_flags(), Widget *parent = nullptr,
                       u32 tag_id = AUIK_TAG_GROUP, u32 style_tag_id = 0u)
            : Widget(id, widget_flags, EventFlagBits::none, parent, {{0.0f, 0.0f}, {0.0f, 0.0f}}, tag_id),
              _style_tag_id(style_tag_id),
              _style({Theme::STYLE_ID_INVALID, style_tag_id})
        {
        }

        ~Group() override { clear_children(); }

        void clear_children();
        void add_child(Widget *child);

        StyleUpdateFlags update_style() override;
        void update_layout_min_size() override;
        void update_layout(bool min_size_known) override;
        void translate(const amal::vec2 &delta) override;
        void rebuild_clip_rects() override;
        void reset_draw_records() override;
        void update_depth(const amal::vec2 &depth_range) override;
        void draw(DrawCtx &ctx) override;
        u16 content_clip_id() const override { return parent() ? parent()->content_clip_id() : clip_id(); }
        amal::vec4 get_content_clip_rect() const override
        {
            return parent() ? parent()->get_content_clip_rect() : get_clip_rect(content_clip_id());
        }
        void on_attach() override;
        void on_detach() override;

    protected:
        const Style *group_style() const;
        amal::vec4 group_margin() const;
        amal::vec4 group_padding() const;
        f32 resolved_inline_spacing() const;
        virtual amal::vec2 compute_content_min_size();
        virtual void layout_children(const amal::vec2 &content_pos, const amal::vec2 &content_size);

    private:
        u32 _style_tag_id = 0u;
        StyleSelector _style;
    };

    class APPLIB_API Block : public Group
    {
    public:
        explicit Block(u32 id, WidgetFlags widget_flags = get_default_block_flags(), Widget *parent = nullptr,
                       u32 tag_id = AUIK_TAG_BLOCK, u32 style_tag_id = 0u)
            : Group(id, widget_flags, parent, tag_id, style_tag_id)
        {
        }

    protected:
        amal::vec2 compute_content_min_size() override;
        void layout_children(const amal::vec2 &content_pos, const amal::vec2 &content_size) override;
    };

    class APPLIB_API InlineBlock : public Block
    {
    public:
        explicit InlineBlock(u32 id, WidgetFlags widget_flags = get_default_block_flags(), Widget *parent = nullptr,
                             u32 style_tag_id = 0u, u32 tag_id = AUIK_TAG_INLINE_BLOCK)
            : Block(id, widget_flags, parent, tag_id, style_tag_id)
        {
        }

    protected:
        amal::vec2 compute_content_min_size() override;
        void layout_children(const amal::vec2 &content_pos, const amal::vec2 &content_size) override;
    };

    class APPLIB_API LabelWidget final : public Block
    {
    public:
        LabelWidget(u32 id, acul::string text, Widget *widget,
                    f32 label_width = AUIK_DEFAULT_LABEL_WIDGET_LABEL_W, u32 label_width_key = 0u,
                    WidgetFlags widget_flags = get_default_block_flags(), Widget *parent = nullptr)
            : Block(id, widget_flags, parent, AUIK_TAG_LABEL_WIDGET),
              _label_width(label_width),
              _width_key(label_width_key)
        {
            _label_width = resolve_label_width();
            _label = acul::alloc<Text>(AUIK_TAG_TEXT, text, amal::vec2{_label_width, 0.0f},
                                       WidgetFlagBits::visible | WidgetFlagBits::fixed, nullptr, AUIK_TAG_TEXT,
                                       detail::TextOverflowMode::ellipsis, detail::TextVerticalAlign::center);
            _label->set_rect_tag_id(AUIK_TAG_LABEL_WIDGET);
            add_child(_label);
            add_child(widget);
        }

        Text *label() const { return _label; }
        Widget *widget() const { return children.size() > 1 ? children[1] : nullptr; }
        f32 label_width() const { return _label_width; }
        u32 width_key() const { return _width_key; }

        void set_label_width(f32 value);
        void set_width_key(u32 key);
        StyleUpdateFlags update_style() override;

    protected:
        amal::vec2 compute_content_min_size() override;
        void layout_children(const amal::vec2 &content_pos, const amal::vec2 &content_size) override;

    private:
        f32 resolve_label_width() const;
        void apply_label_width(f32 width);

        Text *_label = nullptr;
        f32 _label_width = AUIK_DEFAULT_LABEL_WIDGET_LABEL_W;
        u32 _width_key = 0u;
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

    inline Block *make_block(Widget *parent = nullptr, u32 style_tag_id = 0u)
    {
        return acul::alloc<Block>(AUIK_TAG_BLOCK, get_default_block_flags(), parent, AUIK_TAG_BLOCK, style_tag_id);
    }

    inline InlineBlock *make_inline_block(Widget *parent = nullptr, u32 style_tag_id = 0u)
    {
        return acul::alloc<InlineBlock>(AUIK_TAG_INLINE_BLOCK, get_default_block_flags(), parent, style_tag_id);
    }

    inline Group *make_group(Widget *parent = nullptr, u32 style_tag_id = 0u)
    {
        return acul::alloc<Group>(AUIK_TAG_GROUP, get_default_group_flags(), parent, AUIK_TAG_GROUP, style_tag_id);
    }

    inline Dummy *make_dummy(amal::vec2 size = {0.0f, 0.0f}, Widget *parent = nullptr, u32 style_tag_id = 0u)
    {
        return acul::alloc<Dummy>(AUIK_TAG_DUMMY, size, get_default_dummy_flags(), parent, style_tag_id);
    }

    inline LabelWidget *make_label_widget(u32 id, const acul::string &text, Widget *widget,
                                          f32 label_width = AUIK_DEFAULT_LABEL_WIDGET_LABEL_W, Widget *parent = nullptr,
                                          u32 label_width_key = 0u)
    {
        return acul::alloc<LabelWidget>(id, text, widget, label_width, label_width_key, get_default_block_flags(),
                                        parent);
    }
} // namespace auik::v2
