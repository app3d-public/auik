#include <auik/auik.hpp>
#include <auik/detail/depth.hpp>
#include <auik/pipelines.hpp>
#include <auik/sound.hpp>
#include <auik/widgets/checkbox.hpp>
#include <auik/widgets/containers.hpp>
#include <auik/widgets/image.hpp>
#include <auik/widgets/modal_window.hpp>
#include <auik/widgets/text.hpp>
#include <auik/widgets/text_button.hpp>
#include "../core/session_stream_utils.hpp"

#define MODAL_INTERNAL_WIDGET_FLAGS          (WidgetFlagBits::visible)
#define MODAL_INTERNAL_HITTABLE_WIDGET_FLAGS (WidgetFlagBits::visible | WidgetFlagBits::hittable)

namespace auik
{
    static inline bool point_in_rect(const amal::vec2 &point, const amal::rect &rect)
    {
        return point.x >= rect.offset.x && point.y >= rect.offset.y && point.x < rect.offset.x + rect.size.x &&
               point.y < rect.offset.y + rect.size.y;
    }

    class ModalControlsRow final : public Widget
    {
    public:
        ModalControlsRow(u32 owner_id, Checkbox *checkbox, Text *label, acul::vector<TextButton *> buttons)
            : Widget(AUIK_MODAL_CONTROLS_ID, MODAL_INTERNAL_WIDGET_FLAGS, EventFlagBits::none, {},
                     AUIK_STYLE_TAG_MODAL_CONTROLS_AREA),
              _owner_id(owner_id),
              _controls_style({Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_MODAL_CONTROLS_AREA}),
              _checkbox(checkbox),
              _label(label),
              _buttons(std::move(buttons))
        {
            add_control(_checkbox, make_layout_flags(ChildLayout::inline_, HAlign::left, VAlign::center));
            add_control(_label, make_layout_flags(ChildLayout::inline_, HAlign::left, VAlign::center));
            configure_control_hit_rect(_checkbox, AUIK_MODAL_BATCH_ID, 0u);
            for (auto *button : _buttons)
                add_control(button, make_layout_flags(ChildLayout::inline_, HAlign::right, VAlign::center));
            for (u32 i = 0; i < _buttons.size(); ++i) configure_control_hit_rect(_buttons[i], AUIK_MODAL_BUTTON_ID, i);
        }

        ~ModalControlsRow() override
        {
            for (auto *child : _children)
                if (child) acul::release(child);
        }

        StyleUpdateFlags update_style() override
        {
            const u32 parent_id = parent() ? parent()->id() : 0u;
            StyleUpdateFlags flags = resolve_style_selector(_controls_style, id(), parent_id, style_state());
            const auto transition = detail::get_widget_style_selector_transition(_owner_id);
            if (_label) flags |= _label->update_style();
            if (_checkbox)
            {
                const bool selected =
                    transition.current_id.widget_id == _owner_id && transition.current_id.tag_id == AUIK_MODAL_BATCH_ID;
                const StyleState checkbox_state = selected ? transition.current_state : StyleState::normal;
                _checkbox->set_style_state(checkbox_state);
                flags |= _checkbox->update_style();
            }
            for (u32 i = 0; i < _buttons.size(); ++i)
            {
                auto *button = _buttons[i];
                if (!button) continue;
                const bool selected = transition.current_id.widget_id == _owner_id &&
                                      transition.current_id.tag_id == AUIK_MODAL_BUTTON_ID &&
                                      transition.current_id.element_id == i;
                button->set_style_state(selected ? transition.current_state : StyleState::normal);
                flags |= button->update_style();
            }
            return flags;
        }

        void update_layout_min_size() override
        {
            const auto &controls_style = get_theme()->get_style(_controls_style.id);
            const amal::vec4 controls_margin = controls_style.margin();
            const amal::vec4 controls_padding = controls_style.padding();
            const f32 gap = inline_gap();
            const amal::vec2 row_size = detail::compute_children_layout_required_size(_children, _child_layouts, gap);
            set_required_size(
                {controls_margin.x + controls_margin.z + controls_padding.x + controls_padding.z + row_size.x,
                 controls_margin.y + controls_margin.w + controls_padding.y + controls_padding.w + row_size.y});
        }
        void update_layout(bool min_size_known) override
        {
            if (!min_size_known) update_layout_min_size();
            assert(parent() && "ModalControlsRow must have parent");
            set_clip_id(parent()->content_clip_id());

            const auto &controls_style = get_theme()->get_style(_controls_style.id);
            const amal::vec4 controls_margin = controls_style.margin();
            const amal::vec4 controls_padding = controls_style.padding();
            const f32 gap = inline_gap();

            const amal::vec2 origin = position();
            const f32 outer_width = size().x > 0.0f ? size().x : required_size().x;
            const f32 outer_height = required_size().y;
            set_position({origin.x + controls_margin.x, origin.y + controls_margin.y});
            set_layout_size({amal::max(outer_width - controls_margin.x - controls_margin.z, 0.0f),
                             amal::max(outer_height - controls_margin.y - controls_margin.w, 0.0f)});
            Widget::update_layout(true);

            const amal::vec2 content_pos = position() + amal::vec2{controls_padding.x, controls_padding.y};
            const amal::vec2 content_size = {amal::max(size().x - controls_padding.x - controls_padding.z, 0.0f),
                                             amal::max(size().y - controls_padding.y - controls_padding.w, 0.0f)};
            detail::layout_child_widgets(this, _children, _child_layouts, {content_pos, content_size}, gap);
        }

        void translate(const amal::vec2 &delta) override
        {
            if (delta.x == 0.0f && delta.y == 0.0f) return;
            Widget::translate(delta);
            for (auto *child : _children)
                if (child) child->translate(delta);
        }

        void rebuild_clip_rects() override
        {
            if (parent()) set_clip_id(parent()->content_clip_id());
            for (auto *child : _children)
                if (child) child->rebuild_clip_rects();
        }

        void reset_draw_records() override
        {
            for (auto *child : _children)
                if (child) child->reset_draw_records();
        }

        void update_depth(const amal::vec2 &depth_range) override
        {
            Widget::update_depth(depth_range);
            for (auto *child : _children)
                if (child) child->update_depth(depth_range);
        }

        void back_hit_depth() override
        {
            Widget::back_hit_depth();
            for (auto *child : _children)
                if (child) child->back_hit_depth();
        }

        void restore_hit_depth() override
        {
            Widget::restore_hit_depth();
            for (auto *child : _children)
                if (child) child->restore_hit_depth();
        }

        void draw(DrawCtx &ctx) override
        {
            if (_checkbox)
            {
                DrawCtx checkbox_ctx = ctx;
                configure_control_hit_rect(_checkbox, AUIK_MODAL_BATCH_ID, 0u);
                _checkbox->draw_local(checkbox_ctx);
            }
            if (_label)
            {
                DrawCtx label_ctx = ctx;
                label_ctx.is_hit_allowed = false;
                _label->draw_local(label_ctx);
            }
            for (u32 i = 0; i < _buttons.size(); ++i)
            {
                auto *button = _buttons[i];
                if (!button) continue;
                DrawCtx button_ctx = ctx;
                configure_control_hit_rect(button, AUIK_MODAL_BUTTON_ID, i);
                button->draw_local(button_ctx);
            }
        }

        u16 content_clip_id() const override { return parent() ? parent()->content_clip_id() : clip_id(); }
        amal::vec4 get_content_clip_rect() const override
        { return parent() ? parent()->get_content_clip_rect() : get_clip_rect(content_clip_id()); }

    private:
        u32 _owner_id = 0u;
        StyleSelector _controls_style;
        Checkbox *_checkbox = nullptr;
        Text *_label = nullptr;
        acul::vector<Widget *> _children;
        acul::vector<ChildLayoutFlags> _child_layouts;
        acul::vector<TextButton *> _buttons;

        void add_control(Widget *child, ChildLayoutFlags layout)
        {
            if (!child) return;
            child->set_parent(this);
            child->set_focus_parent(this);
            _children.push_back(child);
            _child_layouts.push_back(layout);
        }

        void configure_control_hit_rect(Widget *child, u32 tag_id, u32 element_id) const
        {
            if (!child) return;
            auto &rect = child->get_rect();
            rect.id.widget_id = _owner_id;
            rect.id.tag_id = tag_id;
            rect.id.element_id = element_id;
        }

        f32 inline_gap() const
        {
            if (!parent()) return 4.0f;
            auto *theme = get_theme();
            const u32 parent_id = parent()->parent() ? parent()->parent()->id() : 0u;
            const auto style_id = theme->get_resolved_style(AUIK_STYLE_TAG_WINDOW, parent()->id(), parent_id);
            return amal::max(theme->get_style(style_id).inline_spacing(), 0.0f);
        }
    };

    ModalWindow::ModalWindow(u32 id, acul::string title, const amal::rect &bounds, WindowFlags window_flags,
                             WidgetFlags widget_flags)
        : Window(id, std::move(title), bounds, window_flags, widget_flags)
    { set_window_style_tag(AUIK_STYLE_TAG_MODAL_WINDOW); }

    void ModalWindow::update_layout(bool min_size_known) { Window::update_layout(min_size_known); }

    void ModalWindow::close()
    {
        if (_queue) _queue->remove_modal(this);
    }

    void ModalWindow::invoke_close_callback()
    {
        if (_on_close) _on_close();
    }

    ModalQueue::ModalQueue(u32 id, WidgetFlags widget_flags)
        : Widget(id, widget_flags, EventFlagBits::click | EventFlagBits::hover | EventFlagBits::drag, {},
                 AUIK_TAG_MODAL_BACKDROP)
    {
    }

    ModalQueue::~ModalQueue() { clear_modal(false); }

    bool ModalQueue::is_attached() const
    {
        auto &map = detail::get_context().id_map;
        auto it = map.find(id());
        return it != map.end() && it->second == this;
    }

    void ModalQueue::request_redraw()
    {
        auto &ctx = detail::get_context();
        ctx.dirty_flags |= DirtyFlagBits::redraw;
        detail::mark_host_refresh_request();
    }

    void ModalQueue::update_modal_draw_commands(DrawReasonFlags reason)
    {
        request_redraw();
        if (!is_attached()) return;
        update_draw_commands(reason);
    }

    void ModalQueue::relayout_modal_draw_commands()
    {
        request_redraw();
        if (!is_attached()) return;
        update_layout(false);
        update_active_modal_depth();
        update_draw_commands(DrawReasonBits::layout | DrawReasonBits::record);
    }

    void ModalQueue::invalidate_modal_draw_commands()
    {
        if (!is_attached()) return;
        invalidate_draw_commands(DrawReasonBits::full_redraw);
    }

    void ModalQueue::request_modal_rebuild()
    {
        if (!is_attached())
        {
            rebuild_modal();
            return;
        }
        request_redraw();
        if (_rebuild_pending) return;
        _rebuild_pending = true;
        add_render_command<detail::DeferredEventTraits>(this, [this]() {
            _rebuild_pending = false;
            rebuild_modal();
        });
    }

    void ModalQueue::clear_modal(bool invalidate_draw)
    {
        if (!_modal) return;
        if (invalidate_draw) invalidate_modal_draw_commands();
        _modal->set_queue(nullptr);
        acul::release(_modal);
        _modal = nullptr;
        if (invalidate_draw) request_redraw();
    }

    bool ModalQueue::remove_modal(ModalWindow *modal, bool invoke_callback)
    {
        if (!modal || modal != _modal) return false;
        invalidate_modal_draw_commands();
        if (invoke_callback) modal->invoke_close_callback();
        modal->set_queue(nullptr);
        _modal = nullptr;
        acul::release(modal);
        focus_widget(nullptr);
        request_redraw();
        return true;
    }

    void ModalQueue::close_all_windows_now()
    {
        clear_modal();
        _messages.clear();
        _group_counts.clear();
        _batch_apply = false;
        _rebuild_pending = false;
        _close_all_pending = false;
        _prevent_close_count = 0;
        focus_widget(nullptr);
        request_redraw();
    }

    StyleUpdateFlags ModalQueue::update_style()
    {
        StyleUpdateFlags out = StyleUpdateFlagBits::redraw;
        if (_modal)
        {
            out |= _modal->update_style();
            for (auto *child : _modal->children)
            {
                if (!child) continue;
                out |= child->update_style();
            }
        }
        return out;
    }

    void ModalQueue::update_layout_min_size()
    {
        if (!active_modal())
        {
            set_required_size({0.0f, 0.0f});
            return;
        }
        const auto viewport = get_main_viewport_rect();
        set_required_size({viewport.z, viewport.w});
        active_modal()->update_layout_min_size();
    }

    void ModalQueue::layout_active_modal()
    {
        auto *modal = active_modal();
        if (!modal) return;

        modal->update_layout_min_size();
        amal::vec2 modal_size = modal->size();
        const amal::vec2 required_size = modal->required_size();
        if (modal_size.x <= 0.0f) modal_size.x = required_size.x;
        if (modal_size.y <= 0.0f) modal_size.y = required_size.y;

        for (u32 i = 0; i < 3u; ++i)
        {
            modal->set_layout_size(modal_size);
            modal->update_layout(true);
            modal->update_layout_min_size();

            const amal::vec2 refined_required_size = modal->required_size();
            const amal::vec2 refined_size = {amal::max(modal_size.x, refined_required_size.x), refined_required_size.y};
            if (refined_size == modal_size) break;
            modal_size = refined_size;
        }
        modal->set_layout_size(modal_size);
        modal->set_min_size({modal_size.x, 0.0f});

        const amal::vec2 manager_pos = position();
        const amal::vec2 manager_size = size();
        const amal::vec2 centered_pos = {manager_pos.x + amal::max((manager_size.x - modal_size.x) * 0.5f, 0.0f),
                                         manager_pos.y + amal::max((manager_size.y - modal_size.y) * 0.5f, 0.0f)};
        modal->set_position(centered_pos);
        modal->update_layout(true);
    }

    void ModalQueue::update_layout(bool min_size_known)
    {
        if (!min_size_known) update_layout_min_size();
        if (!active_modal())
        {
            set_position({0.0f, 0.0f});
            set_layout_size({0.0f, 0.0f});
            Widget::update_layout(true);
            return;
        }
        const auto viewport = get_main_viewport_rect();
        set_position({viewport.x, viewport.y});
        set_layout_size({viewport.z, viewport.w});
        Widget::update_layout(true);
        const auto bounds_r = bounds();
        ensure_own_clip_rect({bounds_r.offset.x, bounds_r.offset.y, bounds_r.size.x, bounds_r.size.y});

        layout_active_modal();
    }

    void ModalQueue::translate(const amal::vec2 &delta)
    {
        if (delta.x == 0.0f && delta.y == 0.0f) return;
        Widget::translate(delta);
        if (_modal) _modal->translate(delta);
    }

    void ModalQueue::reset_clip_rect_records()
    {
        Widget::reset_clip_rect_records();
        if (_modal) _modal->reset_clip_rect_records();
    }

    void ModalQueue::rebuild_clip_rects()
    {
        _rect.clip_id = 0xFFFFu;
        if (!active_modal())
        {
            invalidate_hit_rect(_backdrop_draw);
            return;
        }
        const auto bounds_r = bounds();
        ensure_own_clip_rect({bounds_r.offset.x, bounds_r.offset.y, bounds_r.size.x, bounds_r.size.y});
        invalidate_hit_rect(_backdrop_draw);

        if (auto *modal = active_modal()) modal->rebuild_clip_rects();
    }

    void ModalQueue::reset_draw_records()
    {
        _backdrop_draw = {};
        if (_modal) _modal->reset_draw_records();
    }

    void ModalQueue::update_active_modal_depth()
    {
        if (!active_modal()) return;
        const amal::vec2 modal_root_range = detail::depth_foreground_range(this->depth_range());
        _rect.depth = modal_root_range.x;
        _rect.hit_depth = _rect.depth;
        amal::vec2 modal_range{};
        assign_next_depth(modal_root_range, modal_range);
        active_modal()->update_depth(modal_range);
    }

    void ModalQueue::update_depth(const amal::vec2 &depth_range)
    {
        Widget::update_depth(depth_range);
        update_active_modal_depth();
    }

    void ModalQueue::back_hit_depth()
    {
        Widget::back_hit_depth();
        _rect.hit_depth = get_rect().hit_depth;
        if (active_modal()) active_modal()->back_hit_depth();
    }

    void ModalQueue::restore_hit_depth()
    {
        Widget::restore_hit_depth();
        _rect.hit_depth = _rect.depth;
        if (active_modal()) active_modal()->restore_hit_depth();
    }

    void ModalQueue::draw(DrawCtx &ctx)
    {
        if (!is_visible() && !(ctx.reason & DrawReasonBits::invalidate)) return;

        auto *modal = active_modal();
        if (!modal) return;

        const auto viewport = get_main_viewport_rect();
        const amal::rect backdrop_rect{{viewport.x, viewport.y}, {viewport.z, viewport.w}};
        auto backdrop_hit = get_rect();
        backdrop_hit.bounds = backdrop_rect;

        QuadsInstanceData backdrop{};
        backdrop.rect = backdrop_rect;
        backdrop.z_order = get_z_order();
        const auto backdrop_style_id =
            get_theme()->get_resolved_style(AUIK_STYLE_TAG_MODAL_BACKDROP, AUIK_TAG_MODAL_BACKDROP, id());
        const bool backdrop_visible =
            fill_quads_instance_by_style(get_theme()->get_style(backdrop_style_id), clip_id(), backdrop);
        emit_quads_instance(ctx, get_overlay_quads_stream(), _backdrop_draw, backdrop, backdrop_hit, backdrop_visible,
                            true);

        auto &modal_hit = modal->get_rect();
        modal_hit.id = make_element_id(id(), AUIK_TAG_MODAL_WINDOW);

        DrawCtx modal_ctx = ctx;
        modal_ctx.post_fx_chain = nullptr;
        modal->draw_local(modal_ctx);
    }

    void ModalQueue::on_click(MouseKey key, KeyPressState state, u32 click_count)
    {
        (void)click_count;
        if (key != MouseKey::left) return;
        auto *modal = active_modal();
        if (!modal) return;
        const auto hover_id = detail::get_context().hover_id;
        if (hover_id.widget_id == id() && hover_id.tag_id == AUIK_MODAL_BATCH_ID && state == KeyPressState::press)
        {
            add_render_command<detail::ClickEventTraits>(this, [this]() {
                _batch_apply = !_batch_apply;
                update_modal_draw_commands(DrawReasonBits::external);
            });
            return;
        }
        if (hover_id.widget_id == id() && hover_id.tag_id == AUIK_MODAL_BUTTON_ID && state == KeyPressState::release)
        {
            const u32 button_index = hover_id.element_id;
            add_render_command<detail::ClickEventTraits>(this, [this, button_index]() { apply_button(button_index); });
            return;
        }
        if (state != KeyPressState::press) return;
        if (point_in_rect(get_mouse_pos(), modal->bounds())) return;
        play_system_hand_sound(get_sound_context());
    }

    void ModalQueue::on_drag(const amal::vec2 &delta, KeyPressState state)
    {
        auto *modal = active_modal();
        if (!modal) return;
        const auto drag_id = detail::get_context().io.drag_id;
        if (state != KeyPressState::repeat) return;
        if (drag_id.widget_id != id() || drag_id.tag_id != AUIK_TAG_MODAL_WINDOW) return;
        modal->translate(delta);
        update_modal_draw_commands(DrawReasonBits::external);
    }

    void ModalQueue::set_icon(Image *image)
    {
        if (!image)
        {
            _icon = {};
            rebuild_modal();
            return;
        }
        set_icon(image->texture_id(), image->size(), {image->uv_offset(), image->uv_size()});
    }

    void ModalQueue::set_icon(TextureID texture_id, const amal::vec2 &size, const amal::rect &uv_rect)
    {
        _icon.texture_id = texture_id;
        _icon.size = size;
        _icon.uv_rect = uv_rect;
        _icon.valid = texture_id.handle != 0 && size.x > 0.0f && size.y > 0.0f;
        rebuild_modal();
    }

    void ModalQueue::set_modal_width(f32 value)
    {
        _modal_width = amal::max(value, 0.0f);
        rebuild_modal();
    }

    void ModalQueue::push(ModalMessage &&message)
    {
        if (message.prevent_close) ++_prevent_close_count;
        if (message.group_id != 0u) ++_group_counts[message.group_id];
        _messages.push_back(std::move(message));
        request_modal_rebuild();
    }

    void ModalQueue::close_all_windows()
    {
        if (_close_all_pending) return;
        _close_all_pending = true;
        add_render_command<detail::DeferredEventTraits>(this, [this]() { close_all_windows_now(); });
        detail::mark_host_refresh_request();
    }

    u32 ModalQueue::group_count(u32 group_id) const
    {
        if (group_id == 0u) return 0u;
        auto it = _group_counts.find(group_id);
        return it == _group_counts.end() ? 0u : it->second;
    }

    void ModalQueue::decrement_group_count(u32 group_id)
    {
        if (group_id == 0u) return;
        auto it = _group_counts.find(group_id);
        if (it == _group_counts.end()) return;
        if (it->second <= 1u) _group_counts.erase(it);
        else --it->second;
    }

    void ModalQueue::rebuild_modal()
    {
        _rebuild_pending = false;
        clear_modal();
        if (_messages.empty()) return;

        auto &message = _messages.front();
        f32 modal_width = _modal_width;

        auto *modal = acul::alloc<ModalWindow>(AUIK_TAG_MODAL_WINDOW, message.header,
                                               amal::rect{{0.0f, 0.0f}, {modal_width, 0.0f}}, WindowFlagBits::movable,
                                               WidgetFlagBits::visible);
        modal->set_min_size({modal_width, 0.0f});

        auto *theme = get_theme();
        const auto &modal_style =
            theme->get_style(theme->get_resolved_style(AUIK_STYLE_TAG_MODAL_WINDOW, AUIK_TAG_MODAL_WINDOW, id()));
        const auto &message_area_style = theme->get_style(
            theme->get_resolved_style(AUIK_STYLE_TAG_MODAL_MESSAGE_AREA, AUIK_MODAL_MESSAGE_ID, AUIK_TAG_MODAL_WINDOW));
        f32 text_width = modal_width - modal_style.padding().x - modal_style.padding().z -
                         message_area_style.margin().x - message_area_style.margin().z -
                         message_area_style.padding().x - message_area_style.padding().z;
        if (_icon.valid)
        {
            const auto &icon_style = theme->get_style(
                theme->get_resolved_style(AUIK_STYLE_TAG_MODAL_ICON, AUIK_MODAL_DEFAULT_ICON, AUIK_MODAL_MESSAGE_ID));
            text_width -=
                _icon.size.x + icon_style.margin().x + icon_style.margin().z + message_area_style.inline_spacing();
        }
        text_width = amal::max(text_width, 120.0f);
        Image *icon = nullptr;
        if (_icon.valid)
            icon = acul::alloc<Image>(AUIK_MODAL_DEFAULT_ICON, _icon.texture_id, _icon.size, _icon.uv_rect,
                                      MODAL_INTERNAL_WIDGET_FLAGS);
        if (icon) icon->set_rect_tag_id(AUIK_STYLE_TAG_MODAL_ICON);

        auto *title = acul::alloc<Text>(AUIK_MODAL_HEADER_ID, message.header, amal::vec2{text_width, 0.0f},
                                        MODAL_INTERNAL_WIDGET_FLAGS,
                                        make_text_layout_flags(TextOverflowMode::ellipsis, TextWrapMode::word));
        title->set_style_tag(AUIK_STYLE_TAG_MODAL_TITLE);
        title->set_tight_content_height(true);

        auto *body = acul::alloc<Text>(AUIK_MODAL_MESSAGE_ID, message.message, amal::vec2{text_width, 0.0f},
                                       MODAL_INTERNAL_WIDGET_FLAGS,
                                       make_text_layout_flags(TextOverflowMode::ellipsis, TextWrapMode::word));
        body->set_style_tag(AUIK_STYLE_TAG_NO_PAD);
        body->set_tight_content_height(true);

        auto *header_block = acul::alloc<DrawBlock>(AUIK_MODAL_HEADER_ID, MODAL_INTERNAL_WIDGET_FLAGS, AUIK_TAG_BLOCK);
        header_block->set_style_tag(AUIK_STYLE_TAG_MODAL_HEADER);
        header_block->add_child(title);

        auto *message_block = acul::alloc<::auik::Block>(AUIK_MODAL_MESSAGE_ID, MODAL_INTERNAL_WIDGET_FLAGS, 0u);
        message_block->add_child(header_block);
        message_block->add_child(body);

        auto *message_row = acul::alloc<DrawBlock>(AUIK_MODAL_MESSAGE_ID, MODAL_INTERNAL_WIDGET_FLAGS, AUIK_TAG_BLOCK);
        message_row->set_style_tag(AUIK_STYLE_TAG_MODAL_MESSAGE_AREA);
        if (icon)
        {
            auto *icon_box =
                acul::alloc<DrawBlock>(AUIK_MODAL_DEFAULT_ICON, MODAL_INTERNAL_WIDGET_FLAGS, AUIK_TAG_BLOCK);
            icon_box->set_style_tag(AUIK_STYLE_TAG_MODAL_ICON);
            icon_box->add_child(icon);
            message_row->add_child(icon_box, make_layout_flags(ChildLayout::inline_, HAlign::left, VAlign::center));
        }
        message_row->add_child(message_block, make_layout_flags(ChildLayout::inline_, HAlign::left, VAlign::center));
        modal->add_child(message_row);

        Checkbox *batch_checkbox = nullptr;
        Text *batch_label = nullptr;
        if (message.group_id != 0u && group_count(message.group_id) > 1u)
        {
            batch_checkbox =
                acul::alloc<Checkbox>(AUIK_MODAL_BATCH_ID, _batch_apply, MODAL_INTERNAL_HITTABLE_WIDGET_FLAGS);
            batch_label =
                acul::alloc<Text>(AUIK_MODAL_BATCH_ID, "Apply to all", amal::vec2{90.0f, 0.0f},
                                  MODAL_INTERNAL_WIDGET_FLAGS, make_text_layout_flags(TextOverflowMode::ellipsis));
            batch_label->set_style_tag(AUIK_STYLE_TAG_MODAL_BATCH_LABEL);
        }

        acul::vector<TextButton *> buttons;
        for (u32 i = 0; i < message.buttons.size(); ++i)
        {
            const bool is_last_button = i + 1u == message.buttons.size();
            const u32 button_style = is_last_button ? AUIK_STYLE_TAG_TEXT_BUTTON : AUIK_STYLE_TAG_TRANSPARENT_BUTTON;
            auto *button =
                acul::alloc<TextButton>(AUIK_MODAL_BUTTON_ID, message.buttons[i].first, amal::vec2{0.0f, 0.0f},
                                        MODAL_INTERNAL_HITTABLE_WIDGET_FLAGS, EventFlagBits::none, button_style);
            buttons.push_back(button);
        }
        modal->add_child(acul::alloc<ModalControlsRow>(id(), batch_checkbox, batch_label, std::move(buttons)));

        modal->set_parent(this);
        modal->set_focus_parent(this);
        modal->set_queue(this);
        modal->update_style();
        _modal = modal;
        update_active_modal_depth();
        focus_widget(this);
        relayout_modal_draw_commands();
    }

    void ModalQueue::apply_button(u32 button_index)
    {
        if (_messages.empty()) return;
        if (button_index >= _messages.front().buttons.size()) return;

        const u32 active_group_id = _messages.front().group_id;
        const bool consume_all = _batch_apply && active_group_id != 0u && group_count(active_group_id) > 1u;

        for (size_t i = 0; i < _messages.size();)
        {
            auto &message = _messages[i];
            const bool consume_message = i == 0u || (consume_all && message.group_id == active_group_id);
            if (!consume_message)
            {
                ++i;
                continue;
            }

            auto *modal_before_callback = _modal;
            const size_t message_count_before_callback = _messages.size();
            const bool prevent_close = message.prevent_close;
            const u32 message_group_id = message.group_id;
            if (button_index < message.buttons.size())
            {
                auto &callback = message.buttons[button_index].second;
                if (callback) callback();
            }
            if (_close_all_pending)
            {
                close_all_windows_now();
                return;
            }
            if (_modal != modal_before_callback || i >= _messages.size() ||
                _messages.size() != message_count_before_callback)
                return;

            if (prevent_close && _prevent_close_count > 0) --_prevent_close_count;
            decrement_group_count(message_group_id);
            _messages.erase(_messages.begin() + i);

            if (!consume_all) break;
        }
        _batch_apply = false;
        rebuild_modal();
    }

    namespace
    {
        void write_modal_queue(acul::bin_stream &stream, umbf::Block *block)
        {
            auto *widget = static_cast<ModalQueue *>(block);
            detail::write_widget_common_data(stream, *widget);
            stream.write(widget->modal_width());
        }

        umbf::Block *read_modal_queue(acul::bin_stream &stream)
        {
            const auto common = detail::read_widget_common_data(stream);
            f32 modal_width = AUIK_MODAL_WINDOW_WIDTH;
            stream.read(modal_width);

            auto *widget = acul::alloc<ModalQueue>(common.id, WidgetFlags(common.widget_flags));
            widget->set_modal_width(modal_width);
            detail::apply_widget_common_data(widget, common);
            return widget;
        }
    } // namespace

    namespace streams
    {
        AUIK_EXPORT const umbf::streams::Stream modal_queue{read_modal_queue, write_modal_queue};
    } // namespace streams
} // namespace auik
