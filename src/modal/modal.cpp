#include <uikit/button/button.hpp>
#include <uikit/button/checkbox.hpp>
#include <uikit/modal/modal.hpp>
#include <uikit/text/text.hpp>
#ifdef _WIN32
    // Include playsoundapi.h first
    #include <playsoundapi.h>
#endif

namespace uikit
{
    acul::string getBtnName(awin::popup::Buttons button)
    {
        switch (button)
        {
            case awin::popup::Buttons::OK:
                return _("ok");
            case awin::popup::Buttons::Yes:
                return _("yes");
            case awin::popup::Buttons::No:
                return _("no");
            case awin::popup::Buttons::Cancel:
                return _("cancel");
            default:
                return "Unknown";
        };
    }

    void ModalQueue::push(const Message &message)
    {
        _messages[message.header].push_back(message);
        if (message.preventClose) ++_preventCloseCount;
    }

    void ModalQueue::render()
    {
        if (_messages.empty()) return;

        const auto &it = _messages.begin();
        auto &message = it->second.front();
        ImGui::OpenPopup(message.header.c_str());

        // Style
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, style.padding);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, style.borderSize);
        ImGui::PushStyleColor(ImGuiCol_Border, style.borderColor);

        bool wasChanged{false};
        if (state == ChangeState::normal)
            ImGui::PushStyleColor(ImGuiCol_PopupBg, style.backgroundColor);
        else
        {
            ImGui::PushStyleColor(ImGuiCol_PopupBg, style.flipColor);
            if (state == ChangeState::clicked)
            {
                wasChanged = true;
                state = ChangeState::continuing;
#ifdef _WIN32
                PlaySound(TEXT("SystemHand"), NULL, SND_ALIAS | SND_ASYNC);
#endif
            }
            else
                state = ChangeState::normal;
        }

        ImGui::SetNextWindowSize(ImVec2(style.width, 0));
        if (ImGui::BeginPopupModal(message.header.c_str(), nullptr,
                                   ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize))
        {
            int action{-1};
            if (ImGui::IsMouseClicked(0) && !ImGui::IsWindowHovered())
            {
                state = ChangeState::clicked;
                wasChanged = true;
            }

            // Rendering
            // Header
            ImVec2 pos = ImGui::GetCursorPos();
            auto iconPos = ImGui::GetCursorScreenPos() + ImVec2(10, 20);
            style.icon->render(iconPos);
            pos.x += style.icon->size().x + 40;
            pos.y += 20;
            ImGui::PushFont(style.boldFont);
            ImGui::SetCursorPos(pos);
            Text ht(message.header, true);
            ht.render();
            ImGui::PopFont();

            // Message
            ImGui::PushTextWrapPos(style.width);
            pos.y += ht.height() + 20;
            ImGui::SetCursorPos(pos);
            Text mt(message.message, true);
            mt.render();
            pos.x = ImGui::GetCursorPosX() + 10;
            pos.y += mt.height() + 20;
            ImGui::PopTextWrapPos();

            // Checkbox
            if (it->second.size() > 1)
            {
                ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, {style::g_CheckBox.spacing, 0});
                ImGui::SetCursorPos(pos + ImVec2(0, style::g_Button.padding.y * 0.5f));
                _switch.render();
                ImGui::PopStyleVar();
            }

            // Buttons
            acul::vector<acul::string> buttons(message.buttons.size());
            for (int i = 0; i < message.buttons.size(); ++i) buttons[i] = getBtnName(message.buttons[i].first);
            rightControls(buttons, &action, pos.y);

            if (action != -1)
            {
                auto &pair = message.buttons[action];
                if (pair.first == awin::popup::Buttons::Cancel || pair.first == awin::popup::Buttons::OK)
                {
                    if (pair.second) pair.second();
                    if (message.preventClose) _preventCloseCount -= _messages[it->first].size();
                    _messages.erase(it->first);
                }
                else
                {
                    auto &arr = it->second;
                    if (_switch.toogled())
                    {
                        for (auto &msg : arr)
                            if (auto callback = msg.buttons[action].second) callback();
                        if (message.preventClose) _preventCloseCount -= arr.size();
                        _messages.erase(it->first);
                    }
                    else
                    {
                        if (pair.second) pair.second();
                        if (message.preventClose) --_preventCloseCount;
                        arr.erase(arr.begin());
                        if (arr.empty()) _messages.erase(it->first);
                    }
                }
                _switch.toogled(false);
                wasChanged = true;
            }

            ImGui::EndPopup();
        }
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);
        if (wasChanged)
        {
            ImGui::CloseCurrentPopup();
            awin::pushEmptyEvent();
        }
    }

    void ModalQueue::bindEvents()
    {
        ed->bind_event(this, UIKIT_EVENT_MODAL_SIGN,
                       [this](const acul::events::data_event<ModalQueue::Message> &e) { push(e.data); });
    }
} // namespace uikit