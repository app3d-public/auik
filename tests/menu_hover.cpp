#include <auik/auik.hpp>
#include <auik/pipelines.hpp>
#include <auik/widgets/menu.hpp>
#include <auik/widgets/titlebar.hpp>
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

void test_menu_hover()
{
    using namespace auik;
    detail::Context ctx{};
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
            menu->close_all();
            menu->on_detach();
            acul::release(menu);
            root.owner = nullptr;
            ctx.hover_id = {};
            detail::reset_style_selector();
            remove_widget_from_root_unsync(&panel);
            ctx.disposal_queue.discard();
        }
        remove_widget_from_root_unsync(&root);
        ctx.disposal_queue.discard();
    }
    detail::g_context = nullptr;
    std::puts("PASS: service menu hover/click; titlebar popup emits quad/hit depths above a later root panel");
}
