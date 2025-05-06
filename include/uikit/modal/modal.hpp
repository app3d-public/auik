#ifndef UIKIT_WIDGETS_MODAL_H
#define UIKIT_WIDGETS_MODAL_H

#include <acul/locales.hpp>
#include <awin/popup.hpp>
#include <uikit/button/switch.hpp>
#include "../icon/icon.hpp"
#include "../widget.hpp"

#define UIKIT_EVENT_MODAL_SIGN 0x06505A881306AD91

namespace uikit
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

    class APPLIB_API ModalQueue final : public Widget
    {
    public:
        struct Message
        {
            acul::string header;
            acul::string message;
            acul::vector<std::pair<awin::popup::Buttons, std::function<void()>>> buttons;
            bool prevent_close = false;
        };

        style::ModalQueue style;

        ModalQueue(acul::events::dispatcher *ed) : Widget("modalqueue"), ed(ed) {}

        ~ModalQueue() { ed->unbind_listeners(this); }

        virtual void render() override;

        void push(const Message &message);

        void bind_events();

        bool empty() const { return _messages.empty(); }

        int prevent_close_count() const { return _prevent_close_count; }

    private:
        acul::events::dispatcher *ed;
        acul::map<acul::string, acul::vector<Message>> _messages;
        enum class ChangeState
        {
            Normal,
            Clicked,
            Continuing,
        } state{ChangeState::Normal};
        Switch _switch{_("apply_all")};
        int _prevent_close_count{0};
    };
} // namespace uikit

#endif