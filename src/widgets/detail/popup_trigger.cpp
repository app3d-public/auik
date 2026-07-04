#include <amal/trigonometric.hpp>
#include <auik/animation.hpp>
#include <auik/detail/depth.hpp>
#include <auik/detail/pixel_snap.hpp>
#include <auik/pipelines.hpp>
#include <auik/widgets/detail/popup_trigger.hpp>
#include <auik/widgets/image.hpp>
#include <auik/widgets/image_button.hpp>

#define AUIK_POPUP_TRIGGER_ICON_ROTATE_DURATION 0.16f

namespace auik::detail
{
    static inline bool draw_id_valid(const DrawDataID &id) { return id.render_id != AUIK_INVALID_DRAW_DATA_ID; }

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

    class WRotateTransient;
    void start_popup_icon_animation(AnimationState *state, Widget *owner, void *user_data, f64 now);
    bool tick_popup_icon_animation(AnimationState *state, Widget *owner, void *user_data, f64 now);
    void finish_popup_icon_animation(AnimationState *state, Widget *owner, void *user_data);
    void destroy_popup_icon_animation(AnimationState *state, Widget *owner, void *user_data);

    class WRotateTransient final : public Widget
    {
    public:
        explicit WRotateTransient() : Widget(0u, WidgetFlagBits::visible, EventFlagBits::none, {}, 0u) {}

        ~WRotateTransient() override { _animation.clear(_update_target, this); }

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
            const u32 rotate_post_id = _animation.post_data_id;
            if (auto *rotate_data = rotate_data_mut(rotate_post_id))
            {
                rotate_data->center = _icon_rect.offset + _icon_rect.size * 0.5f;
                if (!rotate_data->animating) rotate_data->angle = 0.0f;
            }
        }

        bool is_animating() const { return _animation.active(); }

        bool start(bool opening, f32 open_angle)
        {
            if (!_update_target || !detail::g_context) return false;
            const u32 rotate_post_id = _animation.post_data_id;
            auto *rotate_data = rotate_data_mut(rotate_post_id);
            const f32 current_angle = rotate_data && rotate_data->animating ? rotate_data->angle
                                      : opening                             ? 0.0f
                                                                            : open_angle;
            const f32 target_angle = opening ? open_angle : 0.0f;
            _animation.duration = AUIK_POPUP_TRIGGER_ICON_ROTATE_DURATION;
            _animation.center = _icon_rect.offset + _icon_rect.size * 0.5f;
            _animation.from = current_angle;
            _animation.to = target_angle;
            _animation.current = current_angle;
            _animation.at_start = &start_popup_icon_animation;
            _animation.tick = &tick_popup_icon_animation;
            _animation.at_finish = &finish_popup_icon_animation;
            _animation.destroy = &destroy_popup_icon_animation;
            _animation.scale_finish = nullptr;
            _animation.rotate_finish = nullptr;
            return start_animation(_animation, _update_target, this) != nullptr;
        }

        AnimationState &animation() { return _animation; }

        void reset_clip_rect_records() override { _rect.clip_id = 0xFFFFu; }

        void rebuild_clip_rects() override {}

        void reset_draw_records() override { _draw = {}; }

        StyleUpdateFlags update_style() override { return StyleUpdateFlagBits::none; }

        void draw(DrawCtx &ctx) override
        {
            if (!(ctx.reason & DrawReasonBits::transient)) return;
            auto *vertex_stream = get_primary_textured_vertex_stream();
            auto *rotate_effect = get_rotate_post_effect();
            const u32 rotate_post_id = _animation.post_data_id;
            if (!vertex_stream || !rotate_effect || rotate_post_id == AUIK_INVALID_POST_EFFECT_DATA_ID) return;
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

            RotatePostData rotate_post{rotate_post_id};
            PostFxChain rotate_chain{rotate_effect, &rotate_post, rotate_post_id, ctx.post_fx_chain};
            DrawCtx rotated_ctx = ctx;
            rotated_ctx.post_fx_chain = &rotate_chain;
            emit_context_draw(rotated_ctx, vertex_stream, _draw, &batch, _rect, false);
        }

    private:
        friend void start_popup_icon_animation(AnimationState *state, Widget *owner, void *user_data, f64 now);
        friend bool tick_popup_icon_animation(AnimationState *state, Widget *owner, void *user_data, f64 now);
        friend void finish_popup_icon_animation(AnimationState *state, Widget *owner, void *user_data);
        friend void destroy_popup_icon_animation(AnimationState *state, Widget *owner, void *user_data);

        RotatePostRuntimeData *rotate_data_mut(u32 rotate_post_id)
        { return get_rotate_post_effect_data(get_rotate_post_effect(), rotate_post_id); }

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
        AnimationState _animation;
    };

    void start_popup_icon_animation(AnimationState *state, Widget *owner, void *user_data, f64)
    {
        auto *self = static_cast<WRotateTransient *>(user_data);
        if (!state || !owner || !self) return;
        if (state->post_data_id != AUIK_INVALID_POST_EFFECT_DATA_ID)
            release_rotate_post_effect_data(get_rotate_post_effect(), state->post_data_id);
        state->post_data_id = create_rotate_post_effect_data(get_rotate_post_effect(), owner);
        auto *rotate_data = self->rotate_data_mut(state->post_data_id);
        if (!rotate_data) return;
        rotate_data->animation_start = current_animation_time();
        rotate_data->animation_from = state->from;
        rotate_data->animation_to = state->to;
        rotate_data->angle = state->from;
        rotate_data->center = state->center;
        rotate_data->animating = true;
        push_widget_to_transient_cache(self);
    }

    bool tick_popup_icon_animation(AnimationState *state, Widget *, void *user_data, f64 now)
    {
        auto *self = static_cast<WRotateTransient *>(user_data);
        auto *rotate_data = state && self ? self->rotate_data_mut(state->post_data_id) : nullptr;
        if (!state || !rotate_data || !rotate_data->animating) return false;
        const f32 eased = get_default_animation_progress(rotate_data->animation_start, state->duration, now);
        rotate_data->angle =
            rotate_data->animation_from + (rotate_data->animation_to - rotate_data->animation_from) * eased;
        return eased < 1.0f;
    }

    void finish_popup_icon_animation(AnimationState *state, Widget *, void *user_data)
    {
        auto *self = static_cast<WRotateTransient *>(user_data);
        auto *rotate_data = state && self ? self->rotate_data_mut(state->post_data_id) : nullptr;
        if (rotate_data)
        {
            rotate_data->angle = rotate_data->animation_to;
            rotate_data->animating = false;
        }
        if (state && state->post_data_id != AUIK_INVALID_POST_EFFECT_DATA_ID)
        {
            release_rotate_post_effect_data(get_rotate_post_effect(), state->post_data_id);
            state->post_data_id = AUIK_INVALID_POST_EFFECT_DATA_ID;
        }
        if (!self) return;
        self->clear_draw();
        erase_widget_from_transient_cache(self);
    }

    void destroy_popup_icon_animation(AnimationState *state, Widget *, void *user_data)
    {
        auto *self = static_cast<WRotateTransient *>(user_data);
        if (state && state->post_data_id != AUIK_INVALID_POST_EFFECT_DATA_ID)
        {
            release_rotate_post_effect_data(get_rotate_post_effect(), state->post_data_id);
            state->post_data_id = AUIK_INVALID_POST_EFFECT_DATA_ID;
        }
        if (self)
        {
            self->clear_draw();
            erase_widget_from_transient_cache(self);
        }
    }

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

    void PopupTrigger::update_icon_rect_from_slot()
    {
        amal::vec2 icon_size = _icon_size;
        if (icon_size.x <= 0.0f || icon_size.y <= 0.0f) icon_size = _icon_slot.size;
        _icon_rect = {{_icon_slot.offset.x + amal::max((_icon_slot.size.x - icon_size.x) * 0.5f, 0.0f),
                       _icon_slot.offset.y + amal::max((_icon_slot.size.y - icon_size.y) * 0.5f, 0.0f)},
                      icon_size};
        _icon_rect = snap_rect_offset_to_pixel_grid(_icon_rect);
    }

    bool PopupTrigger::ensure_icon_resources()
    {
        const bool animating = _rotate_transient && _rotate_transient->is_animating();
        const u32 icon_id = animating ? _closed_icon : (_open ? _open_icon : _closed_icon);
        const TextureID old_texture = _icon_texture;
        const amal::vec2 old_size = _icon_size;
        const amal::rect old_uv_rect = _icon_uv_rect;
        if (auto *cached = get_cached_image(icon_id))
        {
            _icon_texture = cached->texture_id();
            _icon_size = cached->size();
            _icon_uv_rect = {cached->uv_offset(), cached->uv_size()};
            const bool changed = old_texture.handle != _icon_texture.handle || old_size.x != _icon_size.x ||
                                 old_size.y != _icon_size.y || old_uv_rect.offset.x != _icon_uv_rect.offset.x ||
                                 old_uv_rect.offset.y != _icon_uv_rect.offset.y ||
                                 old_uv_rect.size.x != _icon_uv_rect.size.x ||
                                 old_uv_rect.size.y != _icon_uv_rect.size.y;
            if (changed) update_icon_rect_from_slot();
            return changed;
        }

        _icon_texture = {};
        _icon_size = {0.0f, 0.0f};
        _icon_uv_rect = {{0.0f, 0.0f}, {1.0f, 1.0f}};
        const bool changed = old_texture.handle != 0 || old_size.x != 0.0f || old_size.y != 0.0f ||
                             old_uv_rect.offset.x != 0.0f || old_uv_rect.offset.y != 0.0f ||
                             old_uv_rect.size.x != 1.0f || old_uv_rect.size.y != 1.0f;
        if (changed) update_icon_rect_from_slot();
        return changed;
    }

    void PopupTrigger::update_layout_min_size(amal::vec2 style_size, bool fixed)
    {
        ensure_icon_resources();
        const auto &style = get_theme()->get_style(_style.id);
        const amal::vec4 margin = style.margin();
        const amal::vec4 padding = style.padding();
        amal::vec2 icon_size = amal::max(resolve_icon_size(_closed_icon), resolve_icon_size(_open_icon));
        if (icon_size.x <= 0.0f || icon_size.y <= 0.0f) icon_size = {style.text_size(), style.text_size()};
        const amal::vec2 content_required = {icon_size.x + padding.x + padding.z, icon_size.y + padding.y + padding.w};

        amal::vec2 min_size = style_size;
        if (fixed && min_size.x <= 0.0f && min_size.y <= 0.0f)
        {
            const f32 side = amal::max(content_required.x, content_required.y);
            min_size = {side + margin.x + margin.z, side + margin.y + margin.w};
        }
        else if (!fixed) min_size.x = 0.0f;

        if (min_size.x <= 0.0f) min_size.x = content_required.x + margin.x + margin.z;
        else min_size.x = amal::max(min_size.x, content_required.x + margin.x + margin.z);
        if (min_size.y <= 0.0f) min_size.y = content_required.y + margin.y + margin.w;
        else min_size.y = amal::max(min_size.y, content_required.y + margin.y + margin.w);

        _required_size = min_size;
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

        _icon_slot = {{_bounds.offset.x + _bounds.size.x - padding.z - slot_size.x,
                       _bounds.offset.y + padding.y + amal::max((content_h - slot_size.y) * 0.5f, 0.0f)},
                      slot_size};
        update_icon_rect_from_slot();
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
    }

    bool PopupTrigger::has_draw_record() const
    {
        if (!draw_id_valid(_bg_draw)) return false;
        if (_icon_texture.handle != 0 && !draw_id_valid(_icon_draw)) return false;
        return true;
    }
} // namespace auik::detail
