#ifndef AUIK_WIDGETS_MODAL_H
#define AUIK_WIDGETS_MODAL_H

#include <acul/locales/locales.hpp>
#include <auik/button/switch.hpp>
#include <awin/popup.hpp>
#include "../icon/icon.hpp"
#include "../widget.hpp"

#define AUIK_EVENT_MODAL_SIGN 0x06505A881306AD91

namespace auik
{
    namespace style
    {
        struct ModalQueue
        {
            ImFont *bold_font;
            ImVec4 background_color;
            ImVec4 flip_color;
            ImVec2 padding;
            f32 width;
            f32 border_size;
            ImVec4 border_color;
            Icon *icon = nullptr;
        };
    } // namespace style

    using ModalBtn = acul::pair<awin::popup::Buttons, acul::unique_function<void()>>;
    struct ModalMessage
    {
        acul::string header;
        acul::string message;
        acul::vector<acul::pair<awin::popup::Buttons, acul::unique_function<void()>>> buttons;
        bool prevent_close = false;
    };

    template <typename... Args>
    acul::vector<ModalBtn> make_modal_btn_list(Args &&...args)
    {
        acul::vector<ModalBtn> v;
        (v.emplace_back(std::forward<Args>(args)), ...);
        return v;
    }

    using ModalEvent = acul::events::data_event<ModalMessage>;

    class APPLIB_API ModalQueue final : public Widget
    {
    public:
        style::ModalQueue style;

        ModalQueue(acul::events::dispatcher *ed, f32 dpi) : Widget("modalqueue"), ed(ed), _switch(_("apply_all"), dpi)
        {
        }

        ~ModalQueue() { ed->unbind_listeners(this); }

        virtual void render() override;

        void push(ModalMessage &&message);

        void bind_events();

        bool empty() const { return _messages.empty(); }

        int prevent_close_count() const { return _prevent_close_count; }

    private:
        acul::events::dispatcher *ed;
        acul::hashmap<acul::string, acul::vector<ModalMessage>> _messages;
        enum class ChangeState
        {
            normal,
            clicked,
            continuing,
        } state{ChangeState::normal};
        Switch _switch;
        int _prevent_close_count{0};
    };
} // namespace auik

#endif
