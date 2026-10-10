#include <auik/auik.hpp>
#include <auik/widgets/textbox.hpp>
#include <cassert>
#include <cstdio>

namespace
{
    acul::vector<amal::vec4> clips;
    struct Parent : auik::Widget
    {
        Parent() : Widget(9000, {}, {}, {{0.f, 0.f}, {800.f, 600.f}}, 0) {}
        auik::StyleUpdateFlags update_style() override { return {}; }
        void draw(auik::DrawCtx &) override {}
    };
    struct Field : auik::Textbox
    {
        Field() : Textbox(9001, "1.25", {100.f, 30.f}, auik::WidgetFlagBits::visible,
                          AUIK_TAG_TEXTBOX) { _content_pos = {10.f, 10.f}; _content_size = {100.f, 30.f}; }
        u16 content_id() const { return _content_clip_id; }
        void move_content(float x) { _content_pos.x = x; }
    };
}

void test_textbox_clip()
{
    auik::detail::Context ctx{};
    auik::Viewport viewport{};
    ctx.main_viewport = &viewport;
    auik::detail::GPUContext gpu{};
    ctx.gpu_ctx = &gpu;
    ctx.dirty_flags |= auik::DirtyFlagBits::destroying;
    ctx.frames_in_flight = 1;
    u32 versions[2]{};
    ctx.shared_sync_state[0].buffer_versions = &versions[0];
    ctx.shared_sync_state[1].buffer_versions = &versions[1];
    auik::detail::g_context = &ctx;
    auik::ModelDB shortcuts;
    assert(auik::setup_shortcut_model(&shortcuts));
    gpu.push_clip_rect = [](auto *, const amal::vec4 &r) -> u16 {
        assert(clips.size() < 0xFFFFu);
        clips.push_back(r);
        return static_cast<u16>(clips.size() - 1);
    };
    gpu.update_clip_rect = [](auto *, u16 id, const amal::vec4 &r) { clips.at(id) = r; };
    gpu.get_clip_rect = [](auto *, u16 id) { return &clips.at(id); };
    {
        Parent parent;
        Field field;
        const amal::vec4 bounds{0.f, 0.f, 800.f, 600.f};
        parent.ensure_own_clip_rect(bounds);
        field.set_parent(&parent);
        field.rebuild_clip_rects();
        const auto id = field.content_id();
        const auto count = clips.size();
        for (int i = 0; i < 100000; ++i)
        {
            field.move_content(static_cast<float>(i % 100));
            field.rebuild_clip_rects();
            assert(field.content_id() == id && clips.size() == count);
            assert(clips[parent.clip_id()] == bounds);
            assert(clips[id].x == static_cast<float>(i % 100));
        }
        // Rebuilding after a global clip reset must allocate fresh IDs.
        parent.reset_clip_rect_records();
        field.reset_clip_rect_records();
        clips.clear();
        parent.ensure_own_clip_rect(bounds);
        field.rebuild_clip_rects();
        assert(clips.size() == count);
        assert(field.content_id() != parent.clip_id());
        field.set_parent(nullptr);
    }
    ctx.io.shortcut_model_db = nullptr;
    auik::detail::g_context = nullptr;
    puts("PASS: Textbox clip stable through 100000 rebuilds and global reset");
}
