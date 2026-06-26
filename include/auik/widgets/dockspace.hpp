#pragma once

#include "tabbar.hpp"
#include "window.hpp"

#define AUIK_TAG_DOCKSPACE                 0xADEAAAECu
#define AUIK_TAG_DOCKSPACE_NODE            0xFC263E77u
#define AUIK_TAG_DOCKSPACE_RESIZE_HELPER_V 0xDA69EE34u
#define AUIK_TAG_DOCKSPACE_RESIZE_HELPER_H 0x9FBF83ACu
#define AUIK_TAG_DOCKSPACE_TAB_PANEL       0x44306332u
#define AUIK_TAG_DOCKSPACE_MENU_BUTTON     0xF26C91A4u
#define AUIK_TAG_DOCKSPACE_MENU_ITEM       0x7B1C71E2u
#define AUIK_DOCKSPACE_TABBAR_ID_BASE      0x84D64482u
#define AUIK_DOCKSPACE_MENU_ID_BASE        0x2C4F6D10u
#define AUIK_DOCK_NODE_INVALID             0xFFFFFFFFu

namespace auik
{
    class Dockspace;
    struct DockspaceStreamAccess;

    struct DockspaceContext
    {
        acul::vector<Dockspace *> docks;
        Window *drag_window = nullptr;
        bool drag_zones_enabled = false;
        bool drag_zones_dirty = false;
    };

    AUIK_EXPORT DockspaceContext *get_dockspace_context();
    AUIK_EXPORT void destroy_dockspace_context();
    AUIK_EXPORT void register_dockspace(Dockspace *dockspace);
    AUIK_EXPORT void unregister_dockspace(Dockspace *dockspace);
    AUIK_EXPORT void enable_dockspace_drag_zones(Window *window);
    AUIK_EXPORT void disable_dockspace_drag_zones(Window *window, const char *reason = nullptr);

    struct DockspaceFlagBits
    {
        enum enum_type : u32
        {
            none = 0x0,
            resize_helper_x = 0x2,
            resize_helper_y = 0x4,
            visible_resize_helper_x = 0x8,
            visible_resize_helper_y = 0x10,
            addable = 0x20,
            tabpanel = 0x40,
            resize_helper = resize_helper_x | resize_helper_y,
            visible_resize_helper = visible_resize_helper_x | visible_resize_helper_y
        };
        using flag_bitmask = std::true_type;
    };
    using DockspaceFlags = acul::flags<DockspaceFlagBits>;

    using DockNodeID = u32;
    constexpr inline WidgetFlags get_default_dockspace_flags()
    {
        return WidgetFlagBits::visible | WidgetFlagBits::attachable | WidgetFlagBits::hittable |
               WidgetFlagBits::configurable;
    }

    struct DockNodeSettings
    {
        amal::vec2 requested_size{AUIK_SIZE_X_FIT, AUIK_SIZE_Y_FILL};
        amal::vec2 size{0.0f, 0.0f};
        amal::vec2 min_size{80.0f, 80.0f};
        DockspaceFlags flags = DockspaceFlagBits::resize_helper | DockspaceFlagBits::visible_resize_helper |
                               DockspaceFlagBits::addable | DockspaceFlagBits::tabpanel;
        TabBarFlags tabbar_flags = TabBarFlagBits::none;
        amal::vec2 tabbar_size = AUIK_SIZE_FIT;

        DockNodeSettings &enable_tabpanel()
        {
            flags |= DockspaceFlagBits::tabpanel;
            return *this;
        }

        DockNodeSettings &disable_tabpanel()
        {
            flags &= ~DockspaceFlagBits::tabpanel;
            return *this;
        }
    };

    inline DockNodeSettings make_dockspace_settings(
        const amal::vec2 &requested_size = {AUIK_SIZE_X_FIT, AUIK_SIZE_Y_FILL}, const amal::vec2 &size = {0.0f, 0.0f},
        const amal::vec2 &min_size = {80.0f, 80.0f},
        DockspaceFlags flags = DockspaceFlagBits::resize_helper | DockspaceFlagBits::visible_resize_helper |
                               DockspaceFlagBits::addable | DockspaceFlagBits::tabpanel,
        TabBarFlags tabbar_flags = TabBarFlagBits::none, const amal::vec2 &tabbar_size = AUIK_SIZE_FIT)
    {
        DockNodeSettings settings{};
        settings.requested_size = requested_size;
        settings.size = size;
        settings.min_size = min_size;
        settings.flags = flags;
        settings.tabbar_flags = tabbar_flags;
        settings.tabbar_size = tabbar_size;
        return settings;
    }

    class Dockspace final : public Widget
    {
        friend struct DockspaceStreamAccess;

    public:
        using MenuGroup = acul::vector<acul::string>;

        AUIK_EXPORT explicit Dockspace(u32 id, WidgetFlags widget_flags = get_default_dockspace_flags(),
                                       Widget *parent = nullptr, u32 style_tag_id = 0u);
        AUIK_EXPORT ~Dockspace() override;

        DockNodeID root_node() const { return 0u; }
        AUIK_EXPORT DockNodeID create_split(DockNodeID parent, amal::axis axis,
                                            DockNodeSettings settings = DockNodeSettings{});
        AUIK_EXPORT DockNodeID create_leaf(DockNodeID parent, DockNodeSettings settings = DockNodeSettings{});
        AUIK_EXPORT void set_split_axis(DockNodeID node, amal::axis axis);
        AUIK_EXPORT void set_node_settings(DockNodeID node, DockNodeSettings settings);
        AUIK_EXPORT void set_node_tabbar_flags(DockNodeID node, TabBarFlags flags);
        AUIK_EXPORT void set_new_node_settings(DockNodeSettings settings);
        AUIK_EXPORT void update_drag_zones();
        AUIK_EXPORT void add_window(DockNodeID node, Window *window);
        AUIK_EXPORT bool dock_drag_window_to_tab_panel(Window *window, DockNodeID node);
        AUIK_EXPORT void undock_window(DockNodeID node, Window *window);
        AUIK_EXPORT void close_window(DockNodeID node, Window *window);
        AUIK_EXPORT void close_group(DockNodeID node);
        AUIK_EXPORT void request_undock_window(DockNodeID node);
        AUIK_EXPORT void request_close_window(DockNodeID node);
        AUIK_EXPORT void request_close_group(DockNodeID node);
        AUIK_EXPORT void clear_windows(DockNodeID node);
        AUIK_EXPORT void clear();
        AUIK_EXPORT void set_menu_group(MenuGroup group);
        const MenuGroup &menu_group() const { return _menu_group; }

        AUIK_EXPORT StyleUpdateFlags update_style() override;
        AUIK_EXPORT void update_layout_min_size() override;
        AUIK_EXPORT void update_layout(bool min_size_known) override;
        AUIK_EXPORT void translate(const amal::vec2 &delta) override;
        AUIK_EXPORT void reset_clip_rect_records() override;
        AUIK_EXPORT void rebuild_clip_rects() override;
        AUIK_EXPORT void reset_draw_records() override;
        AUIK_EXPORT u32 get_depth_requirement() const override;
        AUIK_EXPORT void update_depth(const amal::vec2 &depth_range) override;
        AUIK_EXPORT void back_hit_depth() override;
        AUIK_EXPORT void restore_hit_depth() override;
        AUIK_EXPORT void draw(DrawCtx &ctx) override;
        AUIK_EXPORT void on_hover(HoverState state) override;
        AUIK_EXPORT void on_click(MouseKey key, KeyPressState state, u32 click_count) override;
        AUIK_EXPORT void on_drag(const amal::vec2 &delta, KeyPressState state) override;
        AUIK_EXPORT void on_drop(ElementID drag_id, ElementID drop_id) override;
        AUIK_EXPORT void on_attach() override;
        AUIK_EXPORT void on_detach() override;
        AUIK_EXPORT bool accepts_drag_hover(ElementID drag_id, ElementID hover_id) const override;
        u16 content_clip_id() const override { return clip_id(); }
        amal::vec4 get_content_clip_rect() const override { return get_clip_rect(clip_id()); }
        virtual u32 signature() const override { return AUIK_TAG_DOCKSPACE; }

    private:
        class DockMenu;

        struct Node
        {
            DockNodeSettings settings{};
            amal::axis axis = amal::axis::x;
            DockNodeID parent = AUIK_DOCK_NODE_INVALID;
            acul::vector<DockNodeID> children;
            acul::vector<Window *> windows;
            acul::vector<amal::rect> undocked_bounds;
            acul::vector<acul::string> tab_titles;
            acul::vector<u32> tab_element_ids;
            TabBar *tabbar = nullptr;
            DockMenu *menu = nullptr;
            detail::RectData tab_panel_rect{};
            DrawDataID tab_panel_draw{};
            amal::rect bounds{};
            amal::rect content_bounds{};
            amal::vec2 required_size{0.0f, 0.0f};
            size_t active_window_index = static_cast<size_t>(-1);
            bool record_active_window = false;
        };

        struct ResizeHelperVisual
        {
            detail::RectData rect{};
            detail::RectData hit_rect{};
            DrawDataID draw{};
            DrawDataID hit_draw{};
            DockNodeID parent = AUIK_DOCK_NODE_INVALID;
            u32 before_child = 0;
            u32 after_child = 0;
            amal::axis axis = amal::axis::x;
            bool visible = false;
            bool interactive = false;
            bool drop_zone = false;
            bool draw_in_overlay = false;
        };

        Node *get_node(DockNodeID node);
        const Node *get_node(DockNodeID node) const;
        DockNodeID create_node(DockNodeID parent, bool split, DockNodeSettings settings);
        void clear_node_windows(Node &node);
        void clear_node_chrome(Node &node);
        void attach_window(Window *window);
        void detach_window(Window *window, const amal::rect *undocked_bounds = nullptr);
        Window *active_window(Node &node);
        Window *extract_window(Node &node, Window *window, amal::rect *out_undocked_bounds = nullptr);
        void remove_empty_node(DockNodeID node);
        void close_menu();
        void toggle_menu(DockNodeID node);
        void queue_menu_action(DockNodeID node, u32 action);
        void execute_menu_action(DockNodeID node, u32 action);
        bool has_node_menu(const Node &node) const;
        void sync_node_menu(DockNodeID node_id, Node &node);
        void draw_node_menu(DrawCtx &ctx, DockNodeID node_id, Node &node);
        bool needs_node_tab_panel(const Node &node) const;
        amal::vec2 measure_node(DockNodeID node);
        void layout_node(DockNodeID node, const amal::rect &bounds);
        void layout_leaf_window(Node &node, Window *window);
        void layout_leaf_window_fast(Node &node, Window *window);
        bool prepare_active_window(Node &node, bool force_record, bool record_pass);
        bool sync_active_windows(DockNodeID node, bool record_pass);
        void sync_node_tabbar(DockNodeID node_id, Node &node);
        bool fit_node_to_required_width(DockNodeID node, bool allow_shrink);
        void handle_tabbar_changed(DockNodeID node_id);
        bool handle_tabbar_drag_escape(DockNodeID node_id, u32 element_id);
        size_t selected_window_index(const Node &node) const;
        bool subtree_accepts_drop(DockNodeID node) const;
        void begin_resize_helpers();
        amal::rect add_resize_helper(DockNodeID parent, size_t before_child, size_t after_child, amal::axis axis,
                                     const amal::rect &bounds, bool visible, bool interactive, bool drop_zone = false,
                                     const amal::rect *hit_bounds = nullptr);
        void sync_drag_zone_helpers(bool enabled, bool dirty);
        void add_root_drop_helpers();
        void add_vertical_drop_helpers(DockNodeID node);
        void draw_resize_helpers(DrawCtx &ctx);
        void draw_node_tab_panel(DrawCtx &ctx, DockNodeID node_id, Node &node);
        void draw_node(DrawCtx &ctx, DockNodeID node);
        ResizeHelperVisual *resize_helper_from_element(u32 element_id);
        const ResizeHelperVisual *resize_helper_from_element(u32 element_id) const;
        void update_own_layout(bool record = false);

        acul::vector<Node> _nodes;
        acul::vector<ResizeHelperVisual> _resize_helpers;
        acul::vector<f32> _resize_basis;
        size_t _resize_helper_count = 0u;
        size_t _resizing_helper = static_cast<size_t>(-1);
        DockMenu *_deferred_menu_popup_draw = nullptr;
        amal::vec2 _resize_drag_accum{0.0f, 0.0f};
        f32 _resize_helper_depth = 0.0f;
        f32 _tab_panel_depth = 0.0f;
        amal::vec2 _tabbar_depth_range{0.0f, 0.0f};
        DockNodeID _open_menu_node = AUIK_DOCK_NODE_INVALID;
        bool _drag_zones_dirty = true;
        u32 _style_tag_id = 0u;
        StyleSelector _style{Theme::STYLE_ID_INVALID, 0u};
        StyleSelector _resize_helper_style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_DOCKSPACE_RESIZE_HELPER};
        StyleSelector _resize_helper_drag_style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_DOCKSPACE_RESIZE_HELPER_DRAG};
        StyleSelector _tab_panel_style{Theme::STYLE_ID_INVALID, AUIK_STYLE_TAG_DOCK_NODE_TAB_PANEL};
        DockNodeSettings _new_node_settings =
            make_dockspace_settings(AUIK_SIZE_FILL, {0.0f, 0.0f}, {80.0f, 80.0f},
                                    DockspaceFlagBits::resize_helper | DockspaceFlagBits::visible_resize_helper |
                                        DockspaceFlagBits::addable | DockspaceFlagBits::tabpanel,
                                    TabBarFlagBits::none);
        MenuGroup _menu_group;
    };

    inline Dockspace *make_dockspace(u32 id, Widget *parent = nullptr)
    {
        return acul::alloc<Dockspace>(id, get_default_dockspace_flags(), parent);
    }

    inline Window *make_dock_window(u32 id, Dockspace *dockspace, StringView title = "",
                                    const amal::rect &bounds = {},
                                    WidgetFlags widget_flags = get_default_widget_flags() | WidgetFlagBits::hittable,
                                    Widget *parent = nullptr)
    {
        auto *window = make_decorated_window(id, title, bounds, widget_flags, parent ? parent : dockspace);
        window->window_flags |= WindowFlagBits::docked;
        return window;
    }

    namespace streams
    {
        extern AUIK_EXPORT const umbf::streams::Stream dockspace;
    }
} // namespace auik
