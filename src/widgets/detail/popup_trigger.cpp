#include <amal/trigonometric.hpp>
#include <auik/detail/depth.hpp>
#include <auik/pipelines.hpp>
#include <auik/widgets/detail/popup_trigger.hpp>
#include <auik/widgets/image.hpp>
#include <auik/widgets/image_button.hpp>

#define AUIK_POPUP_TRIGGER_ICON_ROTATE_DURATION 0.16f

namespace auik::detail
{
    static inline bool draw_id_valid(const DrawDataID &id) { return id.render_id != AUIK_INVALID_DRAW_DATA_ID; }

    static inline const RotatePostRuntimeData *get_rotate_data_const(u32 id)
    {
        return get_rotate_post_effect_data(get_rotate_post_effect(), id);
    }

    static inline amal::vec2 resolve_icon_size(u32 icon_id)
    {
        if (auto *cached = get_cached_image(icon_id)) return cached->size();
        return {0.0f, 0.0f};
    }

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

    class WRotateTransient final : public Widget
    {
    public:
        explicit WRotateTransient()
            : Widget(0u, WidgetFlagBits::visible, EventFlagBits::none, nullptr, {}, 0u)
        {
        }

        ~WRotateTransient() override
        {
            if (auto *rotate_effect = get_rotate_post_effect();
                rotate_effect && _rotate_post_id != AUIK_INVALID_POST_EFFECT_DATA_ID)
                release_rotate_post_effect_data(rotate_effect, _rotate_post_id);
        }

        void configure(Widget *update_target, TextureID texture, const amal::rect &uv_rect, const amal::rect &icon_rect,
                       f32 depth, u16 clip_id)
        {
            _update_target = update_target;
            _texture = texture;
            _uv_rect = uv_rect;
            _icon_rect = icon_rect;
            _rect.bounds = icon_rect;
            _rect.depth = depth;
            _rect.hit_depth = depth;
            _rect.clip_id = clip_id;
            if (auto *rotate_data = rotate_data_mut())
            {
                rotate_data->center = _icon_rect.offset + _icon_rect.size * 0.5f;
                if (!rotate_data->animating) rotate_data->angle = 0.0f;
            }
        }

        bool is_animating() const
        {
            const auto *rotate_data = get_rotate_data_const(_rotate_post_id);
            return rotate_data && rotate_data->animating;
        }

        bool start(bool opening, f32 open_angle)
        {
            if (!_update_target || !detail::g_context) return false;
            auto *rotate_effect = get_rotate_post_effect();
            if (!rotate_effect) return false;
            if (_rotate_post_id == AUIK_INVALID_POST_EFFECT_DATA_ID)
            {
                _rotate_post_id = create_rotate_post_effect_data(rotate_effect, _update_target);
                if (_rotate_post_id == AUIK_INVALID_POST_EFFECT_DATA_ID) return false;
            }

            detail::update_window_time(detail::get_context().window_ctx);
            auto *rotate_data = rotate_data_mut();
            if (!rotate_data) return false;
            rotate_data->animation_start = detail::get_context().window_ctx->time;
            rotate_data->animation_from = opening ? 0.0f : open_angle;
            rotate_data->animation_to = opening ? open_angle : 0.0f;
            rotate_data->angle = rotate_data->animation_from;
            rotate_data->center = _icon_rect.offset + _icon_rect.size * 0.5f;
            rotate_data->animating = true;
            push_widget_to_transient_cache(this);
            return true;
        }

        bool tick(f64 now, f32 duration)
        {
            auto *rotate_data = rotate_data_mut();
            if (!rotate_data || !rotate_data->animating) return false;

            f64 raw_t = (now - rotate_data->animation_start) / duration;
            raw_t = amal::clamp(raw_t, 0.0, 1.0);
            const f32 t = static_cast<f32>(raw_t);
            const f32 eased = 1.0f - (1.0f - t) * (1.0f - t);
            rotate_data->angle =
                rotate_data->animation_from + (rotate_data->animation_to - rotate_data->animation_from) * eased;

            if (t < 1.0f) return true;
            rotate_data->angle = rotate_data->animation_to;
            rotate_data->animating = false;
            clear_draw();
            erase_widget_from_transient_cache(this);
            return false;
        }

        void reset_clip_rect_records() override { _rect.clip_id = 0xFFFFu; }

        void rebuild_clip_rects() override {}

        void reset_draw_records() override { _draw = {}; }

        StyleUpdateFlags update_style() override { return StyleUpdateFlagBits::none; }

        void draw(DrawCtx &ctx) override
        {
            if (!(ctx.reason & DrawReasonBits::transient)) return;
            auto *vertex_stream = get_primary_textured_vertex_stream();
            auto *rotate_effect = get_rotate_post_effect();
            if (!vertex_stream || !rotate_effect || _rotate_post_id == AUIK_INVALID_POST_EFFECT_DATA_ID) return;
            if (!is_animating() || _texture.handle == 0 || _rect.clip_id == 0xFFFFu) return;

            TextureID texture = _texture;
            if ((detail::get_context().dirty_flags & DirtyFlagBits::textures) ||
                texture.bind_slot == AUIK_INVALID_DRAW_DATA_ID)
                texture.bind_slot = get_texture_bind_slot(texture.handle);
            if (texture.bind_slot == AUIK_INVALID_DRAW_DATA_ID) return;
            _texture.bind_slot = texture.bind_slot;

            TexturedVertexStreamVertex vertices[4]{};
            const TexturedVertexStreamIndex indices[6]{0u, 1u, 2u, 0u, 2u, 3u};
            build_animated_icon_vertices(vertices, _icon_rect, _uv_rect, _rect.depth, _rect.clip_id, true);

            TexturedVertexStreamBatchData batch{};
            batch.vertices = vertices;
            batch.indices = indices;
            batch.vertex_count = 4u;
            batch.index_count = 6u;
            batch.texture_id = texture;
            batch.flags = AUIK_TEXTURE_INSTANCE_TEXT_BIT;

            RotatePostData rotate_post{_rotate_post_id};
            PostFxChain rotate_chain{rotate_effect, &rotate_post, _rotate_post_id, ctx.post_fx_chain};
            DrawCtx rotated_ctx = ctx;
            rotated_ctx.post_fx_chain = &rotate_chain;
            emit_context_draw(rotated_ctx, vertex_stream, _draw, &batch, _rect, false);
        }

    private:
        RotatePostRuntimeData *rotate_data_mut()
        {
            return get_rotate_post_effect_data(get_rotate_post_effect(), _rotate_post_id);
        }

        void clear_draw()
        {
            if (_draw.render_id == AUIK_INVALID_DRAW_DATA_ID) return;
            auto *stream = get_primary_textured_vertex_stream();
            if (!stream || !stream->invalidate_data_in_stream) return;
            stream->invalidate_data_in_stream(stream, _draw);
            _draw = {};
        }

        Widget *_update_target = nullptr;
        DrawDataID _draw{};
        TextureID _texture{};
        amal::rect _uv_rect{{0.0f, 0.0f}, {1.0f, 1.0f}};
        amal::rect _icon_rect{};
        u32 _rotate_post_id = AUIK_INVALID_POST_EFFECT_DATA_ID;
    };

    PopupTrigger::PopupTrigger(u32 style_tag, u32 hit_tag, u32 closed_icon, u32 open_icon, bool animated,
                               f32 open_angle)
        : _closed_icon(closed_icon),
          _open_icon(open_icon),
          _open_angle(open_angle),
          _animated(animated),
          _style({Theme::STYLE_ID_INVALID, style_tag}),
          _hit_rect(make_rect_data(0u, hit_tag))
    {
        if (_animated) _rotate_transient = acul::alloc<WRotateTransient>();
    }

    PopupTrigger::~PopupTrigger()
    {
        if (_rotate_transient) acul::release(_rotate_transient);
    }

    void PopupTrigger::set_style_tag(u32 style_tag)
    {
        if (_style.tag_id == style_tag) return;
        _style.tag_id = style_tag;
        _style.id = Theme::STYLE_ID_INVALID;
        reset_draw_records();
    }

    void PopupTrigger::set_icons(u32 closed_icon, u32 open_icon)
    {
        if (_closed_icon == closed_icon && _open_icon == open_icon) return;
        if (_rotate_transient) _rotate_transient->reset_draw_records();
        _closed_icon = closed_icon;
        _open_icon = open_icon;
        _icon_texture = {};
        _icon_size = {0.0f, 0.0f};
    }

    StyleUpdateFlags PopupTrigger::update_style(u32 self_id, u32 parent_id, StyleState state)
    {
        _style_state = state;
        return resolve_style_selector(_style, self_id, parent_id, _style_state);
    }

    void PopupTrigger::ensure_icon_resources()
    {
        const bool animating = _rotate_transient && _rotate_transient->is_animating();
        const u32 icon_id = animating ? _closed_icon : (_open ? _open_icon : _closed_icon);
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

    void PopupTrigger::update_layout_min_size(amal::vec2 requested_size, bool fixed)
    {
        ensure_icon_resources();
        const auto &style = get_theme()->get_style(_style.id);
        const amal::vec4 margin = style.margin();
        const amal::vec4 padding = style.padding();
        amal::vec2 icon_size = amal::max(resolve_icon_size(_closed_icon), resolve_icon_size(_open_icon));
        if (icon_size.x <= 0.0f || icon_size.y <= 0.0f) icon_size = {style.text_size(), style.text_size()};
        const amal::vec2 content_required = {icon_size.x + padding.x + padding.z, icon_size.y + padding.y + padding.w};

        amal::vec2 min_size = requested_size;
        if (fixed && min_size.x <= 0.0f && min_size.y <= 0.0f)
        {
            const f32 side = amal::max(content_required.x, content_required.y);
            min_size = {side, side};
        }
        else if (!fixed) min_size.x = 0.0f;

        if (min_size.x <= 0.0f) min_size.x = content_required.x;
        else min_size.x = amal::max(min_size.x, content_required.x);
        if (min_size.y <= 0.0f) min_size.y = content_required.y;
        else min_size.y = amal::max(min_size.y, content_required.y);

        _required_size = {min_size.x + margin.x + margin.z, min_size.y + margin.y + margin.w};
    }

    void PopupTrigger::update_layout(const amal::rect &bounds, u16 clip_id)
    {
        ensure_icon_resources();
        _outer_bounds = bounds;
        const auto &style = get_theme()->get_style(_style.id);
        const amal::vec4 margin = style.margin();
        const amal::vec4 padding = style.padding();
        _bounds = {{bounds.offset.x + margin.x, bounds.offset.y + margin.y},
                   {amal::max(bounds.size.x - margin.x - margin.z, 0.0f),
                    amal::max(bounds.size.y - margin.y - margin.w, 0.0f)}};
        const f32 content_h = amal::max(_bounds.size.y - padding.y - padding.w, 0.0f);
        amal::vec2 slot_size = amal::max(resolve_icon_size(_closed_icon), resolve_icon_size(_open_icon));
        if (slot_size.x <= 0.0f || slot_size.y <= 0.0f) slot_size = {style.text_size(), style.text_size()};
        amal::vec2 icon_size = _icon_size;
        if (icon_size.x <= 0.0f || icon_size.y <= 0.0f) icon_size = slot_size;

        _icon_slot = {{_bounds.offset.x + _bounds.size.x - padding.z - slot_size.x,
                       _bounds.offset.y + padding.y + amal::max((content_h - slot_size.y) * 0.5f, 0.0f)},
                      slot_size};
        _icon_rect = {{_icon_slot.offset.x + amal::max((_icon_slot.size.x - icon_size.x) * 0.5f, 0.0f),
                       _icon_slot.offset.y + amal::max((_icon_slot.size.y - icon_size.y) * 0.5f, 0.0f)},
                      icon_size};
        _hit_rect.bounds = _bounds;
        _hit_rect.clip_id = clip_id;
        _hit_rect.depth = next_depth(_content_depth_range);
        _hit_rect.hit_depth = _hit_rect.depth;
        if (_rotate_transient)
            _rotate_transient->configure(_update_target, _icon_texture, _icon_uv_rect, _icon_rect, _hit_rect.depth,
                                         clip_id);
    }

    void PopupTrigger::translate(const amal::vec2 &delta)
    {
        if (delta.x == 0.0f && delta.y == 0.0f) return;
        _outer_bounds.offset += delta;
        _bounds.offset += delta;
        _icon_slot.offset += delta;
        _icon_rect.offset += delta;
        _hit_rect.bounds.offset += delta;
    }

    void PopupTrigger::rebuild_clip_rects(u16 clip_id)
    {
        DrawDataID *hit_ids[] = {&_bg_draw, &_icon_draw};
        invalidate_hit_rect_batch(hit_ids, 2);
        _hit_rect.clip_id = clip_id;
    }

    void PopupTrigger::reset_draw_records()
    {
        _bg_draw = {};
        _icon_draw = {};
        if (_rotate_transient) _rotate_transient->reset_draw_records();
    }

    void PopupTrigger::update_depth(const amal::vec2 &depth_range)
    {
        DepthCursor cursor(depth_range, get_depth_requirement());
        _bg_depth_range = cursor.next(1u);
        _content_depth_range = cursor.next(1u);
        _hit_rect.depth = next_depth(_content_depth_range);
        _hit_rect.hit_depth = _hit_rect.depth;
    }

    void PopupTrigger::back_hit_depth()
    {
        _hit_rect.hit_depth = get_root_depth_zone_range(DepthZone::background).x;
        get_context().dirty_flags |= DirtyFlagBits::hit_rect_update;
    }

    void PopupTrigger::restore_hit_depth()
    {
        _hit_rect.hit_depth = _hit_rect.depth;
        get_context().dirty_flags |= DirtyFlagBits::hit_rect_update;
    }

    void PopupTrigger::draw(DrawCtx &ctx, bool is_hit_allowed)
    {
        const bool transient = ctx.reason & DrawReasonBits::transient;
        auto *theme = get_theme();

        if (!transient)
        {
            auto *quads_stream = get_primary_quads_stream();
            QuadsInstanceData bg_data{};
            bg_data.rect = _bounds;
            bg_data.z_order = next_depth(_bg_depth_range);
            const bool bg_visible =
                fill_quads_instance_by_style(theme->get_style(_style.id), _hit_rect.clip_id, bg_data);
            emit_quads_instance(ctx, quads_stream, _bg_draw, bg_data, _hit_rect, bg_visible, is_hit_allowed);

            ensure_icon_resources();
            auto *textured_quads_stream = get_primary_textured_quads_stream();
            TextureID icon_texture = _icon_texture;
            if (textured_quads_stream && icon_texture.handle != 0)
            {
                if ((detail::get_context().dirty_flags & DirtyFlagBits::textures) ||
                    icon_texture.bind_slot == AUIK_INVALID_DRAW_DATA_ID)
                    icon_texture.bind_slot = get_texture_bind_slot(icon_texture.handle);

                if (icon_texture.bind_slot != AUIK_INVALID_DRAW_DATA_ID)
                {
                    const bool icon_hidden_for_animation = _rotate_transient && _rotate_transient->is_animating();
                    TexturesInstanceData icon_data{};
                    icon_data.rect = _icon_rect;
                    icon_data.uv_rect = _icon_uv_rect;
                    icon_data.tint_color = icon_hidden_for_animation ? 0u : theme->get_style(_style.id).text_color();
                    icon_data.z_order = _hit_rect.depth;
                    icon_data.texture_id = static_cast<u16>(icon_texture.bind_slot);
                    icon_data.clip_id = _hit_rect.clip_id;
                    icon_data.flags = AUIK_TEXTURE_INSTANCE_TEXT_BIT;
                    emit_context_draw(ctx, textured_quads_stream, _icon_draw, &icon_data, _hit_rect, false);
                }
            }
        }
    }

    void PopupTrigger::start_icon_animation(bool opening)
    {
        if (!_animated || !_rotate_transient || !detail::g_context || !_update_target) return;
        _rotate_transient->configure(_update_target, _icon_texture, _icon_uv_rect, _icon_rect, _hit_rect.depth,
                                     _hit_rect.clip_id);
        if (!_rotate_transient->start(opening, _open_angle)) return;
        detail::mark_host_refresh_request();
        schedule_icon_tick();
    }

    void PopupTrigger::schedule_icon_tick()
    {
        if (!detail::g_context || !_update_target) return;
        detail::update_window_time(detail::get_context().window_ctx);
        const f64 delay = get_max_animation_delay() > 0.0 ? get_max_animation_delay() : (1.0 / 60.0);
        schedule_delayed_host_task(_update_target->id(), detail::get_context().window_ctx->time + delay,
                                   [this]() { tick_icon_animation(); });
    }

    void PopupTrigger::tick_icon_animation()
    {
        if (!detail::g_context || !_update_target) return;
        if (!_rotate_transient || !_rotate_transient->is_animating()) return;

        detail::update_window_time(detail::get_context().window_ctx);
        const f64 now = detail::get_context().window_ctx->time;
        const bool still_animating = _rotate_transient->tick(now, AUIK_POPUP_TRIGGER_ICON_ROTATE_DURATION);
        if (!still_animating)
        {
            ensure_icon_resources();
            update_layout(_outer_bounds, _hit_rect.clip_id);
        }
        else schedule_icon_tick();

        _update_target->update_draw_commands(DrawReasonBits::external);
        detail::get_context().dirty_flags |= DirtyFlagBits::redraw;
        detail::mark_host_refresh_request();
    }

    bool PopupTrigger::has_draw_record() const
    {
        if (!draw_id_valid(_bg_draw)) return false;
        if (_icon_texture.handle != 0 && !draw_id_valid(_icon_draw)) return false;
        return true;
    }
} // namespace auik::detail
