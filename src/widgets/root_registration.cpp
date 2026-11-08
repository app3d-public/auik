#include <algorithm>
#include <auik/auik.hpp>
#include <auik/detail/root_registration.hpp>
#include <auik/detail/depth.hpp>

namespace auik
{
    namespace
    {
        constexpr u32 runtime_tag = 0xA1760101u;
        struct RuntimeState
        {
            struct Child { Widget *widget; acul::shared_ptr<bool> live; };
            Widget *widget = nullptr;
            RuntimeState *root = nullptr;
            Widget *owner = nullptr;
            detail::GlobalDepthAllocation depth;
            DepthZone zone = DepthZone::work;
            bool configured = false;
            bool registered = false;
            bool pending_attach = false;
            acul::shared_ptr<bool> live = acul::make_shared<bool>(true);
            acul::vector<Child> children;
            size_t child_readers = 0;
            bool children_dirty = false;
        };

        RuntimeState *state(const Widget *widget)
        {
            return widget->get_user_data<RuntimeState>(runtime_tag);
        }

        void compact_children(RuntimeState &root)
        {
            if (root.child_readers || !root.children_dirty) return;
            for (size_t i = 0; i < root.children.size();)
                if (!*root.children[i].live) root.children.erase(root.children.begin() + i);
                else ++i;
            root.children_dirty = false;
        }

        void unlink_registration(RuntimeState &entry)
        {
            if (!entry.registered) return;
            entry.registered = false;
            *entry.live = false;
            detail::release_global_depth(entry.depth);
            if (!entry.owner && !(detail::get_context().dirty_flags & DirtyFlagBits::destroying))
            {
                detail::mark_layout_dirty();
                mark_host_refresh_request();
            }
            if (entry.owner)
            {
                entry.root->children_dirty = true;
                compact_children(*entry.root);
            }
        }

        void attached(Widget *widget)
        {
            auto &entry = *state(widget);
            if (entry.registered) return;
            if (!*entry.live) entry.live = acul::make_shared<bool>(true);
            if (!entry.configured) return;
            if (widget->is_visible()) detail::acquire_global_depth(entry.depth);
            entry.registered = true;
            entry.pending_attach = false;
            if (entry.owner) entry.root->children.push_back({widget, entry.live});
            widget->set_depth_zone(entry.zone);
            if (!entry.owner)
            {
                detail::mark_layout_dirty();
                mark_host_refresh_request();
            }
        }

        void detached(Widget *widget)
        {
            auto &entry = *state(widget);
            detail::dismiss_owned_windows(widget);
            unlink_registration(entry);
            *entry.live = false;
        }

        bool visibility_changed(Widget *widget)
        {
            auto &entry = *state(widget);
            if (widget->is_visible())
            {
                if (entry.registered) detail::acquire_global_depth(entry.depth);
                return false;
            }
            detail::dismiss_owned_windows(widget);
            if (!is_overlay_window(widget))
            {
                detail::release_global_depth(entry.depth);
                return false;
            }
            remove_overlay_window(widget);
            return true;
        }

        RuntimeState &ensure_state(Widget *widget)
        {
            if (auto *entry = state(widget)) return *entry;
            // Registration and lifecycle data belong to the widget, not a second draw tree.
            auto *entry = widget->emplace_user_data_head<RuntimeState>(runtime_tag);
            entry->widget = widget;
            entry->depth.owner = widget;
            entry->depth.update_depth = [](void *owner, const amal::vec2 &range) {
                static_cast<Widget *>(owner)->update_depth(range);
            };
            auto &bind = widget->bind();
            bind.on_attach_fn = attached;
            bind.on_detach_fn = detached;
            bind.on_destroy_fn = detached;
            bind.on_visibility_fn = visibility_changed;
            return *entry;
        }

        void bind_owner_lifecycle(Widget *widget)
        {
            auto &bind = widget->bind();
            // Ancestors only need to dismiss descendants, not their own depth UD.
            if (!bind.on_detach_fn) bind.on_detach_fn = detail::dismiss_owned_windows;
            if (!bind.on_destroy_fn) bind.on_destroy_fn = detail::dismiss_owned_windows;
            if (!bind.on_visibility_fn) bind.on_visibility_fn = [](Widget *owner) {
                if (!owner->is_visible()) detail::dismiss_owned_windows(owner);
                return false;
            };
        }

        RuntimeState &resolve_root(Widget *owner)
        {
            while (true)
            {
                if (auto *entry = state(owner); entry && entry->configured && entry->root) return *entry->root;
                if (owner->parent()) owner = owner->parent();
                else if (owner->focus_parent()) owner = owner->focus_parent();
                else return ensure_state(owner);
            }
        }

        void configure(Widget *widget, RuntimeState &root, Widget *owner, DepthZone zone)
        {
            auto &entry = ensure_state(widget);
            assert(!entry.registered);
            *entry.live = false;
            entry.live = acul::make_shared<bool>(true);
            entry.root = &root;
            entry.owner = owner;
            entry.zone = zone;
            entry.configured = true;
            entry.pending_attach = owner != nullptr;
            if (widget->is_attached()) attached(widget);
        }
    }

    bool is_overlay_window(const Widget *widget)
    {
        const auto *entry = state(widget);
        return entry && entry->owner && (entry->registered || entry->pending_attach);
    }

    void prepare_overlay_window(Widget *widget)
    {
        if (is_overlay_window(widget)) return;
        widget->set_visible();
        widget->sync_widget_flags();
        widget->invalidate_style();
        widget->update_locale();
        widget->update_style_invalidated();
    }

    void add_overlay_window(Widget *widget, Widget *owner, DepthZone zone)
    {
        assert(widget && owner && widget != owner && !widget->parent() && widget->viewport());
        if (is_overlay_window(widget))
        {
            widget->update_depth(widget->depth_range());
            return;
        }
        auto &root = resolve_root(owner);
        auto &ctx = detail::get_context();
        assert(std::find(ctx.widget_tree.begin(), ctx.widget_tree.end(), root.widget) != ctx.widget_tree.end());
        assert(std::find(ctx.widget_tree.begin(), ctx.widget_tree.end(), widget) == ctx.widget_tree.end());
        for (auto *ancestor = owner; ancestor && ancestor != root.widget;)
        {
            bind_owner_lifecycle(ancestor);
            const auto *entry = state(ancestor);
            ancestor = entry && entry->registered && entry->owner ? entry->owner :
                       ancestor->parent() ? ancestor->parent() : ancestor->focus_parent();
        }
        prepare_overlay_window(widget);
        widget->reset_draw_records();
        widget->reset_clip_rect_records();
        configure(widget, root, owner, zone);
        if (!widget->is_attached()) widget->on_attach();
        widget->update_style_invalidated();
        widget->rebuild_clip_rects();
    }

    void remove_overlay_window(Widget *widget)
    {
        if (!is_overlay_window(widget)) return;
        auto &ctx = detail::get_context();
        if (widget->is_transient()) erase_widget_from_transient_cache(widget);
        if (!(ctx.dirty_flags & DirtyFlagBits::destroying)) widget->invalidate_draw_commands(DrawReasonBits::external);
        widget->unset_visible();
        if (widget->is_attached()) widget->on_detach();
        unlink_registration(*state(widget));
        widget->reset_draw_records();
        widget->reset_clip_rect_records();
        if (ctx.dirty_flags & DirtyFlagBits::destroying) return;
        auto removed = [&ctx](u32 id) { return id && ctx.id_map.find(id) == ctx.id_map.end(); };
        if (removed(ctx.focus_id)) ctx.focus_id = 0;
        if (removed(ctx.active_id)) ctx.active_id = 0;
        if (removed(ctx.hover_id.widget_id)) ctx.hover_id = {};
        if (removed(ctx.io.clicked_id.widget_id)) ctx.io.clicked_id = {};
        if (removed(ctx.io.drag_id.widget_id))
        {
            detail::cancel_unbounded_mouse_drag();
            ctx.io.drag_id = {};
            ctx.io.drag_key_flags = {};
        }
    }

    void detail::register_root_widget(Widget *widget, DepthZone zone)
    {
        auto &root = ensure_state(widget);
        configure(widget, root, nullptr, zone);
    }

    void detail::unregister_root_widget(Widget *widget)
    {
        if (auto *entry = state(widget)) unlink_registration(*entry);

    }

    void raise_global_depth(Widget *widget)
    {
        auto *entry = state(widget);
        if (!entry || !entry->registered) return;
        detail::raise_global_depth(entry->depth);
    }

    u32 get_global_depth_index(const Widget *widget)
    {
        const auto *entry = state(widget);
        return entry ? entry->depth.index : global_depth_slot_count;
    }

    void detail::update_registered_depths()
    {
        for (auto *widget : get_context().widget_tree)
        {
            const auto *entry = state(widget);
            if (entry && entry->registered && entry->depth.index < global_depth_slot_count)
                widget->update_depth(global_depth_range(entry->depth.index));
        }
    }

    void detail::dismiss_owned_windows(Widget *owner)
    {
        if (!g_context) return;
        auto &root = resolve_root(owner);
        // Keep slots stable while recursively closing descendants.
        ++root.child_readers;
        const size_t count = root.children.size();
        for (size_t i = 0; i < count; ++i)
        {
            if (!*root.children[i].live) continue;
            auto *widget = root.children[i].widget;
            bool owned = false;
            for (auto *parent = state(widget)->owner; parent;)
            {
                if (parent == owner) { owned = true; break; }
                const auto *entry = state(parent);
                parent = entry && entry->registered && entry->owner ? entry->owner :
                         parent->parent() ? parent->parent() : parent->focus_parent();
            }
            if (owned) remove_overlay_window(widget);
        }
        --root.child_readers;
        compact_children(root);
    }

    void detail::defer_widget_update(Widget *widget, acul::unique_function<void()> update)
    {
        auto &bind = widget->bind();
        if (!bind.pending_update_live || !*bind.pending_update_live)
            bind.pending_update_live = acul::make_shared<bool>(true);
        auto live = bind.pending_update_live;
        get_context().disposal_queue.emplace([live, update = std::move(update)]() mutable {
            if (*live) update();
        });
    }
}
