#include <auik/auik.hpp>
#include <auik/detail/depth.hpp>
#include <auik/pipelines.hpp>
#include <auik/widgets/combobox.hpp>
#include <auik/widgets/detail/popup_trigger.hpp>
#include <auik/widgets/detail/selectable.hpp>
#include <auik/widgets/text.hpp>
#include <auik/widgets/window.hpp>
#include "../core/session_stream_utils.hpp"

#define AUIK_COMBO_BOX_POPUP_ITEM_FALLBACK_HEIGHT 24.0f
#define AUIK_DROPDOWN_MIN_VISIBLE_ITEMS           4u

namespace auik
{
    namespace
    {
        struct DropdownPopupPlacement
        {
            bool place_above = false;
            bool need_scroll = false;
            f32 height = 0.0f;
            f32 y = 0.0f;
        };

        static inline amal::vec2 get_combo_popup_depth_range() { return detail::get_global_foreground_depth_range(); }

        static inline bool is_combo_control_element(ElementID element_id, u32 combo_id)
        { return element_id.widget_id == combo_id && element_id.tag_id == AUIK_TAG_COMBO_BOX; }

        static inline StyleState get_combo_control_style_state(bool open, StyleState state, u32 combo_id)
        {
            if (open) return StyleState::focus;
            if (detail::g_context && is_combo_control_element(detail::get_context().hover_id, combo_id))
                return state == StyleState::active ? StyleState::active : StyleState::hover;
            if (state != StyleState::focus) return state;
            return StyleState::normal;
        }

        static inline DropdownPopupPlacement resolve_dropdown_popup_placement(f32 control_y, f32 control_h,
                                                                              f32 desired_h, f32 item_h, u32 item_count,
                                                                              const amal::vec4 &viewport,
                                                                              f32 fallback_h)
        {
            const f32 gap = 0.0f;
            const f32 below_space = amal::max(viewport.y + viewport.w - (control_y + control_h + gap), 0.0f);
            const f32 above_space = amal::max(control_y - gap - viewport.y, 0.0f);
            const bool fits_below = desired_h <= below_space;
            const f32 safe_item_h = amal::max(item_h, 1.0f);
            const f32 below_visible_items = amal::floor(below_space / safe_item_h);
            const u32 min_visible_items = amal::min(AUIK_DROPDOWN_MIN_VISIBLE_ITEMS, item_count);
            const bool place_above =
                !fits_below && below_visible_items < min_visible_items && above_space > below_space;
            const f32 available_h = place_above ? above_space : below_space;
            const bool need_scroll = desired_h > available_h;
            const f32 popup_h = need_scroll ? available_h : amal::max(desired_h, fallback_h);
            const f32 popup_y = place_above ? control_y - gap - popup_h + 1.0f : control_y + control_h + gap - 1.0f;
            return {place_above, need_scroll, popup_h, popup_y};
        }

    } // namespace

    Combobox::Combobox(u32 id, const acul::vector<StringView> &items, u32 selected_index, amal::vec2 inline_size,
                       WidgetFlags widget_flags)
        : Widget(id, widget_flags, EventFlagBits::click | EventFlagBits::focus, {{0.0f, 0.0f}, inline_size},
                 AUIK_TAG_COMBO_BOX),
          _selected_index(selected_index)
    {
        _trigger = acul::alloc<detail::PopupTrigger>(AUIK_STYLE_TAG_COMBO_BOX, AUIK_TAG_COMBO_BOX,
                                                     AUIK_ICON_CHEVRON_DOWN, AUIK_ICON_CHEVRON_UP, true);
        _trigger->set_update_target(this);
        _trigger->set_hit_id(make_element_id(id, AUIK_TAG_COMBO_BOX, 0u));
        _label = acul::alloc<Text>(AUIK_TAG_TEXT, "", amal::vec2{0.0f, 0.0f}, WidgetFlagBits::visible);
        _label->set_style_tag(AUIK_STYLE_TAG_NO_PAD);
        _label->set_parent(this);
        _label->set_anchor_y(TextAnchorY::middle);

        _popup = acul::alloc<Window>(AUIK_TAG_COMBO_BOX_POPUP, "", amal::rect{{0.0f, 0.0f}, {0.0f, 0.0f}},
                                     WindowFlagBits::scrollable, WidgetFlagBits::hittable);
        _popup->get_rect().id.widget_id = this->id();
        _popup->set_window_style_tag(AUIK_STYLE_TAG_COMBO_BOX_POPUP);
        _popup->set_focus_parent(this);

        set_items(items);
    }

    Combobox::~Combobox()
    {
        if (_model_binding)
        {
            _model_binding->on_records = nullptr;
            _model_binding->on_field_change = nullptr;
            detach_model_binding(*_model_binding);
        }
        _open = false;
        if (_popup)
        {
            _popup->unset_visible();
            _popup->sync_widget_flags();
        }
        erase_widget_from_transient_cache(this);
        cancel_delayed_tasks(id());
        if (_popup) acul::release(_popup);
        if (_label) acul::release(_label);
        if (_trigger) acul::release(_trigger);
    }

    const acul::string &Combobox::selected_text() const
    {
        static const acul::string empty{};
        if (!_popup || _selected_index >= _popup->children.size()) return empty;
        auto *item = static_cast<detail::Selectable *>(_popup->children[_selected_index]);
        return item ? item->text() : empty;
    }

    acul::vector<acul::string> Combobox::items() const
    {
        acul::vector<acul::string> out{};
        if (!_popup) return out;
        out.resize(_popup->children.size());
        for (u32 i = 0; i < _popup->children.size(); ++i)
        {
            auto *item = static_cast<detail::Selectable *>(_popup->children[i]);
            out[i] = item ? item->text() : acul::string{};
        }
        return out;
    }

    acul::vector<StringView> Combobox::item_text_views() const
    {
        acul::vector<StringView> out{};
        if (!_popup) return out;
        out.reserve(_popup->children.size());
        for (auto *child : _popup->children)
        {
            auto *item = static_cast<detail::Selectable *>(child);
            if (!item)
            {
                out.push_back({});
                continue;
            }
            out.push_back(item->source_text());
        }
        return out;
    }

    void Combobox::set_model_binding(ModelBinding *binding)
    {
        if (_model_binding)
        {
            _model_binding->on_records = nullptr;
            _model_binding->on_field_change = nullptr;
            detach_model_binding(*_model_binding);
        }
        _model_binding = binding;
        if (!_model_binding) return;

        _model_binding->on_records = [this](const ModelRecordsEvent &) { rebuild_from_model_binding(); };
        _model_binding->on_field_change = [this](ModelRecordID, ModelFieldID) { rebuild_from_model_binding(); };
        attach_model_binding(*_model_binding);
        rebuild_model_binding_records(*_model_binding);
        rebuild_from_model_binding();
    }

    void Combobox::rebuild_from_model_binding()
    {
        if (!_model_binding || !is_model_binding_valid(*_model_binding))
        {
            set_items(acul::vector<StringView>{});
            return;
        }

        auto *model = find_model(_model_binding->db, _model_binding->model_id);
        acul::vector<acul::string> items;
        if (model) items.reserve(_model_binding->records.size());
        bool has_selected = false;
        u32 selected_index = 0u;
        if (model)
        {
            for (u32 item_index = 0; item_index < _model_binding->records.size(); ++item_index)
            {
                const ModelRecordID record_id = _model_binding->records[item_index];
                acul::string text{};
                bool selected = false;
                read_model_binding_value(*_model_binding, record_id, AUIK_COMBO_BOX_TEXT_FIELD, text);
                items.push_back(std::move(text));

                if (!has_selected &&
                    read_model_binding_value(*_model_binding, record_id, AUIK_COMBO_BOX_SELECTED_FIELD, selected) &&
                    selected)
                {
                    has_selected = true;
                    selected_index = item_index;
                }
            }
        }

        acul::vector<StringView> views;
        views.reserve(items.size());
        for (const auto &item : items) views.push_back(StringView{item});
        set_items(views);
        if (has_selected) set_selected_index(selected_index);
    }

    void Combobox::request_model_selected_index(u32 index)
    {
        if (!_model_binding || !is_model_binding_valid(*_model_binding)) return;
        acul::vector<ModelField *> changed_fields;
        for (u32 item_index = 0; item_index < _model_binding->records.size(); ++item_index)
        {
            const ModelRecordID record_id = _model_binding->records[item_index];
            bool selected = false;
            const bool next = item_index == index;
            if (read_model_binding_value(*_model_binding, record_id, AUIK_COMBO_BOX_SELECTED_FIELD, selected) &&
                selected == next)
                continue;

            ModelFieldAccess access{};
            if (!access_model_binding_field(*_model_binding, record_id, AUIK_COMBO_BOX_SELECTED_FIELD, access))
                continue;
            if (!write_model_field_access_value<bool>(_model_binding->db, _model_binding->model_id, access, next))
                continue;
            if (auto *field = resolve_model_field(_model_binding->db, _model_binding->model_id, access))
                changed_fields.push_back(field);
        }
        for (auto *field : changed_fields)
            if (field) dispatch_model_field(*field);
    }

    void Combobox::set_items(const acul::vector<StringView> &items)
    {
        const u32 prev_selected = _selected_index;
        _popup->unset_visible();
        _popup->sync_widget_flags();
        _popup->clear_children();

        for (u32 i = 0; i < items.size(); ++i)
        {
            auto *item = acul::alloc<detail::Selectable>(
                make_element_id(id(), AUIK_TAG_COMBO_BOX_ITEM, i), items[i], false,
                amal::vec2{AUIK_SIZE_X_FILL, AUIK_SIZE_Y_FIT}, _popup, detail::get_selectable_item_flags());
            item->set_style_tag(AUIK_STYLE_TAG_COMBO_BOX_ITEM);
            item->set_selected_style_tag(AUIK_STYLE_TAG_COMBO_BOX_ITEM_SELECTED);
            item->set_focus_parent(_popup);
            _popup->add_child(item);
        }

        if (_popup->children.empty()) _selected_index = 0u;
        else if (prev_selected >= _popup->children.size())
            _selected_index = static_cast<u32>(_popup->children.size() - 1u);
        else _selected_index = prev_selected;

        for (u32 i = 0; i < _popup->children.size(); ++i)
        {
            auto *item = static_cast<detail::Selectable *>(_popup->children[i]);
            if (!item) continue;
            item->set_style_state(StyleState::normal);
            item->set_selected(i == _selected_index);
            item->update_style();
        }

        sync_label_text();
        auto &ctx = detail::get_context();
        ctx.dirty_flags |= DirtyFlagBits::redraw;
        detail::mark_host_refresh_request();
        if (_open)
        {
            update_popup_layout();
            redraw_all_commands();
        }
    }

    void Combobox::add_item(StringView text)
    {
        if (!_popup) return;
        const u32 index = static_cast<u32>(_popup->children.size());
        auto *item = acul::alloc<detail::Selectable>(
            make_element_id(id(), AUIK_TAG_COMBO_BOX_ITEM, index), text, false,
            amal::vec2{AUIK_SIZE_X_FILL, AUIK_SIZE_Y_FIT}, _popup, detail::get_selectable_item_flags());
        item->set_style_tag(AUIK_STYLE_TAG_COMBO_BOX_ITEM);
        item->set_selected_style_tag(AUIK_STYLE_TAG_COMBO_BOX_ITEM_SELECTED);
        item->set_focus_parent(_popup);
        item->set_style_state(StyleState::normal);
        item->set_selected(index == _selected_index);
        item->update_style();
        _popup->add_child(item);
        if (index == 0u) _selected_index = 0u;
        sync_label_text();

        auto &ctx = detail::get_context();
        ctx.dirty_flags |= DirtyFlagBits::redraw;
        detail::mark_host_refresh_request();
        if (_open)
        {
            update_popup_layout();
            redraw_all_commands();
        }
    }

    void Combobox::add_items(const acul::vector<StringView> &items)
    {
        for (StringView item : items) add_item(item);
    }

    void Combobox::set_selected_index(u32 index)
    {
        const u32 count = _popup ? static_cast<u32>(_popup->children.size()) : 0u;
        if (count == 0u)
        {
            _selected_index = 0u;
            sync_label_text();
            return;
        }
        if (index >= count) index = count - 1u;
        if (_selected_index == index) return;

        if (_selected_index < count)
        {
            auto *prev = static_cast<detail::Selectable *>(_popup->children[_selected_index]);
            if (prev)
            {
                prev->set_style_state(StyleState::normal);
                prev->set_selected(false);
                prev->update_style();
            }
        }
        auto *next = static_cast<detail::Selectable *>(_popup->children[index]);
        if (next)
        {
            next->set_style_state(StyleState::normal);
            next->set_selected(true);
            next->update_style();
        }
        _selected_index = index;
        sync_label_text();
        auto &ctx = detail::get_context();
        ctx.dirty_flags |= DirtyFlagBits::redraw;
        detail::mark_host_refresh_request();
    }

    StyleUpdateFlags Combobox::update_style()
    {
        const u32 parent_id = parent() ? parent()->id() : 0u;
        const StyleState control_state = get_combo_control_style_state(_open, style_state(), id());
        StyleUpdateFlags flags = resolve_style_selector(_style, id(), parent_id, control_state);
        apply_style_layout(get_theme()->get_style(_style.id));
        if (_trigger)
        {
            _trigger->set_open(_open);
            flags |= _trigger->update_style(id(), parent_id, control_state);
        }
        flags |= _label->update_style();
        if (_popup)
        {
            const auto transition = detail::get_widget_style_selector_transition(id());
            if (transition.current_id.tag_id == AUIK_TAG_WINDOW) flags |= _popup->update_style();
            if (transition.prev_id.tag_id == AUIK_TAG_COMBO_BOX_ITEM &&
                transition.prev_id.element_id < _popup->children.size())
            {
                auto *item = static_cast<detail::Selectable *>(_popup->children[transition.prev_id.element_id]);
                item->set_style_state(StyleState::normal);
                flags |= item->update_style();
            }
            if (transition.current_id.tag_id == AUIK_TAG_COMBO_BOX_ITEM &&
                transition.current_id.element_id < _popup->children.size())
            {
                auto *item = static_cast<detail::Selectable *>(_popup->children[transition.current_id.element_id]);
                item->set_style_state(transition.current_state);
                flags |= item->update_style();
            }
        }
        return flags;
    }

    void Combobox::update_layout_min_size()
    {
        const auto &style = get_theme()->get_style(_style.id);
        const StyleID popup_style_id = get_theme()->get_resolved_style(
            AUIK_STYLE_TAG_COMBO_BOX_POPUP, _popup ? _popup->id() : 0u, 0, StyleState::normal);
        const amal::vec4 popup_padding = get_theme()->get_style(popup_style_id).padding();
        const amal::vec2 prev_label_size = _label->size();
        // Measure label natural size, independent of the currently assigned combo bounds.
        _label->set_layout_size({0.0f, 0.0f});
        _label->update_layout_min_size();
        const amal::vec2 label_required = _label->required_size();
        _label->set_layout_size(prev_label_size);
        amal::vec2 max_item_required = label_required;
        if (_popup)
        {
            for (auto *child : _popup->children)
            {
                auto *item = static_cast<detail::Selectable *>(child);
                if (!item) continue;
                item->update_layout_min_size();
                const amal::vec2 item_required = item->required_size();
                max_item_required.x = amal::max(max_item_required.x, item_required.x);
                max_item_required.y = amal::max(max_item_required.y, item_required.y);
            }
        }
        if (_trigger) _trigger->update_layout_min_size({0.0f, 0.0f}, true);

        const f32 icon_height =
            _trigger && _trigger->icon_size().y > 0.0f ? _trigger->icon_size().y : style.text_size();
        const amal::vec4 margin = style.margin();
        const amal::vec4 padding = style.padding();
        const f32 trigger_width =
            _trigger ? _trigger->required_size().x : (style.text_size() + style.padding().x + style.padding().z);
        const f32 control_width = padding.x + max_item_required.x + 6.0f + trigger_width + padding.z;
        const f32 popup_width = popup_padding.x + max_item_required.x + popup_padding.z;
        const f32 compact_width = amal::max(control_width, popup_width);
        amal::vec2 min_size = {is_width_fixed() ? style_size().x : 0.0f,
                               is_height_fixed() ? style_size().y : 0.0f};
        if (fill_width()) min_size.x = compact_width;
        if (fill_height()) min_size.y = 0.0f;
        if (min_size.x <= 0.0f) min_size.x = is_width_fixed() ? 140.0f : compact_width;
        if (min_size.y <= 0.0f) min_size.y = amal::max(label_required.y, icon_height) + padding.y + padding.w;
        set_required_size({min_size.x + margin.x + margin.z, min_size.y + margin.y + margin.w});
    }

    void Combobox::update_layout(bool min_size_known)
    {
        if (!min_size_known) update_layout_min_size();

        const auto &style = get_theme()->get_style(_style.id);
        const amal::vec4 margin = style.margin();
        const amal::vec2 layout_origin = position();
        const amal::vec2 min_required = required_size();
        const amal::vec2 min_combo = {amal::max(min_required.x - margin.x - margin.z, 0.0f),
                                      amal::max(min_required.y - margin.y - margin.w, 0.0f)};
        amal::vec2 widget_size = {amal::max(size().x - margin.x - margin.z, 0.0f),
                                  amal::max(size().y - margin.y - margin.w, 0.0f)};
        if (fill_width()) widget_size.x = amal::max(widget_size.x, min_combo.x);
        else if (!is_width_fixed()) widget_size.x = min_combo.x;
        else widget_size.x = amal::max(widget_size.x, min_combo.x);
        if (!fill_height() && !is_height_fixed()) widget_size.y = min_combo.y;
        else widget_size.y = amal::max(widget_size.y, min_combo.y);

        const amal::vec2 pos = {layout_origin.x + margin.x, layout_origin.y + margin.y};
        set_position(pos);
        set_layout_size(widget_size);
        Widget::update_layout(true);
        set_clip_id(parent()->content_clip_id());

        if (_trigger)
        {
            const amal::rect trigger_outer_bounds = {
                layout_origin, {widget_size.x + margin.x + margin.z, widget_size.y + margin.y + margin.w}};
            _trigger->update_layout(trigger_outer_bounds, clip_id());
        }
        rebuild_control_layout();
        if (_open) update_popup_layout();
        else if (_popup)
        {
            _popup->unset_visible();
            _popup->sync_widget_flags();
        }
    }

    void Combobox::translate(const amal::vec2 &delta)
    {
        if (delta.x == 0.0f && delta.y == 0.0f) return;
        Widget::translate(delta);
        _label_rect.offset += delta;
        if (_trigger) _trigger->translate(delta);
        if (_label) _label->translate(delta);
        if (_open && _popup) static_cast<Widget *>(_popup)->translate(delta);
    }

    void Combobox::rebuild_clip_rects()
    {
        assert(parent() && "Combobox must have parent");
        set_clip_id(parent()->content_clip_id());
        if (_trigger) _trigger->rebuild_clip_rects(clip_id());
        if (_label) _label->rebuild_clip_rects();
        if (_popup) _popup->rebuild_clip_rects();
    }

    void Combobox::update_depth(const amal::vec2 &depth_range)
    {
        Widget::update_depth(depth_range);
        amal::vec2 trigger_range{};
        assign_next_depth(this->depth_range(), trigger_range);
        if (_trigger) _trigger->update_depth(trigger_range);
        assign_next_depth(trigger_range, _content_depth_range);
        if (_label) _label->update_depth(_content_depth_range);
        if (_popup) static_cast<Widget *>(_popup)->update_depth(get_combo_popup_depth_range());
    }

    void Combobox::back_hit_depth()
    {
        Widget::back_hit_depth();
        if (_trigger) _trigger->back_hit_depth();
        if (_label) _label->back_hit_depth();
        if (_popup) static_cast<Widget *>(_popup)->back_hit_depth();
    }

    void Combobox::restore_hit_depth()
    {
        Widget::restore_hit_depth();
        if (_trigger) _trigger->restore_hit_depth();
        if (_label) _label->restore_hit_depth();
        if (_popup) static_cast<Widget *>(_popup)->restore_hit_depth();
    }

    void Combobox::draw(DrawCtx &ctx)
    {
        const bool transient = ctx.reason & DrawReasonBits::transient;
        const bool draw_transient_payload = transient || (ctx.reason & DrawReasonBits::record);

        if (!transient)
        {
            if (_trigger) _trigger->draw(ctx, can_emit_hit(ctx));

            DrawCtx label_ctx = ctx;
            label_ctx.is_hit_allowed = false;
            _label->draw_local(label_ctx);
        }

        if (transient && draw_transient_payload && _trigger) _trigger->draw(ctx, false);

        if (transient) return;

        if (_open && _popup)
        {
            DrawCtx popup_ctx = ctx;
            _popup->draw_local(popup_ctx);
        }
    }

    void Combobox::on_focus(bool focused)
    {
        if (!focused)
        {
            add_render_command<detail::FocusEventTraits>(this, [this]() {
                if (!_open) return;
                close();
                redraw_all_commands();
            });
            detail::mark_host_refresh_request();
            return;
        }
        if (_open) set_style_state(StyleState::focus);
    }

    void Combobox::on_click(MouseKey key, KeyPressState state, u32 click_count)
    {
        (void)click_count;
        if (key != MouseKey::left || state != KeyPressState::press) return;
        const auto hover_id = detail::get_context().hover_id;
        if (_open && hover_id.widget_id == id() && hover_id.tag_id == AUIK_TAG_COMBO_BOX_ITEM && _popup)
        {
            const u32 index = hover_id.element_id;
            if (index < _popup->children.size() && _popup->children[index] && _popup->children[index]->is_visible())
            {
                add_render_command<detail::ClickEventTraits>(this, [this, index]() {
                    if (!_popup || index >= _popup->children.size()) return;
                    close();
                    if (_model_binding) request_model_selected_index(index);
                    else set_selected_index(index);
                    const bool prevented = mark_changed();
                    if (prevented) return;
                    redraw_all_commands();
                });
                detail::mark_host_refresh_request();
            }
            return;
        }
        add_render_command<detail::ClickEventTraits>(this, [this]() {
            toggle();
            redraw_all_commands();
        });
        detail::mark_host_refresh_request();
    }

    void Combobox::open()
    {
        if (_open) return;
        _open = true;
        set_style_state(StyleState::focus);
        if (_trigger) _trigger->set_open(true);
        update_style();
        update_popup_layout();
        schedule_outside_click_tick();
        if (_trigger) _trigger->start_icon_animation(true);
    }

    void Combobox::close()
    {
        if (!_open) return;
        _open = false;
        set_style_state(StyleState::normal);
        if (_trigger) _trigger->set_open(false);
        update_style();
        if (_popup)
        {
            _popup->unset_visible();
            _popup->sync_widget_flags();
        }
        if (_trigger) _trigger->start_icon_animation(false);
    }

    void Combobox::toggle()
    {
        if (_open) close();
        else open();
    }

    void Combobox::rebuild_control_layout()
    {
        const auto &style = get_theme()->get_style(_style.id);
        const amal::vec4 padding = style.padding();
        const f32 icon_slot_x = _trigger ? _trigger->icon_slot_left() : position().x + size().x - padding.z;
        const f32 label_w = amal::max(icon_slot_x - (position().x + padding.x) - 6.0f, 0.0f);
        const f32 label_h = _label->required_size().y;
        const f32 content_h = amal::max(size().y - padding.y - padding.w, 0.0f);
        _label_rect = {{position().x + padding.x, position().y + padding.y + amal::floor((content_h - label_h) * 0.5f)},
                       {label_w, label_h}};
        _label->set_position(_label_rect.offset);
        _label->set_layout_size(_label_rect.size);
        _label->update_layout(false);
        _label->set_clip_id(clip_id());
    }

    void Combobox::sync_label_text()
    {
        if (!_popup || _selected_index >= _popup->children.size())
        {
            _label->set_text(acul::string{});
            return;
        }
        auto *item = static_cast<detail::Selectable *>(_popup->children[_selected_index]);
        if (!item) _label->set_text(acul::string{});
        else _label->set_text(*item);
    }

    void Combobox::update_popup_layout()
    {
        _popup->set_window_style_tag(AUIK_STYLE_TAG_COMBO_BOX_POPUP);
        _popup->update_style();
        _popup->window_flags = (get_popup_window_flags() | WindowFlagBits::docked) & ~WindowFlagBits::scrollable;
        const auto &popup_style = get_theme()->get_style(
            get_theme()->get_resolved_style(AUIK_STYLE_TAG_COMBO_BOX_POPUP, _popup->id(), 0, StyleState::normal));
        const amal::vec4 popup_padding = popup_style.padding();
        const f32 content_width = amal::max(size().x - popup_padding.x - popup_padding.z, 0.0f);
        const amal::vec2 measure_popup_pos = {position().x, position().y + size().y - 1.0f};
        _popup->set_position(measure_popup_pos);
        _popup->set_size({size().x, AUIK_COMBO_BOX_POPUP_ITEM_FALLBACK_HEIGHT});
        const amal::vec4 viewport = get_widget_viewport_rect(this);
        _popup->attach_to_viewport(this->viewport());
        _popup->set_visible();
        _popup->sync_widget_flags();
        static_cast<Widget *>(_popup)->update_depth(get_combo_popup_depth_range());
        static_cast<Widget *>(_popup)->update_layout(false);

        const amal::vec2 content_origin = measure_popup_pos + amal::vec2{popup_padding.x, popup_padding.y};
        amal::vec2 cursor = content_origin;
        u32 visible_items = 0u;
        for (u32 i = 0; i < _popup->children.size(); ++i)
        {
            auto *child = static_cast<detail::Selectable *>(_popup->children[i]);
            if (!child || !child->is_visible()) continue;
            ++visible_items;
            child->update_layout_min_size();
            child->set_layout_size({content_width, child->required_size().y});
            child->set_position(cursor);
            child->update_layout(true);
            cursor = {content_origin.x, cursor.y + child->required_size().y};
        }

        const f32 measured_h =
            (cursor.y > content_origin.y) ? (cursor.y - measure_popup_pos.y + popup_padding.w) : 0.0f;
        const f32 desired_h = amal::max(measured_h, AUIK_COMBO_BOX_POPUP_ITEM_FALLBACK_HEIGHT);
        const f32 content_h = amal::max(measured_h - popup_padding.y - popup_padding.w, 0.0f);
        const f32 item_h = visible_items > 0u ? content_h / static_cast<f32>(visible_items)
                                              : AUIK_COMBO_BOX_POPUP_ITEM_FALLBACK_HEIGHT;
        const auto placement =
            resolve_dropdown_popup_placement(position().y, size().y, desired_h, item_h, visible_items, viewport,
                                             AUIK_COMBO_BOX_POPUP_ITEM_FALLBACK_HEIGHT);
        _popup->set_window_style_tag(AUIK_STYLE_TAG_COMBO_BOX_POPUP);
        _popup->update_style();
        if (placement.need_scroll) _popup->window_flags = get_popup_window_flags() | WindowFlagBits::docked;
        else _popup->window_flags = (get_popup_window_flags() | WindowFlagBits::docked) & ~WindowFlagBits::scrollable;

        _popup->set_position({position().x, placement.y});
        _popup->set_size({size().x, placement.height});
        _popup->attach_to_viewport(this->viewport());
        _popup->set_visible();
        _popup->sync_widget_flags();
        static_cast<Widget *>(_popup)->update_depth(get_combo_popup_depth_range());
        static_cast<Widget *>(_popup)->update_layout(false);
    }

    void Combobox::schedule_outside_click_tick()
    {
        if (!detail::g_context) return;
        detail::update_window_time(detail::get_context().window_ctx);
        const f64 delay = get_max_animation_delay() > 0.0 ? get_max_animation_delay() : (1.0 / 60.0);
        schedule_delayed_host_task(id(), detail::get_context().window_ctx->time + delay,
                                   [this]() { tick_outside_click(); });
    }

    void Combobox::tick_outside_click()
    {
        if (!_open || !detail::g_context) return;

        add_render_command<detail::ClickEventTraits>(this, [this]() {
            if (!_open || !detail::g_context) return;

            auto &ctx = detail::get_context();
            if (style_state() != StyleState::focus)
            {
                set_style_state(StyleState::focus);
                update_style();
                redraw_external(has_draw_record());
            }
            const bool mouse_down = ctx.io.mouse_down;
            if (!mouse_down)
            {
                const amal::vec2 mouse_pos = get_mouse_pos();
                const auto &combo_rect = bounds();
                const bool in_combo = mouse_pos.x >= combo_rect.offset.x && mouse_pos.y >= combo_rect.offset.y &&
                                      mouse_pos.x < (combo_rect.offset.x + combo_rect.size.x) &&
                                      mouse_pos.y < (combo_rect.offset.y + combo_rect.size.y);
                bool in_popup = false;
                if (_popup)
                {
                    const auto &popup_rect = _popup->bounds();
                    in_popup = mouse_pos.x >= popup_rect.offset.x && mouse_pos.y >= popup_rect.offset.y &&
                               mouse_pos.x < (popup_rect.offset.x + popup_rect.size.x) &&
                               mouse_pos.y < (popup_rect.offset.y + popup_rect.size.y);
                }
                if (!in_combo && !in_popup && ctx.focus_id != id())
                {
                    close();
                    redraw_all_commands();
                    return;
                }
            }
            schedule_outside_click_tick();
        });
        detail::mark_host_refresh_request();
    }

    bool Combobox::has_draw_record() const { return _trigger && _trigger->has_draw_record(); }

    MultipleCombobox::MultipleCombobox(u32 id, const acul::vector<StringView> &items, StringView placeholder,
                                       amal::vec2 inline_size,
                                       WidgetFlags widget_flags)
        : Widget(id, widget_flags, EventFlagBits::click | EventFlagBits::focus, {{0.0f, 0.0f}, inline_size},
                 AUIK_TAG_COMBO_BOX)
    {
        _trigger = acul::alloc<detail::PopupTrigger>(AUIK_STYLE_TAG_COMBO_BOX, AUIK_TAG_COMBO_BOX,
                                                     AUIK_ICON_CHEVRON_DOWN, AUIK_ICON_CHEVRON_UP, true);
        _trigger->set_update_target(this);
        _trigger->set_hit_id(make_element_id(id, AUIK_TAG_COMBO_BOX, 0u));
        _label = acul::alloc<Text>(AUIK_TAG_TEXT, "", amal::vec2{0.0f, 0.0f}, WidgetFlagBits::visible);
        _label->set_style_tag(AUIK_STYLE_TAG_NO_PAD);
        _label->set_parent(this);
        _label->set_anchor_y(TextAnchorY::middle);

        _popup = acul::alloc<Window>(AUIK_TAG_COMBO_BOX_POPUP, "", amal::rect{{0.0f, 0.0f}, {0.0f, 0.0f}},
                                     WindowFlagBits::scrollable, WidgetFlagBits::hittable);
        _popup->get_rect().id.widget_id = this->id();
        _popup->set_window_style_tag(AUIK_STYLE_TAG_COMBO_BOX_POPUP);
        _popup->set_focus_parent(this);
        set_items(items);
        set_placeholder(placeholder);
    }

    MultipleCombobox::~MultipleCombobox()
    {
        _open = false;
        if (_popup)
        {
            _popup->unset_visible();
            _popup->sync_widget_flags();
        }
        erase_widget_from_transient_cache(this);
        cancel_delayed_tasks(id());
        if (_popup) acul::release(_popup);
        if (_label) acul::release(_label);
        if (_trigger) acul::release(_trigger);
    }

    acul::vector<acul::string> MultipleCombobox::items() const
    {
        acul::vector<acul::string> out{};
        if (!_popup) return out;
        out.resize(_popup->children.size());
        for (u32 i = 0; i < _popup->children.size(); ++i)
        {
            auto *item = static_cast<detail::Selectable *>(_popup->children[i]);
            out[i] = item ? item->text() : acul::string{};
        }
        return out;
    }

    acul::vector<StringView> MultipleCombobox::item_text_views() const
    {
        acul::vector<StringView> out{};
        if (!_popup) return out;
        out.reserve(_popup->children.size());
        for (auto *child : _popup->children)
        {
            auto *item = static_cast<detail::Selectable *>(child);
            if (!item) out.push_back({});
            else out.push_back(item->source_text());
        }
        return out;
    }

    void MultipleCombobox::set_items(const acul::vector<StringView> &items)
    {
        _popup->unset_visible();
        _popup->sync_widget_flags();
        _popup->clear_children();
        _selected_indices.clear();
        for (u32 i = 0; i < items.size(); ++i)
        {
            auto *item = acul::alloc<detail::Selectable>(
                make_element_id(id(), AUIK_TAG_COMBO_BOX_ITEM, i), items[i], false,
                amal::vec2{AUIK_SIZE_X_FILL, AUIK_SIZE_Y_FIT}, _popup, detail::get_selectable_item_flags());
            item->set_style_tag(AUIK_STYLE_TAG_COMBO_BOX_ITEM);
            item->set_selected_icon(AUIK_ICON_CHECKMARK);
            item->set_focus_parent(_popup);
            _popup->add_child(item);
        }
        sync_label_text();
        detail::get_context().dirty_flags |= DirtyFlagBits::redraw;
        detail::mark_host_refresh_request();
    }

    void MultipleCombobox::set_placeholder(acul::string value) { set_placeholder(StringView{value}); }

    void MultipleCombobox::set_placeholder(StringView value)
    {
        _placeholder = value.str ? value.str : "";
        _translated_placeholder = value.is_translated;
        _placeholder_literal = value.is_translated ? _placeholder : acul::string{};
        sync_label_text();
    }

    bool MultipleCombobox::is_selected(u32 index) const
    {
        for (u32 selected : _selected_indices)
            if (selected == index) return true;
        return false;
    }

    void MultipleCombobox::set_selected_indices(const acul::vector<u32> &indices)
    {
        const auto prev_selected = _selected_indices;
        _selected_indices.clear();
        const u32 count = _popup ? static_cast<u32>(_popup->children.size()) : 0u;
        for (u32 index : indices)
        {
            if (index >= count || is_selected(index)) continue;
            _selected_indices.push_back(index);
        }
        for (u32 i = 0; i < count; ++i)
        {
            auto *item = static_cast<detail::Selectable *>(_popup->children[i]);
            if (!item) continue;
            item->set_style_state(StyleState::normal);
            item->set_selected(is_selected(i));
            item->update_style();
        }
        const bool changed = _selected_indices != prev_selected;
        if (!changed) return;
        const bool prevented = changed && mark_changed();
        sync_label_text();
        if (prevented) return;
        detail::get_context().dirty_flags |= DirtyFlagBits::redraw;
        detail::mark_host_refresh_request();
    }

    StyleUpdateFlags MultipleCombobox::update_style()
    {
        const u32 parent_id = parent() ? parent()->id() : 0u;
        const StyleState control_state = get_combo_control_style_state(_open, style_state(), id());
        StyleUpdateFlags flags = resolve_style_selector(_style, id(), parent_id, control_state);
        apply_style_layout(get_theme()->get_style(_style.id));
        if (_trigger)
        {
            _trigger->set_open(_open);
            flags |= _trigger->update_style(id(), parent_id, control_state);
        }
        flags |= _label->update_style();
        if (_popup)
        {
            const auto transition = detail::get_widget_style_selector_transition(id());
            if (transition.current_id.tag_id == AUIK_TAG_WINDOW) flags |= _popup->update_style();
            if (transition.prev_id.tag_id == AUIK_TAG_COMBO_BOX_ITEM &&
                transition.prev_id.element_id < _popup->children.size())
            {
                auto *item = static_cast<detail::Selectable *>(_popup->children[transition.prev_id.element_id]);
                item->set_style_state(StyleState::normal);
                item->set_selected(is_selected(item->get_rect().id.element_id));
                flags |= item->update_style();
            }
            if (transition.current_id.tag_id == AUIK_TAG_COMBO_BOX_ITEM &&
                transition.current_id.element_id < _popup->children.size())
            {
                auto *item = static_cast<detail::Selectable *>(_popup->children[transition.current_id.element_id]);
                item->set_style_state(transition.current_state);
                item->set_selected(is_selected(item->get_rect().id.element_id));
                flags |= item->update_style();
            }
        }
        return flags;
    }

    void MultipleCombobox::update_layout_min_size()
    {
        const auto &style = get_theme()->get_style(_style.id);
        const StyleID popup_style_id = get_theme()->get_resolved_style(
            AUIK_STYLE_TAG_COMBO_BOX_POPUP, _popup ? _popup->id() : 0u, 0, StyleState::normal);
        const amal::vec4 popup_padding = get_theme()->get_style(popup_style_id).padding();
        const amal::vec2 prev_label_size = _label->size();
        _label->set_layout_size({0.0f, 0.0f});
        _label->update_layout_min_size();
        const amal::vec2 label_required = _label->required_size();
        _label->set_layout_size(prev_label_size);
        amal::vec2 max_item_required = label_required;
        if (_popup)
        {
            for (auto *child : _popup->children)
            {
                auto *item = static_cast<detail::Selectable *>(child);
                if (!item) continue;
                item->update_layout_min_size();
                const amal::vec2 item_required = item->required_size();
                max_item_required.x = amal::max(max_item_required.x, item_required.x);
                max_item_required.y = amal::max(max_item_required.y, item_required.y);
            }
        }
        if (_trigger) _trigger->update_layout_min_size({0.0f, 0.0f}, true);
        const f32 icon_height =
            _trigger && _trigger->icon_size().y > 0.0f ? _trigger->icon_size().y : style.text_size();
        const amal::vec4 margin = style.margin();
        const amal::vec4 padding = style.padding();
        const f32 trigger_width =
            _trigger ? _trigger->required_size().x : (style.text_size() + style.padding().x + style.padding().z);
        const f32 control_width = padding.x + max_item_required.x + 6.0f + trigger_width + padding.z;
        const f32 popup_width = popup_padding.x + max_item_required.x + popup_padding.z;
        const f32 compact_width = amal::max(control_width, popup_width);
        amal::vec2 min_size = {is_width_fixed() ? style_size().x : 0.0f,
                               is_height_fixed() ? style_size().y : 0.0f};
        if (fill_width()) min_size.x = compact_width;
        if (fill_height()) min_size.y = 0.0f;
        if (min_size.x <= 0.0f) min_size.x = is_width_fixed() ? 140.0f : compact_width;
        if (min_size.y <= 0.0f) min_size.y = amal::max(label_required.y, icon_height) + padding.y + padding.w;
        set_required_size({min_size.x + margin.x + margin.z, min_size.y + margin.y + margin.w});
    }

    void MultipleCombobox::update_layout(bool min_size_known)
    {
        if (!min_size_known) update_layout_min_size();
        const auto &style = get_theme()->get_style(_style.id);
        const amal::vec4 margin = style.margin();
        const amal::vec2 layout_origin = position();
        const amal::vec2 min_required = required_size();
        const amal::vec2 min_combo = {amal::max(min_required.x - margin.x - margin.z, 0.0f),
                                      amal::max(min_required.y - margin.y - margin.w, 0.0f)};
        amal::vec2 widget_size = {amal::max(size().x - margin.x - margin.z, 0.0f),
                                  amal::max(size().y - margin.y - margin.w, 0.0f)};
        if (fill_width()) widget_size.x = amal::max(widget_size.x, min_combo.x);
        else if (!is_width_fixed()) widget_size.x = min_combo.x;
        else widget_size.x = amal::max(widget_size.x, min_combo.x);
        if (!fill_height() && !is_height_fixed()) widget_size.y = min_combo.y;
        else widget_size.y = amal::max(widget_size.y, min_combo.y);
        set_position({layout_origin.x + margin.x, layout_origin.y + margin.y});
        set_layout_size(widget_size);
        Widget::update_layout(true);
        set_clip_id(parent()->content_clip_id());
        if (_trigger)
        {
            const amal::rect trigger_outer_bounds = {
                layout_origin, {widget_size.x + margin.x + margin.z, widget_size.y + margin.y + margin.w}};
            _trigger->update_layout(trigger_outer_bounds, clip_id());
        }
        rebuild_control_layout();
        if (_open) update_popup_layout();
        else if (_popup)
        {
            _popup->unset_visible();
            _popup->sync_widget_flags();
        }
    }

    void MultipleCombobox::translate(const amal::vec2 &delta)
    {
        if (delta.x == 0.0f && delta.y == 0.0f) return;
        Widget::translate(delta);
        _label_rect.offset += delta;
        if (_trigger) _trigger->translate(delta);
        if (_label) _label->translate(delta);
        if (_open && _popup) static_cast<Widget *>(_popup)->translate(delta);
    }

    void MultipleCombobox::rebuild_clip_rects()
    {
        assert(parent() && "MultipleCombobox must have parent");
        set_clip_id(parent()->content_clip_id());
        if (_trigger) _trigger->rebuild_clip_rects(clip_id());
        if (_label) _label->rebuild_clip_rects();
        if (_popup) _popup->rebuild_clip_rects();
    }

    void MultipleCombobox::update_depth(const amal::vec2 &depth_range)
    {
        Widget::update_depth(depth_range);
        amal::vec2 trigger_range{};
        assign_next_depth(this->depth_range(), trigger_range);
        if (_trigger) _trigger->update_depth(trigger_range);
        assign_next_depth(trigger_range, _content_depth_range);
        if (_label) _label->update_depth(_content_depth_range);
        if (_popup) static_cast<Widget *>(_popup)->update_depth(get_combo_popup_depth_range());
    }

    void MultipleCombobox::back_hit_depth()
    {
        Widget::back_hit_depth();
        if (_trigger) _trigger->back_hit_depth();
        if (_label) _label->back_hit_depth();
        if (_popup) static_cast<Widget *>(_popup)->back_hit_depth();
    }

    void MultipleCombobox::restore_hit_depth()
    {
        Widget::restore_hit_depth();
        if (_trigger) _trigger->restore_hit_depth();
        if (_label) _label->restore_hit_depth();
        if (_popup) static_cast<Widget *>(_popup)->restore_hit_depth();
    }

    void MultipleCombobox::draw(DrawCtx &ctx)
    {
        const bool transient = ctx.reason & DrawReasonBits::transient;
        const bool draw_transient_payload = transient || (ctx.reason & DrawReasonBits::record);

        if (!transient)
        {
            if (_trigger) _trigger->draw(ctx, can_emit_hit(ctx));
            DrawCtx label_ctx = ctx;
            label_ctx.is_hit_allowed = false;
            _label->draw_local(label_ctx);
        }
        if (transient && draw_transient_payload && _trigger) _trigger->draw(ctx, false);
        if (transient) return;
        if (_open && _popup)
        {
            DrawCtx popup_ctx = ctx;
            _popup->draw_local(popup_ctx);
        }
    }

    void MultipleCombobox::on_focus(bool focused)
    {
        if (!focused)
        {
            add_render_command<detail::FocusEventTraits>(this, [this]() {
                if (!_open) return;
                close();
                redraw_all_commands();
            });
            detail::mark_host_refresh_request();
            return;
        }
        if (_open) set_style_state(StyleState::focus);
    }

    void MultipleCombobox::on_click(MouseKey key, KeyPressState state, u32 click_count)
    {
        (void)click_count;
        if (key != MouseKey::left || state != KeyPressState::press) return;
        const auto hover_id = detail::get_context().hover_id;
        if (_open && hover_id.widget_id == id() && hover_id.tag_id == AUIK_TAG_COMBO_BOX_ITEM && _popup)
        {
            const u32 index = hover_id.element_id;
            if (index < _popup->children.size() && _popup->children[index] && _popup->children[index]->is_visible())
            {
                add_render_command<detail::ClickEventTraits>(this, [this, index]() {
                    if (is_selected(index))
                    {
                        for (u32 i = 0; i < _selected_indices.size(); ++i)
                        {
                            if (_selected_indices[i] != index) continue;
                            _selected_indices.erase(_selected_indices.begin() + i);
                            break;
                        }
                    }
                    else _selected_indices.push_back(index);
                    const bool prevented = mark_changed();
                    for (u32 i = 0; i < _popup->children.size(); ++i)
                    {
                        auto *item = static_cast<detail::Selectable *>(_popup->children[i]);
                        if (!item) continue;
                        item->set_style_state(StyleState::normal);
                        item->set_selected(is_selected(i));
                        item->update_style();
                    }
                    sync_label_text();
                    if (!prevented) redraw_all_commands();
                });
                detail::mark_host_refresh_request();
            }
            return;
        }
        add_render_command<detail::ClickEventTraits>(this, [this]() {
            toggle();
            redraw_all_commands();
        });
        detail::mark_host_refresh_request();
    }

    void MultipleCombobox::open()
    {
        if (_open) return;
        _open = true;
        set_style_state(StyleState::focus);
        if (_trigger) _trigger->set_open(true);
        update_style();
        update_popup_layout();
        schedule_outside_click_tick();
        if (_trigger) _trigger->start_icon_animation(true);
    }

    void MultipleCombobox::close()
    {
        if (!_open) return;
        _open = false;
        set_style_state(StyleState::normal);
        if (_trigger) _trigger->set_open(false);
        update_style();
        if (_popup)
        {
            _popup->unset_visible();
            _popup->sync_widget_flags();
        }
        if (_trigger) _trigger->start_icon_animation(false);
    }

    void MultipleCombobox::toggle()
    {
        if (_open) close();
        else open();
    }

    void MultipleCombobox::rebuild_control_layout()
    {
        const auto &style = get_theme()->get_style(_style.id);
        const amal::vec4 padding = style.padding();
        const f32 icon_slot_x = _trigger ? _trigger->icon_slot_left() : position().x + size().x - padding.z;
        const f32 label_w = amal::max(icon_slot_x - (position().x + padding.x) - 6.0f, 0.0f);
        const f32 label_h = _label->required_size().y;
        const f32 content_h = amal::max(size().y - padding.y - padding.w, 0.0f);
        _label_rect = {{position().x + padding.x, position().y + padding.y + amal::floor((content_h - label_h) * 0.5f)},
                       {label_w, label_h}};
        _label->set_position(_label_rect.offset);
        _label->set_layout_size(_label_rect.size);
        _label->update_layout(false);
        _label->set_clip_id(clip_id());
    }

    void MultipleCombobox::sync_label_text()
    {
        if (_selected_indices.size() == 1u && _selected_indices[0] < _popup->children.size())
        {
            auto *item = static_cast<detail::Selectable *>(_popup->children[_selected_indices[0]]);
            if (!item) _label->set_text(_placeholder);
            else _label->set_text(*item);
            return;
        }

        if (_translated_placeholder) _label->set_text(StringView{_placeholder_literal.c_str(), true});
        else _label->set_text(_placeholder);
    }

    void MultipleCombobox::update_popup_layout()
    {
        _popup->set_window_style_tag(AUIK_STYLE_TAG_COMBO_BOX_POPUP);
        _popup->update_style();
        _popup->window_flags = (get_popup_window_flags() | WindowFlagBits::docked) & ~WindowFlagBits::scrollable;
        const auto &popup_style = get_theme()->get_style(
            get_theme()->get_resolved_style(AUIK_STYLE_TAG_COMBO_BOX_POPUP, _popup->id(), 0, StyleState::normal));
        const amal::vec4 popup_padding = popup_style.padding();
        const f32 content_width = amal::max(size().x - popup_padding.x - popup_padding.z, 0.0f);
        const amal::vec2 measure_popup_pos = {position().x, position().y + size().y - 1.0f};
        _popup->set_position(measure_popup_pos);
        _popup->set_size({size().x, AUIK_COMBO_BOX_POPUP_ITEM_FALLBACK_HEIGHT});
        const amal::vec4 viewport = get_widget_viewport_rect(this);
        _popup->attach_to_viewport(this->viewport());
        _popup->set_visible();
        _popup->sync_widget_flags();
        static_cast<Widget *>(_popup)->update_depth(get_combo_popup_depth_range());
        static_cast<Widget *>(_popup)->update_layout(false);
        const amal::vec2 content_origin = measure_popup_pos + amal::vec2{popup_padding.x, popup_padding.y};
        amal::vec2 cursor = content_origin;
        u32 visible_items = 0u;
        for (u32 i = 0; i < _popup->children.size(); ++i)
        {
            auto *child = static_cast<detail::Selectable *>(_popup->children[i]);
            if (!child || !child->is_visible()) continue;
            ++visible_items;
            child->update_layout_min_size();
            child->set_layout_size({content_width, child->required_size().y});
            child->set_position(cursor);
            child->update_layout(true);
            cursor = {content_origin.x, cursor.y + child->required_size().y};
        }
        const f32 measured_h =
            (cursor.y > content_origin.y) ? (cursor.y - measure_popup_pos.y + popup_padding.w) : 0.0f;
        const f32 desired_h = amal::max(measured_h, AUIK_COMBO_BOX_POPUP_ITEM_FALLBACK_HEIGHT);
        const f32 content_h = amal::max(measured_h - popup_padding.y - popup_padding.w, 0.0f);
        const f32 item_h = visible_items > 0u ? content_h / static_cast<f32>(visible_items)
                                              : AUIK_COMBO_BOX_POPUP_ITEM_FALLBACK_HEIGHT;
        const auto placement =
            resolve_dropdown_popup_placement(position().y, size().y, desired_h, item_h, visible_items, viewport,
                                             AUIK_COMBO_BOX_POPUP_ITEM_FALLBACK_HEIGHT);
        if (placement.need_scroll) _popup->window_flags = get_popup_window_flags() | WindowFlagBits::docked;
        else _popup->window_flags = (get_popup_window_flags() | WindowFlagBits::docked) & ~WindowFlagBits::scrollable;
        _popup->set_position({position().x, placement.y});
        _popup->set_size({size().x, placement.height});
        _popup->attach_to_viewport(this->viewport());
        _popup->set_visible();
        _popup->sync_widget_flags();
        static_cast<Widget *>(_popup)->update_depth(get_combo_popup_depth_range());
        static_cast<Widget *>(_popup)->update_layout(false);
    }

    void MultipleCombobox::schedule_outside_click_tick()
    {
        if (!detail::g_context) return;
        detail::update_window_time(detail::get_context().window_ctx);
        const f64 delay = get_max_animation_delay() > 0.0 ? get_max_animation_delay() : (1.0 / 60.0);
        schedule_delayed_host_task(id(), detail::get_context().window_ctx->time + delay,
                                   [this]() { tick_outside_click(); });
    }

    void MultipleCombobox::tick_outside_click()
    {
        if (!_open || !detail::g_context) return;
        add_render_command<detail::ClickEventTraits>(this, [this]() {
            if (!_open || !detail::g_context) return;
            auto &ctx = detail::get_context();
            if (style_state() != StyleState::focus)
            {
                set_style_state(StyleState::focus);
                update_style();
                redraw_external(has_draw_record());
            }
            if (!ctx.io.mouse_down)
            {
                const amal::vec2 mouse_pos = get_mouse_pos();
                const auto &combo_rect = bounds();
                const bool in_combo = mouse_pos.x >= combo_rect.offset.x && mouse_pos.y >= combo_rect.offset.y &&
                                      mouse_pos.x < (combo_rect.offset.x + combo_rect.size.x) &&
                                      mouse_pos.y < (combo_rect.offset.y + combo_rect.size.y);
                bool in_popup = false;
                if (_popup)
                {
                    const auto &popup_rect = _popup->bounds();
                    in_popup = mouse_pos.x >= popup_rect.offset.x && mouse_pos.y >= popup_rect.offset.y &&
                               mouse_pos.x < (popup_rect.offset.x + popup_rect.size.x) &&
                               mouse_pos.y < (popup_rect.offset.y + popup_rect.size.y);
                }
                if (!in_combo && !in_popup && ctx.focus_id != id())
                {
                    close();
                    redraw_all_commands();
                    return;
                }
            }
            schedule_outside_click_tick();
        });
        detail::mark_host_refresh_request();
    }

    bool MultipleCombobox::has_draw_record() const { return _trigger && _trigger->has_draw_record(); }

    namespace
    {
        void write_combobox(acul::bin_stream &stream, umbf::Block *block)
        {
            auto *widget = static_cast<Combobox *>(block);
            detail::write_widget_common_data(stream, *widget);
            const auto items = widget->item_text_views();
            stream.write(static_cast<u32>(items.size()));
            for (const auto &item : items)
                detail::write_localized_string(stream, item.str ? item.str : "", item.is_translated);
            stream.write(widget->selected_index()).write(widget->style_tag());
        }

        umbf::Block *read_combobox(acul::bin_stream &stream)
        {
            const auto common = detail::read_widget_common_data(stream);
            u32 item_count = 0u;
            stream.read(item_count);
            acul::vector<acul::string> item_storage;
            acul::vector<bool> item_translated;
            item_storage.resize(item_count);
            item_translated.resize(item_count);
            for (u32 i = 0u; i < item_count; ++i)
            {
                auto item = detail::read_localized_string(stream);
                item_storage[i] = std::move(item.text);
                item_translated[i] = item.translated;
            }
            u32 selected_index = 0u;
            u32 style_tag = AUIK_STYLE_TAG_COMBO_BOX;
            stream.read(selected_index).read(style_tag);

            acul::vector<StringView> items;
            items.reserve(item_storage.size());
            for (u32 i = 0u; i < item_storage.size(); ++i)
                items.push_back({item_storage[i].c_str(), item_translated[i]});
            auto *widget =
                acul::alloc<Combobox>(common.id, items, selected_index, common.inline_size,
                                      WidgetFlags(common.widget_flags));
            widget->set_style_tag(style_tag);
            detail::apply_widget_common_data(widget, common);
            return widget;
        }

        void write_multiple_combobox(acul::bin_stream &stream, umbf::Block *block)
        {
            auto *widget = static_cast<MultipleCombobox *>(block);
            detail::write_widget_common_data(stream, *widget);
            const auto items = widget->item_text_views();
            stream.write(static_cast<u32>(items.size()));
            for (const auto &item : items)
                detail::write_localized_string(stream, item.str ? item.str : "", item.is_translated);
            detail::write_localized_string(
                stream, widget->is_translated_placeholder() ? widget->placeholder_literal() : widget->placeholder(),
                widget->is_translated_placeholder());
            const auto &selected_indices = widget->selected_indices();
            stream.write(static_cast<u32>(selected_indices.size()));
            if (!selected_indices.empty())
                stream.write(selected_indices.data(), static_cast<u32>(selected_indices.size()));
            stream.write(widget->style_tag());
        }

        umbf::Block *read_multiple_combobox(acul::bin_stream &stream)
        {
            const auto common = detail::read_widget_common_data(stream);
            u32 item_count = 0u;
            stream.read(item_count);
            acul::vector<acul::string> item_storage;
            acul::vector<bool> item_translated;
            item_storage.resize(item_count);
            item_translated.resize(item_count);
            for (u32 i = 0u; i < item_count; ++i)
            {
                auto item = detail::read_localized_string(stream);
                item_storage[i] = std::move(item.text);
                item_translated[i] = item.translated;
            }
            auto placeholder = detail::read_localized_string(stream);
            u32 selected_count = 0u;
            stream.read(selected_count);
            acul::vector<u32> selected_indices;
            selected_indices.resize(selected_count);
            if (selected_count != 0u) stream.read(selected_indices.data(), selected_count);
            u32 style_tag = AUIK_STYLE_TAG_COMBO_BOX;
            stream.read(style_tag);

            acul::vector<StringView> items;
            items.reserve(item_storage.size());
            for (u32 i = 0u; i < item_storage.size(); ++i)
                items.push_back({item_storage[i].c_str(), item_translated[i]});
            auto *widget = acul::alloc<MultipleCombobox>(common.id, items,
                                                         StringView{placeholder.text.c_str(), placeholder.translated},
                                                         common.inline_size, WidgetFlags(common.widget_flags));
            widget->set_style_tag(style_tag);
            widget->set_selected_indices(selected_indices);
            detail::apply_widget_common_data(widget, common);
            return widget;
        }
    } // namespace

    namespace streams
    {
        AUIK_EXPORT const umbf::streams::Stream combobox{read_combobox, write_combobox};
        AUIK_EXPORT const umbf::streams::Stream multiple_combobox{read_multiple_combobox, write_multiple_combobox};
    } // namespace streams
} // namespace auik
