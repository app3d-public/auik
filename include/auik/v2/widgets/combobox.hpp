#pragma once

#include "../post_effects.hpp"
#include "../theme.hpp"
#include "widget.hpp"

#define AUIK_TAG_COMBO_BOX       0x48F5BBCAu
#define AUIK_TAG_COMBO_BOX_ICON  0xAF09C8C6u
#define AUIK_TAG_COMBO_BOX_POPUP 0xC94B7B61u
#define AUIK_TAG_COMBO_BOX_ITEM  0xF2B7E06Eu

namespace auik::v2
{
    class Text;
    class Window;

    constexpr inline WidgetFlags get_default_combo_box_flags()
    {
        return get_default_widget_flags() | WidgetFlagBits::hittable;
    }
    constexpr inline WidgetFlags get_default_fixed_combo_box_flags()
    {
        return get_default_combo_box_flags() | WidgetFlagBits::fixed;
    }

    class APPLIB_API ComboBox final : public Widget
    {
    public:
        ComboBox(u32 id, acul::vector<acul::string> items = {}, u32 selected_index = 0u,
                 amal::vec2 size = {0.0f, 0.0f}, WidgetFlags widget_flags = get_default_combo_box_flags(),
                 Widget *parent = nullptr);
        ~ComboBox() override;

        StyleUpdateFlags update_style() override;
        void update_layout_min_size() override;
        void update_layout(bool min_size_known) override;
        void translate(const amal::vec2 &delta) override;
        void rebuild_clip_rects() override;
        void update_depth(const amal::vec2 &depth_range) override;
        void draw(DrawCtx &ctx) override;
        void on_focus(bool focused) override;
        void on_click(MouseKey key, KeyPressState state, u32 click_count) override;

        acul::vector<acul::string> items() const;
        void set_items(const acul::vector<acul::string>& items);

        u32 selected_index() const { return _selected_index; }
        const acul::string &selected_text() const;
        void set_selected_index(u32 index);

        bool is_open() const { return _open; }
        void open();
        void close();
        void toggle();

    private:
        void ensure_icon_resources();
        amal::vec2 resolve_icon_size() const;
        void rebuild_control_layout();
        void sync_label_text();
        void update_popup_layout();
        void start_icon_animation(bool opening);
        void schedule_icon_tick();
        void tick_icon_animation();
        void schedule_outside_click_tick();
        void tick_outside_click();
        bool has_draw_record() const;

        u32 _selected_index = 0u;
        bool _open = false;

        StyleSelector _style{Theme::STYLE_ID_INVALID, AUIK_TAG_COMBO_BOX};
        DrawDataID _bg_draw{};
        DrawDataID _icon_draw{};
        DrawDataID _animated_icon_draw{};
        Text *_label = nullptr;
        Window *_popup = nullptr;

        TextureID _icon_texture{};
        amal::rect _icon_uv_rect{{0.0f, 0.0f}, {1.0f, 1.0f}};
        amal::vec2 _icon_size{0.0f, 0.0f};
        amal::rect _label_rect{};
        amal::rect _icon_rect{};
        detail::RectData _icon_hit_rect{};
        amal::vec2 _bg_depth_range{0.0f, 1.0f};
        amal::vec2 _content_depth_range{0.0f, 1.0f};

        u32 _rotate_post_id = AUIK_INVALID_POST_EFFECT_DATA_ID;
    };

    inline ComboBox *make_combo_box(u32 id, acul::vector<acul::string> items = {}, u32 selected_index = 0u)
    {
        return acul::alloc<ComboBox>(id, std::move(items), selected_index, amal::vec2{0.0f, 0.0f},
                                     get_default_combo_box_flags());
    }

    inline ComboBox *make_fixed_combo_box(u32 id, acul::vector<acul::string> items = {}, u32 selected_index = 0u,
                                          amal::vec2 size = {0.0f, 0.0f})
    {
        return acul::alloc<ComboBox>(id, std::move(items), selected_index, size, get_default_fixed_combo_box_flags());
    }
} // namespace auik::v2
