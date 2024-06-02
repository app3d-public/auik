#include <imgui/imgui_internal.h>
#include <uikit/modal/modal.hpp>
#include <uikit/text/text.hpp>
#include <window/window.hpp>
#ifdef _WIN32
    // Include windows.h first
    #include <mmsystem.h>
#endif

namespace ui
{
    std::string getBtnName(window::popup::Buttons button,
                           const std::function<std::string(window::popup::Buttons)> &callback)
    {
        if (callback) return callback(button);
        switch (button)
        {
            case window::popup::Buttons::OK:
                return "OK";
            case window::popup::Buttons::Yes:
                return "Yes";
            case window::popup::Buttons::No:
                return "No";
            case window::popup::Buttons::Cancel:
                return "Cancel";
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
        ImGui::SetNextWindowSize({style.width, 0});

        if (ImGui::BeginPopupModal(message.header.c_str(), nullptr,
                                   ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar))
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
            auto icon = message.level == window::popup::Style::Warning ? style.warningIcon : style.errorIcon;
            auto iconPos = ImGui::GetCursorScreenPos() + ImVec2(10, 20);
            icon->render(iconPos);
            pos.x += icon->width() + 40;
            pos.y += 20;
            ImGui::PushFont(style.boldFont);
            ImGui::SetCursorPos(pos);
            Text ht(message.header, true);
            ht.render();
            ImGui::PopFont();

            // Message
            pos.y += ht.height() + 20;
            ImGui::SetCursorPos(pos);
            Text mt(message.message, true);
            mt.render();
            pos.x = ImGui::GetCursorPosX() + 10;
            pos.y += mt.height() + 40;

            // Checkbox
            if (it->second.size() > 1)
            {
                ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, {style::checkbox.spacing, 0});
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                                    {style::checkbox.sizePadding, style::checkbox.sizePadding});
                ImGui::SetCursorPos(pos + ImVec2(0, style::button.padding.y * 0.5f));
                _switch.render();
                ImGui::PopStyleVar(2);
            }

            // Buttons
            ImGui::SetCursorPosY(pos.y);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, style::button.padding);
            ImGui::PushStyleColor(ImGuiCol_Button, {0, 0, 0, 0});
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, style::button.colorActive);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, style::button.colorHovered);
            f32 offsetX{0};
            for (int i = message.buttons.size() - 1; i >= 0; --i)
            {
                bool isLast = i == message.buttons.size() - 1;
                if (isLast) ImGui::PushStyleColor(ImGuiCol_Button, style::button.color);
                std::string name = getBtnName(message.buttons[i].first, _btnLocaleCallback);
                f32 btnWidth = ImGui::CalcTextSize(name.c_str()).x + style::button.padding.x * 2 + 5;
                f32 cursorX = style.width - btnWidth - offsetX - style::button.padding.x;
                offsetX += btnWidth;
                ImGui::SetCursorPosX(cursorX);
                if (ImGui::Button(name.c_str())) action = i;
                if (isLast) ImGui::PopStyleColor();
                if (i > 0) ImGui::SameLine();
            }
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(3);
            if (action != -1)
            {
                auto &pair = message.buttons[action];
                if (pair.first == window::popup::Buttons::Cancel || pair.first == window::popup::Buttons::OK)
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
            window::pushEmptyEvent();
        }
    }

    void ModalQueue::bindEvents()
    {
        using namespace events;
        bindEvent<Event>(this, "notification:msgbox", [this](const Event &e) { push(e.data<ModalQueue::Message>()); });
    }
} // namespace ui