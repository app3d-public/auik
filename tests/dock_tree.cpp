#include <auik/auik.hpp>
#include <auik/pipelines.hpp>
#include <auik/widgets/detail/dock_tree.hpp>
#include <auik/widgets/dock_layout.hpp>
#include <auik/widgets/dockspace.hpp>
#include <auik/widgets/drag.hpp>
#include <auik/widgets/grid_layout.hpp>
#include <cassert>
#include <cstdio>

struct TestNode
{
    std::uint32_t parent = 0xFFFFFFFFu;
    acul::vector<std::uint32_t> children;
    int payload = 0;
};

struct CountedWidget : auik::Widget
{
    static inline int live = 0;
    explicit CountedWidget(unsigned id) : Widget(id, {}, {}, {{0.f, 0.f}, {1.f, 1.f}}, 0) { ++live; }
    ~CountedWidget() override { --live; }
    auik::StyleUpdateFlags update_style() override { return {}; }
    void draw(auik::DrawCtx &) override {}
};

struct CountedWindow : auik::Window
{
    static inline int live = 0;
    explicit CountedWindow(unsigned id) : Window(id, "", {}, {}, {}) { ++live; }
    ~CountedWindow() override { --live; }
};

struct SizedWindow : CountedWindow
{
    using CountedWindow::CountedWindow;
    void draw(auik::DrawCtx &) override {}
    void update_layout_min_size_force() override { set_required_size({280.f, 40.f}); }
    void update_layout(bool) override {}
    void rebuild_clip_rects() override {}
};
struct DepthDrawProbe : CountedWidget
{
    using CountedWidget::CountedWidget;
    f32 drawn_depth = -1.f;
    int depth_updates = 0;
    void update_depth(const amal::vec2 &range) override
    {
        ++depth_updates;
        Widget::update_depth(range);
    }
    void draw(auik::DrawCtx &) override { drawn_depth = get_z_order(); }
};
struct TallWindow : SizedWindow
{
    using SizedWindow::SizedWindow;
    void update_layout_min_size_force() override { set_required_size({280.f, 220.f}); }
};
struct DeepWindow : SizedWindow
{
    using SizedWindow::SizedWindow;
    u32 get_depth_requirement() const override { return 24u; }
};
struct LayoutWindow : CountedWindow
{
    using CountedWindow::CountedWindow;
    void draw(auik::DrawCtx &) override {}
};
static acul::vector<amal::vec4> clips;
static acul::vector<auik::detail::RectData> hits;
static auik::QuadsInstanceData captured_window_quad{};
static bool capture_window_quad = false;

struct ValueProbe : auik::detail::Draggable<float>
{
    ValueProbe() : Draggable(8001, AUIK_TAG_DRAG_FLOAT, 1.f, 0.f, 100.f, 1.f, {100.f, 20.f}, {}) {}
    // Exercise the presentation step used by measure/layout without a font renderer.
    using Draggable::set_value_internal;
    using Draggable::sync_text_presentation_from_value;
};

struct GeometryTabbar : auik::Tabbar
{
    using Tabbar::adopt_drag;
    using Tabbar::end_drag;
    using Tabbar::set_drag_bounds;
    auik::StyleState resolve_tab_item_state(unsigned,
                                            const auik::detail::WidgetStyleSelectorTransition &) const override
    {
        return auik::StyleState::normal;
    }
    GeometryTabbar()
        : Tabbar(99, acul::vector<acul::string>{"", ""}, auik::TabbarFlagBits::movable,
                 auik::detail::get_tabbar_widget_flags(), {200.f, 30.f})
    {
    }
    void draw(auik::DrawCtx &) override {}
};

struct DeferredRemovingDrag : CountedWidget
{
    mutable int boundary_checks = 0;
    DeferredRemovingDrag() : CountedWidget(8101) { add_event_flags(auik::EventFlagBits::drag); }
    void on_drag(const amal::vec2 &, auik::KeyPressState) override
    {
        // Keep the widget registered for the entire input dispatch.
        auik::add_render_command([widget_id = id()]() {
            auik::detail::get_context().id_map.erase(widget_id);
        }, true);
    }
    bool allows_unbounded_drag() const override
    {
        ++boundary_checks;
        return false;
    }
};

void test_dock_tree()
{
    {
        auik::Theme theme;
        theme.add_style(AUIK_STYLE_TAG_GLOBAL,
                        auik::make_style(auik::Style::extra_storage_size(sizeof(auik::StyleExtraAlign)))
                            .width(10.f).text_size(13.f).align_extra(auik::StyleExtraAlign{}));
        theme.add_style(0x8203u,
                        auik::make_style().width(20.f).disable(auik::detail::StylePropertiesBits::extra));
        const auto resolved = theme.get_resolved_style(0x8203u, 0u, 0u);
        assert(theme.get_style(resolved).width() == 20.f);
        assert(theme.get_style(resolved).text_size() == 13.f);
        assert(theme.get_style(resolved).extra(AUIK_STYLE_EXTRA_ALIGN) == nullptr);
        theme.add_style(0x8205u,
                        auik::make_style(auik::Style::extra_storage_size(sizeof(auik::StyleExtraAspectRatio)))
                            .background_color(0xffffffffu)
                            .aspect_ratio_extra(auik::StyleExtraAspectRatio{auik::AspectRatioMode::preserve}));
        theme.add_style(0x8205u,
                        auik::make_style()
                            .disable(auik::detail::StylePropertiesBits::background_color)
                            .disable(auik::detail::StylePropertiesBits::extra),
                        auik::StyleState::active);
        const auto active = theme.get_resolved_style(0x8205u, 0u, 0u, auik::StyleState::active);
        assert(theme.get_style(active).background_color() == 0u);
        assert(theme.get_style(active).extra(AUIK_STYLE_EXTRA_ASPECT_RATIO) == nullptr);
        const auto fallback = theme.get_resolved_style(0x8204u, 0u, 0u, auik::StyleState::hover);
        assert(theme.get_style(fallback).width() == 10.f);
        assert(theme.get_style(fallback).text_size() == 13.f);
        assert(theme.get_style(fallback).extra(AUIK_STYLE_EXTRA_ALIGN) != nullptr);
    }
    auik::detail::DockTree<TestNode> tree;
    const auto first = tree.append(0, {});
    const auto second = tree.append(0, {});
    TestNode middle_node{};
    middle_node.payload = 42;
    const auto middle = tree.append(0, std::move(middle_node), 1);
    assert(tree.get(0)->children[0] == first);
    assert(tree.get(0)->children[1] == middle);
    assert(tree.get(0)->children[2] == second);
    const auto nested = tree.append(middle, {});
    // Force storage growth, checking that IDs and links survive reallocations.
    for (int i = 0; i < 1000; ++i) tree.append(0, {});
    tree.detach(nested);
    tree.detach(nested);
    assert(tree.get(middle)->children.empty());
    assert(!tree.attached(nested));
    tree.detach(middle);
    assert(tree.get(middle)->payload == 42);
    assert(tree.get(0)->children[1] == second);
    assert(!tree.get(tree.invalid_node));
    tree.detach(0);
    assert(tree.attached(0));
    tree.reset();
    tree.reset();
    assert(tree.nodes().size() == 1 && tree.get(0)->children.empty());

    auik::detail::Context ctx{};
    auik::Viewport viewport{};
    auik::Theme theme;
    acul::vector<auik::DrawStream *> streams;
    streams.resize(auik::get_default_streams_count());
    for (auto &stream : streams) stream = nullptr;
    ctx.streams.default_streams = streams.data();
    ctx.theme = &theme;
    ctx.main_viewport = &viewport;
    ctx.dirty_flags |= auik::DirtyFlagBits::destroying;
    auik::detail::g_context = &ctx;
    auik::detail::GPUContext gpu{};
    gpu.push_clip_rect = [](auto *, const amal::vec4 &rect) -> std::uint16_t {
        clips.push_back(rect);
        return static_cast<std::uint16_t>(clips.size() - 1);
    };
    gpu.update_clip_rect = [](auto *, std::uint16_t id, const amal::vec4 &rect) { clips[id] = rect; };
    gpu.get_clip_rect = [](auto *, std::uint16_t id) { return id < clips.size() ? &clips[id] : nullptr; };
    gpu.push_hit_rect = [](auto *, const auik::detail::RectData &rect) -> std::uint32_t {
        hits.push_back(rect);
        return static_cast<std::uint32_t>(hits.size() - 1);
    };
    gpu.update_hit_rect = [](auto *, std::uint32_t id, const auik::detail::RectData &rect) { hits[id] = rect; };
    ctx.gpu_ctx = &gpu;
    std::uint32_t versions[2]{};
    ctx.frames_in_flight = 1;
    ctx.shared_sync_state[0].buffer_versions = &versions[0];
    ctx.shared_sync_state[1].buffer_versions = &versions[1];
    {
        // Restored preference panels can drop their children before the first clip layout.
        auik::DrawStream invalidation_stream{};
        for (auto *&stream : streams) stream = &invalidation_stream;
        auto *layout = auik::make_dock_layout(8695);
        auto *panel = auik::make_block(8696);
        panel->set_parent(layout);
        panel->add_child(auik::make_collapse_header(8697, "Options"));
        panel->on_attach();
        panel->clear_children();
        panel->on_detach();
        acul::release(panel);
        acul::release(layout);
        for (auto *&stream : streams) stream = nullptr;
    }
    {
        CountedWidget parent(8698);
        auik::Window window(8699, "Preferences", {}, auik::WindowFlagBits::decorated,
                            auik::WidgetFlagBits::visible);
        window.set_parent(&parent);
        auto &widget = static_cast<auik::Widget &>(window);
        widget.on_attach();
        window.set_header(nullptr);
        assert(!window.header());
        window.sync_window_flags();
        assert(window.header());
        widget.on_detach();
    }
    {
        auik::DrawStream invalidation_stream{};
        for (auto *&stream : streams) stream = &invalidation_stream;
        umbf::registry::HashResolver resolver;
        auik::insert_umbf_streams(resolver);
        umbf::registry::resolver = &resolver;
        acul::bin_stream snapshot;
        auto *original = auik::make_dockspace(8700);
        auto *panel = auik::make_dock_window(8701, "Inspector");
        panel->set_menu_bar(auik::make_menu_bar(8702));
        original->add_window(original->root_node(), panel);
        auik::streams::dockspace.write(snapshot, original);
        acul::release(original);
        auto *restored = static_cast<auik::Dockspace *>(auik::streams::dockspace.read(snapshot));
        unsigned headers = 0, attachments = 0;
        auik::Window *selected = nullptr;
        unsigned switches = 0;
        restored->on_node_attach([&](auik::DockNodeID, auik::Block &, auik::Tabbar &tabs) {
            assert(tabs.empty());
            ++headers;
        });
        restored->on_tab_attach([&](auik::DockNodeID, auik::Window *window) {
            if (window->id() == 8701) assert(window->menu_bar() && window->menu_bar()->id() == 8702);
            ++attachments;
        });
        restored->on_tab_switched([&](auik::DockNodeID node, auik::Window *previous, auik::Window *window) {
            assert(previous == selected);
            if (previous) assert(previous->id() == (switches == 1 ? 8701u : 8704u));
            assert(node == restored->root_node());
            selected = window;
            ++switches;
        });
        restored->on_attach();
        assert(headers == 1 && attachments == 1 && switches == 1 && selected && selected->id() == 8701);
        auto *second = auik::make_dock_window(8704, "Second");
        restored->add_window(restored->root_node(), second);
        assert(attachments == 2 && switches == 2 && selected == second);
        restored->clear_windows(restored->root_node());
        assert(switches == 3 && !selected);
        restored->on_detach();
        acul::release(restored);
        ctx.disposal_queue.discard();
        umbf::registry::resolver = nullptr;
        for (auto *&stream : streams) stream = nullptr;
    }
    {
        umbf::registry::HashResolver resolver;
        auik::insert_umbf_streams(resolver);
        umbf::registry::resolver = &resolver;

        auto *original = auik::make_dockspace(8710);
        original->set_policy_flags(original->policy_flags() | auik::DockspaceFlagBits::collapse);
        original->set_split_axis(original->root_node(), amal::axis::y);
        const auto top = original->create_leaf(original->root_node());
        const auto bottom = original->create_leaf(original->root_node());
        auto *inspector = auik::make_dock_window(8711, "Inspector");
        auto *attributes = auik::make_dock_window(8712, "Attributes");
        original->add_window(top, inspector);
        original->add_window(bottom, attributes);
        original->set_layout_size({640.0f, 480.0f});
        original->update_layout(false);
        const f32 expanded_boundary = attributes->position().y;
        original->set_node_collapsed(top, true);
        original->update_layout(false);
        const f32 collapsed_boundary = attributes->position().y;
        assert(collapsed_boundary < expanded_boundary);

        acul::bin_stream snapshot;
        auik::streams::dockspace.write(snapshot, original);
        original->set_node_collapsed(top, false);
        original->update_layout(false);
        assert(amal::abs(attributes->position().y - expanded_boundary) < 1.0f);
        acul::release(original);
        auto *restored = static_cast<auik::Dockspace *>(auik::streams::dockspace.read(snapshot));
        assert(restored->node_collapsed(top) && !restored->node_collapsed(bottom));
        restored->on_attach();
        restored->set_layout_size({640.0f, 480.0f});
        restored->update_layout(false);
        auto *restored_inspector = auik::get_widget_by_id(8711);
        auto *restored_attributes = auik::get_widget_by_id(8712);
        assert(restored_inspector && !restored_inspector->is_visible());
        assert(restored_attributes && restored_attributes->is_visible());
        assert(amal::abs(restored_attributes->position().y - collapsed_boundary) < 1.0f);

        restored->set_node_collapsed(top, false);
        restored->update_layout(false);
        assert(restored_inspector->is_visible());
        assert(amal::abs(restored_attributes->position().y - expanded_boundary) < 1.0f);

        for (int cycle = 0; cycle < 4; ++cycle)
        {
            restored->set_node_collapsed(top, true);
            restored->update_layout(false);
            assert(amal::abs(restored_attributes->position().y - collapsed_boundary) < 1.0f);
            restored->set_node_collapsed(top, false);
            restored->update_layout(false);
            assert(amal::abs(restored_attributes->position().y - expanded_boundary) < 1.0f);
        }

        restored->on_detach();
        acul::release(restored);
        ctx.disposal_queue.discard();
        umbf::registry::resolver = nullptr;
    }
    {
        // A full command rewrite after dismissing a popup rebuilds clip IDs without running layout.
        theme.add_style(AUIK_STYLE_TAG_DOCK_TABBAR_MENU, auik::make_style().background_color(0xffffffffu));
        auto *space = auik::make_dockspace(8900);
        space->set_layout_size({600.f, 400.f});
        auik::Widget *button = nullptr;
        space->on_node_attach([&](auik::DockNodeID, auik::Block &row, auik::Tabbar &tabs) {
            assert(tabs.empty());
            button = auik::make_popup_menu(8902);
            row.add_child(button, auik::make_layout_flags(auik::ChildLayout::inline_, auik::HAlign::right));
        });
        space->on_attach();
        space->add_window(0, acul::alloc<SizedWindow>(8901));
        space->update_layout(false);
        assert(button && button->clip_id() != 0xFFFFu);
        auik::QuadsInstanceData button_record{};
        auik::DrawStream button_stream{};
        // GPU records remain separate from their clipping metadata.
        button_stream.runtime_data = &button_record;
        button_stream.push_data_to_stream = [](auik::DrawStream *stream, const void *data) {
            *static_cast<auik::QuadsInstanceData *>(stream->runtime_data) =
                *static_cast<const auik::QuadsInstanceData *>(data);
            return auik::DrawDataID{17u, auik::AUIK_INVALID_DRAW_DATA_ID};
        };
        streams[AUIK_PRIMARY_QUAD_STREAM] = &button_stream;
        button->update_draw_commands(auik::DrawReasonBits::record);
        assert((button_record.mask & 0xFFFFu) == space->clip_id());
        space->reset_clip_rect_records();
        space->reset_draw_records();
        clips.clear();
        gpu.push_clip_rect(&gpu, {0, 0, 1, 1}); // Force a different parent clip ID in the new pool.
        space->rebuild_clip_rects();
        button->update_draw_commands(auik::DrawReasonBits::record);
        assert((button_record.mask & 0xFFFFu) != 0xFFFFu);
        assert((button_record.mask & 0xFFFFu) == space->clip_id());
        space->on_detach();
        streams[AUIK_PRIMARY_QUAD_STREAM] = nullptr;
        acul::release(space);
        ctx.disposal_queue.discard();
    }
    {
        // Parent layout repeatedly supplies its inherited clip before the child
        // rebuilds its own bounds. Neither allocation nor parent data may leak.
        CountedWidget parent(8390), child(8391);
        const amal::vec4 parent_bounds{0.f, 0.f, 800.f, 600.f};
        parent.ensure_own_clip_rect(parent_bounds);
        child.set_parent(&parent);
        child.set_clip_id(parent.clip_id());
        child.ensure_own_clip_rect({10.f, 10.f, 100.f, 30.f});
        const auto child_clip = child.clip_id();
        const auto allocated = clips.size();
        assert(child_clip != parent.clip_id());
        for (int i = 0; i < 100000; ++i)
        {
            child.set_clip_id(parent.clip_id());
            // Column also clears the active inherited ID before rebuilding.
            if (i % 2) child.set_clip_id(0xFFFFu);
            const amal::vec4 bounds{float(i % 100), 10.f, 100.f, 30.f};
            child.ensure_own_clip_rect(bounds);
            assert(child.clip_id() == child_clip);
            assert(clips.size() == allocated);
            assert(clips[parent.clip_id()] == parent_bounds);
            assert(clips[child_clip] == bounds);
        }
        child.reset_clip_rect_records();
        child.set_clip_id(parent.clip_id());
        child.ensure_own_clip_rect({20.f, 20.f, 100.f, 30.f});
        assert(clips.size() == allocated + 1);
        assert(child.clip_id() != child_clip && child.clip_id() != parent.clip_id());
        child.set_parent(nullptr);
    }
    {
        // A tool cursor lives only in the transient cache. Toggling its flag
        // must also remove and restore its recorded graphics.
        struct TransientProbe : CountedWidget
        {
            using CountedWidget::CountedWidget;
            int invalidations = 0, draws = 0;
            void draw(auik::DrawCtx &draw_ctx) override
            {
                if (draw_ctx.reason & auik::DrawReasonBits::invalidate) ++invalidations;
                else ++draws;
            }
            void update_layout(bool) override {}
        };
        ctx.disposal_queue.discard();
        ctx.dirty_flags &= ~auik::DirtyFlagBits::destroying;
        auto *cursor = acul::alloc<TransientProbe>(8301);
        cursor->widget_flags = auik::WidgetFlagBits::visible | auik::WidgetFlagBits::attachable;
        cursor->sync_widget_flags();
        auik::push_widget_to_transient_cache(cursor);
        assert(ctx.id_map.find(cursor->id()) == ctx.id_map.end());
        cursor->unset_visible();
        cursor->sync_widget_flags();
        ctx.disposal_queue.flush_main_queue();
        assert(!cursor->is_visible() && cursor->invalidations == 1);
        cursor->set_visible();
        cursor->sync_widget_flags();
        ctx.disposal_queue.flush_main_queue();
        assert(cursor->is_visible() && cursor->draws == 1);
        cursor->unset_visible();
        cursor->sync_widget_flags();
        ctx.disposal_queue.flush_main_queue();
        auik::erase_widget_from_transient_cache(cursor);
        acul::release(cursor);
        ctx.dirty_flags |= auik::DirtyFlagBits::destroying;
    }
    {
        theme.add_style(7101, auik::make_style().min_width(300.f));
        const auto resolved = theme.get_resolved_style(7101, 0, 0);
        assert(theme.get_style(resolved).min_width() == 300.f);
        assert(theme.get_resolved_style(7101, 0, 0) == resolved);

        auto overflow_style = auik::make_style(auik::Style::extra_storage_size(sizeof(auik::StyleExtraOverflow)));
        overflow_style.overflow_extra(auik::StyleExtraOverflow{auik::OverflowMode::auto_, auik::OverflowMode::scroll});
        theme.add_style(7102, overflow_style);
        overflow_style.destroy_extra();
        theme.add_style(7103, auik::make_style(auik::Style::extra_storage_size(sizeof(auik::StyleExtraAspectRatio)))
                                  .aspect_ratio_extra(auik::StyleExtraAspectRatio{auik::AspectRatioMode::preserve}));
        const auto composed = theme.get_resolved_style(7102, 7103, 0);
        const auto &composed_style = theme.get_style(composed);
        assert(composed_style.overflow_settings() == nullptr);
        assert(composed_style.aspect_ratio_settings());
        assert(composed_style.aspect_ratio_settings()->mode == auik::AspectRatioMode::preserve);
    }
    {
        auik::Theme window_theme;
        window_theme.add_style(AUIK_STYLE_TAG_WINDOW, auik::make_style()
                                                          .min_width(160.f)
                                                          .min_height(120.f)
                                                          .background_color(0xffffffffu)
                                                          .border_thickness(2.f)
                                                          .border_color(0xffccccccu)
                                                          .border_radius(6.f));
        window_theme.add_style(AUIK_STYLE_TAG_DOCKED_WINDOW,
                               auik::make_style()
                                   .min_width(220.f)
                                   .min_height(120.f)
                                   .padding({10.f, 8.f, 10.f, 8.f})
                                   .background_color(0xff778899u)
                                   .border_thickness(0.f)
                                   .border_color(0u)
                                   .border_radius(0.f));
        window_theme.add_style(7110, auik::make_style()
                                         .padding({0.f, 0.f, 0.f, 0.f})
                                         .background_color(0xff336699u)
                                         .border_thickness(3.f)
                                         .border_color(0xffffffffu)
                                         .border_radius(9.f));
        window_theme.add_style(7114, auik::make_style().background_color(0xff112233u));
        ctx.theme = &window_theme;
        auik::DrawStream quad_stream{};
        quad_stream.push_data_to_stream = [](auik::DrawStream *, const void *data) {
            if (capture_window_quad) captured_window_quad = *static_cast<const auik::QuadsInstanceData *>(data);
            capture_window_quad = false;
            return auik::DrawDataID{0u, auik::AUIK_INVALID_DRAW_DATA_ID};
        };
        streams[AUIK_PRIMARY_QUAD_STREAM] = &quad_stream;
        auto *bar = acul::alloc<auik::Window>(7111, "", amal::rect{}, auik::WindowFlagBits::none,
                                              auik::WidgetFlagBits::visible);
        bar->set_window_style_tag(7110);
        bar->update_style_invalidated();
        assert(bar->min_size() == amal::vec2(0.f));
        capture_window_quad = true;
        auik::DrawCtx draw_ctx{auik::DrawReasonBits::record};
        bar->draw(draw_ctx);
        assert(captured_window_quad.background_color == 0xff336699u);
        assert(captured_window_quad.border_thickness == 3.f);
        assert(captured_window_quad.border_radius == 9.f);
        assert(captured_window_quad.mask & (AUIK_HAS_BORDER_BIT << 20u));
        assert(captured_window_quad.mask & (AUIK_HAS_RADIUS_BIT << 20u));
        acul::release(bar);
        auto *viewport_window = acul::alloc<auik::Window>(7112, "", amal::rect{}, auik::WindowFlagBits::docked,
                                                          auik::WidgetFlagBits::visible);
        viewport_window->set_window_style_tag(7110);
        viewport_window->update_style_invalidated();
        assert(viewport_window->min_size() == amal::vec2(220.f, 120.f));
        capture_window_quad = true;
        viewport_window->draw(draw_ctx);
        assert(captured_window_quad.background_color == 0xff778899u);
        assert(captured_window_quad.border_thickness == 0.f);
        assert(captured_window_quad.border_radius == 0.f);
        assert(!(captured_window_quad.mask & (AUIK_HAS_BORDER_BIT << 20u)));
        assert(!(captured_window_quad.mask & (AUIK_HAS_RADIUS_BIT << 20u)));
        acul::release(viewport_window);
        auto *detached_window = acul::alloc<auik::Window>(
            7114, "", amal::rect{}, auik::WindowFlagBits::decorated | auik::WindowFlagBits::docked,
            auik::WidgetFlagBits::visible);
        detached_window->update_style_invalidated();
        capture_window_quad = true;
        detached_window->draw(draw_ctx);
        assert(captured_window_quad.background_color == 0xff112233u);
        assert(captured_window_quad.border_thickness == 0.f);
        assert(captured_window_quad.border_radius == 0.f);
        detached_window->window_flags &= ~auik::WindowFlagBits::docked;
        detached_window->reset_draw_records();
        capture_window_quad = true;
        detached_window->draw(draw_ctx);
        assert(captured_window_quad.background_color == 0xff112233u);
        assert(captured_window_quad.border_thickness == 2.f);
        assert(captured_window_quad.border_radius == 6.f);
        assert(captured_window_quad.mask & (AUIK_HAS_BORDER_BIT << 20u));
        assert(captured_window_quad.mask & (AUIK_HAS_RADIUS_BIT << 20u));
        acul::release(detached_window);
        streams[AUIK_PRIMARY_QUAD_STREAM] = nullptr;
        ctx.theme = &theme;
    }
    theme.add_style(7001, auik::make_style().size(AUIK_SIZE_FILL));
    theme.add_style(7002, auik::make_style().size(AUIK_SIZE_FIT));
    theme.add_style(7003, auik::make_style().min_width(180.f).min_height(60.f));
    theme.add_style(7004, auik::make_style().min_width(240.f).min_height(90.f));
    {
        // Removing the active tab must measure the newly visible window's children.
        auto *space = auik::make_dockspace(96);
        space->set_layout_size({1000.f, 400.f});
        auik::DockNodeSettings settings{};
        settings.tabpanel = false;
        settings.flags = {};
        space->create_leaf(0, settings);
        const auto node = space->create_leaf(0, settings);
        auto *remaining = acul::alloc<LayoutWindow>(97);
        remaining->set_window_style_tag(7003);
        auto *content = acul::alloc<CountedWidget>(98);
        content->set_visible();
        content->set_size({320.f, 40.f});
        remaining->add_child(content);
        space->add_window(node, remaining);
        auto *active = acul::alloc<LayoutWindow>(99);
        space->add_window(node, active);
        remaining->unset_visible();
        space->update_layout(false);
        space->close_window(node, active);
        assert(remaining->size().x >= 320.f);
        acul::release(space);
    }
    {
        // Requested window sizes seed new dock nodes, but sibling FILL leaves share the available height equally.
        // Adding a larger tab must not turn the existing leaf height into a new request.
        auto *space = auik::make_dockspace(107);
        auik::DrawStream invalidation_stream{};
        for (auto *&stream : streams) stream = &invalidation_stream;
        space->set_layout_size({400.f, 1000.f});
        space->set_split_axis(space->root_node(), amal::axis::y);
        auik::DockNodeSettings settings{};
        settings.flags = {};
        const auto object_node = space->create_leaf(space->root_node(), settings);
        const auto attributes_node = space->create_leaf(space->root_node(), settings);
        auto *object = acul::alloc<SizedWindow>(108);
        object->set_size({280.f, 420.f});
        auto *attributes = acul::alloc<SizedWindow>(109);
        attributes->set_size({280.f, 320.f});
        space->add_window(object_node, object);
        space->add_window(attributes_node, attributes);
        space->update_layout(false);
        assert(object->size().y == attributes->size().y);

        const f32 object_height = object->size().y;
        const f32 attributes_height = attributes->size().y;
        space->set_layout_size({400.f, 1200.f});
        space->update_layout(false);
        assert(object->size().y == object_height + 100.f);
        assert(attributes->size().y == attributes_height + 100.f);
        space->set_layout_size({400.f, 1000.f});
        space->update_layout(false);
        assert(object->size().y == object_height);
        assert(attributes->size().y == attributes_height);

        auto *large_tab = acul::alloc<SizedWindow>(110);
        large_tab->set_size({280.f, 700.f});
        space->add_window(attributes_node, large_tab);
        space->update_layout(false);
        assert(object->size().y == object_height);
        assert(large_tab->size().y == attributes_height);
        acul::release(space);
        for (auto *&stream : streams) stream = nullptr;
    }
    {
        // A preserved dock size follows the active window and remains soft when space is insufficient.
        theme.add_style(7012, auik::make_style(auik::Style::extra_storage_size(sizeof(auik::StyleExtraAspectRatio)))
                                  .aspect_ratio_extra(auik::StyleExtraAspectRatio{auik::AspectRatioMode::preserve}));
        auto *space = auik::make_dockspace(112);
        auik::DrawStream invalidation_stream{};
        for (auto *&stream : streams) stream = &invalidation_stream;
        space->on_attach();
        space->set_layout_size({600.f, 600.f});
        space->set_split_axis(space->root_node(), amal::axis::y);
        auik::DockNodeSettings settings{};
        settings.flags = {};
        settings.tabpanel = false;
        space->create_leaf(space->root_node(), settings);
        settings.tabpanel = true;
        const auto tabs_node = space->create_leaf(space->root_node(), settings);

        auto *preserved = acul::alloc<TallWindow>(113);
        preserved->set_size({280.f, 220.f});
        preserved->set_dock_style_tag(7012);
        space->add_window(tabs_node, preserved);
        space->update_layout(false);
        const f32 origin_height = preserved->size().y;
        assert(origin_height == 220.f);

        auto *regular = acul::alloc<SizedWindow>(114);
        space->add_window(tabs_node, regular);
        space->update_layout(false);
        const f32 regular_height = regular->size().y;
        assert(regular_height == origin_height);

        auik::Tabbar *tabbar = nullptr;
        for (const auto &entry : ctx.id_map)
            if (entry.second->signature() == AUIK_TAG_TABBAR && entry.second->parent() &&
                entry.second->parent()->parent() == space)
                tabbar = static_cast<auik::Tabbar *>(entry.second);
        assert(tabbar && tabbar->child_size() == 2u);
        auto select_tab = [&](u32 index) {
            const u32 element_id = tabbar->item_element_id(index);
            tabbar->set_selected(element_id);
            tabbar->mark_changed(auik::TabbarChangeReason::selection, element_id);
        };
        select_tab(0u);
        assert(preserved->size().y == origin_height);
        select_tab(1u);
        assert(regular->size().y == regular_height);

        select_tab(0u);
        space->set_layout_size({600.f, 250.f});
        space->update_layout(false);
        assert(preserved->size().y < origin_height);
        space->set_layout_size({600.f, 600.f});
        space->update_layout(false);
        assert(preserved->size().y == origin_height);
        assert(preserved->requested_size().y == 220.f);

        // The preserve policy belongs to docking. Once detached, width remains fixed but height
        // returns to the floating window's normal fit-content behavior.
        unsigned undocked = 0;
        space->on_tab_undock([&](auik::Window *floating) {
            assert(floating == preserved && floating->parent() == nullptr);
            assert(!(floating->window_flags & auik::WindowFlagBits::docked));
            ++undocked;
        });
        space->undock_window(tabs_node, preserved);
        assert(undocked == 1);
        assert(auik::is_size_fit(preserved->requested_size().y));
        assert(preserved->parent() == nullptr);
        assert(auik::remove_widget_from_root_unsync(preserved));
        acul::release(preserved);
        space->on_detach();
        acul::release(space);
        for (auto *&stream : streams) stream = nullptr;
    }
    {
        // Removing a window cannot shrink an initialized dock branch. A newly selected tab with a larger
        // content minimum also fits into that allocation instead of reinitializing the branch width.
        theme.add_style(7013, auik::make_style().min_width(600.f));
        theme.add_style(7014, auik::make_style().min_width(280.f));
        auto *space = auik::make_dockspace(115);
        auik::DrawStream invalidation_stream{};
        for (auto *&stream : streams) stream = &invalidation_stream;
        space->set_layout_size({1000.f, 500.f});
        space->set_split_axis(space->root_node(), amal::axis::x);
        auik::DockNodeSettings settings{};
        settings.flags = {};
        settings.tabpanel = false;
        space->create_leaf(space->root_node(), settings);
        const auto side = space->create_split(space->root_node(), amal::axis::y, settings);
        const auto attributes_node = space->create_leaf(side, settings);
        settings.tabpanel = true;
        const auto transform_node = space->create_leaf(side, settings);

        auto *attributes = acul::alloc<SizedWindow>(116);
        attributes->set_size({420.f, 200.f});
        auto *transform = acul::alloc<SizedWindow>(117);
        transform->set_size({360.f, 200.f});
        transform->set_dock_style_tag(7014);
        space->add_window(attributes_node, attributes);
        space->add_window(transform_node, transform);
        space->update_layout(false);
        const f32 initialized_width = transform->size().x;
        assert(initialized_width == 280.f);

        space->close_window(attributes_node, attributes);
        assert(transform->size().x == initialized_width);

        auto *wide_tab = acul::alloc<SizedWindow>(118);
        wide_tab->set_window_style_tag(7013);
        space->add_window(transform_node, wide_tab);
        space->update_layout(false);
        assert(wide_tab->size().x == initialized_width);
        acul::release(space);
        for (auto *&stream : streams) stream = nullptr;
    }
    {
        // Adding a new FILL leaf rebalances all FILL siblings and leaves a preserved sibling untouched.
        auto *space = auik::make_dockspace(124);
        auik::DrawStream invalidation_stream{};
        for (auto *&stream : streams) stream = &invalidation_stream;
        space->set_layout_size({400.f, 1000.f});
        space->set_split_axis(space->root_node(), amal::axis::y);
        auik::DockNodeSettings settings{};
        settings.flags = {};
        settings.tabpanel = false;
        const auto object_node = space->create_leaf(space->root_node(), settings);
        const auto transform_node = space->create_leaf(space->root_node(), settings);
        auto *object = acul::alloc<SizedWindow>(125);
        auto *transform = acul::alloc<TallWindow>(126);
        transform->set_dock_style_tag(7012);
        space->add_window(object_node, object);
        space->add_window(transform_node, transform);
        space->update_layout(false);
        const auto transform_bounds = transform->bounds();

        const auto attributes_node = space->create_leaf(space->root_node(), settings);
        auto *attributes = acul::alloc<SizedWindow>(127);
        space->add_window(attributes_node, attributes);
        space->update_layout(false);
        assert(object->size().y == attributes->size().y);
        assert(transform->size() == transform_bounds.size);
        acul::release(space);
        for (auto *&stream : streams) stream = nullptr;
    }
    {
        // A preserved trailing leaf stays pinned to the same bottom edge when a sibling disappears,
        // and removing that trailing leaf does not resize the remaining dock branch.
        theme.add_style(7015, auik::make_style(auik::Style::extra_storage_size(sizeof(auik::StyleExtraAspectRatio)))
                                  .min_width(280.f)
                                  .aspect_ratio_extra(auik::StyleExtraAspectRatio{auik::AspectRatioMode::preserve}));
        auto *space = auik::make_dockspace(119);
        auik::DrawStream invalidation_stream{};
        for (auto *&stream : streams) stream = &invalidation_stream;
        space->set_layout_size({1000.f, 601.f});
        space->set_split_axis(space->root_node(), amal::axis::x);
        auik::DockNodeSettings settings{};
        settings.flags = {};
        settings.tabpanel = false;
        const auto viewport_node = space->create_leaf(space->root_node(), settings);
        const auto side = space->create_split(space->root_node(), amal::axis::y, settings);
        const auto object_node = space->create_leaf(side, settings);
        const auto attributes_node = space->create_leaf(side, settings);
        const auto transform_node = space->create_leaf(side, settings);
        auto *viewport = acul::alloc<SizedWindow>(120);
        auto *object = acul::alloc<SizedWindow>(121);
        auto *attributes = acul::alloc<SizedWindow>(122);
        auto *transform = acul::alloc<TallWindow>(123);
        transform->set_dock_style_tag(7015);
        space->add_window(viewport_node, viewport);
        space->add_window(object_node, object);
        space->add_window(attributes_node, attributes);
        space->add_window(transform_node, transform);
        space->update_layout(false);
        const auto transform_bounds = transform->bounds();
        const f32 branch_width = object->size().x;

        space->close_window(attributes_node, attributes);
        assert(transform->position() == transform_bounds.offset);
        assert(transform->size() == transform_bounds.size);
        assert(object->position().y + object->size().y == transform->position().y);
        space->close_window(transform_node, transform);
        assert(object->size().x == branch_width);
        acul::release(space);
        for (auto *&stream : streams) stream = nullptr;
    }
    {
        // Shared tree storage must retain both siblings used to build the resize helper.
        auto *space = auik::make_dockspace(103);
        space->set_layout_size({600.f, 400.f});
        auik::DockNodeSettings settings{};
        settings.flags =
            auik::DockspaceResizeFlagBits::resize_helper_x | auik::DockspaceResizeFlagBits::visible_resize_helper_x;
        settings.tabpanel = false;
        const auto first_node = space->create_leaf(space->root_node(), settings);
        const auto second_node = space->create_leaf(space->root_node(), settings);
        auto *first_window = acul::alloc<SizedWindow>(104);
        auto *second_window = acul::alloc<SizedWindow>(105);
        space->add_window(first_node, first_window);
        space->add_window(second_node, second_window);
        space->update_layout(false);
        assert(first_window->size().x + second_window->size().x < space->size().x);
        acul::release(space);
    }
    {
        // A nested dock must redistribute its root's range when its depth requirement grows.
        auto *root = auik::make_dock_layout(110);
        auto *space = auik::make_dockspace(111);
        auik::DockNodeSettings settings{};
        settings.tabpanel = false;
        settings.flags = {};
        const auto leaf = space->create_leaf(0, settings);
        auto *sibling = acul::alloc<DepthDrawProbe>(112);
        sibling->set_visible();
        sibling->sync_widget_flags();
        root->add_child(0, space);
        root->add_child(0, sibling);
        auik::add_widget_to_root(root);
        const auto sibling_range = sibling->depth_range();
        const auto root_range = root->depth_range();
        const int updates = sibling->depth_updates;
        auto *window = acul::alloc<DeepWindow>(113);
        space->add_window(leaf, window);
        assert(root->depth_range() == root_range);
        assert(sibling->depth_updates == updates + 1);
        assert(sibling->depth_range() != sibling_range);
        assert(window->depth_range().y <= space->depth_range().y);
        assert(space->depth_range().y <= sibling->depth_range().x);
        auik::remove_widget_from_root_unsync(root);
        acul::release(root);
        ctx.disposal_queue.discard();
    }
    {
        // A dropped leaf must use its style on the very first layout, just like a created leaf.
        theme.add_style(7005, auik::make_style().size(AUIK_SIZE_FILL).min_width(400.f));
        auto *space = auik::make_dockspace(94);
        space->set_policy_flags(auik::DockspaceFlagBits::docking);
        space->set_layout_size({1000.f, 400.f});
        auik::DockNodeSettings settings{};
        settings.tabpanel = false;
        settings.flags = {};
        space->create_leaf(0, settings);
        space->set_new_node_settings(settings);
        auto *window = acul::alloc<SizedWindow>(95);
        window->set_dock_style_tag(7005);
        ctx.widget_tree.push_back(window);
        auto *toolbar = acul::alloc<DepthDrawProbe>(106);
        toolbar->set_visible();
        toolbar->sync_widget_flags();
        auik::add_widget_to_root(toolbar);
        const int toolbar_depth_updates = toolbar->depth_updates;
        toolbar->update_draw_commands();
        auik::enable_dockspace_drag_zones(window);
        space->update_layout(false);
        // With one leaf and no split handles, the first helper is the left root drop zone.
        space->on_drop({window->id(), AUIK_TAG_WINDOW_HEADER, 0u},
                       {space->id(), AUIK_TAG_DOCKSPACE_RESIZE_HELPER_V, 0u});
        assert(window->parent() == space);
        assert(toolbar->drawn_depth == toolbar->get_z_order());
        assert(toolbar->depth_updates == toolbar_depth_updates);
        auik::remove_widget_from_root_unsync(toolbar);
        ctx.widget_tree.clear();
        acul::release(toolbar);
        const auto docked_size = window->size();
        assert(docked_size.x == 500.f);
        space->invalidate_style();
        space->update_style_invalidated();
        space->update_layout(false);
        assert(window->size() == docked_size);
        acul::release(space);
        auik::destroy_dockspace_context();
    }
    {
        theme.add_style(7010, auik::make_style().width(80.f).height(20.f));
        auto *space = auik::make_dockspace(96);
        space->on_attach();
        space->add_window(0, acul::alloc<CountedWindow>(97));
        auto find_tabbar = [&]() -> auik::Tabbar * {
            for (const auto &entry : ctx.id_map)
                if (entry.second->signature() == AUIK_TAG_TABBAR && entry.second->parent() &&
                    entry.second->parent()->parent() == space)
                    return static_cast<auik::Tabbar *>(entry.second);
            return nullptr;
        };
        assert(find_tabbar() && find_tabbar()->movable());
        auto *tabs = acul::alloc<GeometryTabbar>();
        tabs->set_parent(space);
        tabs->set_item_style_tag(7010);
        tabs->set_selected_item_style_tag(7010);
        space->set_layout_size({1000.f, 400.f});
        space->rebuild_clip_rects();
        tabs->set_position({100.f, 0.f});
        tabs->set_layout_size({200.f, 30.f});
        // The menu adds 40 pixels to the panel beyond the tabbar's own layout area.
        tabs->set_drag_bounds({{100.f, 0.f}, {240.f, 30.f}});
        tabs->update_style_invalidated();
        tabs->update_layout(false);
        auto *item = tabs->item_at(0);
        ctx.io.mouse_pos = item->position() + amal::vec2{10.f, 10.f};
        ctx.io.drag_id = auik::make_element_id(tabs->id(), tabs->item_style_tag(), tabs->item_element_id(0));
        tabs->adopt_drag();
        ctx.io.mouse_pos = {1000.f, 10.f};
        tabs->on_drag({800.f, 0.f}, auik::KeyPressState::repeat);
        tabs->update_layout(true);
        assert(item->position().x + item->size().x == 340.f);
        assert(tabs->item_at(1) == item);
        ctx.io.mouse_pos = {0.f, 10.f};
        tabs->on_drag({-1000.f, 0.f}, auik::KeyPressState::repeat);
        tabs->update_layout(true);
        assert(item->position().x == 100.f);
        assert(tabs->item_at(0) == item);
        tabs->end_drag();
        assert(!ctx.io.drag_id);
        acul::release(tabs);

        space->on_detach();
        acul::release(space);
        auik::destroy_dockspace_context();
    }
    {
        auik::ModelDB shortcuts;
        assert(auik::setup_shortcut_model(&shortcuts));
        auto *drag = acul::alloc<ValueProbe>();
        int changes = 0;
        drag->bind().on_change([&](auik::ChangeEvent &) { ++changes; });
        const auto text_before = drag->Textbox::value();
        ctx.dirty_flags = {};
        drag->set_value(15.f);
        assert(drag->value() == 15.f && drag->last_value() == 1.f);
        assert(changes == 0 && !ctx.dirty_flags);
        assert(drag->Textbox::value() == text_before);
        drag->mark_changed();
        assert(drag->Textbox::value() == "15");
        assert(changes == 1);
        changes = 0;
        drag->set_value(150.f);
        assert(drag->value() == 100.f && changes == 0);
        drag->set_value_internal(-5.f, true);
        assert(drag->value() == 0.f && drag->last_value() == 100.f && changes == 1);
        assert(drag->Textbox::value() == "0");
        drag->set_value_internal(0.f, false);
        assert(changes == 1);
        bool refresh = false;
        ctx.host_refresh_request = &refresh;
        ctx.dirty_flags = {};
        drag->on_drag({}, auik::KeyPressState::press);
        drag->on_drag({3.f, 0.f}, auik::KeyPressState::repeat);
        assert(drag->value() == 3.f && refresh);
        assert(!(ctx.dirty_flags & auik::detail::layout_update_dirty_mask));
        drag->on_drag({}, auik::KeyPressState::release);

        auik::ModelDB values;
        auto *model = auik::make_value_model<float>(&values, 9001, 1, 10.f);
        auto *binding = auik::make_model_binding(9001, model->records[0].id, 1);
        assert(values.register_binding(binding));
        ctx.dirty_flags = {};
        refresh = false;
        drag->set_model_binding(binding);
        assert(drag->value() == 10.f && !refresh);
        assert(!(ctx.dirty_flags & auik::detail::layout_update_dirty_mask));
        drag->on_attach();
        ctx.dirty_flags = {};
        refresh = false;
        assert(auik::set_model_binding_value<float>(*binding, 25.f));
        assert(drag->value() == 25.f && refresh);
        assert(!(ctx.dirty_flags & auik::detail::layout_update_dirty_mask));
        drag->on_detach();
        drag->set_model_binding(nullptr);
        assert(values.unregister_binding(binding));
        acul::release(drag);
        ctx.host_refresh_request = nullptr;
        ctx.io.shortcut_model_db = nullptr;
        ctx.dirty_flags |= auik::DirtyFlagBits::destroying;
    }
    {
        ctx.dirty_flags &= ~auik::DirtyFlagBits::destroying;
        ctx.disposal_queue.discard();
        DeferredRemovingDrag widget;
        ctx.io.mouse_down = true;
        ctx.io.clicked_id.widget_id = widget.id();
        ctx.io.active_mouse_buttons = auik::MouseKey::left;
        ctx.id_map.emplace(widget.id(), &widget);
        auik::detail::on_mouse_move({1.f, 0.f});
        assert(ctx.id_map.find(widget.id()) != ctx.id_map.end() && widget.boundary_checks == 1);
        ctx.disposal_queue.flush_main_queue();
        assert(ctx.id_map.find(widget.id()) == ctx.id_map.end());
        ctx.id_map.emplace(widget.id(), &widget);
        ctx.frame_cache.drag_widget_id = widget.id();
        ctx.frame_cache.drag_delta = {1.f, 0.f};
        ctx.frame_cache.changes = auik::detail::FrameChangesBits::drag_delta;
        auik::detail::flush_frame_changes();
        assert(ctx.id_map.find(widget.id()) != ctx.id_map.end() && widget.boundary_checks == 2);
        ctx.disposal_queue.flush_main_queue();
        assert(ctx.id_map.find(widget.id()) == ctx.id_map.end());
        ctx.io.mouse_down = false;
        ctx.io.clicked_id = {};
        ctx.io.drag_id = {};
        ctx.io.active_mouse_buttons = {};
        ctx.dirty_flags |= auik::DirtyFlagBits::destroying;
    }
    for (int i = 0; i < 100; ++i)
    {
        auto *dock = auik::make_dock_layout(1);
        const auto split = dock->create_split(0, amal::axis::x);
        const auto leaf = dock->create_leaf(split);
        auto *grid = auik::make_grid_layout(2, 1, 1);
        grid->set_cell(0, 0, acul::alloc<CountedWidget>(3));
        dock->add_child(leaf, grid);
        acul::release(dock);
        assert(CountedWidget::live == 0);

        dock = auik::make_dock_layout(4);
        auto *retained = acul::alloc<CountedWidget>(5);
        dock->add_child(0, retained);
        dock->clear();
        dock->clear();
        assert(CountedWidget::live == 1 && retained->parent() == nullptr);
        dock->add_child(dock->create_leaf(0), retained);
        acul::release(dock);
        assert(CountedWidget::live == 0);

        auto *space = auik::make_dockspace(6);
        auto group = space->create_split(0, amal::axis::y);
        auik::DockNodeSettings settings{};
        settings.tabpanel = false;
        const auto window_leaf = space->create_leaf(group, settings);
        space->create_leaf(group, settings);
        space->add_window(window_leaf, acul::alloc<CountedWindow>(7));
        space->clear();
        assert(CountedWindow::live == 0);
        space->clear();
        assert(space->create_leaf(0) == 1);
        acul::release(space);
    }
    {
        // Releasing a dock window must not resolve its cached presentation style.
        auto *space = auik::make_dockspace(8200);
        auto *window = acul::alloc<CountedWindow>(8201);
        space->add_window(space->root_node(), window);
        window->update_style_invalidated();
        theme.set_var(0x8202u, 1.f); // Invalidates the resolved style pool.
        acul::release(space);
        assert(CountedWindow::live == 0);
    }
    {
        // Padding and the shared gap are present with interactive, hidden resize helpers.
        theme.add_style(AUIK_STYLE_TAG_DOCKSPACE,
                        auik::make_style().padding(amal::vec4{8.0f}).inline_spacing(8.0f));
        auto *space = auik::make_dockspace(8800);
        space->set_layout_size({640.0f, 480.0f});
        auto settings = auik::make_dockspace_settings(auik::DockspaceResizeFlagBits::resize_helper);
        settings.tabpanel = false;
        const auto left = space->create_leaf(space->root_node(), settings);
        const auto right = space->create_split(space->root_node(), amal::axis::y, settings);
        auto *viewport_window = acul::alloc<SizedWindow>(8801);
        auto *top_window = acul::alloc<SizedWindow>(8802);
        auto *bottom_window = acul::alloc<SizedWindow>(8803);
        space->add_window(left, viewport_window);
        space->add_window(space->create_leaf(right, settings), top_window);
        space->add_window(space->create_leaf(right, settings), bottom_window);
        space->update_layout(false);
        assert(viewport_window->position() == amal::vec2(8.0f, 8.0f));
        assert(top_window->position().x - (viewport_window->position().x + viewport_window->size().x) == 8.0f);
        assert(bottom_window->position().y - (top_window->position().y + top_window->size().y) == 8.0f);
        assert(bottom_window->position() + bottom_window->size() == amal::vec2(632.0f, 472.0f));
        auik::DrawStream invalidation_stream{};
        for (auto *&stream : streams) stream = &invalidation_stream;
        const auto previous_width = viewport_window->size().x;
        auik::detail::WindowContext window_context{};
        window_context.set_cursor = [](auto, auto *) {};
        ctx.window_ctx = &window_context;
        ctx.io.drag_id = auik::make_element_id(space->id(), AUIK_TAG_DOCKSPACE_RESIZE_HELPER_V, 0);
        space->on_drag({-20.0f, 0.0f}, auik::KeyPressState::press);
        space->on_drag({}, auik::KeyPressState::release);
        ctx.io.drag_id = {};
        ctx.window_ctx = nullptr;
        assert(viewport_window->size().x == previous_width - 20.0f);
        assert(top_window->position().x - (viewport_window->position().x + viewport_window->size().x) == 8.0f);
        acul::release(space);
        for (auto *&stream : streams) stream = nullptr;
    }
    {
        auto *grid = auik::make_grid_layout(8900, 1, 2);
        grid->set_layout_size({640.0f, 480.0f});
        grid->set_column_resizable(true);
        auto *left = acul::alloc<CountedWidget>(8901);
        auto *right = acul::alloc<CountedWidget>(8902);
        grid->set_cell(0, 0, left);
        grid->set_cell(0, 1, right);
        grid->update_layout(false);
        grid->set_clip_id(auik::push_clip_rect({0.f, 0.f, 640.f, 480.f}));
        grid->rebuild_clip_rects();
        const auto previous_right_x = right->position().x;

        auik::DrawStream invalidation_stream{};
        for (auto *&stream : streams) stream = &invalidation_stream;
        auik::detail::WindowContext window_context{};
        window_context.set_cursor = [](auto, auto *) {};
        ctx.window_ctx = &window_context;
        ctx.io.drag_id = auik::make_element_id(grid->id(), AUIK_TAG_GRID_LAYOUT_RESIZE_HELPER_V, 0);
        grid->on_drag({20.0f, 0.0f}, auik::KeyPressState::press);
        grid->on_drag({}, auik::KeyPressState::release);
        ctx.io.drag_id = {};
        ctx.window_ctx = nullptr;

        assert(amal::abs(right->position().x - previous_right_x - 20.0f) < 0.01f);
        acul::release(grid);
        for (auto *&stream : streams) stream = nullptr;
    }
    {
        auto *space = auik::make_dockspace(9000);
        const auto node = space->root_node();
        auto *window = acul::alloc<CountedWindow>(9001);
        window->set_visible();
        space->add_window(node, window);
        space->set_node_collapsed(node, true);
        assert(!space->node_collapsed(node) && window->is_visible());

        const auto flags = space->policy_flags();
        space->set_policy_flags(flags | auik::DockspaceFlagBits::collapse);
        space->set_node_collapsed(node, true);
        assert(space->node_collapsed(node) && !window->is_visible());
        space->set_node_collapsed(node, false);
        assert(!space->node_collapsed(node) && window->is_visible());
        space->set_node_collapsed(node, true);
        space->set_policy_flags(flags);
        assert(!space->node_collapsed(node) && window->is_visible());
        acul::release(space);
    }
    {
        auik::Theme rounded_theme;
        rounded_theme.add_style(AUIK_STYLE_TAG_DOCKSPACE_NODE, auik::make_style().border_radius(8.f));
        rounded_theme.add_style(AUIK_STYLE_TAG_DOCKED_WINDOW, auik::make_style().background_color(0xff333333u));
        ctx.theme = &rounded_theme;
        acul::vector<auik::QuadsInstanceData> records;
        auik::DrawStream stream{};
        stream.runtime_data = &records;
        stream.push_data_to_stream = [](auto *stream, const void *data) {
            auto &quads = *static_cast<decltype(records) *>(stream->runtime_data);
            const auto index = static_cast<u32>(quads.size());
            quads.push_back(*static_cast<const auik::QuadsInstanceData *>(data));
            return auik::DrawDataID{index, auik::AUIK_INVALID_DRAW_DATA_ID};
        };
        stream.update_data_in_stream = [](auto *stream, auik::DrawDataID id, const void *data) {
            (*static_cast<decltype(records) *>(stream->runtime_data))[id.render_id] =
                *static_cast<const auik::QuadsInstanceData *>(data);
        };
        stream.invalidate_data_in_stream = [](auto *, auik::DrawDataID) {};
        streams[AUIK_PRIMARY_QUAD_STREAM] = &stream;
        auto *dock = auik::make_dockspace(9010);
        dock->set_policy_flags(auik::DockspaceFlagBits::none);
        dock->set_layout_size({640.f, 480.f});
        dock->add_window(dock->root_node(), acul::alloc<auik::Window>(
            9011, "", amal::rect{}, auik::WindowFlagBits::none, auik::WidgetFlagBits::visible));
        dock->update_depth({0.f, 1.f});
        dock->update_layout(false);
        auik::DrawCtx draw{auik::DrawReasonBits::record};
        draw.is_hit_allowed = false;
        dock->draw(draw);
        assert(records.size() >= 2u);
        assert(records[0].mask & (AUIK_INVERT_RADIUS_BIT << 20u));
        assert(records[0].background_color == 0u);
        assert(records[1].background_color == 0xff333333u && records[0].z_order > records[1].z_order);
        dock->draw(draw);
        size_t mask_count = 0u;
        for (const auto &record : records)
            if (record.mask & (AUIK_INVERT_RADIUS_BIT << 20u)) ++mask_count;
        assert(mask_count == 1u);
        acul::release(dock);

        constexpr u32 dock_color = 0xff414141u;
        constexpr u32 panel_color = 0xff1e1e1eu;
        rounded_theme.add_style(AUIK_STYLE_TAG_DOCKSPACE, auik::make_style().background_color(dock_color));
        rounded_theme.add_style(AUIK_STYLE_TAG_DOCK_NODE_TAB_PANEL,
                                auik::make_style().background_color(panel_color).padding({0.f, 4.f, 0.f, 4.f}));
        // Tab panels may invalidate text records even for empty labels.
        auik::DrawStream unused_stream{};
        for (auto *&entry : streams)
            if (!entry) entry = &unused_stream;
        records.clear();
        dock = auik::make_dockspace(9020);
        dock->set_policy_flags(auik::DockspaceFlagBits::tabpanel);
        dock->set_split_axis(dock->root_node(), amal::axis::x);
        dock->set_layout_size({640.f, 480.f});
        const auto viewport = dock->create_leaf(dock->root_node(),
            auik::make_dockspace_settings(auik::DockspaceResizeFlagBits::none, auik::TabbarFlagBits::none, false));
        const auto panels = dock->create_split(dock->root_node(), amal::axis::y);
        const auto top = dock->create_leaf(panels);
        const auto bottom = dock->create_leaf(panels);
        for (const auto node : {viewport, top, bottom})
            dock->add_window(node, auik::make_dock_window(9021u + node, ""));
        dock->update_depth({0.f, 1.f});
        dock->update_layout(false);
        dock->draw(draw);

        size_t masks = 0u;
        size_t panel_backgrounds = 0u;
        f32 nearest_panel_depth = 0.f;
        for (const auto &record : records)
        {
            if (record.mask & (AUIK_INVERT_RADIUS_BIT << 20u))
            {
                assert(record.background_color == dock_color);
                ++masks;
            }
            else if (record.background_color == dock_color)
            {
                // Dock background strips stay in the gaps between the occupied leaves.
                for (const auto &mask : records)
                {
                    if (!(mask.mask & (AUIK_INVERT_RADIUS_BIT << 20u))) continue;
                    const auto bounds = mask.rect;
                    const auto end = record.rect.offset + record.rect.size;
                    const auto bounds_end = bounds.offset + bounds.size;
                    assert(end.x <= bounds.offset.x || end.y <= bounds.offset.y ||
                           record.rect.offset.x >= bounds_end.x || record.rect.offset.y >= bounds_end.y);
                }
            }
            else if (record.background_color == panel_color)
            {
                assert(record.z_order > dock->get_rect().depth);
                nearest_panel_depth = amal::max(nearest_panel_depth, record.z_order);
                ++panel_backgrounds;
            }
        }
        assert(masks == 3u && panel_backgrounds == 2u);
        for (const auto &record : records)
            if (record.mask & (AUIK_INVERT_RADIUS_BIT << 20u)) assert(record.z_order > nearest_panel_depth);
        acul::release(dock);
        for (auto *&entry : streams) entry = nullptr;
        ctx.theme = &theme;
    }
    auik::detail::g_context = nullptr;
    puts("PASS: tree order, stable IDs, detach/reset; 100 container lifetime cycles");
}
