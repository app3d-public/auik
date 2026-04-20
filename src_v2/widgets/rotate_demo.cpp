#include <auik/v2/auik.hpp>
#include <auik/v2/pipelines.hpp>
#include <auik/v2/post_effects.hpp>
#include <auik/v2/widgets/rotate_demo.hpp>

namespace auik::v2
{
    namespace
    {
        constexpr f32 g_rotate_demo_speed = 1.75f;
        constexpr f32 g_two_pi = 6.28318530718f;

        static inline amal::vec4 intersect_rect(const amal::vec4 &a, const amal::vec4 &b)
        {
            const amal::vec2 a_min = {a.x, a.y};
            const amal::vec2 a_max = {a.x + a.z, a.y + a.w};
            const amal::vec2 b_min = {b.x, b.y};
            const amal::vec2 b_max = {b.x + b.z, b.y + b.w};

            const amal::vec2 out_min = {amal::max(a_min.x, b_min.x), amal::max(a_min.y, b_min.y)};
            const amal::vec2 out_max = {amal::min(a_max.x, b_max.x), amal::min(a_max.y, b_max.y)};
            const amal::vec2 out_size = {amal::max(out_max.x - out_min.x, 0.0f), amal::max(out_max.y - out_min.y, 0.0f)};
            return {out_min, out_size};
        }
    } // namespace

    RotateDemo::RotateDemo(u32 id, amal::rect bounds, Widget *parent)
        : Widget(id, get_default_widget_flags() | WidgetFlagBits::fixed, EventFlagBits::none, parent, bounds,
                 AUIK_TAG_ROTATE_DEMO)
    {
        _batch.vertices = _vertices;
        _batch.indices = _indices;
        _batch.vertex_count = 4u;
        _batch.index_count = 6u;
    }

    StyleUpdateFlags RotateDemo::update_style() { return StyleUpdateFlagBits::none; }

    void RotateDemo::update_layout_min_size()
    {
        amal::vec2 min_size = size();
        if (min_size.x <= 0.0f) min_size.x = 72.0f;
        if (min_size.y <= 0.0f) min_size.y = 72.0f;
        set_required_size(min_size);
    }

    void RotateDemo::update_layout(bool min_size_known)
    {
        if (!min_size_known) update_layout_min_size();

        if (parent())
        {
            const amal::vec2 cursor = detail::get_context().screen_cursor;
            set_position(cursor);
            set_size(required_size());
            detail::get_context().screen_cursor = {cursor.x, cursor.y + size().y};
            set_clip_id(parent()->content_clip_id());
        }
        else
        {
            set_size(required_size());
            const amal::vec4 viewport = get_main_viewport();
            ensure_own_clip_rect(intersect_rect({position().x, position().y, size().x, size().y}, viewport));
        }

        Widget::update_layout(true);
        rebuild_geometry();
    }

    void RotateDemo::update_depth(const amal::vec2 &depth_range)
    {
        Widget::update_depth(depth_range);
        rebuild_geometry();
    }

    void RotateDemo::translate(const amal::vec2 &delta)
    {
        if (delta.x == 0.0f && delta.y == 0.0f) return;
        Widget::translate(delta);
        rebuild_geometry();
    }

    void RotateDemo::rebuild_clip_rects()
    {
        if (parent()) set_clip_id(parent()->content_clip_id());
        else
        {
            const amal::vec4 viewport = get_main_viewport();
            ensure_own_clip_rect(intersect_rect({position().x, position().y, size().x, size().y}, viewport));
        }
        rebuild_geometry();
    }

    void RotateDemo::draw(DrawCtx &ctx)
    {
        auto *vertex_stream = get_primary_vertex_stream();
        if (!vertex_stream) return;
        auto *rotate_effect = get_rotate_post_effect();
        if (!rotate_effect) return;
        if (_rotate_post.id == AUIK_INVALID_POST_EFFECT_DATA_ID) return;

        _rotate_post.center = position() + size() * 0.5f;
        _rotate_post.angle = _angle;
        update_post_effect_instance(rotate_effect, _rotate_post.id, this, &_rotate_post);

        DrawCtx rotated_ctx = ctx;
        rotated_ctx.post_effect = rotate_effect;
        rotated_ctx.post_data = &_rotate_post;
        rotated_ctx.emit(vertex_stream, _draw_id, &_batch, get_rect(), false);
    }

    void RotateDemo::on_attach()
    {
        Widget::on_attach();
        if (auto *rotate_effect = get_rotate_post_effect())
            _rotate_post.id = push_post_effect_instance(rotate_effect, this, &_rotate_post);
        schedule_tick();
    }

    void RotateDemo::on_detach()
    {
        if (auto *rotate_effect = get_rotate_post_effect();
            rotate_effect && _rotate_post.id != AUIK_INVALID_POST_EFFECT_DATA_ID)
        {
            release_post_effect_instance(rotate_effect, _rotate_post.id);
            _rotate_post.id = AUIK_INVALID_POST_EFFECT_DATA_ID;
        }
        cancel_delayed_tasks(id());
        Widget::on_detach();
    }

    void RotateDemo::rebuild_geometry()
    {
        const amal::vec2 min = position();
        const amal::vec2 max = position() + size();
        const u32 clip = static_cast<u32>(clip_id());
        const u32 color = detail::pack_rgba8(77, 198, 255, 255);
        const f32 z = get_z_order();

        _vertices[0] = {{min.x, min.y}, z, 0.0f, color, clip};
        _vertices[1] = {{max.x, min.y}, z, 0.0f, color, clip};
        _vertices[2] = {{max.x, max.y}, z, 0.0f, color, clip};
        _vertices[3] = {{min.x, max.y}, z, 0.0f, color, clip};
    }

    void RotateDemo::schedule_tick()
    {
        if (!detail::g_context) return;
        detail::update_window_time(detail::get_context().window_ctx);
        const f64 delay = get_max_animation_delay() > 0.0 ? get_max_animation_delay() : (1.0 / 60.0);
        schedule_delayed_host_task(id(), detail::get_context().window_ctx->time + delay, [this]() { tick_animation(); });
    }

    void RotateDemo::tick_animation()
    {
        if (!detail::g_context) return;
        detail::update_window_time(detail::get_context().window_ctx);
        const f64 now = detail::get_context().window_ctx->time;
        if (_last_tick_time < 0.0) _last_tick_time = now;
        const f32 dt = static_cast<f32>(amal::max(now - _last_tick_time, 0.0));
        _last_tick_time = now;
        _angle += dt * g_rotate_demo_speed;
        while (_angle > g_two_pi) _angle -= g_two_pi;
        redraw_external(_draw_id.render_id != AUIK_INVALID_DRAW_DATA_ID, DrawReasonBits::external);
        detail::mark_host_refresh_request();
        schedule_tick();
    }
} // namespace auik::v2
