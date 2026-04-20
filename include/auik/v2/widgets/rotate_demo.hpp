#pragma once

#include "../pipelines.hpp"
#include "../post_effects.hpp"
#include "widget.hpp"

#define AUIK_TAG_ROTATE_DEMO 0xE18B4A12u

namespace auik::v2
{
    class APPLIB_API RotateDemo final : public Widget
    {
    public:
        RotateDemo(u32 id, amal::rect bounds = {{0.0f, 0.0f}, {72.0f, 72.0f}}, Widget *parent = nullptr);

        StyleUpdateFlags update_style() override;
        void update_layout_min_size() override;
        void update_layout(bool min_size_known) override;
        void update_depth(const amal::vec2 &depth_range) override;
        void translate(const amal::vec2 &delta) override;
        void rebuild_clip_rects() override;
        void draw(DrawCtx &ctx) override;
        void on_attach() override;
        void on_detach() override;

    private:
        void rebuild_geometry();
        void schedule_tick();
        void tick_animation();

        DrawDataID _draw_id{};
        VertexStreamVertex _vertices[4]{};
        VertexStreamIndex _indices[6]{0u, 1u, 2u, 0u, 2u, 3u};
        VertexStreamBatchData _batch{};
        RotatePostData _rotate_post{};
        f32 _angle = 0.0f;
        f64 _last_tick_time = -1.0;
    };

    inline RotateDemo *make_rotate_demo(u32 id, amal::rect bounds = {{0.0f, 0.0f}, {72.0f, 72.0f}},
                                        Widget *parent = nullptr)
    {
        return acul::alloc<RotateDemo>(id, bounds, parent);
    }
} // namespace auik::v2
