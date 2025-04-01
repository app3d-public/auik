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
            ImFont *boldFont;
            ImVec4 backgroundColor;
            ImVec4 flipColor;
            ImVec2 padding;
            f32 width;
            f32 borderSize;
            ImVec4 borderColor;
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
            bool preventClose = false;
        };

        style::ModalQueue style;

        ModalQueue(acul::events::dispatcher *ed) : Widget("modalqueue"), ed(ed) {}

        ~ModalQueue() { ed->unbind_listeners(this); }

        virtual void render() override;

        void push(const Message &message);

        void bindEvents();

        bool empty() const { return _messages.empty(); }

        int preventCloseCount() const { return _preventCloseCount; }

    private:
        acul::events::dispatcher *ed;
        acul::map<acul::string, acul::vector<Message>> _messages;
        enum class ChangeState
        {
            normal,
            clicked,
            continuing,
        } state{ChangeState::normal};
        Switch _switch{_("apply_all")};
        int _preventCloseCount{0};
    };
} // namespace uikit

#endif