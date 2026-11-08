#include <auik/auik.hpp>
#include <auik/detail/depth.hpp>
#include <auik/widgets/window.hpp>
#include <auik/widgets/dockspace.hpp>
#include <cassert>
#include <cstdio>

namespace
{
    int theme_epoch = 0, locale_epoch = 0;
    acul::vector<amal::vec4> clips;
    struct Probe : auik::Widget
    {
        int depths = 0;
        bool require_allocated_depth_on_layout = false;
        acul::unique_function<void()> during_depth;
        int theme_stamp = -1, locale_stamp = -1;
        Probe(u32 id) : Widget(id, auik::WidgetFlagBits::visible | auik::WidgetFlagBits::attachable) {}
        auik::StyleUpdateFlags update_style() override
        {
            if (is_style_invalidated()) theme_stamp = theme_epoch;
            return {};
        }
        bool update_locale() override
        {
            const bool changed = locale_stamp != locale_epoch;
            locale_stamp = locale_epoch;
            return changed;
        }
        void draw(auik::DrawCtx &) override {}
        void update_depth(const amal::vec2 &range) override
        {
            ++depths;
            Widget::update_depth(range);
            if (during_depth) { auto callback = std::move(during_depth); callback(); }
        }
        void update_layout(bool measured) override
        {
            if (require_allocated_depth_on_layout)
            {
                const auto index = auik::get_global_depth_index(this);
                assert(index < auik::global_depth_slot_count);
                assert(depth_range() == auik::global_depth_range(index));
            }
            Widget::update_layout(measured);
        }
        void open(auik::Widget *owner)
        {
            auik::add_overlay_window(this, owner, auik::DepthZone::foreground);
        }
    };
    struct WindowProbe : auik::Window
    {
        WindowProbe(u32 id) : Window(id, "", {{0, 0}, {100, 100}}, auik::WindowFlagBits::none,
                                     auik::WidgetFlagBits::visible | auik::WidgetFlagBits::attachable) {}
        auik::StyleUpdateFlags update_style() override { return {}; }
        void rebuild_clip_rects() override {}
        void draw(auik::DrawCtx &) override {}
    };
    struct ClippedProbe : Probe
    {
        using Probe::Probe;
        void rebuild_clip_rects() override { ensure_own_clip_rect({0, 0, 100, 100}); }
    };
}

void test_depth_order()
{
    using namespace auik;
    detail::Context ctx{};
    Viewport viewport{};
    viewport.rect = {{0, 0}, {800, 600}};
    ctx.main_viewport = &viewport;
    detail::GPUContext gpu{};
    ctx.gpu_ctx = &gpu;
    ctx.frames_in_flight = 1;
    u32 versions[2]{};
    ctx.shared_sync_state[0].buffer_versions = &versions[0];
    ctx.shared_sync_state[1].buffer_versions = &versions[1];
    gpu.push_clip_rect = [](auto *, const amal::vec4 &rect) -> u16 {
        clips.push_back(rect);
        return static_cast<u16>(clips.size() - 1);
    };
    gpu.update_clip_rect = [](auto *, u16 id, const amal::vec4 &rect) { clips.at(id) = rect; };
    gpu.get_clip_rect = [](auto *, u16 id) { return &clips.at(id); };
    gpu.reset_clip_rects = [](auto *) { clips.clear(); };
    gpu.clear_hit_rects = [](auto *) {};
    gpu.copy_hit_rects_frame = [](auto *, u32, u32) {};
    detail::g_context = &ctx;
    {
        Probe root(8100), background(8101), foreground(8102), menu(8103), a(8104), b(8105), tip(8106);
        auto *custom = root.emplace_user_data<int>(42);
        add_widget_to_root(&root);
        assert(root.get_user_data<int>() == custom && *custom == 42);
        add_widget_to_root(&foreground, DepthZone::foreground);
        add_widget_to_root(&background, DepthZone::background);
        assert(ctx.widget_tree.size() == 3);
        // An implementation widget can be connected to its owner by focus ancestry alone.
        menu.set_focus_parent(&root);
        a.open(&menu);
        b.open(&a);
        tip.open(&b);
        assert(ctx.widget_tree.size() == 3);
        push_widget_to_transient_cache(&tip);
        const int root_depths_before_popup = root.depths;
        Probe extra_popup(8107);
        extra_popup.open(&menu);
        remove_overlay_window(&extra_popup);
        assert(root.depths == root_depths_before_popup);
        assert(get_global_depth_index(&a) == 3);
        assert(get_global_depth_index(&b) == 4);
        assert(get_global_depth_index(&tip) == 5);
        assert(a.depth_range() == global_depth_range(3));
        assert(b.depth_range() == global_depth_range(4));
        assert(a.get_z_order() < tip.get_z_order());
        raise_global_depth(&foreground);
        assert(a.get_z_order() < tip.get_z_order() && tip.get_z_order() < foreground.get_z_order());
        a.unset_visible();
        a.sync_widget_flags(); // Closing a parent also dismisses its popup and tooltip descendants.
        assert(!is_overlay_window(&a) && !is_overlay_window(&b) && !is_overlay_window(&tip));
        assert(ctx.transient_cache.empty());
        assert(ctx.widget_tree.size() == 3);
        background.unset_visible();
        background.sync_widget_flags();
        assert(get_global_depth_index(&background) == global_depth_slot_count);
        a.open(&menu);
        background.set_visible();
        background.sync_widget_flags();
        assert(background.get_z_order() > a.get_z_order()); // Showing an existing root is a new presentation.
        remove_overlay_window(&a);
        {
            const auto root_count = ctx.root_window_count;
            WindowProbe popup(8700);
            add_overlay_window(&popup, &root);
            assert(ctx.root_window_count == root_count);
            remove_overlay_window(&popup);
            assert(ctx.root_window_count == root_count);
            Probe embedded(root.id());
            embedded.unset_widget_flag(WidgetFlagBits::attachable);
            embedded.open(&root);
            assert(is_overlay_window(&embedded));
            assert(ctx.id_map.find(root.id())->second == &root);
            remove_overlay_window(&embedded);
            assert(ctx.id_map.find(root.id())->second == &root);
        }
        // Raising one popup preserves the ranges of other live windows.
        a.open(&menu);
        b.open(&root);
        const auto b_range = b.depth_range();
        raise_global_depth(&a);
        assert(a.get_z_order() > b.get_z_order() && b.depth_range() == b_range);
        remove_overlay_window(&a);
        remove_overlay_window(&b);
        {
            Probe first(8110), middle(8111), last(8112);
            first.open(&root);
            middle.open(&root);
            last.open(&root);
            const auto middle_index = get_global_depth_index(&middle);
            remove_overlay_window(&middle);
            remove_overlay_window(&last);
            last.open(&root);
            assert(get_global_depth_index(&last) == middle_index); // Release tail through adjacent holes.
            remove_overlay_window(&last);
            remove_overlay_window(&first);
        }
        // Alternating tooltip/popup lifetimes accumulate interior holes; compaction must preserve show order.
        a.open(&menu);
        b.open(&root);
        for (u32 cycle = 0; cycle < 2000; ++cycle)
        {
            remove_overlay_window(&a);
            a.open(&menu);
            assert(a.get_z_order() > b.get_z_order());
            remove_overlay_window(&b);
            b.open(&root);
            assert(b.get_z_order() > a.get_z_order());
        }
        remove_overlay_window(&a);
        remove_overlay_window(&b);
        // Each concurrently open global window gets its own slot, without joining the root tree.
        acul::vector<Probe *> grouped;
        for (u32 i = 0; i < 3; ++i)
        {
            auto *popup = acul::alloc<Probe>(8200 + i);
            popup->open(&menu);
            grouped.push_back(popup);
        }
        assert(ctx.widget_tree.size() == 3);
        for (size_t i = 1; i < grouped.size(); ++i)
            assert(grouped[i - 1]->get_z_order() < grouped[i]->get_z_order());
        menu.on_detach();
        for (auto *popup : grouped) { assert(!is_overlay_window(popup)); acul::release(popup); }
        assert(ctx.widget_tree.size() == 3);
        // Hiding an intermediate container must dismiss a popup even while the layout root stays visible.
        Probe container(8300), trigger(8301), nested(8302);
        container.set_parent(&root);
        container.on_attach();
        trigger.set_parent(&container);
        nested.open(&trigger);
        container.unset_visible();
        container.sync_widget_flags();
        assert(!is_overlay_window(&nested) && root.is_visible());
        container.on_detach();
        ctx.disposal_queue.discard();
        // A hidden popup is absent from active traversal but must refresh before its next measurement.
        ++theme_epoch;
        ++locale_epoch;
        prepare_overlay_window(&a);
        assert(a.theme_stamp == theme_epoch && a.locale_stamp == locale_epoch);
        {
            ClippedProbe clipped(8400);
            clipped.open(&root);
            assert(clipped.clip_id() != 0xFFFFu);
            remove_overlay_window(&clipped);
            assert(clipped.clip_id() == 0xFFFFu);
            clips.clear(); // Replace the common pool while the popup is excluded from all active passes.
            clipped.ensure_own_clip_rect({0, 0, 200, 100}); // Owner measures it before registering it again.
            clipped.open(&root);
            assert(clipped.clip_id() < clips.size());
            remove_overlay_window(&clipped);
        }
        // Fragmented indices compact only at capacity, keeping every live window in opening order.
        acul::vector<Probe *> contexts;
        for (u32 i = 0; i < global_depth_slot_count - ctx.widget_tree.size(); ++i)
        {
            auto *popup = acul::alloc<Probe>(8500 + i);
            popup->open(&root);
            contexts.push_back(popup);
        }
        assert(ctx.global_depth_end == global_depth_slot_count);
        for (size_t i = 1; i < contexts.size(); ++i)
            assert(contexts[i - 1]->get_z_order() < contexts[i]->get_z_order());
        detail::GlobalDepthAllocation overflow;
        overflow.update_depth = [](void *, const amal::vec2 &) {};
        bool rejected = false;
        try { detail::acquire_global_depth(overflow); }
        catch (const std::exception &) { rejected = true; }
        assert(rejected && overflow.index == global_depth_slot_count);
        const auto later = contexts[2]->depth_range();
        ctx.dirty_flags &= ~DirtyFlagBits::layout;
        remove_overlay_window(contexts[1]); // An interior hole cannot move a window displayed later.
        assert(contexts[2]->depth_range() == later);
        assert(!(ctx.dirty_flags & DirtyFlagBits::layout));
        contexts[3]->during_depth = [&contexts] {
            remove_overlay_window(contexts[4]);
            acul::release(contexts[4]);
            contexts[4] = nullptr;
        };
        Probe after_hole(8599);
        after_hole.open(&root); // Reclaims the hole by compacting at the exhausted tail.
        assert(ctx.dirty_flags & DirtyFlagBits::layout); // Moved retained GPU records must be refreshed.
        assert(after_hole.get_z_order() > contexts.back()->get_z_order());
        for (auto *popup : contexts)
            if (popup) { remove_overlay_window(popup); acul::release(popup); }
        remove_overlay_window(&after_hole);
        // Docking drag helpers share chronological allocation with ordinary popup windows.
        WindowProbe drag(8598);
        enable_dockspace_drag_zones(&drag);
        const auto dock_range = detail::get_dock_overlay_depth_range();
        after_hole.open(&root);
        assert(after_hole.depth_range().x >= dock_range.y);
        remove_overlay_window(&after_hole);
        disable_dockspace_drag_zones(&drag);
        destroy_dockspace_context();
        ctx.disposal_queue.discard();
        int callbacks = 0;
        auto *child = acul::alloc<Probe>(8600);
        child->set_parent(&root);
        child->on_attach();
        add_render_command<detail::FocusEventTraits>(child, [&] { ++callbacks; });
        child->on_detach();
        acul::release(child);
        ctx.disposal_queue.flush_main_queue();
        assert(callbacks == 0);
        a.open(&root);
        add_render_command<detail::FocusEventTraits>(&a, [&] { ++callbacks; });
        remove_overlay_window(&a);
        a.open(&root);
        add_render_command<detail::FocusEventTraits>(&a, [&] { callbacks += 10; });
        ctx.disposal_queue.flush_main_queue();
        assert(callbacks == 10); // The callback from the previous registration must stay cancelled.
        remove_overlay_window(&a);
        add_render_command<detail::FocusEventTraits>(&root, [&] { ++callbacks; });
        remove_widget_from_root_unsync(&root);
        add_widget_to_root(&root);
        ctx.disposal_queue.flush_main_queue();
        assert(callbacks == 10);
        a.open(&menu);
        assert(a.is_attached());
        root.on_detach();
        assert(!is_overlay_window(&a) && !a.is_attached());
        remove_widget_from_root_unsync(&background);

        remove_widget_from_root_unsync(&root);

        remove_widget_from_root_unsync(&foreground);
        assert(ctx.widget_tree.empty());
        ctx.disposal_queue.discard();
    }
    {
        ctx.dirty_flags = {};
        ctx.io.display_size = {800, 600};
        ctx.viewports.push_back(&viewport);
        // The public root preparation path must arrange the widget with its allocated depth already set.
        Probe root(8800);
        root.require_allocated_depth_on_layout = true;
        add_widget_to_root(&root);
        mark_widget_attached_to_tree(&root);
        ctx.dirty_flags |= DirtyFlagBits::layout;
        record_layout_commands();
        remove_widget_from_root_unsync(&root);
        ctx.viewports.clear();
        ctx.disposal_queue.discard();
    }
    detail::g_context = nullptr;
    std::puts("PASS: chronological depth allocation, compaction, popup ownership and deferred callback lifetime");
}
