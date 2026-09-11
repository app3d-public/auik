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
    void draw(auik::DrawCtx &) override { drawn_depth = get_z_order(); }
};
struct TallWindow : SizedWindow
{
    using SizedWindow::SizedWindow;
    void update_layout_min_size_force() override { set_required_size({280.f, 220.f}); }
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

struct RemovingDrag : CountedWidget
{
    mutable int boundary_checks = 0;
    RemovingDrag() : CountedWidget(8101) { add_event_flags(auik::EventFlagBits::drag); }
    void on_drag(const amal::vec2 &, auik::KeyPressState) override { auik::detail::get_context().id_map.clear(); }
    bool allows_unbounded_drag() const override
    {
        ++boundary_checks;
        return false;
    }
};

int main()
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
    gpu.get_clip_rect = [](auto *, std::uint16_t id) { return &clips[id]; };
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
            if (entry.second->parent() == space && entry.second->signature() == AUIK_TAG_TABBAR)
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
        space->undock_window(tabs_node, preserved);
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
        ctx.widget_tree.push_back(toolbar);
        auik::rebuild_root_widget_depths();
        toolbar->update_draw_commands();
        auto *dock_context = auik::get_dockspace_context();
        dock_context->drag_window = window;
        dock_context->drag_zones_enabled = true;
        space->update_layout(false);
        // With one leaf and no split handles, the first helper is the left root drop zone.
        space->on_drop({window->id(), AUIK_TAG_WINDOW_HEADER, 0u},
                       {space->id(), AUIK_TAG_DOCKSPACE_RESIZE_HELPER_V, 0u});
        assert(window->parent() == space);
        assert(toolbar->drawn_depth == toolbar->get_z_order());
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
                if (entry.second->parent() == space && entry.second->signature() == AUIK_TAG_TABBAR)
                    return static_cast<auik::Tabbar *>(entry.second);
            return nullptr;
        };
        assert(find_tabbar() && find_tabbar()->movable());
        auto *tabs = acul::alloc<GeometryTabbar>();
        tabs->set_parent(space);
        tabs->set_item_style_tag(7010);
        tabs->set_selected_item_style_tag(7010);
        space->set_layout_size({1000.f, 400.f});
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
        drag->sync_text_presentation_from_value();
        assert(drag->Textbox::value() == "15");
        assert(changes == 0);
        drag->set_value(150.f);
        assert(drag->value() == 100.f && changes == 0);
        drag->set_value_internal(-5.f, true);
        assert(drag->value() == 0.f && drag->last_value() == 100.f && changes == 1);
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
        RemovingDrag widget;
        ctx.io.mouse_down = true;
        ctx.io.clicked_id.widget_id = widget.id();
        ctx.io.active_mouse_buttons = auik::MouseKey::left;
        ctx.id_map.emplace(widget.id(), &widget);
        auik::detail::on_mouse_move({1.f, 0.f});
        assert(ctx.id_map.empty() && widget.boundary_checks == 0);
        ctx.id_map.emplace(widget.id(), &widget);
        ctx.frame_cache.drag_widget_id = widget.id();
        ctx.frame_cache.drag_delta = {1.f, 0.f};
        ctx.frame_cache.changes = auik::detail::FrameChangesBits::drag_delta;
        auik::detail::flush_frame_changes();
        assert(ctx.id_map.empty() && widget.boundary_checks == 0);
        ctx.io.mouse_down = false;
        ctx.io.clicked_id = {};
        ctx.io.drag_id = {};
        ctx.io.active_mouse_buttons = {};
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
    auik::detail::g_context = nullptr;
    puts("PASS: tree order, stable IDs, detach/reset; 100 container lifetime cycles");
}
