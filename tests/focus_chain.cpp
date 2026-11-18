#include <auik/auik.hpp>
#include "focus_helpers.hpp"
#include <cassert>

namespace
{
    struct FocusNode : auik::Widget
    {
        int gains = 0, losses = 0;
        bool accepts_press = true;
        explicit FocusNode(u32 id) : Widget(id, auik::WidgetFlagBits::visible | auik::WidgetFlagBits::hittable,
                                            auik::EventFlagBits::focus) {}
        auik::StyleUpdateFlags update_style() override { return {}; }
        void draw(auik::DrawCtx &) override {}
        bool accepts_focus_on_mouse_press(auik::ElementID) const override { return accepts_press; }
        void on_focus(bool value) override
        {
            if (value) ++gains;
            else ++losses;
        }
    };
}

void test_focus_chain()
{
    using namespace auik;
    detail::Context ctx{};
    detail::WindowContext window{};
    Viewport viewport{};
    Theme theme;
    ctx.window_ctx = &window;
    ctx.main_viewport = &viewport;
    ctx.theme = &theme;
    detail::g_context = &ctx;
    {
        FocusNode root(9900), combo(9901), popup(9902), scroll(9903), thumb(9904), item(9905),
                  outside(9906), outside_scroll(9907), textbox(9908), nested(9909);
        combo.set_focus_parent(&root);
        popup.set_focus_parent(&combo);
        scroll.set_focus_parent(&popup);
        thumb.set_focus_parent(&scroll);
        item.set_focus_parent(&popup);
        outside.set_focus_parent(&root);
        outside_scroll.set_focus_parent(&root);
        textbox.set_focus_parent(&root);
        nested.set_focus_parent(&popup);
        scroll.accepts_press = thumb.accepts_press = outside_scroll.accepts_press = false;
        for (Widget *node : {&root, &combo, &popup, &scroll, &thumb, &item,
                             &outside, &outside_scroll, &textbox, &nested})
            ctx.id_map[node->id()] = node;
        auto press = [&](Widget *target) {
            window.time += 1.0;
            ctx.hover_id = target ? target->get_rect().id : ElementID{};
            detail::on_mouse_click_event(MouseKey::left, KeyPressState::press);
            detail::on_mouse_click_event(MouseKey::left, KeyPressState::release);
            ctx.disposal_queue.discard();
        };
        focus_widget(&combo);
        assert(root.gains == 1 && combo.gains == 1 && combo.losses == 0);
        focus_widget(&item);
        assert(combo.losses == 0 && combo.gains == 1);
        assert(popup.gains == 1 && item.gains == 1);
        assert(auik_test::is_in_focus_chain(&combo) && auik_test::is_in_focus_chain(&popup));
        assert(is_widget_focused(&item) && !is_widget_focused(&combo));
        focus_widget(&thumb);
        assert(item.losses == 1 && popup.losses == 0 && combo.losses == 0);
        assert(scroll.gains == 1 && thumb.gains == 1);
        press(&thumb);
        assert(ctx.focus_id == thumb.id() && combo.losses == 0);
        press(&scroll);
        assert(ctx.focus_id == thumb.id() && popup.losses == 0 && combo.losses == 0);
        focus_widget(&combo);
        assert(combo.losses == 0 && combo.gains == 1 && popup.losses == 1);
        press(&outside);
        assert(combo.losses == 1 && root.losses == 0 && ctx.focus_id == outside.id());

        // Technical hits preserve keyboard focus, for popups as well as text editing.
        focus_widget(&combo);
        press(&outside_scroll);
        assert(combo.losses == 1 && ctx.focus_id == combo.id());
        focus_widget(&textbox);
        press(&outside_scroll);
        assert(ctx.focus_id == textbox.id() && textbox.losses == 0);

        focus_widget(&nested);
        const int combo_losses = combo.losses;
        press(&scroll);
        assert(nested.losses == 0 && combo.losses == combo_losses && ctx.focus_id == nested.id());
        focus_widget(&nested);
        press(&outside_scroll);
        assert(nested.losses == 0 && combo.losses == combo_losses && ctx.focus_id == nested.id());
        focus_widget(&combo);
        press(nullptr);
        assert(ctx.focus_id == 0 && !auik_test::is_in_focus_chain(&combo));
        ctx.id_map.clear();
        ctx.disposal_queue.discard();
    }
    detail::g_context = nullptr;
}
