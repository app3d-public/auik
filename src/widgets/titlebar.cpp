#include <auik/auik.hpp>
#include <auik/detail/depth.hpp>
#include <auik/detail/platform.hpp>
#include <auik/pipelines.hpp>
#include <auik/widgets/image.hpp>
#include <auik/widgets/menu.hpp>
#include <auik/widgets/titlebar.hpp>
#include "../core/session_stream_utils.hpp"
#ifdef _WIN32
    #include <windowsx.h>
#endif

#define AUIK_TITLEBAR_ICON_ID        0x54CC3922
#define AUIK_TITLEBAR_HEIGHT_DEFAULT 32.0f
#ifdef _WIN32
    #define AUIK_TITLEBAR_STATE L"AUIK_TITLEBAR_STATE"
#endif

namespace auik
{
#ifdef _WIN32
    static bool update_win32_titlebar_metrics(TitlebarState &base_state, UINT dpi = 0u);
#endif

    inline void destroy_titlebar_state(TitlebarState *state)
    {
        assert(state->destroy && "destroy function is null");
        state->destroy(state);
    }

    static void destroy_titlebar_icon(Image *&icon, acul::vector<Widget *> &children,
                                      detail::ImageTextureResource &icon_texture)
    {
        if (icon)
        {
            erase_cached_image(icon->id());
            for (auto it = children.begin(); it != children.end(); ++it)
            {
                if (*it != icon) continue;
                children.erase(it);
                break;
            }
            acul::release(icon);
            icon = nullptr;
        }
        if (icon_texture.handle) detail::destroy_image_texture(detail::get_context().gpu_ctx, &icon_texture);
    }

    static u32 caption_button_icon_id(u32 index, bool maximized)
    {
        switch (index)
        {
            case AUIK_WINDOW_CAPTION_BTN_MIN:
                return AUIK_ICON_CAP_MINIMIZE;
            case AUIK_WINDOW_CAPTION_BTN_MAX:
                return maximized ? AUIK_ICON_CAP_RESTORE : AUIK_ICON_CAP_MAXIMIZE;
            case AUIK_WINDOW_CAPTION_BTN_CLOSE:
                return AUIK_ICON_CAP_CLOSE;
            default:
                return 0u;
        }
    }

    static u32 caption_button_widget_id(u32 index)
    {
        static constexpr u32 ids[AUIK_WINDOW_CAPTION_BTN_COUNT] = {0xAE947B41u, 0xAE947B42u, 0xAE947B43u};
        return index < AUIK_WINDOW_CAPTION_BTN_COUNT ? ids[index] : 0u;
    }

    static void refresh_caption_button_style(TitlebarState &state, i32 index)
    {
        if (index < 0 || index >= AUIK_WINDOW_CAPTION_BTN_COUNT) return;
        auto *button = state.caption_button_widgets[index];
        if (!button) return;
        const bool active = state.active_button == index;
        const bool hover = state.hover_button == index;
        const StyleState next_state = active ? StyleState::active : (hover ? StyleState::hover : StyleState::normal);
        if (!button->set_style_state(next_state)) return;

        const StyleUpdateFlags flags = button->update_style_invalidated();
        if (flags & StyleUpdateFlagBits::layout)
        {
            if (state.titlebar) state.titlebar->update_layout(false);
            if (state.titlebar) state.titlebar->update_draw_commands(DrawReasonBits::layout);
        }
        else button->update_draw_commands(get_draw_reason_from_style_update(flags));

        auto &ctx = detail::get_context();
        ctx.dirty_flags |= DirtyFlagBits::redraw;
        mark_host_refresh_request();
    }

    static void update_titlebar_child_depths(const amal::vec2 &parent_depth_range,
                                             const acul::vector<Widget *> &children)
    {
        const amal::vec2 work_range = detail::depth_work_range(parent_depth_range);
        for (auto *child : children)
        {
            if (!child) continue;
            child->update_depth(work_range);
        }
    }

    static void update_titlebar_caption_button_depths(const amal::vec2 &parent_depth_range,
                                                      ImageButton *const (&buttons)[AUIK_WINDOW_CAPTION_BTN_COUNT])
    {
        const amal::vec2 work_range = detail::depth_foreground_range(parent_depth_range);
        for (auto *button : buttons)
        {
            if (!button) continue;
            button->update_depth(work_range);
        }
    }

    static inline f32 snap_titlebar_size_from(f32 offset, f32 size)
    {
        return amal::max(amal::round(offset + size) - offset, 0.0f);
    }

    Titlebar::Titlebar(u32 id, WidgetFlags widget_flags)
        : Widget(id, widget_flags, EventFlagBits::none, {{0.0f, 0.0f}, AUIK_SIZE_INHERIT}, AUIK_TAG_TITLEBAR)
    {
    }

    void Titlebar::set_show_icon(bool value)
    {
        if (_show_icon == value) return;
        _show_icon = value;
        detail::mark_layout_dirty();
        mark_host_refresh_request();
    }

    void Titlebar::set_leading_count(u32 count) { _leading_count = count; }

    void Titlebar::set_menu_popup_viewport(Viewport *viewport)
    {
        _menu_popup_viewport = viewport;
        for (auto *child : _children)
            if (child && child->signature() == AUIK_TAG_MENU_BAR)
                static_cast<MenuBar *>(child)->set_popup_viewport(viewport);
    }

    void Titlebar::add_child(Widget *child)
    {
        assert(child && "titlebar child is null");
        child->set_parent(this);
        child->set_focus_parent(this);
        if (child->signature() == AUIK_TAG_MENU_BAR)
        {
            auto *menu = static_cast<MenuBar *>(child);
            menu->set_popup_parent(this);
            menu->set_popup_viewport(_menu_popup_viewport);
        }
        _children.push_back(child);
        if (child->widget_flags & WidgetFlagBits::attachable) child->on_attach();
    }

    void Titlebar::add_children(const acul::vector<Widget *> &children)
    {
        for (auto *child : children)
        {
            if (!child) continue;
            add_child(child);
        }
    }

    void Titlebar::ensure_icon_widget()
    {
        if (!_show_icon)
        {
            destroy_titlebar_icon(_icon, _children, _icon_texture);
            return;
        }

        if (_icon) return;

        umbf::Image2D icon_image{};
        if (!detail::get_window_icon_image(detail::get_window_context(), icon_image)) return;

        if (!detail::create_image_texture(detail::get_context().gpu_ctx, &_icon_texture, icon_image))
        {
            acul::release(icon_image.pixels);
            return;
        }

        const amal::vec2 icon_size{static_cast<f32>(_icon_texture.width), static_cast<f32>(_icon_texture.height)};
        auto *icon = make_image(AUIK_TITLEBAR_ICON_ID, _icon_texture.texture_id, icon_size);
        cache_image(icon);
        icon->set_parent(this);
        icon->set_focus_parent(this);
        _children.insert(_children.begin(), icon);
        _icon = icon;
        update_titlebar_child_depths(depth_range(), _children);
        acul::release(icon_image.pixels);
    }
    void Titlebar::ensure_caption_buttons()
    {
        if (!_state) return;
        const bool maximized = detail::get_context().window_ctx->host_state == HostWindowState::maximized;
        for (u32 i = 0; i < AUIK_WINDOW_CAPTION_BTN_COUNT; ++i)
        {
            const u32 icon_id = caption_button_icon_id(i, maximized);
            auto *image = get_cached_image(icon_id);
            if (!image) continue;
            if (!_caption_buttons[i])
            {
                const u32 style_tag = i == AUIK_WINDOW_CAPTION_BTN_CLOSE ? AUIK_STYLE_TAG_WINDOW_CAPTION_CLOSE_BUTTON
                                                                         : AUIK_STYLE_TAG_WINDOW_CAPTION_BUTTON;
                _caption_buttons[i] =
                    acul::alloc<ImageButton>(caption_button_widget_id(i), image, amal::vec2{0.0f, 0.0f},
                                             amal::vec2{0.0f, 0.0f}, WidgetFlagBits::visible, style_tag);
                _caption_buttons[i]->set_parent(this);
                _caption_buttons[i]->set_focus_parent(this);
            }
            else _caption_buttons[i]->set_image(image);
            if (_state) _state->caption_button_widgets[i] = _caption_buttons[i];
        }
        update_titlebar_caption_button_depths(depth_range(), _caption_buttons);
    }

    void Titlebar::set_caption_hover_button(i32 index)
    {
        if (!_state || _state->hover_button == index) return;
        const i32 prev = _state->hover_button;
        _state->hover_button = index;
        refresh_caption_button_style(*_state, prev);
        refresh_caption_button_style(*_state, _state->hover_button);
    }

    void Titlebar::set_caption_active_button(i32 index)
    {
        if (!_state || _state->active_button == index) return;
        const i32 prev = _state->active_button;
        _state->active_button = index;
        refresh_caption_button_style(*_state, prev);
        refresh_caption_button_style(*_state, _state->active_button);
    }

    StyleUpdateFlags Titlebar::update_style()
    {
        StyleUpdateFlags out = resolve_style_selector(_style, id(), 0, style_state());
        out |= resolve_style_selector(_icon_style, _icon_style.tag_id, 0, style_state());
        out |= resolve_style_selector(_leading_region_style, _leading_region_style.tag_id, 0, style_state());
        for (auto *child : _children)
        {
            if (!child) continue;
            out |= child->update_style_invalidated();
        }
        for (auto *button : _caption_buttons)
        {
            if (!button) continue;
            out |= button->update_style_invalidated();
        }
        return out;
    }

    void Titlebar::update_layout_min_size_force()
    {
        const f32 resolved_height = _state && _state->height > 0.0f ? _state->height : AUIK_TITLEBAR_HEIGHT_DEFAULT;
        set_required_size({size().x, resolved_height});
    }

    void Titlebar::update_layout(bool min_size_known)
    {
        ensure_icon_widget();
        ensure_caption_buttons();
        if (layout_measure_required(min_size_known)) update_layout_min_size_force();
        const auto display = get_display_size();
        const f32 resolved_height = _state && _state->height > 0.0f ? _state->height : AUIK_TITLEBAR_HEIGHT_DEFAULT;
        const f32 resolved_width = size().x > 0.0f ? size().x : display.x;
        set_layout_size({resolved_width, resolved_height});
        set_required_size(size());
        Widget::update_layout(true);

        ensure_own_clip_rect({position().x, position().y, size().x, size().y});

        _caption_buttons_width = 0.0f;
        const amal::vec2 caption_button_size = _state ? _state->caption_button_size : amal::vec2{};
        if (caption_button_size.x > 0.0f && caption_button_size.y > 0.0f)
        {
            f32 button_x = position().x + size().x;
            for (u32 i = AUIK_WINDOW_CAPTION_BTN_COUNT; i > 0u; --i)
            {
                const u32 index = i - 1u;
                auto *button = _caption_buttons[index];
                if (!button || (_state && !_state->caption_buttons[index])) continue;
                button_x -= caption_button_size.x;
                button->set_position({button_x, position().y});
                button->set_size(caption_button_size);
                button->update_style_invalidated();
                button->update_layout(true);
                _caption_buttons_width += caption_button_size.x;
            }
        }
        const amal::vec4 icon_margin = get_theme()->get_style(_icon_style.id).margin();

        struct ChildLayoutMeasure
        {
            Widget *child = nullptr;
            amal::vec2 required{};
            f32 width = 0.0f;
            bool fill_width = false;
        };

        acul::vector<ChildLayoutMeasure> child_measures;
        child_measures.reserve(_children.size());
        f32 fixed_children_width = 0.0f;
        f32 fill_children_min_width = 0.0f;
        u32 fill_child_count = 0u;
        for (auto *child : _children)
        {
            if (!child) continue;
            child->update_layout_min_size();
            ChildLayoutMeasure measure{};
            measure.child = child;
            measure.required = child->required_size();
            measure.fill_width = child->fill_width() && child != _icon;

            if (child == _icon) fixed_children_width += icon_margin.x + measure.required.x + icon_margin.z;
            else if (measure.fill_width)
            {
                fill_children_min_width += measure.required.x;
                ++fill_child_count;
            }
            else fixed_children_width += measure.required.x;

            child_measures.push_back(measure);
        }

        const f32 layout_right = position().x + amal::max(size().x - _caption_buttons_width, 0.0f);
        const f32 available_children_width = amal::max(layout_right - position().x, 0.0f);
        const f32 fill_children_width = amal::max(available_children_width - fixed_children_width, 0.0f);
        const f32 fill_extra_width =
            fill_child_count > 0u
                ? amal::max(fill_children_width - fill_children_min_width, 0.0f) / static_cast<f32>(fill_child_count)
                : 0.0f;
        for (auto &measure : child_measures)
        {
            if (measure.fill_width) measure.width = measure.required.x + fill_extra_width;
            else measure.width = measure.required.x;
        }

        amal::vec2 cursor = position();
        f32 leading_region_end_x = position().x;
        f32 content_end_x = position().x;
        u32 visible_child_index = 0u;
        for (const auto &measure : child_measures)
        {
            auto *child = measure.child;
            if (!child) continue;
            if (child == _icon) cursor.x += icon_margin.x;

            const amal::vec2 measured_required = measure.required;

            if (child == _icon)
            {
                const f32 child_w = snap_titlebar_size_from(cursor.x, child->required_size().x);
                child->set_position(cursor);
                child->set_layout_size({child_w, child->required_size().y});
                child->update_layout(true);

                const f32 centered_y = position().y + amal::round(amal::max((size().y - child->size().y) * 0.5f, 0.0f));
                const amal::vec2 icon_pos = child->position();
                child->translate({0.0f, centered_y - icon_pos.y});
                cursor.x = child->position().x + child->size().x + icon_margin.z;
                content_end_x = amal::max(content_end_x, cursor.x);
            }
            else
            {
                const bool fill_child_height = child->fill_height();
                const bool fill_child_width = measure.fill_width;
                const f32 child_h = fill_child_height ? size().y : measured_required.y;
                const f32 outer_y =
                    fill_child_height
                        ? position().y
                        : position().y + amal::round(amal::max((size().y - measured_required.y) * 0.5f, 0.0f));
                const amal::vec2 outer_pos = {cursor.x, outer_y};
                const f32 child_w = snap_titlebar_size_from(outer_pos.x, measure.width);
                child->set_position(outer_pos);
                child->set_layout_size({child_w, child_h});
                child->update_layout(true);
                cursor.x = outer_pos.x + amal::max(child_w, child->size().x);
                const f32 interactive_end_x = fill_child_width ? outer_pos.x + measured_required.x : cursor.x;
                content_end_x = amal::max(content_end_x, interactive_end_x);
            }
            if (_leading_count > 0u && visible_child_index < _leading_count) leading_region_end_x = cursor.x;
            ++visible_child_index;
        }
        if (_leading_count > 0u && leading_region_end_x > position().x)
        {
            const amal::vec4 margin = get_theme()->get_style(_leading_region_style.id).margin();
            const f32 x0 = position().x + margin.x;
            const f32 y0 = position().y + margin.y;
            const f32 x1 = leading_region_end_x + margin.z;
            const f32 y1 = position().y + size().y - margin.w;
            _leading_region_rect = {{x0, y0}, {amal::max(x1 - x0, 0.0f), amal::max(y1 - y0, 0.0f)}};
        }
        else _leading_region_rect = {};
        if (_state)
        {
            _state->content_end_x = amal::max(content_end_x - position().x, 0.0f);
            _state->caption_buttons_width = _caption_buttons_width;
        }
    }

    void Titlebar::update_depth(const amal::vec2 &depth_range)
    {
        Widget::update_depth(depth_range);
        update_titlebar_child_depths(this->depth_range(), _children);
        update_titlebar_caption_button_depths(this->depth_range(), _caption_buttons);
    }

    void Titlebar::back_hit_depth()
    {
        Widget::back_hit_depth();
        for (auto *child : _children)
            if (child) child->back_hit_depth();
        for (auto *button : _caption_buttons)
            if (button) button->back_hit_depth();
    }

    void Titlebar::restore_hit_depth()
    {
        Widget::restore_hit_depth();
        for (auto *child : _children)
            if (child) child->restore_hit_depth();
        for (auto *button : _caption_buttons)
            if (button) button->restore_hit_depth();
    }

    void Titlebar::reset_clip_rect_records()
    {
        Widget::reset_clip_rect_records();
        for (auto *button : _caption_buttons)
            if (button) button->reset_clip_rect_records();
        for (auto *child : _children)
            if (child) child->reset_clip_rect_records();
    }

    void Titlebar::rebuild_clip_rects()
    {
        ensure_own_clip_rect({position().x, position().y, size().x, size().y});
        DrawDataID *hit_ids[] = {&_bg, &_icon_bg, &_leading_region_bg};
        invalidate_hit_rect_batch(hit_ids, 3);
        for (auto *button : _caption_buttons)
        {
            if (!button) continue;
            button->set_clip_id(clip_id());
            button->rebuild_clip_rects();
        }
        for (auto *child : _children)
        {
            if (!child) continue;
            child->set_clip_id(clip_id());
            child->rebuild_clip_rects();
        }
    }

    void Titlebar::reset_draw_records()
    {
        Widget::reset_draw_records();
        _bg = {};
        _icon_bg = {};
        _leading_region_bg = {};
        for (auto *button : _caption_buttons)
            if (button) button->reset_draw_records();
        for (auto *child : _children)
            if (child) child->reset_draw_records();
    }

    void Titlebar::draw(DrawCtx &ctx)
    {
        auto *theme = get_theme();
        auto *quads_stream = get_primary_quads_stream();
        const amal::vec2 background_range = detail::depth_background_range(depth_range());
        const f32 titlebar_bg_z = background_range.x;
        const f32 leading_region_bg_z = next_depth(background_range);

        QuadsInstanceData bg_data{};
        bg_data.rect = bounds();
        bg_data.z_order = titlebar_bg_z;
        const bool bg_visible = fill_quads_instance_by_style(theme->get_style(_style.id), clip_id(), bg_data);
        emit_quads_instance(ctx, quads_stream, _bg, bg_data, get_rect(), bg_visible, can_emit_hit(ctx));

        if (_leading_count > 0u && _leading_region_rect.size.x > 0.0f && _leading_region_rect.size.y > 0.0f)
        {
            QuadsInstanceData leading_region_bg{};
            leading_region_bg.rect = _leading_region_rect;
            leading_region_bg.z_order = leading_region_bg_z;
            const bool leading_region_visible =
                fill_quads_instance_by_style(theme->get_style(_leading_region_style.id), clip_id(), leading_region_bg);
            emit_quads_instance(ctx, quads_stream, _leading_region_bg, leading_region_bg, get_rect(),
                                leading_region_visible, false);
        }

        const Style &icon_style = theme->get_style(_icon_style.id);
        const amal::vec4 icon_margin = icon_style.margin();
        const bool draw_icon_background = _icon && (icon_style.mask() & detail::StylePropertiesBits::background_color);
        if (draw_icon_background)
        {
            QuadsInstanceData icon_bg_data{};
            const f32 x0 = position().x;
            const f32 x1 = _icon->position().x + _icon->size().x + icon_margin.z;
            icon_bg_data.rect = {{x0, position().y}, {amal::max(x1 - x0, 0.0f), size().y}};
            icon_bg_data.z_order = next_depth(background_range);
            const bool icon_bg_visible = fill_quads_instance_by_style(icon_style, clip_id(), icon_bg_data);
            emit_quads_instance(ctx, quads_stream, _icon_bg, icon_bg_data, get_rect(), icon_bg_visible, false);
        }

        for (auto *child : _children)
        {
            if (!child) continue;
            DrawCtx child_ctx = ctx;
            child->draw_local(child_ctx);
        }

        for (u32 i = 0; i < AUIK_WINDOW_CAPTION_BTN_COUNT; ++i)
        {
            auto *button = _caption_buttons[i];
            if (!button || (_state && !_state->caption_buttons[i])) continue;
            if (_state)
            {
                const bool active = _state->active_button == static_cast<i32>(i);
                const bool hover = _state->hover_button == static_cast<i32>(i);
                const StyleState button_state =
                    active ? StyleState::active : (hover ? StyleState::hover : StyleState::normal);
                button->set_style_state(button_state);
                button->update_style_invalidated();
            }
            DrawCtx button_ctx = ctx;
            button_ctx.is_hit_allowed = false;
            button->draw_local(button_ctx);
        }
    }

    void Titlebar::translate(const amal::vec2 &delta)
    {
        if (delta.x == 0.0f && delta.y == 0.0f) return;
        Widget::translate(delta);
        for (auto *child : _children)
        {
            if (!child) continue;
            child->translate(delta);
        }
        for (auto *button : _caption_buttons)
            if (button) button->translate(delta);
    }

    void Titlebar::on_attach()
    {
        Widget::on_attach();
        for (auto *child : _children)
            if (child && (child->widget_flags & WidgetFlagBits::attachable)) child->on_attach();
        for (auto *button : _caption_buttons)
            if (button && (button->widget_flags & WidgetFlagBits::attachable)) button->on_attach();
    }

    void Titlebar::on_detach()
    {
        Widget::on_detach();
        for (auto *child : _children)
            if (child && (child->widget_flags & WidgetFlagBits::attachable)) child->on_detach();
        for (auto *button : _caption_buttons)
            if (button && (button->widget_flags & WidgetFlagBits::attachable)) button->on_detach();
    }

#ifdef _WIN32
    struct Win32TitlebarState : TitlebarState
    {
        HWND hwnd = nullptr;
        WNDPROC prev_proc = nullptr;
    };

    static bool update_win32_titlebar_metrics(TitlebarState &base_state, UINT dpi)
    {
        auto &state = static_cast<Win32TitlebarState &>(base_state);
        if (dpi == 0u) dpi = GetDpiForWindow(state.hwnd);
        const auto previous_frame = state.frame;
        const i32 previous_padding = state.padding;
        const amal::vec2 previous_button_size = state.caption_button_size;
        const f32 previous_height = state.height;
        state.frame.x = GetSystemMetricsForDpi(SM_CXFRAME, dpi);
        state.frame.y = GetSystemMetricsForDpi(SM_CYFRAME, dpi);
        state.padding = GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
        const f32 scale = static_cast<f32>(dpi) / 96.0f;
        state.caption_button_size = {GetSystemMetrics(SM_CXSIZE) * scale, GetSystemMetrics(SM_CYSIZE) * scale};
        state.height = GetSystemMetrics(SM_CYCAPTION) * scale;
        return previous_frame != state.frame || previous_padding != state.padding ||
               previous_button_size != state.caption_button_size || previous_height != state.height;
    }

    static i32 hit_test_caption_button_index(const TitlebarState &state, const amal::ivec2 &pos, i32 client_width)
    {
        if (state.caption_button_size.x <= 0.0f || state.caption_button_size.y <= 0.0f) return -1;
        if (pos.y < 0 || pos.y >= static_cast<i32>(state.caption_button_size.y)) return -1;

        i32 x = client_width;
        for (u32 i = AUIK_WINDOW_CAPTION_BTN_COUNT; i > 0u; --i)
        {
            const u32 index = i - 1u;
            if (!state.caption_buttons[index]) continue;
            x -= static_cast<i32>(state.caption_button_size.x);
            if (pos.x >= x && pos.x < x + static_cast<i32>(state.caption_button_size.x)) return static_cast<i32>(index);
        }
        return -1;
    }

    static LRESULT caption_button_hit_code(i32 index)
    {
        switch (index)
        {
            case AUIK_WINDOW_CAPTION_BTN_MIN:
                return HTMINBUTTON;
            case AUIK_WINDOW_CAPTION_BTN_MAX:
                return HTMAXBUTTON;
            case AUIK_WINDOW_CAPTION_BTN_CLOSE:
                return HTCLOSE;
            default:
                return HTCLIENT;
        }
    }

    static LRESULT hit_test_titlebar_drag_area(const TitlebarState &state, const amal::ivec2 &pos, i32 client_width)
    {
        if (state.height <= 0.0f) return HTCLIENT;
        if (pos.y < 0 || pos.y >= static_cast<i32>(state.height)) return HTCLIENT;
        const i32 x0 = amal::max(static_cast<i32>(state.content_end_x), 0);
        const i32 x1 = amal::max(client_width - static_cast<i32>(state.caption_buttons_width), x0);
        return pos.x >= x0 && pos.x < x1 ? HTCAPTION : HTCLIENT;
    }

    static bool get_client_hit_pos(HWND hwnd, LPARAM lParam, amal::ivec2 &pos, i32 &client_width)
    {
        POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        if (!ScreenToClient(hwnd, &point)) return false;
        RECT client_rect{};
        if (!GetClientRect(hwnd, &client_rect)) return false;
        pos = {point.x, point.y};
        client_width = client_rect.right - client_rect.left;
        return true;
    }

    static LRESULT CALLBACK titlebar_window_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        auto *state = reinterpret_cast<Win32TitlebarState *>(GetPropW(hwnd, AUIK_TITLEBAR_STATE));
        const WNDPROC prev_proc = state ? state->prev_proc : nullptr;

        switch (msg)
        {
            case WM_DPICHANGED:
            {
                if (state && update_win32_titlebar_metrics(*state, LOWORD(wParam)))
                {
                    detail::mark_layout_dirty();
                    mark_host_refresh_request();
                }
                break;
            }
            case WM_NCHITTEST:
            {
                if (!state) break;
                const LRESULT prev_hit = prev_proc ? CallWindowProcW(prev_proc, hwnd, msg, wParam, lParam)
                                                   : DefWindowProcW(hwnd, msg, wParam, lParam);
                if (prev_hit != HTCLIENT) return prev_hit;

                amal::ivec2 pos{};
                i32 client_width = 0;
                if (!get_client_hit_pos(hwnd, lParam, pos, client_width)) return prev_hit;
                const i32 button_index = hit_test_caption_button_index(*state, pos, client_width);
                if (button_index >= 0)
                {
                    if (state->titlebar) state->titlebar->set_caption_hover_button(button_index);
                    return caption_button_hit_code(button_index);
                }

                const LRESULT drag_hit = hit_test_titlebar_drag_area(*state, pos, client_width);
                if (state->titlebar) state->titlebar->set_caption_hover_button(-1);
                return drag_hit != HTCLIENT ? drag_hit : prev_hit;
            }
            case WM_NCMOUSEMOVE:
            {
                if (!state || !state->titlebar) break;
                TRACKMOUSEEVENT tme{};
                tme.cbSize = sizeof(tme);
                tme.dwFlags = TME_LEAVE | TME_NONCLIENT;
                tme.hwndTrack = hwnd;
                TrackMouseEvent(&tme);
                amal::ivec2 pos{};
                i32 client_width = 0;
                if (!get_client_hit_pos(hwnd, lParam, pos, client_width)) break;
                state->titlebar->set_caption_hover_button(hit_test_caption_button_index(*state, pos, client_width));
                return 0;
            }
            case WM_NCMOUSELEAVE:
            {
                if (!state || !state->titlebar) break;
                state->titlebar->set_caption_hover_button(-1);
                if (state->active_button < 0) state->titlebar->set_caption_active_button(-1);
                return 0;
            }
            case WM_MOUSEMOVE:
            {
                if (!state || !state->titlebar || state->active_button < 0) break;
                RECT rect{};
                if (!GetClientRect(hwnd, &rect)) break;
                const amal::ivec2 pos{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                state->titlebar->set_caption_hover_button(
                    hit_test_caption_button_index(*state, pos, rect.right - rect.left));
                return 0;
            }
            case WM_NCLBUTTONDOWN:
            {
                if (!state) break;
                if (wParam == HTCAPTION)
                {
                    amal::ivec2 pos{};
                    i32 client_width = 0;
                    if (get_client_hit_pos(hwnd, lParam, pos, client_width) &&
                        hit_test_titlebar_drag_area(*state, pos, client_width) == HTCAPTION && state->titlebar)
                        focus_widget(state->titlebar);
                    break;
                }
                const i32 button_index = [&]() -> i32 {
                    switch (wParam)
                    {
                        case HTMINBUTTON:
                            return AUIK_WINDOW_CAPTION_BTN_MIN;
                        case HTMAXBUTTON:
                            return AUIK_WINDOW_CAPTION_BTN_MAX;
                        case HTCLOSE:
                            return AUIK_WINDOW_CAPTION_BTN_CLOSE;
                        default:
                            return -1;
                    }
                }();
                if (button_index < 0) break;
                if (state->titlebar)
                {
                    state->titlebar->set_caption_hover_button(button_index);
                    state->titlebar->set_caption_active_button(button_index);
                }
                else
                {
                    state->hover_button = button_index;
                    state->active_button = button_index;
                }
                SetCapture(hwnd);
                return 0;
            }
            case WM_LBUTTONUP:
            case WM_NCLBUTTONUP:
            {
                const i32 active_button = state ? state->active_button : -1;
                if (state && state->titlebar) state->titlebar->set_caption_active_button(-1);
                else if (state) state->active_button = -1;
                if (GetCapture() == hwnd) ReleaseCapture();
                if (!state || active_button < 0) break;

                RECT rect{};
                amal::ivec2 pos{};
                i32 client_width = 0;
                if (msg == WM_NCLBUTTONUP)
                {
                    if (!get_client_hit_pos(hwnd, lParam, pos, client_width)) break;
                }
                else
                {
                    if (!GetClientRect(hwnd, &rect)) break;
                    pos = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                    client_width = rect.right - rect.left;
                }
                const i32 release_button = hit_test_caption_button_index(*state, pos, client_width);
                if (state->titlebar) state->titlebar->set_caption_hover_button(release_button);
                else state->hover_button = release_button;
                if (release_button != active_button) return 0;

                switch (active_button)
                {
                    case AUIK_WINDOW_CAPTION_BTN_MIN:
                        SendMessageW(hwnd, WM_SYSCOMMAND, SC_MINIMIZE, 0);
                        return 0;
                    case AUIK_WINDOW_CAPTION_BTN_MAX:
                        SendMessageW(hwnd, WM_SYSCOMMAND, IsZoomed(hwnd) ? SC_RESTORE : SC_MAXIMIZE, 0);
                        return 0;
                    case AUIK_WINDOW_CAPTION_BTN_CLOSE:
                        SendMessageW(hwnd, WM_SYSCOMMAND, SC_CLOSE, 0);
                        return 0;
                    default:
                        break;
                }
                break;
            }
            case WM_NCDESTROY:
            {
                RemovePropW(hwnd, AUIK_TITLEBAR_STATE);
                if (prev_proc) SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(prev_proc));
                break;
            }
        }

        return prev_proc ? CallWindowProcW(prev_proc, hwnd, msg, wParam, lParam)
                         : DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    static void destroy_win32_titlebar_state(TitlebarState *state)
    {
        auto *win32_state = static_cast<Win32TitlebarState *>(state);
        HWND hwnd = win32_state->hwnd;
        if (hwnd && GetPropW(hwnd, AUIK_TITLEBAR_STATE) == state)
        {
            RemovePropW(hwnd, AUIK_TITLEBAR_STATE);
            if (win32_state->prev_proc)
                SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(win32_state->prev_proc));
        }
        acul::release(state);
    }

    static bool add_window_caption_button_icons(const FontRegistry &fonts, f32 dpi)
    {
        FontInfo *font_info = nullptr;
        if (detail::is_win_11_or_greater()) font_info = get_font_info_by_family(fonts, "Segoe Fluent Icons");
        if (!font_info) font_info = get_font_info_by_family(fonts, "Segoe MDL2 Assets");
        if (!font_info) return false;

        Font font;
        const u32 size = round_font_px(10.0f * dpi);
        const acul::vector<u32> codepoints{0xE921u, 0xE922u, 0xE923u, 0xE8BBu};
        const acul::vector<u32> ids{AUIK_ICON_CAP_MINIMIZE, AUIK_ICON_CAP_MAXIMIZE, AUIK_ICON_CAP_RESTORE,
                                    AUIK_ICON_CAP_CLOSE};
        if (!font.load(font_info->path)) return false;
        if (!font.load_glyphs(size, codepoints)) return false;

        for (u32 i = 0; i < codepoints.size(); ++i)
        {
            auto *glyph = font.find_glyph(size, codepoints[i]);
            if (!glyph) return false;
            const amal::vec2 image_size{static_cast<f32>(glyph->size.x), static_cast<f32>(glyph->size.y)};
            auto *image = get_cached_image(ids[i]);
            if (image)
            {
                image->set_texture_id(glyph->texture_id);
                image->set_uv_offset(glyph->uv_rect.offset);
                image->set_uv_size(glyph->uv_rect.size);
                image->set_size(image_size);
                image->set_layout_size(image_size);
            }
            else image = make_image(ids[i], glyph->texture_id, image_size, glyph->uv_rect);
            if (!image) return false;
            image->set_coverage_mode(true);
            cache_image(ids[i], image);
        }
        return true;
    }

    static bool bind_win32_titlebar_state(Win32TitlebarState *state)
    {
        if (!state || state->prev_proc) return true;
        HWND hwnd = state->hwnd;
        if (!hwnd) return false;
        state->prev_proc = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&titlebar_window_proc)));
        if (!state->prev_proc) return false;

        SetPropW(hwnd, AUIK_TITLEBAR_STATE, state);
        return true;
    }
#endif

    bool Titlebar::reload_caption_button_icons(f32 dpi, const FontRegistry &fonts)
    {
#ifdef _WIN32
        if (!_state || dpi <= 0.0f) return false;
        if (!add_window_caption_button_icons(fonts, dpi)) return false;
        detail::mark_layout_dirty();
        mark_host_refresh_request();
        return true;
#else
        (void)dpi;
        (void)fonts;
        return true;
#endif
    }

    bool adjust_window_by_titlebar_settings(Titlebar *titlebar, TitlebarCreateFlags flags, const FontRegistry &fonts)
    {
#ifdef _WIN32
        auto *wnd_ctx = detail::get_context().window_ctx;
        assert(wnd_ctx && "window context is null");

        HWND hwnd = (HWND)wnd_ctx->get_window_handle(wnd_ctx);
        assert(hwnd && "window handle is null");
        const UINT dpi_value = GetDpiForWindow(hwnd);
        const f32 dpi = static_cast<f32>(dpi_value) / 96.0f;
        if (!add_window_caption_button_icons(fonts, dpi)) return false;

        auto *state = acul::alloc<Win32TitlebarState>();
        state->flags = flags;
        state->hwnd = hwnd;
        state->destroy = &destroy_win32_titlebar_state;
        state->caption_buttons[AUIK_WINDOW_CAPTION_BTN_MIN] = flags & TitlebarCreateFlagBits::minimize_box;
        state->caption_buttons[AUIK_WINDOW_CAPTION_BTN_MAX] = flags & TitlebarCreateFlagBits::maximize_box;
        state->caption_buttons[AUIK_WINDOW_CAPTION_BTN_CLOSE] = true;
        update_win32_titlebar_metrics(*state, dpi_value);

        if (!bind_win32_titlebar_state(state))
        {
            acul::release(state);
            return false;
        }
        titlebar->set_titlebar_state(state);
#endif
        return true;
    }

    Titlebar::~Titlebar()
    {
        if (_state) destroy_titlebar_state(_state);
        destroy_titlebar_icon(_icon, _children, _icon_texture);
        for (auto *&button : _caption_buttons)
        {
            if (!button) continue;
            acul::release(button);
            button = nullptr;
        }
        for (auto *child : _children)
        {
            if (!child) continue;
            erase_cached_image(child->id());
            acul::release(child);
        }
        _children.clear();
    }

    struct TitlebarStreamAccess
    {
        static const acul::vector<Widget *> &children(const Titlebar &titlebar) { return titlebar._children; }
    };

    namespace
    {
        void write_titlebar(acul::bin_stream &stream, umbf::Block *block)
        {
            auto *titlebar = static_cast<Titlebar *>(block);
            detail::write_widget_common_data(stream, *titlebar);
            stream.write(titlebar->show_icon()).write(titlebar->leading_count());

            acul::vector<umbf::Block *> children;
            const auto &source_children = TitlebarStreamAccess::children(*titlebar);
            children.reserve(source_children.size());
            for (auto *child : source_children)
                if (child && (child->widget_flags & WidgetFlagBits::configurable)) children.push_back(child);
            stream.write(children);
        }

        umbf::Block *read_titlebar(acul::bin_stream &stream)
        {
            const auto common = detail::read_widget_common_data(stream);
            bool show_icon = false;
            u32 leading_count = 0u;
            stream.read(show_icon).read(leading_count);

            acul::vector<umbf::Block *> children;
            stream.read(children);

            auto *titlebar = acul::alloc<Titlebar>(common.id, WidgetFlags(common.widget_flags));
            detail::apply_widget_common_data(titlebar, common);
            titlebar->set_show_icon(show_icon);
            titlebar->set_leading_count(leading_count);
            for (auto *child : children)
                if (child) titlebar->add_child(static_cast<Widget *>(child));
            return titlebar;
        }
    } // namespace

    namespace streams
    {
        AUIK_EXPORT const umbf::streams::Stream titlebar{read_titlebar, write_titlebar};
    }
} // namespace auik
