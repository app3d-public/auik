#pragma once

#include <acul/memory/alloc.hpp>
#include <acul/string/string.hpp>
#include <acul/vector.hpp>
#include "text.hpp"
#include "widget.hpp"

#define AUIK_TAG_BLOCK                    0x237AFC8Eu
#define AUIK_TAG_LABEL_WIDGET             0xDB9EBBC9u
#define AUIK_DEFAULT_LABEL_WIDGET_LABEL_W 100.0f

namespace auik::v2
{
    constexpr inline WidgetFlags get_default_block_flags()
    {
        return get_default_widget_flags() | WidgetFlagBits::fixed;
    }

    class APPLIB_API Block : public Widget
    {
    public:
        acul::vector<Widget *> children;

        explicit Block(u32 id, f32 spacing = 0.0f, WidgetFlags widget_flags = get_default_block_flags(),
                       Widget *parent = nullptr, u32 tag_id = AUIK_TAG_BLOCK)
            : Widget(id, widget_flags, EventFlagBits::none, parent, {{0.0f, 0.0f}, {0.0f, 0.0f}}, tag_id),
              _spacing(spacing)
        {
        }

        ~Block() override { clear_children(); }

        void clear_children()
        {
            for (auto *child : children)
            {
                if (!child) continue;
                if (child->widget_flags & WidgetFlagBits::attachable) child->on_detach();
                acul::release(child);
            }
            children.clear();
        }

        void add_child(Widget *child)
        {
            assert(child && "child is null");
            child->set_parent(this);
            child->set_focus_parent(this);
            child->update_style();
            children.push_back(child);
        }

        void set_spacing(f32 value)
        {
            if (_spacing == value) return;
            _spacing = value;
            detail::get_context().dirty_flags |= DirtyFlagBits::layout;
        }

        f32 spacing() const { return _spacing; }

        StyleUpdateFlags update_style() override
        {
            StyleUpdateFlags out = StyleUpdateFlagBits::none;
            for (auto *child : children)
            {
                if (!child) continue;
                out |= child->update_style();
            }
            return out;
        }

        void update_layout_min_size() override;
        void update_layout(bool min_size_known) override;

        void translate(const amal::vec2 &delta) override
        {
            if (delta.x == 0.0f && delta.y == 0.0f) return;
            Widget::translate(delta);
            for (auto *child : children)
            {
                if (!child) continue;
                child->translate(delta);
            }
        }

        void rebuild_clip_rects() override
        {
            set_clip_id(content_clip_id());
            for (auto *child : children)
            {
                if (!child) continue;
                child->rebuild_clip_rects();
            }
        }

        void update_depth(const amal::vec2 &depth_range) override
        {
            Widget::update_depth(depth_range);
            amal::vec2 next_range = this->depth_range();
            for (auto *child : children)
            {
                if (!child) continue;
                amal::vec2 child_range{};
                assign_next_depth(next_range, child_range);
                child->update_depth(child_range);
                next_range = child_range;
            }
        }

        void draw(DrawCtx &ctx) override
        {
            if (!(widget_flags & WidgetFlagBits::visible)) return;
            for (auto *child : children)
            {
                if (!child) continue;
                DrawCtx child_ctx = ctx;
                child_ctx.emit_hit_rect = child->is_hittable();
                child->draw(child_ctx);
            }
        }

        u16 content_clip_id() const override { return parent() ? parent()->content_clip_id() : clip_id(); }

        amal::vec4 get_content_clip_rect() const override
        {
            return parent() ? parent()->get_content_clip_rect() : get_clip_rect(content_clip_id());
        }

        void on_attach() override
        {
            detail::get_context().id_map.emplace(id(), this);
            for (auto *child : children)
                if (child && (child->widget_flags & WidgetFlagBits::attachable)) child->on_attach();
        }

        void on_detach() override
        {
            auto &map = detail::get_context().id_map;
            map.erase(id());
            for (auto *child : children)
                if (child && (child->widget_flags & WidgetFlagBits::attachable)) child->on_detach();
        }

    protected:
        virtual f32 child_spacing() const { return _spacing; }

    private:
        f32 _spacing = 0.0f;
    };

    class APPLIB_API LabelWidget final : public Block
    {
    public:
        LabelWidget(u32 id, acul::string text, Widget *widget, f32 label_width = AUIK_DEFAULT_LABEL_WIDGET_LABEL_W,
                    WidgetFlags widget_flags = get_default_block_flags(), Widget *parent = nullptr)
            : Block(id, 0.0f, widget_flags, parent, AUIK_TAG_LABEL_WIDGET), _label_width(label_width)
        {
            _label = acul::alloc<Text>(AUIK_TAG_TEXT, text, amal::vec2{_label_width, 0.0f},
                                       WidgetFlagBits::visible | WidgetFlagBits::fixed, nullptr, AUIK_TAG_TEXT,
                                       detail::TextOverflowMode::ellipsis, detail::TextVerticalAlign::center);
            add_child(_label);
            add_child(widget);
        }

        Text *label() const { return _label; }
        Widget *widget() const { return children.size() > 1 ? children[1] : nullptr; }
        f32 label_width() const { return _label_width; }
        u32 width_key() const { return _width_key; }

        void set_label_width(f32 value)
        {
            if (_label_width == value) return;
            _label_width = value;
            if (_label) _label->set_size({_label_width, _label->size().y});
            detail::get_context().dirty_flags |= DirtyFlagBits::layout;
        }

        void set_width_key(u32 key)
        {
            if (_width_key == key) return;
            _width_key = key;
            const f32 next_width = resolve_label_width();
            if (next_width != _label_width)
            {
                _label_width = next_width;
                if (_label) _label->set_size({_label_width, _label->size().y});
            }
            detail::get_context().dirty_flags |= DirtyFlagBits::layout;
        }

        StyleUpdateFlags update_style() override
        {
            StyleUpdateFlags out = Block::update_style();
            const f32 next_width = resolve_label_width();
            if (next_width != _label_width)
            {
                _label_width = next_width;
                if (_label) _label->set_size({_label_width, _label->size().y});
                out |= StyleUpdateFlagBits::layout;
            }
            return out;
        }

    private:
        f32 resolve_label_width() const
        {
            if (_width_key == 0u) return _label_width;
            const f32 width = get_theme()->get_var<f32>(_width_key);
            return width > 0.0f ? width : AUIK_DEFAULT_LABEL_WIDGET_LABEL_W;
        }

        Text *_label = nullptr;
        f32 _label_width = AUIK_DEFAULT_LABEL_WIDGET_LABEL_W;
        u32 _width_key = 0u;
    };

    inline Block *make_block(u32 id, f32 spacing = 0.0f, Widget *parent = nullptr)
    {
        return acul::alloc<Block>(id, spacing, get_default_block_flags(), parent);
    }

    inline LabelWidget *make_label_widget(u32 id, const acul::string &text, Widget *widget,
                                          f32 label_width = AUIK_DEFAULT_LABEL_WIDGET_LABEL_W, Widget *parent = nullptr)
    {
        return acul::alloc<LabelWidget>(id, text, widget, label_width, get_default_block_flags(), parent);
    }
} // namespace auik::v2
