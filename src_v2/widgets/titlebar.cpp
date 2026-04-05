#include <auik/v2/auik.hpp>
#include <auik/v2/detail/depth.hpp>
#include <auik/v2/pipelines.hpp>
#include <auik/v2/widgets/image.hpp>
#include <auik/v2/widgets/titlebar.hpp>

namespace auik::v2
{
    namespace
    {
        constexpr u32 AUIK_TITLEBAR_ICON_ID = AUIK_TAG_TITLEBAR ^ 0x51C04F31u;

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
            if (icon_texture.handle)
                detail::destroy_image_texture(detail::get_context().gpu_ctx, &icon_texture);
        }

        static void update_titlebar_child_depths(const amal::vec2 &parent_depth_range, const acul::vector<Widget *> &children)
        {
            const amal::vec2 background_range = detail::depth_zone_range(parent_depth_range, detail::DepthZone::background);
            amal::vec2 child_range{};
            assign_next_depth(background_range, child_range);
            for (auto *child : children)
            {
                if (!child) continue;
                child->update_depth(child_range);
                assign_next_depth(child_range, child_range);
            }
        }
    }

    Titlebar::Titlebar(u32 id, WidgetFlags widget_flags)
        : Widget(id, widget_flags, EventFlagBits::none, nullptr, {}, AUIK_TAG_TITLEBAR)
    {
    }

    Titlebar::~Titlebar()
    {
        destroy_titlebar_icon(_icon, _children, _icon_texture);
        for (auto *child : _children)
        {
            if (!child) continue;
            erase_cached_image(child->id());
            acul::release(child);
        }
        _children.clear();
    }

    void Titlebar::set_metrics(const TitlebarMetrics &metrics)
    {
        if (_metrics.left_inset == metrics.left_inset && _metrics.right_inset == metrics.right_inset &&
            _metrics.height == metrics.height)
            return;
        _metrics = metrics;
        detail::get_context().dirty_flags |= DirtyFlagBits::layout;
        detail::mark_host_refresh_request();
    }

    void Titlebar::set_show_icon(bool value)
    {
        if (_show_icon == value) return;
        _show_icon = value;
        detail::get_context().dirty_flags |= DirtyFlagBits::layout;
        detail::mark_host_refresh_request();
    }

    void Titlebar::add_child(Widget *child)
    {
        assert(child && "titlebar child is null");
        child->set_parent(this);
        child->set_focus_parent(this);
        _children.push_back(child);
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

        if (_icon)
            return;

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

    StyleUpdateFlags Titlebar::update_style()
    {
        StyleUpdateFlags out = resolve_style_selector(_style, id(), 0, style_state());
        out |= resolve_style_selector(_icon_style, id(), 0, style_state());
        ensure_icon_widget();
        for (auto *child : _children)
        {
            if (!child) continue;
            out |= child->update_style();
        }
        return out;
    }

    void Titlebar::update_layout_min_size()
    {
        const f32 resolved_height = _metrics.height > 0.0f ? _metrics.height : amal::max(size().y, 32.0f);
        set_required_size({size().x, resolved_height});
    }

    void Titlebar::update_layout(bool min_size_known)
    {
        ensure_icon_widget();
        if (!min_size_known) update_layout_min_size();
        const auto display = get_display_size();
        const f32 resolved_height = _metrics.height > 0.0f ? _metrics.height : amal::max(size().y, 32.0f);

        set_position({0.0f, 0.0f});
        set_size({display.x, resolved_height});
        set_required_size(size());
        Widget::update_layout(true);

        ensure_own_clip_rect({position().x, position().y, size().x, size().y});

        reserve_main_viewport_top(resolved_height);
        _content_start_x = _metrics.left_inset;
        const amal::vec4 icon_margin = get_theme()->get_style(_icon_style.id).margin();

        amal::vec2 cursor = {position().x + _content_start_x, position().y};
        for (auto *child : _children)
        {
            if (!child) continue;
            if (child == _icon)
                cursor.x = position().x + _metrics.left_inset + icon_margin.x;

            child->update_layout_min_size();
            const amal::vec2 prev_cursor = detail::get_context().screen_cursor;
            detail::get_context().screen_cursor = cursor;
            child->update_layout(true);
            detail::get_context().screen_cursor = prev_cursor;

            if (child == _icon)
            {
                const amal::vec2 icon_pos = child->position();
                child->set_position({icon_pos.x, position().y + (size().y - child->size().y) * 0.5f});
                cursor.x = child->position().x + child->size().x + icon_margin.z;
            }
            else cursor.x = child->position().x + child->size().x;
        }
        _content_start_x = amal::max(cursor.x - position().x, _metrics.left_inset);
        update_drag_regions();
    }

    void Titlebar::update_depth(const amal::vec2 &depth_range)
    {
        Widget::update_depth(depth_range);
        update_titlebar_child_depths(this->depth_range(), _children);
    }

    void Titlebar::rebuild_clip_rects()
    {
        _rect.clip_id = 0xFFFFu;
        ensure_own_clip_rect({position().x, position().y, size().x, size().y});
        _bg.hit_id = AUIK_INVALID_DRAW_DATA_ID;
        _icon_bg.hit_id = AUIK_INVALID_DRAW_DATA_ID;
        for (auto *child : _children)
        {
            if (!child) continue;
            child->set_clip_id(clip_id());
            child->rebuild_clip_rects();
        }
    }

    void Titlebar::draw(DrawCtx &ctx)
    {
        auto *theme = get_theme();
        auto *quads_stream = get_primary_quad_stream();
        const amal::vec2 background_range = detail::depth_zone_range(depth_range(), detail::DepthZone::background);

        QuadsInstanceData bg_data{};
        bg_data.rect = bounds();
        bg_data.z_order = (background_range.x + background_range.y) * 0.5f;
        fill_quads_instance_by_style(theme->get_style(_style.id), clip_id(), bg_data);
        ctx.emit(quads_stream, _bg, &bg_data, get_rect(), false);

        const Style &icon_style = theme->get_style(_icon_style.id);
        const amal::vec4 icon_margin = icon_style.margin();
        const bool draw_icon_background =
            _icon && (icon_style.mask() & detail::StylePropertiesBits::background_color);
        if (draw_icon_background)
        {
            QuadsInstanceData icon_bg_data{};
            const f32 x0 = position().x + _metrics.left_inset;
            const f32 x1 = _icon->position().x + _icon->size().x + icon_margin.z;
            icon_bg_data.rect = {{x0, position().y}, {amal::max(x1 - x0, 0.0f), size().y}};
            icon_bg_data.z_order = next_depth(background_range);
            fill_quads_instance_by_style(icon_style, clip_id(), icon_bg_data);
            ctx.emit(quads_stream, _icon_bg, &icon_bg_data, get_rect(), false);
        }

        for (auto *child : _children)
        {
            if (!child) continue;
            DrawCtx child_ctx = ctx;
            child_ctx.emit_hit_rect = child->is_hittable();
            child->draw(child_ctx);
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
    }

    void Titlebar::update_drag_regions()
    {
        acul::vector<amal::irect> next_regions;
        const i32 drag_start = amal::max(static_cast<i32>(_content_start_x), 0);
        const i32 drag_width = amal::max(static_cast<i32>(size().x - _metrics.right_inset) - drag_start, 0);
        const i32 drag_height = amal::max(static_cast<i32>(size().y), 0);
        if (drag_width > 0 && drag_height > 0) next_regions.push_back({drag_start, 0, drag_width, drag_height});

        bool changed = next_regions.size() != _drag_regions.size();
        if (!changed)
        {
            for (size_t i = 0; i < next_regions.size(); ++i)
            {
                if (next_regions[i].offset != _drag_regions[i].offset || next_regions[i].size != _drag_regions[i].size)
                {
                    changed = true;
                    break;
                }
            }
        }

        if (!changed) return;
        _drag_regions = std::move(next_regions);
        if (on_drag_regions_changed) on_drag_regions_changed(_drag_regions);
    }
} // namespace auik::v2
