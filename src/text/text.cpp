#include <imgui/imgui_internal.h>
#include <uikit/text/text.hpp>

namespace uikit
{
    void Text::render()
    {
        ImGuiContext &g = *GImGui;
        ImGuiWindow *window = ImGui::GetCurrentWindow();
        if (window->SkipItems) return;
        const ImVec2 text_pos(window->DC.CursorPos.x, window->DC.CursorPos.y + window->DC.CurrLineTextBaseOffset);
        if (_wrapped)
        {
            bool need_backup = (g.CurrentWindow->DC.TextWrapPos < 0.0f);
            if (need_backup) ImGui::PushTextWrapPos(0.0f);
            renderTextWrapped(text_pos);
            if (need_backup) ImGui::PopTextWrapPos();
        }
        else
            renderText(text_pos);
    }

    void Text::renderText(const ImVec2 text_pos)
    {
        // Long text!
        // Perform manual coarse clipping to optimize for long multi-line text
        // - From this point we will only compute the width of lines that are visible. Optimization only available when
        // word-wrapping is disabled.
        // - We also don't vertically center the text within the line full height, which is unlikely to matter because
        // we are likely the biggest and only item on the line.
        // - We use memchr(), pay attention that well optimized versions of those str/mem functions are much faster than
        // a casually written loop.
        const char *line = name.c_str();
        const float line_height = ImGui::GetTextLineHeight();
        auto text_end = line + name.size();
        ImGuiWindow *window = ImGui::GetCurrentWindow();
        ImGuiContext &g = *GImGui;

        // Lines to skip (can't skip when logging text)
        ImVec2 pos = text_pos;
        if (!g.LogEnabled)
        {
            int lines_skippable = (int)((window->ClipRect.Min.y - text_pos.y) / line_height);
            if (lines_skippable > 0)
            {
                int lines_skipped = 0;
                while (line < text_end && lines_skipped < lines_skippable)
                {
                    const char *line_end = (const char *)memchr(line, '\n', text_end - line);
                    if (!line_end) line_end = text_end;
                    _text_size.x = ImMax(_text_size.x, ImGui::CalcTextSize(line, line_end).x);
                    line = line_end + 1;
                    lines_skipped++;
                }
                pos.y += lines_skipped * line_height;
            }

            if (line < text_end)
            {
                ImRect line_rect(pos, pos + ImVec2(FLT_MAX, line_height));
                while (line < text_end)
                {
                    if (ImGui::IsClippedEx(line_rect, 0)) break;

                    const char *line_end = (const char *)memchr(line, '\n', text_end - line);
                    if (!line_end) line_end = text_end;
                    _text_size.x = ImMax(_text_size.x, ImGui::CalcTextSize(line, line_end).x);
                    ImGui::RenderText(pos, line, line_end, false);
                    line = line_end + 1;
                    line_rect.Min.y += line_height;
                    line_rect.Max.y += line_height;
                    pos.y += line_height;
                }

                // Count remaining lines
                int lines_skipped = 0;
                while (line < text_end)
                {
                    const char *line_end = (const char *)memchr(line, '\n', text_end - line);
                    if (!line_end) line_end = text_end;
                    _text_size.x = ImMax(_text_size.x, ImGui::CalcTextSize(line, line_end).x);
                    line = line_end + 1;
                    lines_skipped++;
                }
                pos.y += lines_skipped * line_height;
            }
            _text_size.y = (pos - text_pos).y;

            ImRect bb(text_pos, text_pos + _text_size);
            ImGui::ItemSize(_text_size, 0.0f);
            ImGui::ItemAdd(bb, 0);
        }
    }

    void Text::renderTextWrapped(const ImVec2 text_pos)
    {
        // Common case
        ImGuiWindow *window = ImGui::GetCurrentWindow();
        const float wrap_pos_x = window->DC.TextWrapPos;
        const bool wrap_enabled = (wrap_pos_x >= 0.0f);
        const float wrap_width = wrap_enabled ? ImGui::CalcWrapWidthForPos(window->DC.CursorPos, wrap_pos_x) : 0.0f;
        auto start = name.c_str();
        auto end = start + name.size();
        _text_size = ImGui::CalcTextSize(start, end, false, wrap_width);

        ImRect bb(text_pos, text_pos + _text_size);
        ImGui::ItemSize(_text_size, 0.0f);
        if (!ImGui::ItemAdd(bb, 0)) return;

        // Render (we don't hide text after ## in this end-user function)
        ImGui::RenderTextWrapped(bb.Min, start, end, wrap_width);
    }

    struct InputTextCallback_UserData
    {
        acul::string *Str;
        ImGuiInputTextCallback ChainCallback;
        void *ChainCallbackUserData;
    };

    static int inputTextCallback(ImGuiInputTextCallbackData *data)
    {
        InputTextCallback_UserData *user_data = (InputTextCallback_UserData *)data->UserData;
        if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
        {
            // Resize string callback
            // If for some reason we refuse the new length (BufTextLen) and/or capacity (BufSize) we need to set them
            // back to what we want.
            acul::string *str = user_data->Str;
            IM_ASSERT(data->Buf == str->c_str());
            str->resize(data->BufTextLen);
            data->Buf = (char *)str->c_str();
        }
        else if (user_data->ChainCallback)
        {
            // Forward to user callback, if any
            data->UserData = user_data->ChainCallbackUserData;
            return user_data->ChainCallback(data);
        }
        return 0;
    }

    bool inputText(const char *label, acul::string *str, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback,
                   void *user_data)
    {
        IM_ASSERT((flags & ImGuiInputTextFlags_CallbackResize) == 0);
        flags |= ImGuiInputTextFlags_CallbackResize;

        InputTextCallback_UserData cb_user_data;
        cb_user_data.Str = str;
        cb_user_data.ChainCallback = callback;
        cb_user_data.ChainCallbackUserData = user_data;
        return ImGui::InputText(label, (char *)str->c_str(), str->capacity() + 1, flags, inputTextCallback,
                                &cb_user_data);
    }

    bool inputTextWithHint(const char *label, const char *hint, acul::string *str, ImGuiInputTextFlags flags,
                           ImGuiInputTextCallback callback, void *user_data)
    {
        IM_ASSERT((flags & ImGuiInputTextFlags_CallbackResize) == 0);
        flags |= ImGuiInputTextFlags_CallbackResize;

        InputTextCallback_UserData cb_user_data;
        cb_user_data.Str = str;
        cb_user_data.ChainCallback = callback;
        cb_user_data.ChainCallbackUserData = user_data;
        return ImGui::InputTextWithHint(label, hint, (char *)str->c_str(), str->capacity() + 1, flags,
                                        inputTextCallback, &cb_user_data);
    }
} // namespace uikit