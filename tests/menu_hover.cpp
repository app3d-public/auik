#include <auik/auik.hpp>
#include "focus_helpers.hpp"
#include <auik/pipelines.hpp>
#include <auik/widgets/menu.hpp>
#include <auik/widgets/combobox.hpp>
#include <auik/widgets/detail/scrollbar.hpp>
#include <auik/widgets/titlebar.hpp>
#include <auik/widgets/window.hpp>
#include <cassert>
#include <cstdio>

namespace
{
    constexpr u32 normal_color = 0u, hover_color = 0xff654321u;
    acul::vector<auik::QuadsInstanceData> records;
    acul::vector<auik::detail::RectData> hits;
    acul::vector<amal::vec4> clips;
    struct Root : auik::Widget
    {
        Root() : Widget(9400, auik::WidgetFlagBits::visible | auik::WidgetFlagBits::attachable) {}
        auik::Widget *owner = nullptr;
        auik::StyleUpdateFlags update_style() override { return {}; }
        void reset_draw_records() override { Widget::reset_draw_records(); if (owner) owner->reset_draw_records(); }
        void reset_clip_rect_records() override { Widget::reset_clip_rect_records(); if (owner) owner->reset_clip_rect_records(); }
        void rebuild_clip_rects() override
        {
            ensure_own_clip_rect({0, 0, 800, 600});
            if (owner) { owner->set_clip_id(clip_id()); owner->rebuild_clip_rects(); }
        }
        void draw(auik::DrawCtx &ctx) override { if (owner) owner->draw_local(ctx); }
    };
    struct Panel : auik::Widget
    {
        auik::DrawDataID draw_id;
        Panel() : Widget(9450, auik::WidgetFlagBits::visible | auik::WidgetFlagBits::hittable |
                               auik::WidgetFlagBits::attachable) {}
        auik::StyleUpdateFlags update_style() override { return {}; }
        void inject_parent(auik::Widget *widget) { _parent = widget; }
        void draw(auik::DrawCtx &ctx) override
        {
            auik::QuadsInstanceData data{};
            data.rect = bounds();
            data.z_order = get_z_order();
            data.background_color = 0xff999999u;
            data.mask = clip_id();
            auik::emit_quads_instance(ctx, auik::get_primary_quads_stream(), draw_id, data, get_rect(), true, true);
        }
    };
    struct ContextWindow : auik::Window
    {
        u32 opens = 0;
        ContextWindow() : Window(9490, "", {{0, 0}, {300, 200}}, auik::WindowFlagBits::none,
                                 auik::WidgetFlagBits::visible | auik::WidgetFlagBits::hittable |
                                 auik::WidgetFlagBits::attachable) {}
        bool open_context_menu(const amal::vec2 &) override { ++opens; return true; }
        void on_click(auik::MouseKey, auik::KeyPressState, u32) override {}
        void on_drag(const amal::vec2 &, auik::KeyPressState) override {}
    };
    bool covers(const auik::QuadsInstanceData &data, amal::vec2 point)
    {
        return point.x >= data.rect.offset.x && point.y >= data.rect.offset.y &&
               point.x < data.rect.offset.x + data.rect.size.x && point.y < data.rect.offset.y + data.rect.size.y;
    }
    void flush(auik::detail::Context &ctx)
    {
        for (int wave = 0; wave < 10 && !ctx.disposal_queue.is_main_queue_empty(); ++wave)
            ctx.disposal_queue.flush_main_queue();
        assert(ctx.disposal_queue.is_main_queue_empty());
    }
}

// Both suites use the same GPU stub; each runs independently.
static void run_popup_tests(bool comboboxes)
{
    using namespace auik;
    detail::Context ctx{};
    detail::WindowContext window_context{};
    window_context.set_cursor = [](CursorID::enum_type, detail::WindowContext *) {};
    ctx.window_ctx = &window_context;
    detail::GPUContext gpu{};
    Viewport viewport{};
    viewport.rect = {{0, 0}, {800, 600}};
    Theme theme;
    theme.add_style(AUIK_STYLE_TAG_GLOBAL, make_style().min_width(80.f).min_height(20.f));
    theme.add_style(AUIK_STYLE_TAG_MENU_POPUP, make_style().background_color(0xff191919u));
    for (u32 tag : {AUIK_STYLE_TAG_COMBO_BOX_ITEM, AUIK_STYLE_TAG_COMBO_BOX_ITEM_MULTI})
    {
        theme.add_style(tag, make_style().padding({4, 3, 4, 3}).background_color(normal_color));
        theme.add_style(tag, make_style().background_color(hover_color), StyleState::hover);
    }
    ctx.main_viewport = &viewport;
    ctx.gpu_ctx = &gpu;
    ctx.theme = &theme;
    ctx.frames_in_flight = 1;
    u32 versions[2]{};
    ctx.shared_sync_state[0].buffer_versions = &versions[0];
    ctx.shared_sync_state[1].buffer_versions = &versions[1];
    gpu.push_clip_rect = [](auto *, const amal::vec4 &rect) -> u16 { clips.push_back(rect); return clips.size() - 1; };
    gpu.reset_clip_rects = [](auto *) { clips.clear(); };
    gpu.clear_hit_rects = [](auto *) { hits.clear(); };
    gpu.copy_clip_rects_frame = [](auto *, u32, u32) {};
    gpu.copy_hit_rects_frame = [](auto *, u32, u32) {};
    gpu.update_clip_rect = [](auto *, u16 id, const amal::vec4 &rect) { clips.at(id) = rect; };
    gpu.get_clip_rect = [](auto *, u16 id) { return &clips.at(id); };
    gpu.push_hit_rect = [](auto *, const detail::RectData &rect) -> u32 { hits.push_back(rect); return hits.size() - 1; };
    gpu.update_hit_rect = [](auto *, u32 id, const detail::RectData &rect) { hits.at(id) = rect; };
    DrawStream stream{};
    stream.clear = [](DrawStream *, u32) { records.clear(); };
    ctx.streams.attached_streams = &stream;
    ctx.streams.stream_count = 1;
    stream.push_data_to_stream = [](DrawStream *, const void *data) {
        records.push_back(*static_cast<const QuadsInstanceData *>(data));
        return DrawDataID{static_cast<u32>(records.size() - 1), AUIK_INVALID_DRAW_DATA_ID};
    };
    stream.update_data_in_stream = [](DrawStream *, DrawDataID id, const void *data) {
        records.at(id.render_id) = *static_cast<const QuadsInstanceData *>(data);
    };
    stream.invalidate_data_in_stream = [](DrawStream *, DrawDataID id) { records.at(id.render_id).rect.size = {0, 0}; };
    DrawStream unused{};
    unused.push_data_to_stream = [](DrawStream *, const void *) { return DrawDataID{}; };
    unused.update_data_in_stream = [](DrawStream *, DrawDataID, const void *) {};
    unused.invalidate_data_in_stream = [](DrawStream *, DrawDataID) {};
    acul::vector<DrawStream *> streams(get_default_streams_count(), &unused);
    streams[AUIK_PRIMARY_QUAD_STREAM] = &stream;
    ctx.streams.default_streams = streams.data();
    detail::g_context = &ctx;
    {
        Root root;
        add_widget_to_root(&root);
        root.ensure_own_clip_rect({0, 0, 800, 600});
        if (comboboxes)
        {
            auto popup_style = make_style(Style::extra_storage_size(sizeof(StyleExtraOverflow)));
            popup_style.min_height(30.f).max_height(100.f).overflow_extra({OverflowMode::hidden, OverflowMode::auto_});
            theme.add_style(AUIK_STYLE_TAG_COMBO_BOX_POPUP, popup_style);
            popup_style.destroy_extra();
            theme.add_style(AUIK_STYLE_TAG_SCROLLBAR_TRACK, make_style().width(10.f));
            ContextWindow outside;
            add_widget_to_root(&outside);
            auto check_combo = [&](auto &combo) {
                root.owner = &combo;
                combo.set_parent(&root);
                combo.set_focus_parent(&root);
                combo.set_position({100, 50});
                combo.set_layout_size({160, 24});
                combo.set_clip_id(root.clip_id());
                combo.on_attach();
                combo.update_style_invalidated();
                combo.update_layout(false);
                auto press = [&](ElementID target, amal::vec2 point) {
                    window_context.time += 1;
                    ctx.hover_id = target;
                    ctx.io.mouse_pos = point;
                    detail::on_mouse_click_event(MouseKey::left, KeyPressState::press);
                };
                auto release = [&] {
                    detail::on_mouse_click_event(MouseKey::left, KeyPressState::release);
                    flush(ctx);
                };
                press({combo.id(), AUIK_TAG_COMBO_BOX, 0}, {110, 60});
                release();
                assert(combo.is_open() && auik_test::is_in_focus_chain(&combo));
                root.update_draw_commands(DrawReasonBits::record);
                auto first_row_y = [&] {
                    f32 y = 0;
                    bool found = false;
                    for (const auto &hit : hits)
                        if (hit.id == ElementID{combo.id(), AUIK_TAG_COMBO_BOX_ITEM, 0} && hit.bounds.size.y > 0)
                        { y = hit.bounds.offset.y; found = true; }
                    assert(found);
                    return y;
                };
                const f32 initial_row_y = first_row_y();
                ElementID thumb{};
                amal::vec2 center{};
                for (const auto &hit : hits)
                    if (hit.id.widget_id == combo.id() && hit.id.tag_id == AUIK_TAG_SCROLLBAR_THUMB_Y &&
                        hit.bounds.size.x > 0 && hit.bounds.size.y > 0)
                    { thumb = hit.id; center = hit.bounds.offset + hit.bounds.size * 0.5f; }
                assert(thumb && "The real combo popup must emit a scrollbar thumb");
                press(thumb, center);
                ctx.io.mouse_pos += amal::vec2{0, 200};
                detail::on_mouse_move({0, 200});
                release();
                assert(combo.is_open() && auik_test::is_in_focus_chain(&combo));
                root.update_draw_commands(DrawReasonBits::record);
                assert(first_row_y() < initial_row_y && "Thumb dragging must actually scroll the popup content");
                // Preserve the pre-existing policy: an outside scrollbar does not
                // steal keyboard focus, so the combo remains open.
                press({outside.id(), AUIK_TAG_SCROLLBAR_THUMB_Y, 0}, {500, 300});
                release();
                assert(combo.is_open());
                combo.open();
                flush(ctx);
                assert(combo.is_open() && auik_test::is_in_focus_chain(&combo));
                press({}, {700, 550});
                release();
                assert(!combo.is_open());
                // Wheel scrolling uses the same routed owner and retains focus.
                combo.open();
                flush(ctx);
                root.update_draw_commands(DrawReasonBits::record);
                const f32 before_wheel = first_row_y();
                combo.on_scroll({0, -1});
                flush(ctx);
                root.update_draw_commands(DrawReasonBits::record);
                assert(combo.is_open() && first_row_y() < before_wheel);
                combo.close();
                // A queued blur cannot close a popup which has regained focus.
                combo.open();
                focus_widget(&outside);
                focus_widget(&combo);
                flush(ctx);
                assert(combo.is_open());
                focus_widget(&outside);
                flush(ctx);
                assert(!combo.is_open());
                combo.on_detach();
                flush(ctx);
                root.owner = nullptr;
                ctx.hover_id = {};
                detail::reset_style_selector();
            };
            acul::vector<StringView> items(40, StringView{""});
            Combobox single(9460, items, 0, {160, 24}, detail::get_combobox_widget_flags());
            check_combo(single);
            MultipleCombobox multiple(9461, items, "", {160, 24}, detail::get_combobox_widget_flags());
            check_combo(multiple);
            remove_widget_from_root_unsync(&outside);
            flush(ctx);
        }
        if (!comboboxes)
        {
            // Both a bare service menu (Attributes) and one appended to custom items (Object Manager).
            for (u32 own_count : {0u, 2u})
            {
                acul::vector<StringView> own_items(own_count, StringView{""});
                PopupMenu owner(9410 + own_count, own_items, detail::get_tabbar_widget_flags(), true);
                root.owner = &owner;
                owner.set_parent(&root);
                owner.set_focus_parent(&root);
                owner.set_position({100, 50});
                owner.set_layout_size({20, 20});
                owner.set_clip_id(root.clip_id());
                owner.on_attach();
                owner.update_style_invalidated();
                owner.update_layout(false);
                auto *menu = owner.menu_model();
                u32 clicked = 0;
                u32 suffix_ids[3];
                for (u32 i = 0; i < 3; ++i)
                    suffix_ids[i] = menu->append_item("", [&clicked, i](ClickEvent &) { clicked = i + 1; })->element_id();
                // The pointer menu reuses panel commands and omits all service commands.
                assert(owner.open_context_at({310, 170}) == (own_count != 0u));
                if (own_count)
                {
                    root.update_draw_commands(DrawReasonBits::record);
                    bool found = false;
                    for (const auto &hit : hits)
                    {
                        if (hit.id.widget_id != menu->id() || hit.id.tag_id != AUIK_TAG_COMBO_BOX_ITEM ||
                            hit.bounds.size.x <= 0 || hit.bounds.size.y <= 0) continue;
                        for (auto suffix_id : suffix_ids) assert(hit.id.element_id != suffix_id);
                        if (hit.id.element_id == (*menu)[0]->element_id())
                        {
                            assert(hit.bounds.offset.x >= 310 && hit.bounds.offset.y >= 170);
                            found = true;
                        }
                    }
                    assert(found);
                    owner.set_open(false);
                    flush(ctx);
                }
                for (u32 index = 0; index < 3; ++index)
                {
                    owner.set_open(true);
                    root.update_draw_commands(DrawReasonBits::record);
                    ElementID target{menu->id(), AUIK_TAG_COMBO_BOX_ITEM, suffix_ids[index]};
                    amal::vec2 center{};
                    bool found = false;
                    for (const auto &hit : hits)
                        if (hit.id == target && hit.bounds.size.x > 0 && hit.bounds.size.y > 0)
                        { center = hit.bounds.offset + hit.bounds.size * 0.5f; found = true; }
                    assert(found);
                    const auto previous = ctx.hover_id;
                    ctx.hover_id = target;
                    detail::on_hover_id_updated(previous, target);
                    flush(ctx);
                    bool hovered = false;
                    for (const auto &record : records)
                        if (record.background_color == hover_color && covers(record, center)) hovered = true;
                    assert(hovered && "Service item hover must update retained GPU records without a full root redraw");
                    const size_t recorded = records.size();
                    menu->update_draw_commands(DrawReasonBits::external);
                    assert(records.size() == recorded); // Targeted refresh updates IDs, it must not duplicate records.
                    menu->on_click(MouseKey::left, KeyPressState::press, 1);
                    flush(ctx);
                    assert(clicked == index + 1);
                    owner.set_open(false);
                    ctx.hover_id = {};
                    detail::reset_style_selector();
                    flush(ctx);
                }
                owner.on_detach();
                flush(ctx);
                root.owner = nullptr;
            }
            {
                // A main menu owned by an earlier root must emit higher quad/hit depths than a later toolbar.
                Panel panel;
                panel.set_position({0, 30});
                panel.set_layout_size({300, 200});
                panel.set_clip_id(root.clip_id());
                add_widget_to_root(&panel);
                auto *menu = make_titlebar_menubar(9460, {StringView{""}});
                root.owner = menu;
                menu->set_parent(&root);
                menu->set_focus_parent(&root);
                menu->set_popup_parent(&root);
                menu->set_clip_id(root.clip_id());
                menu->set_layout_size({300, 20});
                menu->on_attach();
                auto *group = (*(*menu)[0]->add_group_layer(1u))[0];
                group->add_items({StringView{""}, StringView{""}, StringView{""}});
                menu->update_style_invalidated();
                menu->update_layout(false);
                assert(menu->open_root_at((*menu)[0]->element_id(), {{10, 10}, {80, 20}}));
                root.update_draw_commands(DrawReasonBits::record);
                panel.update_draw_commands(DrawReasonBits::record);
                const ElementID target{menu->id(), AUIK_TAG_COMBO_BOX_ITEM, (*group)[1]->element_id()};
                amal::vec2 center{};
                bool found = false;
                for (const auto &hit : hits)
                    if (hit.id == target && hit.bounds.size.x > 0 && hit.bounds.size.y > 0)
                    { center = hit.bounds.offset + hit.bounds.size * 0.5f; found = true; }
                assert(found);
                ElementID picked{};
                float top_depth = -1;
                for (const auto &hit : hits)
                    if (center.x >= hit.bounds.offset.x && center.y >= hit.bounds.offset.y &&
                        center.x < hit.bounds.offset.x + hit.bounds.size.x &&
                        center.y < hit.bounds.offset.y + hit.bounds.size.y && hit.hit_depth >= top_depth)
                    { picked = hit.id; top_depth = hit.hit_depth; }
                assert(picked == target && "Emitted menu hit rectangles must be above a later root toolbar");
                ctx.hover_id = picked;
                detail::on_hover_id_updated({}, picked);
                flush(ctx);
                const QuadsInstanceData *front = nullptr;
                for (const auto &record : records)
                    if (covers(record, center) && (!front || record.z_order >= front->z_order)) front = &record;
                assert(front && front->background_color == hover_color);
                for (u32 cycle = 0; cycle < 5; ++cycle)
                {
                    MenuBar::MenuGroupLayer commands(menu->menu_base());
                    auto *context_group = commands.add_group();
                    for (auto *item : group->items()) menu->menu_base()->add_existing_item(context_group, item);
                    assert(menu->open_root_items_at({{320.f + cycle * 10.f, 220.f}, {0, 0}}, &commands));
                    flush(ctx);
                    bool reopened = false;
                    for (const auto &hit : hits)
                        if (hit.id == target && hit.bounds.size.x > 0 && hit.bounds.size.y > 0 &&
                            hit.bounds.offset.x >= 320.f)
                            reopened = true;
                    assert(reopened && "A viewport context menu must remain visible when reopened at another position");
                }
                menu->close_all();
                menu->on_detach();
                acul::release(menu);
                root.owner = nullptr;
                ctx.hover_id = {};
                detail::reset_style_selector();
                remove_widget_from_root_unsync(&panel);
                ctx.disposal_queue.discard();
            }
            {
                // A context menu uses sentinel 0 for its root path. Reusing a popup
                // containing separators must not make a separator its own owner.
                PopupMenu owner(9470, acul::vector<StringView>{StringView{""}, StringView{""}}, detail::get_tabbar_widget_flags(), false);
                root.owner = &owner;
                owner.set_parent(&root);
                owner.set_focus_parent(&root);
                owner.set_clip_id(root.clip_id());
                owner.set_position({100, 50});
                owner.set_layout_size({20, 20});
                owner.on_attach();
                auto *menu = owner.menu_model();
                auto *geometry = (*menu)[0];
                auto *create = (*menu)[1];
                auto *geometry_groups = geometry->add_group_layer(2);
                (*geometry_groups)[0]->add_items({StringView{""}, StringView{""}});
                auto *nested = (*geometry_groups)[1]->add_item("");
                auto *nested_leaf = (*nested->add_group_layer())[0]->add_item("");
                auto *create_leaf = (*create->add_group_layer())[0]->add_item("");
                owner.update_style_invalidated();
                owner.update_layout(false);
                auto hover = [&](MenuBar::MenuItem *item, MenuBar::MenuItem *expected) {
                    ctx.hover_id = make_element_id(menu->id(), AUIK_TAG_COMBO_BOX_ITEM, item->element_id());
                    menu->on_hover(HoverState::enter);
                    flush(ctx);
                    root.update_draw_commands(DrawReasonBits::record);
                    bool found = false;
                    for (const auto &hit : hits)
                        if (hit.id == make_element_id(menu->id(), AUIK_TAG_COMBO_BOX_ITEM, expected->element_id()) &&
                            hit.bounds.size.x > 0 && hit.bounds.size.y > 0) found = true;
                    assert(found && "Switching a context submenu must retain a valid visible popup");
                };
                for (int cycle = 0; cycle < 20; ++cycle)
                {
                    assert(owner.open_context_at({310, 170}));
                    root.update_draw_commands(DrawReasonBits::record);
                    hover(geometry, nested);
                    hover(nested, nested_leaf);
                    hover(create, create_leaf);
                    hover(geometry, nested);
                    owner.set_open(false);
                    flush(ctx);
                }
                auto row_bounds = [&](MenuBar::MenuItem *item) {
                    amal::rect result{};
                    for (const auto &hit : hits)
                        if (hit.id == make_element_id(menu->id(), AUIK_TAG_COMBO_BOX_ITEM, item->element_id()) &&
                            hit.bounds.size.x > 0 && hit.bounds.size.y > 0) result = hit.bounds;
                    assert(result.size.x > 0);
                    return result;
                };
                for (const f32 x : {10.0f, 350.0f, 780.0f})
                {
                    assert(owner.open_context_at({x, 170}));
                    root.update_draw_commands(DrawReasonBits::record);
                    hover(geometry, nested);
                    const auto parent = row_bounds(nested);
                    hover(nested, nested_leaf);
                    const auto child = row_bounds(nested_leaf);
                    if (x > 700.0f)
                        assert(child.offset.x + child.size.x <= parent.offset.x + 2.0f &&
                               "A left-opening cascade must continue left instead of covering its root");
                    else
                        assert(child.offset.x >= parent.offset.x + parent.size.x - 2.0f &&
                               "A right-opening cascade must continue right while space is available");
                    assert(child.offset.x >= 0 && child.offset.x + child.size.x <= 800);
                    owner.set_open(false);
                    flush(ctx);
                }
                assert(owner.open_context_at({310, 590}));
                root.update_draw_commands(DrawReasonBits::record);
                const auto anchor = row_bounds(geometry);
                hover(geometry, nested);
                const auto first = row_bounds((*(*geometry_groups)[0])[0]);
                const auto last = row_bounds(nested);
                assert(first.offset.y < anchor.offset.y && "Move a fitting submenu up instead of shortening it");
                assert(first.offset.y >= 0 && last.offset.y + last.size.y <= 600);
                owner.set_open(false);
                flush(ctx);
                owner.on_detach();
                flush(ctx);
                root.owner = nullptr;
                ctx.hover_id = {};
                detail::reset_style_selector();
            }
            {
                Panel orphan, first, second;
                Window overlay(9480, "", {{0, 0}, {80, 40}}, WindowFlagBits::none, WidgetFlagBits::visible);
                add_overlay_window(&overlay, &orphan);
                assert(!is_overlay_window(&overlay));
                first.inject_parent(&second);
                second.inject_parent(&first);
                add_overlay_window(&overlay, &first);
                assert(!is_overlay_window(&overlay));
                first.inject_parent(nullptr);
                second.inject_parent(nullptr);
                first.set_focus_parent(&second);
                second.set_focus_parent(&first);
                add_overlay_window(&overlay, &first);
                assert(!is_overlay_window(&overlay));
                first.set_focus_parent(nullptr);
                second.set_focus_parent(nullptr);
                add_overlay_window(&overlay, &root);
                assert(is_overlay_window(&overlay));
                remove_overlay_window(&overlay);
            }
            {
                ContextWindow target;
                add_widget_to_root(&target);
                target.add_event_flags(EventFlagBits::click | EventFlagBits::drag);
                ctx.hover_id = {target.id(), AUIK_TAG_WINDOW, 0u};
                auto click = [&](KeyMode mods, f32 movement) {
                    ctx.io.active_mods = mods;
                    ctx.io.mouse_pos = {100, 100};
                    detail::on_mouse_click_event(MouseKey::right, KeyPressState::press);
                    if (movement != 0.f)
                    {
                        ctx.io.mouse_pos.x += movement;
                        detail::on_mouse_move({movement, 0});
                    }
                    detail::on_mouse_click_event(MouseKey::right, KeyPressState::release);
                };
                click({}, 0);
                assert(target.opens == 1);
                click(KeyModeBits::num_lock | KeyModeBits::caps_lock, 0);
                assert(target.opens == 2 && "Lock keys must not suppress a context menu");
                click({}, 2);
                assert(target.opens == 3 && "Pointer jitter must not turn a context click into a drag");
                click({}, 20);
                assert(target.opens == 3 && "A real right-button drag must not open a context menu");
                for (auto mod : {KeyModeBits::shift, KeyModeBits::control, KeyModeBits::alt, KeyModeBits::super})
                    click(mod, 0);
                assert(target.opens == 3 && "Modified gestures must not open a context menu");
                ctx.io.active_mods = {};
                ctx.hover_id = {};
                remove_widget_from_root_unsync(&target);
                flush(ctx);
            }
        }
        remove_widget_from_root_unsync(&root);
        ctx.disposal_queue.discard();
    }
    detail::g_context = nullptr;
    std::puts(comboboxes ? "PASS: combo routing, scroll, focus and outside dismissal" :
                          "PASS: service menu hover/click; titlebar popup emits quad/hit depths above a later root panel");
}

void test_menu_hover() { run_popup_tests(false); }
void test_combobox_focus() { run_popup_tests(true); }
