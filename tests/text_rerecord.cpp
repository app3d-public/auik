#include <auik/auik.hpp>
#include <auik/pipelines.hpp>
#include <auik/widgets/text.hpp>
#include <cassert>
#include <cstdio>

namespace
{
    acul::vector<bool> live;
    u32 retired = 0;
    struct Glyphs : auik::Text
    {
        Glyphs() : Text(9400, "unchanged", {40.f, 20.f}, auik::WidgetFlagBits::visible)
        {
            _style.id = auik::get_theme()->get_resolved_style(AUIK_TAG_TEXT, id(), 0);
            set_clip_id(0);
            _instances.resize(5);
            for (auto &g : _instances) g.rect = {{10.f, 10.f}, {5.f, 10.f}};
        }
        void empty() { _instances.clear(); }
    };
} // namespace
void test_text_rerecord()
{
    auik::detail::Context ctx{};
    auik::Viewport viewport{};
    auik::Theme theme;
    ctx.main_viewport = &viewport;
    ctx.theme = &theme;
    acul::vector<auik::DrawStream *> defaults(auik::get_default_streams_count(), nullptr);
    ctx.streams.default_streams = defaults.data();
    auik::detail::g_context = &ctx;
    auik::DrawStream stream{};
    stream.post_slot_id = 1;
    stream.push_data_batch_to_stream = [](auto *, const void *, u32 count, auik::DrawDataID *ids) {
        for (u32 i = 0; i < count; ++i)
        {
            ids[i].render_id = static_cast<u32>(live.size());
            live.push_back(true);
        }
    };
    stream.invalidate_data_batch_in_stream = [](auto *, const auik::DrawDataID *ids, u32 count) {
        for (u32 i = 0; i < count; ++i)
        {
            if (ids[i].render_id == auik::AUIK_INVALID_DRAW_DATA_ID) continue;
            assert(ids[i].render_id < live.size());
            assert(live[ids[i].render_id]);
            live[ids[i].render_id] = false;
            ++retired;
        }
    };
    auik::set_primary_textured_quads_stream(&stream);
    {
        Glyphs text;
        for (int n = 0; n < 1000; ++n)
        {
            text.update_draw_commands(auik::DrawReasonBits::external | auik::DrawReasonBits::record);
            u32 active = 0;
            for (bool v : live)
                if (v) ++active;
            assert(active == 5); // Re-recording is deliberately retained.
            assert(retired == static_cast<u32>(n) * 5);
        }
        text.empty();
        text.update_draw_commands(auik::DrawReasonBits::record);
        for (bool v : live) assert(!v);
    }
    auik::detail::g_context = nullptr;
    puts("PASS: 1000 forced text re-records leave only current glyphs live; empty replacement retires all");
}
