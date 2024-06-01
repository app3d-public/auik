#ifndef UIKIT_WIDGETS_MODAL_H
#define UIKIT_WIDGETS_MODAL_H

#include <core/event/event.hpp>
#include <functional>
#include <string>
#include <window/popup.hpp>
#include "../icon/icon.hpp"
#include "../widget.hpp"

namespace ui
{
    class APPLIB_API ModalQueue : public Widget
    {
    public:
        struct Message
        {
            window::popup::Style level;
            std::string header;
            std::string message;
            DArray<std::pair<window::popup::Buttons, std::function<void()>>> buttons;
            bool preventClose = false;
        };

        struct Style
        {
            ImFont *boldFont;
            ImVec4 backgroundColor;
            ImVec4 flipColor;
            ImVec2 padding;
            f32 width;
            f32 borderSize;
            ImVec4 borderColor;
            std::shared_ptr<Icon> warningIcon;
            std::shared_ptr<Icon> errorIcon;
            style::Button button;
            style::CheckBox checkbox;
        } style;

        ~ModalQueue() { events::unbindListeners(this); }

        virtual void render() override;

        void push(const Message &message);

        void bindLocaleCallback(const std::function<std::string(window::popup::Buttons)> &callback)
        {
            _btnLocaleCallback = callback;
        }

        void bindEvents();

        bool empty() const { return _messages.empty(); }

        int preventCloseCount() const { return _preventCloseCount; }

    private:
        Map<std::string, DArray<Message>> _messages;
        enum class ChangeState
        {
            normal,
            clicked,
            continuing,
        } state{ChangeState::normal};
        bool _applyAll{false};
        int _preventCloseCount{0};

        std::function<std::string(window::popup::Buttons)> _btnLocaleCallback{nullptr};
    };
} // namespace ui

#endif