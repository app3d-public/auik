#ifndef UIKIT_WIDGETS_MODAL_H
#define UIKIT_WIDGETS_MODAL_H

#include <core/event.hpp>
#include <core/locales.hpp>
#include <functional>
#include <string>
#include <uikit/button/switch.hpp>
#include <window/popup.hpp>
#include "../icon/icon.hpp"
#include "../widget.hpp"

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
            std::string header;
            std::string message;
            astl::vector<std::pair<window::popup::Buttons, std::function<void()>>> buttons;
            bool preventClose = false;
        };

        style::ModalQueue style;

        ModalQueue(events::Manager *e) : Widget("modalqueue"), e(e) {}

        ~ModalQueue() { e->unbindListeners(this); }

        virtual void render() override;

        void push(const Message &message);

        void bindEvents();

        bool empty() const { return _messages.empty(); }

        int preventCloseCount() const { return _preventCloseCount; }

    private:
        events::Manager *e;
        astl::map<std::string, astl::vector<Message>> _messages;
        enum class ChangeState
        {
            normal,
            clicked,
            continuing,
        } state{ChangeState::normal};
        Switch _switch{_("UI:Switch:All")};
        int _preventCloseCount{0};
    };
} // namespace uikit

#endif