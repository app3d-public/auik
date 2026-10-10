#include <auik/auik.hpp>
#include <auik/pipelines.hpp>
#include <auik/widgets/tree.hpp>
#include <cassert>
#include <cstdio>

namespace
{
    struct TextProbe : auik::Text
    {
        TextProbe() : Text(9200, "Cube", {40.f, 20.f}, auik::WidgetFlagBits::visible) {}
        void seed() { _draw_ids.resize(4); for (u32 i = 0; i < 4; ++i) _draw_ids[i].render_id = 100 + i; }
        void check() const { assert(_draw_ids.size() == 4); for (u32 i = 0; i < 4; ++i) assert(_draw_ids[i].render_id == 100 + i); }
        auik::StyleUpdateFlags update_style() override { return {}; }
    };
    struct Row : auik::DrawBlock
    {
        TextProbe text;
        Row() : DrawBlock(9201, auik::WidgetFlagBits::visible, 0) { text.set_parent(this); }
        auik::StyleUpdateFlags update_style() override { return {}; }
        void draw(auik::DrawCtx &ctx) override
        {
            // Exercise real Text invalidation; unchanged glyph IDs must survive.
            if (ctx.reason & auik::DrawReasonBits::invalidate) text.draw(ctx);
            else assert(!(ctx.reason & auik::DrawReasonBits::record));
        }
    };
    struct Owner : auik::Widget
    {
        auik::Tree *tree = nullptr;
        Owner() : Widget(9100, auik::WidgetFlagBits::visible, {}, {{0.f, 0.f}, {300.f, 400.f}}, 0) {}
        auik::StyleUpdateFlags update_style() override { return {}; }
        void update_layout(bool) override {}
        void draw(auik::DrawCtx &ctx) override
        {
            assert(!(ctx.reason & auik::DrawReasonBits::record));
            if (tree) for (auto &node : tree->nodes()) node.label->draw_local(ctx);
        }
    };
}

void test_tree_refresh()
{
    auik::detail::Context ctx{};
    auik::Viewport viewport{};
    ctx.main_viewport = &viewport;
    acul::vector<auik::DrawStream *> streams(auik::get_default_streams_count(), nullptr);
    ctx.streams.default_streams = streams.data();
    auik::detail::g_context = &ctx;
    {
        Owner owner;
        owner.set_clip_id(0);
        auik::Tree tree(9101, {300.f, 400.f}, auik::WidgetFlagBits::visible, 0);
        owner.tree = &tree;
        tree.set_parent(&owner);
        tree.set_clip_id(0);
        auto *root = acul::alloc<Row>();
        auto *child = acul::alloc<Row>();
        auto *sibling = acul::alloc<Row>();
        const auto parent = tree.add_node(root);
        assert(tree.node_from_widget(&root->text) == parent);
        root->text.seed();
        tree.add_node(child, parent);
        assert(tree.node_from_widget(&child->text) == 1u);
        root->text.check();
        child->text.seed();
        tree.add_node(sibling);
        root->text.check();
        child->text.check();
        sibling->text.seed();
        for (int i = 0; i < 1000; ++i)
        {
            tree.set_node_expanded(parent, false, false);
            assert(child->text.draw_record_count() == 0);
            root->text.check();
            sibling->text.check();
            tree.set_node_expanded(parent, true, false);
            child->text.seed();
            root->text.check();
            sibling->text.check();
        }
        assert(!(ctx.dirty_flags & auik::DirtyFlagBits::layout));
        ctx.dirty_flags |= auik::DirtyFlagBits::destroying;
    }
    auik::detail::g_context = nullptr;
    puts("PASS: unchanged row Text draw IDs survive rebuilds; only hidden rows invalidate");
}
