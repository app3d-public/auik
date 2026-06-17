#pragma once

#include <acul/functional/unique_function.hpp>
#include <acul/hash/hashmap.hpp>
#include <acul/pair.hpp>
#include <acul/string/string.hpp>
#include <acul/vector.hpp>
#include "../draw.hpp"
#include "window.hpp"

#define AUIK_TAG_MODAL_QUEUE    0xFBEE6E6Cu
#define AUIK_TAG_MODAL_WINDOW   0x7A0F97D2u
#define AUIK_TAG_MODAL_BACKDROP 0xE3F8B526u
#define AUIK_MODAL_DEFAULT_ICON 0x1C3316A6u
#define AUIK_MODAL_HEADER_ID    0xFA09C0D1u
#define AUIK_MODAL_MESSAGE_ID   0x42185C11u
#define AUIK_MODAL_BATCH_ID     0x1C67D2A9u
#define AUIK_MODAL_BUTTON_ID    0x7AD8C901u
#define AUIK_MODAL_CONTROLS_ID  0x3BE7B6D4u
#define AUIK_MODAL_WINDOW_WIDTH 420.0f

namespace auik
{
    class ModalQueue;

    constexpr inline WidgetFlags get_default_modal_queue_flags()
    {
        return WidgetFlagBits::visible | WidgetFlagBits::attachable | WidgetFlagBits::hittable;
    }

    constexpr inline WindowFlags get_default_modal_window_flags() { return WindowFlagBits::movable; }
    constexpr inline WidgetFlags get_default_modal_window_widget_flags() { return WidgetFlagBits::visible; }

    using ModalButton = acul::pair<acul::string, acul::unique_function<void()>>;

    struct ModalMessage
    {
        acul::string header;
        acul::string message;
        acul::vector<ModalButton> buttons;
        bool prevent_close = false;
        u32 group_id = 0u;
    };

    template <typename... Args>
    acul::vector<ModalButton> make_modal_btn_list(Args &&...args)
    {
        acul::vector<ModalButton> v;
        (v.emplace_back(std::forward<Args>(args)), ...);
        return v;
    }

    class ModalWindow : public Window
    {
    public:
        AUIK_EXPORT ModalWindow(u32 id, acul::string title = "", const amal::rect &bounds = {},
                                WindowFlags window_flags = get_default_modal_window_flags(),
                                WidgetFlags widget_flags = get_default_modal_window_widget_flags(),
                                Widget *parent = nullptr);

        ModalQueue *queue() const { return _queue; }
        void set_on_close(acul::unique_function<void()> fn) { _on_close = std::move(fn); }
        AUIK_EXPORT void close();
        AUIK_EXPORT void update_layout(bool min_size_known) override;

    private:
        friend class ModalQueue;

        void set_queue(ModalQueue *queue) { _queue = queue; }
        void invoke_close_callback();

        ModalQueue *_queue = nullptr;
        acul::unique_function<void()> _on_close = nullptr;
    };

    class ModalQueue final : public Widget
    {
    public:
        AUIK_EXPORT explicit ModalQueue(u32 id, WidgetFlags widget_flags = get_default_modal_queue_flags(),
                                        Widget *parent = nullptr);
        AUIK_EXPORT ~ModalQueue() override;

        AUIK_EXPORT void set_icon(Image *image);
        void set_icon(TextureID texture_id, const amal::vec2 &size,
                      const amal::rect &uv_rect = {{0.0f, 0.0f}, {1.0f, 1.0f}});
        AUIK_EXPORT void set_modal_width(f32 value);
        f32 modal_width() const { return _modal_width; }
        AUIK_EXPORT void push(ModalMessage &&message);
        AUIK_EXPORT void close_all_windows();
        bool empty() const { return _messages.empty(); }
        u32 message_count() const { return static_cast<u32>(_messages.size()); }
        int prevent_close_count() const { return _prevent_close_count; }

        AUIK_EXPORT StyleUpdateFlags update_style() override;
        AUIK_EXPORT void update_layout_min_size() override;
        AUIK_EXPORT void update_layout(bool min_size_known) override;
        AUIK_EXPORT void translate(const amal::vec2 &delta) override;
        AUIK_EXPORT void reset_clip_rect_records() override;
        AUIK_EXPORT void rebuild_clip_rects() override;
        AUIK_EXPORT void reset_draw_records() override;
        AUIK_EXPORT void update_depth(const amal::vec2 &depth_range) override;
        AUIK_EXPORT void back_hit_depth() override;
        AUIK_EXPORT void restore_hit_depth() override;
        AUIK_EXPORT void draw(DrawCtx &ctx) override;
        AUIK_EXPORT void on_click(MouseKey key, KeyPressState state, u32 click_count) override;
        AUIK_EXPORT void on_drag(const amal::vec2 &delta, KeyPressState state) override;
        virtual u32 signature() const override { return AUIK_TAG_MODAL_QUEUE; }

    private:
        friend class ModalWindow;

        struct ModalIcon
        {
            TextureID texture_id{};
            amal::vec2 size{0.0f, 0.0f};
            amal::rect uv_rect{{0.0f, 0.0f}, {1.0f, 1.0f}};
            bool valid = false;
        };

        bool is_attached() const;
        void request_redraw();
        void update_modal_draw_commands(DrawReasonFlags reason);
        void relayout_modal_draw_commands();
        void invalidate_modal_draw_commands();
        void request_modal_rebuild();
        void clear_modal(bool invalidate_draw = true);
        void close_all_windows_now();
        bool remove_modal(ModalWindow *modal, bool invoke_callback = true);
        ModalWindow *active_modal() const { return _modal; }
        void layout_active_modal();
        void update_active_modal_depth();
        void rebuild_modal();
        void apply_button(u32 button_index);
        u32 group_count(u32 group_id) const;
        void decrement_group_count(u32 group_id);

        ModalWindow *_modal = nullptr;
        DrawDataID _backdrop_draw;
        acul::vector<ModalMessage> _messages;
        acul::hashmap<u32, u32> _group_counts;
        ModalIcon _icon{};
        f32 _modal_width = AUIK_MODAL_WINDOW_WIDTH;
        bool _batch_apply = false;
        bool _rebuild_pending = false;
        bool _close_all_pending = false;
        int _prevent_close_count = 0;
    };

    inline ModalWindow *make_modal_window(u32 id, const acul::string &title = "", const amal::rect &bounds = {})
    {
        return acul::alloc<ModalWindow>(id, title, bounds);
    }

    inline ModalQueue *make_modal_queue(u32 id) { return acul::alloc<ModalQueue>(id); }

    namespace streams
    {
        extern AUIK_EXPORT const umbf::streams::Stream modal_queue;
    } // namespace streams
} // namespace auik
