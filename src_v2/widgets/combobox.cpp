#include <amal/trigonometric.hpp>
#include <auik/v2/auik.hpp>
#include <auik/v2/detail/depth.hpp>
#include <auik/v2/pipelines.hpp>
#include <auik/v2/widgets/combobox.hpp>
#include <auik/v2/widgets/detail/selectable.hpp>
#include <auik/v2/widgets/image.hpp>
#include <auik/v2/widgets/text.hpp>
#include <auik/v2/widgets/window.hpp>

#define AUIK_COMBO_BOX_POPUP_ITEM_FALLBACK_HEIGHT 24.0f
#define AUIK_COMBO_BOX_ICON_ROTATE_DURATION       0.16f

namespace auik::v2
{
    namespace
    {
        static inline amal::vec2 get_combo_popup_depth_range()
        {
            return detail::get_root_depth_range(detail::DepthZone::foreground, 31);
        }

        static inline const RotatePostRuntimeData *get_combo_rotate_data_const(u32 id)
        {
            return get_rotate_post_effect_data(get_rotate_post_effect(), id);
        }

        static inline bool draw_id_valid(const DrawDataID &id) { return id.render_id != AUIK_INVALID_DRAW_DATA_ID; }

        static inline void build_animated_icon_vertices(TexturedVertexStreamVertex (&vertices)[4],
                                                        const amal::rect &icon_rect, const amal::rect &uv_rect, f32 z,
                                                        u32 clip_id, bool visible)
        {
            const amal::vec2 min = icon_rect.offset;
            const amal::vec2 max = icon_rect.offset + icon_rect.size;
            const amal::vec2 uv_min = uv_rect.offset;
            const amal::vec2 uv_max = uv_rect.offset + uv_rect.size;
            if (!visible)
            {
                const amal::vec2 center = icon_rect.offset + icon_rect.size * 0.5f;
                for (auto &vertex : vertices) vertex = {center, z, 0.0f, uv_min, clip_id};
                return;
            }

            vertices[0] = {{min.x, min.y}, z, 0.0f, {uv_min.x, uv_min.y}, clip_id};
            vertices[1] = {{max.x, min.y}, z, 0.0f, {uv_max.x, uv_min.y}, clip_id};
            vertices[2] = {{max.x, max.y}, z, 0.0f, {uv_max.x, uv_max.y}, clip_id};
            vertices[3] = {{min.x, max.y}, z, 0.0f, {uv_min.x, uv_max.y}, clip_id};
        }
    } // namespace

    ComboBox::ComboBox(u32 id, acul::vector<acul::string> items, u32 selected_index, amal::vec2 size,
                       WidgetFlags widget_flags, Widget *parent)
        : Widget(id, widget_flags, EventFlagBits::click | EventFlagBits::focus, parent, {{0.0f, 0.0f}, size},
                 AUIK_TAG_COMBO_BOX),
          _selected_index(selected_index),
          _icon_hit_rect(detail::make_rect_data(AUIK_TAG_COMBO_BOX_ICON, AUIK_TAG_COMBO_BOX_ICON))
    {
        ensure_icon_resources();
        _label = acul::alloc<Text>(AUIK_TAG_TEXT, "", amal::vec2{0.0f, 0.0f},
                                   get_default_fixed_text_flags() & ~WidgetFlagBits::attachable, this, AUIK_TAG_NO_PAD);
        _label->set_horizontal_align(detail::TextHorizontalAlign::left);
        _label->set_vertical_align(detail::TextVerticalAlign::center);
        _label->update_style();

        _popup = acul::alloc<Window>(
            AUIK_TAG_COMBO_BOX_POPUP, "", amal::rect{{0.0f, 0.0f}, {0.0f, 0.0f}}, WindowFlagBits::scrollable,
            WidgetFlagBits::visible | WidgetFlagBits::hittable | WidgetFlagBits::foreground | WidgetFlagBits::fixed);
        _popup->get_rect().widget_id = this->id();
        _popup->set_window_style_tag(AUIK_TAG_COMBO_BOX_POPUP);
        _popup->set_focus_parent(this);
        _popup->update_style();
        _popup->hide();

        set_items(std::move(items));
        if (auto *rotate_effect = get_rotate_post_effect())
            _rotate_post_id = create_rotate_post_effect_data(rotate_effect, this);
    }

    ComboBox::~ComboBox()
    {
        _open = false;
        if (_popup) _popup->hide();
        if (auto *rotate_effect = get_rotate_post_effect();
            rotate_effect && _rotate_post_id != AUIK_INVALID_POST_EFFECT_DATA_ID)
            release_rotate_post_effect_data(rotate_effect, _rotate_post_id);
        cancel_delayed_tasks(id());
        if (_popup) acul::release(_popup);
        if (_label) acul::release(_label);
    }

    const acul::string &ComboBox::selected_text() const
    {
        static const acul::string empty{};
        if (!_popup || _selected_index >= _popup->children.size()) return empty;
        auto *item = static_cast<detail::Selectable *>(_popup->children[_selected_index]);
        return item ? item->text() : empty;
    }

    acul::vector<acul::string> ComboBox::items() const
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

    void ComboBox::set_items(const acul::vector<acul::string> &items)
    {
        const u32 prev_selected = _selected_index;
        _popup->hide();
        _popup->clear_children();

        for (u32 i = 0; i < items.size(); ++i)
        {
            auto *item = acul::alloc<detail::Selectable>(AUIK_TAG_COMBO_BOX_ITEM, AUIK_TAG_COMBO_BOX_ITEM, i, items[i],
                                                         amal::vec2{0.0f, 0.0f}, _popup, AUIK_TAG_COMBO_BOX_ITEM,
                                                         WidgetFlagBits::visible | WidgetFlagBits::hittable);
            item->get_rect().widget_id = id();
            item->set_focus_parent(_popup);
            _popup->add_child(item, WindowChildLayout::block);
        }

        if (_popup->children.empty()) _selected_index = 0u;
        else if (prev_selected >= _popup->children.size())
            _selected_index = static_cast<u32>(_popup->children.size() - 1u);
        else _selected_index = prev_selected;

        for (u32 i = 0; i < _popup->children.size(); ++i)
        {
            auto *item = _popup->children[i];
            if (!item) continue;
            item->set_style_state(i == _selected_index ? StyleState::focus : StyleState::normal);
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

    void ComboBox::set_selected_index(u32 index)
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
            auto *prev = _popup->children[_selected_index];
            if (prev)
            {
                prev->set_style_state(StyleState::normal);
                prev->update_style();
            }
        }
        auto *next = _popup->children[index];
        if (next) next->set_style_state(StyleState::focus);
        _selected_index = index;
        sync_label_text();
        auto &ctx = detail::get_context();
        ctx.dirty_flags |= DirtyFlagBits::redraw;
        detail::mark_host_refresh_request();
    }

    StyleUpdateFlags ComboBox::update_style()
    {
        const u32 parent_id = parent() ? parent()->id() : 0u;
        StyleUpdateFlags flags = resolve_style_selector(_style, id(), parent_id, style_state());
        flags |= _label->update_style();
        if (_popup)
        {
            const auto transition = detail::get_widget_style_selector_transition(id());
            if (transition.current_id.tag_id == AUIK_TAG_WINDOW) flags |= _popup->update_style();
            if (transition.prev_id.tag_id == AUIK_TAG_COMBO_BOX_ITEM &&
                transition.prev_id.element_id < _popup->children.size())
            {
                auto &item = _popup->children[transition.prev_id.element_id];
                auto next_style_state =
                    item->get_rect().element_id == _selected_index ? StyleState::focus : StyleState::normal;
                item->set_style_state(next_style_state);
                flags |= item->update_style();
            }
            if (transition.current_id.tag_id == AUIK_TAG_COMBO_BOX_ITEM &&
                transition.current_id.element_id < _popup->children.size())
            {
                auto &item = _popup->children[transition.current_id.element_id];
                item->set_style_state(transition.current_state);
                flags |= item->update_style();
            }
        }
        return flags;
    }

    void ComboBox::update_layout_min_size()
    {
        ensure_icon_resources();

        const auto &style = get_theme()->get_style(_style.id);
        const amal::vec2 prev_label_size = _label->size();
        // Measure label natural size, independent of the currently assigned combo bounds.
        _label->set_size({0.0f, 0.0f});
        _label->update_layout_min_size();
        const amal::vec2 label_required = _label->required_size();
        _label->set_size(prev_label_size);

        const f32 icon_height = _icon_size.y > 0.0f ? _icon_size.y : style.text_size();
        const amal::vec4 margin = style.margin();
        const amal::vec4 padding = style.padding();
        amal::vec2 min_size = size();
        if (min_size.x <= 0.0f) min_size.x = 140.0f;
        if (min_size.y <= 0.0f) min_size.y = amal::max(label_required.y, icon_height) + padding.y + padding.w;
        set_required_size({min_size.x + margin.x + margin.z, min_size.y + margin.y + margin.w});
    }

    void ComboBox::update_layout(bool min_size_known)
    {
        if (!min_size_known) update_layout_min_size();

        const auto &style = get_theme()->get_style(_style.id);
        const amal::vec4 margin = style.margin();
        const amal::vec2 layout_origin = position();
        const amal::vec2 min_required = required_size();
        const amal::vec2 min_combo = {amal::max(min_required.x - margin.x - margin.z, 0.0f),
                                      amal::max(min_required.y - margin.y - margin.w, 0.0f)};
        amal::vec2 widget_size = size();
        if (!is_fixed()) widget_size.x = amal::max(widget_size.x - margin.x - margin.z, min_combo.x);
        else widget_size.x = amal::max(widget_size.x, min_combo.x);
        widget_size.y = amal::max(widget_size.y, min_combo.y);

        const amal::vec2 pos = {layout_origin.x + margin.x, layout_origin.y + margin.y};
        set_position(pos);
        set_size(widget_size);
        Widget::update_layout(true);
        set_clip_id(parent()->content_clip_id());

        rebuild_control_layout();
        if (_open) update_popup_layout();
        else if (_popup) _popup->hide();
    }

    void ComboBox::translate(const amal::vec2 &delta)
    {
        if (delta.x == 0.0f && delta.y == 0.0f) return;
        Widget::translate(delta);
        _label_rect.offset += delta;
        _icon_rect.offset += delta;
        _icon_hit_rect.bounds.offset += delta;
        if (_label) _label->translate(delta);
        if (_open && _popup) static_cast<Widget *>(_popup)->translate(delta);
    }

    void ComboBox::rebuild_clip_rects()
    {
        assert(parent() && "ComboBox must have parent");
        set_clip_id(parent()->content_clip_id());
        _bg_draw.hit_id = AUIK_INVALID_DRAW_DATA_ID;
        _icon_draw.hit_id = AUIK_INVALID_DRAW_DATA_ID;
        _animated_icon_draw.hit_id = AUIK_INVALID_DRAW_DATA_ID;
        _icon_hit_rect.clip_id = clip_id();
        if (_label) _label->rebuild_clip_rects();
        if (_popup) _popup->rebuild_clip_rects();
    }

    void ComboBox::update_depth(const amal::vec2 &depth_range)
    {
        Widget::update_depth(depth_range);
        assign_next_depth(this->depth_range(), _bg_depth_range);
        assign_next_depth(_bg_depth_range, _content_depth_range);
        if (_label) _label->update_depth(_content_depth_range);
        _icon_hit_rect.depth = next_depth(_content_depth_range);
        if (_popup) static_cast<Widget *>(_popup)->update_depth(get_combo_popup_depth_range());
    }

    void ComboBox::draw(DrawCtx &ctx)
    {
        auto *theme = get_theme();
        auto *quads_stream = get_primary_quads_stream();
        auto *textured_quads_stream = get_primary_textured_quads_stream();

        QuadsInstanceData bg_data{};
        bg_data.rect = bounds();
        bg_data.z_order = next_depth(_bg_depth_range);
        fill_quads_instance_by_style(theme->get_style(_style.id), clip_id(), bg_data);
        ctx.emit(quads_stream, _bg_draw, &bg_data, get_rect(), ctx.emit_hit_rect);

        DrawCtx label_ctx = ctx;
        label_ctx.emit_hit_rect = false;
        _label->draw(label_ctx);

        ensure_icon_resources();
        TextureID icon_texture = _icon_texture;
        if (textured_quads_stream && icon_texture.handle != 0)
        {
            if ((detail::get_context().dirty_flags & DirtyFlagBits::textures) ||
                icon_texture.bind_slot == AUIK_INVALID_DRAW_DATA_ID)
                icon_texture.bind_slot = get_texture_bind_slot(icon_texture.handle);

            if (icon_texture.bind_slot != AUIK_INVALID_DRAW_DATA_ID)
            {
                const auto *rotate_data = get_combo_rotate_data_const(_rotate_post_id);
                TexturesInstanceData icon_data{};
                icon_data.rect = _icon_rect;
                icon_data.uv_rect = _icon_uv_rect;
                icon_data.tint_color = (rotate_data && rotate_data->animating)
                                           ? detail::pack_rgba8(255, 255, 255, 0)
                                           : theme->get_style(_style.id).text_color_packed();
                icon_data.z_order = _icon_hit_rect.depth;
                icon_data.texture_id = static_cast<u16>(icon_texture.bind_slot);
                icon_data.clip_id = clip_id();
                icon_data.flags = AUIK_TEXTURE_INSTANCE_TEXT_BIT;
                ctx.emit(textured_quads_stream, _icon_draw, &icon_data, _icon_hit_rect, false);
            }
        }

        auto *vertex_stream = get_primary_textured_vertex_stream();
        auto *rotate_effect = get_rotate_post_effect();
        if (vertex_stream && rotate_effect && _rotate_post_id != AUIK_INVALID_POST_EFFECT_DATA_ID)
        {
            TextureID animated_texture = _icon_texture;
            if (animated_texture.handle != 0)
            {
                if ((detail::get_context().dirty_flags & DirtyFlagBits::textures) ||
                    animated_texture.bind_slot == AUIK_INVALID_DRAW_DATA_ID)
                    animated_texture.bind_slot = get_texture_bind_slot(animated_texture.handle);

                if (animated_texture.bind_slot != AUIK_INVALID_DRAW_DATA_ID)
                {
                    auto *rotate_data = get_rotate_post_effect_data(rotate_effect, _rotate_post_id);
                    const bool animated_visible = rotate_data && rotate_data->animating;
                    if (rotate_data)
                    {
                        rotate_data->center = _icon_rect.offset + _icon_rect.size * 0.5f;
                        if (!rotate_data->animating) rotate_data->angle = 0.0f;
                    }

                    TexturedVertexStreamVertex vertices[4]{};
                    const TexturedVertexStreamIndex indices[6]{0u, 1u, 2u, 0u, 2u, 3u};
                    build_animated_icon_vertices(vertices, _icon_rect, _icon_uv_rect, _icon_hit_rect.depth,
                                                 static_cast<u32>(clip_id()), animated_visible);

                    TexturedVertexStreamBatchData animated_batch{};
                    animated_batch.vertices = vertices;
                    animated_batch.indices = indices;
                    animated_batch.vertex_count = 4u;
                    animated_batch.index_count = 6u;
                    animated_batch.texture_id = animated_texture;
                    animated_batch.flags = AUIK_TEXTURE_INSTANCE_TEXT_BIT;

                    RotatePostData rotate_post{_rotate_post_id};
                    DrawCtx rotated_ctx = ctx;
                    rotated_ctx.post_effect = rotate_effect;
                    rotated_ctx.post_data = &rotate_post;
                    rotated_ctx.emit(vertex_stream, _animated_icon_draw, &animated_batch, _icon_hit_rect, false);
                }
            }
        }

        if (_open && _popup)
        {
            DrawCtx popup_ctx = ctx;
            popup_ctx.emit_hit_rect = _popup->is_hittable();
            if (popup_ctx.reason & DrawReasonBits::style)
            {
                const auto transition = detail::get_widget_style_selector_transition(id());
                const bool popup_item_transition = transition.current_id.tag_id == AUIK_TAG_COMBO_BOX_ITEM ||
                                                   transition.prev_id.tag_id == AUIK_TAG_COMBO_BOX_ITEM;
                if (popup_item_transition) popup_ctx.reason &= ~DrawReasonBits::style;
            }
            _popup->draw(popup_ctx);
        }
    }

    void ComboBox::on_focus(bool focused)
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

    void ComboBox::on_click(MouseKey key, KeyPressState state, u32 click_count)
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
                    set_selected_index(index);
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

    void ComboBox::open()
    {
        if (_open) return;
        _open = true;
        set_style_state(StyleState::focus);
        update_style();
        update_popup_layout();
        schedule_outside_click_tick();
        start_icon_animation(true);
    }

    void ComboBox::close()
    {
        if (!_open) return;
        _open = false;
        set_style_state(StyleState::normal);
        update_style();
        if (_popup) _popup->hide();
        start_icon_animation(false);
    }

    void ComboBox::toggle()
    {
        if (_open) close();
        else open();
    }

    void ComboBox::ensure_icon_resources()
    {
        const auto *rotate_data = get_combo_rotate_data_const(_rotate_post_id);
        const bool animating = rotate_data && rotate_data->animating;
        const u32 icon_id =
            animating ? AUIK_ICON_CHEVRON_DOWN : (_open ? AUIK_ICON_CHEVRON_RIGHT : AUIK_ICON_CHEVRON_DOWN);
        if (auto *cached = get_cached_image(icon_id))
        {
            _icon_texture = cached->texture_id();
            _icon_size = cached->size();
            _icon_uv_rect = {cached->uv_offset(), cached->uv_size()};
            return;
        }

        _icon_texture = {};
        _icon_size = {0.0f, 0.0f};
        _icon_uv_rect = {{0.0f, 0.0f}, {1.0f, 1.0f}};
    }

    amal::vec2 ComboBox::resolve_icon_size() const { return _icon_size; }

    void ComboBox::rebuild_control_layout()
    {
        ensure_icon_resources();
        const auto &style = get_theme()->get_style(_style.id);
        const amal::vec4 padding = style.padding();
        const f32 content_height = amal::max(size().y - padding.y - padding.w, 0.0f);
        amal::vec2 icon_size = resolve_icon_size();
        if (icon_size.x <= 0.0f || icon_size.y <= 0.0f) icon_size = {style.text_size(), style.text_size()};
        const amal::vec2 icon_slot_size = icon_size;
        const f32 icon_slot_x = position().x + size().x - padding.z - icon_slot_size.x;
        const f32 icon_slot_y = position().y + padding.y + amal::max((content_height - icon_slot_size.y) * 0.5f, 0.0f);
        const f32 icon_x = icon_slot_x + amal::max((icon_slot_size.x - icon_size.x) * 0.5f, 0.0f);
        const f32 icon_y = icon_slot_y + amal::max((icon_slot_size.y - icon_size.y) * 0.5f, 0.0f);
        _icon_rect = {{icon_x, icon_y}, icon_size};
        _icon_hit_rect.bounds = _icon_rect;
        _icon_hit_rect.clip_id = clip_id();
        _icon_hit_rect.depth = next_depth(_content_depth_range);

        const f32 label_w = amal::max(icon_slot_x - (position().x + padding.x) - 6.0f, 0.0f);
        _label_rect = {{position().x + padding.x, position().y + padding.y},
                       {label_w, amal::max(size().y - padding.y - padding.w, 0.0f)}};
        _label->set_position(_label_rect.offset);
        _label->set_size(_label_rect.size);
        _label->update_layout(false);
        _label->set_clip_id(clip_id());
    }

    void ComboBox::sync_label_text() { _label->set_text(selected_text()); }

    void ComboBox::update_popup_layout()
    {
        _popup->set_window_style_tag(AUIK_TAG_COMBO_BOX_POPUP);
        _popup->update_style();
        _popup->window_flags = (get_popup_window_flags() | WindowFlagBits::docked) & ~WindowFlagBits::scrollable;
        const auto &popup_style = get_theme()->get_style(
            get_theme()->get_resolved_style(AUIK_TAG_COMBO_BOX_POPUP, _popup->id(), 0, StyleState::normal));
        const amal::vec4 popup_padding = popup_style.padding();
        const f32 content_width = amal::max(size().x - popup_padding.x - popup_padding.z, 0.0f);
        const amal::vec2 measure_popup_pos = {position().x, position().y + size().y - 1.0f};
        _popup->set_position(measure_popup_pos);
        _popup->set_size({size().x, AUIK_COMBO_BOX_POPUP_ITEM_FALLBACK_HEIGHT});
        _popup->show();
        static_cast<Widget *>(_popup)->update_depth(get_combo_popup_depth_range());
        static_cast<Widget *>(_popup)->update_layout(false);

        const amal::vec2 content_origin = measure_popup_pos + amal::vec2{popup_padding.x, popup_padding.y};
        amal::vec2 cursor = content_origin;
        for (u32 i = 0; i < _popup->children.size(); ++i)
        {
            auto *child = static_cast<detail::Selectable *>(_popup->children[i]);
            if (!child || !child->is_visible()) continue;
            child->update_layout_min_size();
            if (!child->is_fixed()) child->set_size({content_width, child->size().y});
            child->set_position(cursor);
            child->update_layout(true);
            cursor = {content_origin.x, cursor.y + child->required_size().y};
        }

        const f32 measured_h =
            (cursor.y > content_origin.y) ? (cursor.y - measure_popup_pos.y + popup_padding.w) : 0.0f;
        const f32 desired_h = amal::max(measured_h, AUIK_COMBO_BOX_POPUP_ITEM_FALLBACK_HEIGHT);

        const amal::vec4 viewport = get_main_viewport();
        const f32 gap = 0.0f;
        const f32 below_space = amal::max(viewport.y + viewport.w - (position().y + size().y + gap), 0.0f);
        const f32 above_space = amal::max(position().y - gap - viewport.y, 0.0f);
        const bool fits_below = desired_h <= below_space;
        const bool fits_above = desired_h <= above_space;
        const bool place_above = !fits_below && (fits_above || above_space > below_space);
        _popup->set_window_style_tag(AUIK_TAG_COMBO_BOX_POPUP);
        _popup->update_style();
        const f32 available_h = place_above ? above_space : below_space;
        const bool need_scroll = desired_h > available_h;
        const f32 popup_h = need_scroll ? amal::max(available_h, AUIK_COMBO_BOX_POPUP_ITEM_FALLBACK_HEIGHT) : desired_h;
        if (need_scroll) _popup->window_flags = get_popup_window_flags() | WindowFlagBits::docked;
        else _popup->window_flags = (get_popup_window_flags() | WindowFlagBits::docked) & ~WindowFlagBits::scrollable;

        // Slight overlap removes rasterized 1px seam between control and popup.
        const f32 popup_y = place_above ? position().y - gap - popup_h + 1.0f : position().y + size().y + gap - 1.0f;
        _popup->set_position({position().x, popup_y});
        _popup->set_size({size().x, popup_h});
        _popup->show();
        static_cast<Widget *>(_popup)->update_depth(get_combo_popup_depth_range());
        static_cast<Widget *>(_popup)->update_layout(false);
    }

    void ComboBox::start_icon_animation(bool opening)
    {
        if (!detail::g_context) return;
        auto *rotate_effect = get_rotate_post_effect();
        if (!rotate_effect) return;
        if (_rotate_post_id == AUIK_INVALID_POST_EFFECT_DATA_ID)
        {
            _rotate_post_id = create_rotate_post_effect_data(rotate_effect, this);
            if (_rotate_post_id == AUIK_INVALID_POST_EFFECT_DATA_ID) return;
        }

        detail::update_window_time(detail::get_context().window_ctx);
        auto *rotate_data = get_rotate_post_effect_data(rotate_effect, _rotate_post_id);
        if (!rotate_data) return;
        rotate_data->animation_start = detail::get_context().window_ctx->time;
        rotate_data->animation_from = opening ? 0.0f : amal::pi<f32>();
        rotate_data->animation_to = opening ? amal::pi<f32>() : 0.0f;
        rotate_data->angle = rotate_data->animation_from;
        rotate_data->animating = true;
        schedule_icon_tick();
    }

    void ComboBox::schedule_icon_tick()
    {
        if (!detail::g_context) return;
        detail::update_window_time(detail::get_context().window_ctx);
        const f64 delay = get_max_animation_delay() > 0.0 ? get_max_animation_delay() : (1.0 / 60.0);
        schedule_delayed_host_task(id(), detail::get_context().window_ctx->time + delay,
                                   [this]() { tick_icon_animation(); });
    }

    void ComboBox::schedule_outside_click_tick()
    {
        if (!detail::g_context) return;
        detail::update_window_time(detail::get_context().window_ctx);
        const f64 delay = get_max_animation_delay() > 0.0 ? get_max_animation_delay() : (1.0 / 60.0);
        schedule_delayed_host_task(id(), detail::get_context().window_ctx->time + delay,
                                   [this]() { tick_outside_click(); });
    }

    void ComboBox::tick_outside_click()
    {
        if (!_open || !detail::g_context) return;

        add_render_command<detail::ClickEventTraits>(this, [this]() {
            if (!_open || !detail::g_context) return;

            auto &ctx = detail::get_context();
            if (style_state() != StyleState::focus)
            {
                set_style_state(StyleState::focus);
                update_style();
                redraw_external(has_draw_record(), DrawReasonBits::style);
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

    void ComboBox::tick_icon_animation()
    {
        if (!detail::g_context) return;
        auto *rotate_effect = get_rotate_post_effect();
        if (!rotate_effect) return;
        auto *rotate_data = get_rotate_post_effect_data(rotate_effect, _rotate_post_id);
        if (!rotate_data || !rotate_data->animating) return;

        add_render_command<detail::ClickEventTraits>(this, [this]() {
            if (!detail::g_context) return;
            auto *rotate_effect = get_rotate_post_effect();
            if (!rotate_effect) return;
            auto *rotate_data = get_rotate_post_effect_data(rotate_effect, _rotate_post_id);
            if (!rotate_data || !rotate_data->animating) return;

            detail::update_window_time(detail::get_context().window_ctx);
            const f64 now = detail::get_context().window_ctx->time;
            f64 raw_t = (now - rotate_data->animation_start) / AUIK_COMBO_BOX_ICON_ROTATE_DURATION;
            if (raw_t < 0.0) raw_t = 0.0;
            if (raw_t > 1.0) raw_t = 1.0;
            const f32 t = static_cast<f32>(raw_t);
            const f32 eased = 1.0f - (1.0f - t) * (1.0f - t);
            rotate_data->angle =
                rotate_data->animation_from + (rotate_data->animation_to - rotate_data->animation_from) * eased;

            if (t >= 1.0f)
            {
                rotate_data->angle = rotate_data->animation_to;
                rotate_data->animating = false;
                rebuild_control_layout();
                redraw_external(has_draw_record(), DrawReasonBits::external);
                detail::mark_host_refresh_request();
                return;
            }

            redraw_external(has_draw_record(), DrawReasonBits::external);
            detail::mark_host_refresh_request();
            schedule_icon_tick();
        });
        detail::mark_host_refresh_request();
    }

    bool ComboBox::has_draw_record() const
    {
        if (!draw_id_valid(_bg_draw)) return false;
        if (!draw_id_valid(_icon_draw)) return false;
        if (_icon_texture.handle != 0 && !draw_id_valid(_animated_icon_draw)) return false;
        return true;
    }

} // namespace auik::v2
