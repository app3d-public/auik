#include <auik/button/button.hpp>
#include <auik/button/checkbox.hpp>
#include <auik/modal/modal.hpp>
#include <auik/text/text.hpp>
#ifdef _WIN32
    // Include playsoundapi.h first
    #include <playsoundapi.h>
#endif

namespace auik
{
    acul::string get_btn_name(awin::popup::Buttons button)
    {
        switch (button)
        {
            case awin::popup::Buttons::ok:
                return _("ok");
            case awin::popup::Buttons::yes:
                return _("yes");
            case awin::popup::Buttons::no:
                return _("no");
            case awin::popup::Buttons::cancel:
                return _("cancel");
            default:
                return "Unknown";
        };
    }

    void ModalQueue::push(const Message &message)
    {
        _messages[message.header].push_back(message);
        if (message.prevent_close) ++_prevent_close_count;
    }

    void ModalQueue::render()
    {
        if (_messages.empty()) return;

        const auto &it = _messages.begin();
        auto &message = it->second.front();
        ImGui::OpenPopup(message.header.c_str());

        // Style
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, style.padding);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, style.border_size);
        ImGui::PushStyleColor(ImGuiCol_Border, style.border_color);

        bool was_changed{false};
        if (state == ChangeState::normal)
            ImGui::PushStyleColor(ImGuiCol_PopupBg, style.background_color);
        else
        {
            ImGui::PushStyleColor(ImGuiCol_PopupBg, style.flip_color);
            if (state == ChangeState::clicked)
            {
                was_changed = true;
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
                was_changed = true;
            }

            // Rendering
            // Header
            ImVec2 pos = ImGui::GetCursorPos();
            auto icon_pos = ImGui::GetCursorScreenPos() + ImVec2(10, 20);
            style.icon->render(icon_pos);
            pos.x += style.icon->size().x + 40;
            pos.y += 20;
            ImGui::PushFont(style.bold_font);
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
                ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, {style::g_check_box.spacing, 0});
                ImGui::SetCursorPos(pos + ImVec2(0, style::g_button.padding.y * 0.5f));
                _switch.render();
                ImGui::PopStyleVar();
            }

            // Buttons
            acul::vector<acul::string> buttons(message.buttons.size());
            for (size_t i = 0; i < message.buttons.size(); ++i) buttons[i] = get_btn_name(message.buttons[i].first);
            right_controls(buttons, &action, pos.y);

            if (action != -1)
            {
                auto &pair = message.buttons[action];
                if (pair.first == awin::popup::Buttons::cancel || pair.first == awin::popup::Buttons::ok)
                {
                    if (pair.second) pair.second();
                    if (message.prevent_close) _prevent_close_count -= _messages[it->first].size();
                    _messages.erase(it->first);
                }
                else
                {
                    auto &arr = it->second;
                    if (_switch.toogled())
                    {
                        for (auto &msg : arr)
                            if (auto callback = msg.buttons[action].second) callback();
                        if (message.prevent_close) _prevent_close_count -= arr.size();
                        _messages.erase(it->first);
                    }
                    else
                    {
                        if (pair.second) pair.second();
                        if (message.prevent_close) --_prevent_close_count;
                        arr.erase(arr.begin());
                        if (arr.empty()) _messages.erase(it->first);
                    }
                }
                _switch.toogled(false);
                was_changed = true;
            }

            ImGui::EndPopup();
        }
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);
        if (was_changed)
        {
            ImGui::CloseCurrentPopup();
            awin::push_empty_event();
        }
    }

    void ModalQueue::bind_events()
    {
        ed->bind_event(this, AUIK_EVENT_MODAL_SIGN,
                       [this](const acul::events::data_event<ModalQueue::Message> &e) { push(e.data); });
    }
} // namespace auik