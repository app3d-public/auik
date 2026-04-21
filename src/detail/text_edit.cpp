// Based on Dear ImGui's imstb_textedit.h
// (https://github.com/ocornut/imgui/blob/master/imstb_textedit.h)

#include <algorithm>
#include <auik/detail/text_edit.hpp>

namespace auik::detail
{
    static int string_len(TextEditString *str)
    {
        return (str && str->string_len) ? str->string_len(str->user_data) : 0;
    }

    static TextEditChar get_char(TextEditString *str, int idx)
    {
        return (str && str->get_char) ? str->get_char(str->user_data, idx) : 0;
    }

    static bool is_space(TextEditString *str, TextEditChar ch)
    {
        if (str && str->is_space) return str->is_space(ch);
        return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r';
    }

    static int clamp_pos(TextEditString *str, int pos) { return std::clamp(pos, 0, string_len(str)); }

    static int next_char(TextEditString *str, int idx)
    {
        const int len = string_len(str);
        if (idx >= len) return len;
        if (str && str->next_char_index) return clamp_pos(str, str->next_char_index(str->user_data, idx));
        return idx + 1;
    }

    static int prev_char(TextEditString *str, int idx)
    {
        if (idx <= 0) return 0;
        if (str && str->prev_char_index) return clamp_pos(str, str->prev_char_index(str->user_data, idx));
        return idx - 1;
    }

    static int selection_min(const TextEditCursor &cursor) { return std::min(cursor.select_start, cursor.select_end); }

    static int selection_max(const TextEditCursor &cursor) { return std::max(cursor.select_start, cursor.select_end); }

    static void clear_selection(TextEditCursor &cursor)
    {
        cursor.select_start = cursor.cursor;
        cursor.select_end = cursor.cursor;
    }

    static void ensure_cursor(TextEditState *state)
    {
        if (state && state->cursors.empty()) state->cursors.push_back({});
    }

    static void clamp_cursor(TextEditString *str, TextEditCursor &cursor)
    {
        cursor.cursor = clamp_pos(str, cursor.cursor);
        cursor.select_start = clamp_pos(str, cursor.select_start);
        cursor.select_end = clamp_pos(str, cursor.select_end);
    }

    static bool insert_chars(TextEditString *str, int pos, const TextEditChar *text, int text_len)
    {
        return (str && str->insert_chars) ? str->insert_chars(str->user_data, pos, text, text_len) : false;
    }

    static void delete_chars(TextEditString *str, int pos, int count)
    {
        if (str && str->delete_chars && count > 0) str->delete_chars(str->user_data, pos, count);
    }

    static void adjust_cursors_after_edit(TextEditState *state, int pos, int deleted, int inserted)
    {
        const int delta = inserted - deleted;
        for (auto &cursor : state->cursors)
        {
            auto adjust = [&](int value) {
                if (value <= pos) return value;
                if (value <= pos + deleted) return pos + inserted;
                return value + delta;
            };

            cursor.cursor = adjust(cursor.cursor);
            cursor.select_start = adjust(cursor.select_start);
            cursor.select_end = adjust(cursor.select_end);
        }
    }

    static bool replace_range(TextEditString *str, TextEditState *state, int start, int end, const TextEditChar *text,
                              int text_len)
    {
        start = clamp_pos(str, start);
        end = clamp_pos(str, end);
        if (end < start) std::swap(start, end);

        const int deleted = end - start;
        if (deleted > 0) delete_chars(str, start, deleted);
        if (text_len > 0 && !insert_chars(str, start, text, text_len)) return false;

        adjust_cursors_after_edit(state, start, deleted, text_len);
        return true;
    }

    static int line_start(TextEditString *str, int pos)
    {
        pos = clamp_pos(str, pos);
        while (pos > 0 && get_char(str, prev_char(str, pos)) != '\n') pos = prev_char(str, pos);
        return pos;
    }

    static int line_end(TextEditString *str, int pos)
    {
        const int len = string_len(str);
        pos = clamp_pos(str, pos);
        while (pos < len && get_char(str, pos) != '\n') pos = next_char(str, pos);
        return pos;
    }

    static int word_left(TextEditString *str, int pos)
    {
        pos = prev_char(str, clamp_pos(str, pos));
        while (pos > 0 && is_space(str, get_char(str, pos))) pos = prev_char(str, pos);
        while (pos > 0 && !is_space(str, get_char(str, prev_char(str, pos)))) pos = prev_char(str, pos);
        return pos;
    }

    static int word_right(TextEditString *str, int pos)
    {
        const int len = string_len(str);
        pos = clamp_pos(str, pos);
        while (pos < len && !is_space(str, get_char(str, pos))) pos = next_char(str, pos);
        while (pos < len && is_space(str, get_char(str, pos))) pos = next_char(str, pos);
        return pos;
    }

    static f32 width_to_pos(TextEditString *str, int line_start_pos, int pos)
    {
        f32 width = 0.0f;
        for (int i = line_start_pos, char_idx = 0; i < pos; i = next_char(str, i), ++char_idx)
            width += (str && str->get_width) ? str->get_width(str->user_data, line_start_pos, char_idx) : 1.0f;
        return width;
    }

    static int seek_x(TextEditString *str, int line_start_pos, f32 x)
    {
        const int end = line_end(str, line_start_pos);
        f32 width = 0.0f;
        int char_idx = 0;
        for (int pos = line_start_pos; pos < end; pos = next_char(str, pos), ++char_idx)
        {
            const f32 char_width =
                (str && str->get_width) ? str->get_width(str->user_data, line_start_pos, char_idx) : 1.0f;
            if (x < width + char_width * 0.5f) return pos;
            width += char_width;
        }
        return end;
    }

    static int vertical_move(TextEditString *str, TextEditCursor &cursor, int dir)
    {
        const int current_line_start = line_start(str, cursor.cursor);
        if (!cursor.has_preferred_x)
        {
            cursor.preferred_x = width_to_pos(str, current_line_start, cursor.cursor);
            cursor.has_preferred_x = 1;
        }

        if (dir < 0)
        {
            if (current_line_start == 0) return 0;
            const int prev_end = prev_char(str, current_line_start);
            return seek_x(str, line_start(str, prev_end), cursor.preferred_x);
        }

        const int current_line_end = line_end(str, cursor.cursor);
        if (current_line_end >= string_len(str)) return string_len(str);
        return seek_x(str, next_char(str, current_line_end), cursor.preferred_x);
    }

    static void move_cursor(TextEditString *str, TextEditCursor &cursor, int new_pos, bool select)
    {
        new_pos = clamp_pos(str, new_pos);
        if (select)
        {
            if (!cursor.has_selection()) cursor.select_start = cursor.cursor;
            cursor.select_end = new_pos;
        }
        cursor.cursor = new_pos;
        if (!select) clear_selection(cursor);
    }

    static void apply_to_cursors(TextEditString *str, TextEditState *state, const TextEditChar *text, int text_len)
    {
        acul::vector<unsigned char> done;
        for (size_t i = 0; i < state->cursors.size(); ++i) done.push_back(0);

        for (;;)
        {
            int best = -1;
            int best_pos = -1;
            for (size_t i = 0; i < state->cursors.size(); ++i)
            {
                if (done[i]) continue;
                const TextEditCursor &cursor = state->cursors[i];
                const int pos = cursor.has_selection() ? selection_max(cursor) : cursor.cursor;
                if (pos > best_pos)
                {
                    best = static_cast<int>(i);
                    best_pos = pos;
                }
            }
            if (best < 0) break;

            done[best] = 1;
            TextEditCursor &cursor = state->cursors[best];
            const int start = cursor.has_selection() ? selection_min(cursor) : cursor.cursor;
            const int end = cursor.has_selection() ? selection_max(cursor) : cursor.cursor;
            if (replace_range(str, state, start, end, text, text_len))
            {
                cursor.cursor = start + text_len;
                clear_selection(cursor);
            }
        }
    }

    bool TextEditState::has_selection() const
    {
        for (const auto &cursor : cursors)
            if (cursor.has_selection()) return true;
        return false;
    }

    void TextEditState::clear_cursors()
    {
        cursors.clear();
        cursors.push_back({});
    }

    TextEditCursor &TextEditState::primary_cursor()
    {
        ensure_cursor(this);
        return cursors[0];
    }

    const TextEditCursor &TextEditState::primary_cursor() const { return cursors[0]; }

    void text_edit_initialize_state(TextEditState *state, bool is_single_line)
    {
        if (state == nullptr) return;
        state->undostate = {};
        state->insert_mode = 0;
        state->single_line = is_single_line ? 1 : 0;
        state->row_count_per_page = 0;
        state->clear_cursors();
    }

    void text_edit_click(TextEditString *str, TextEditState *state, f32 x, f32 y)
    {
        if (str == nullptr || state == nullptr) return;
        TextEditCursor &cursor = state->primary_cursor();

        TextEditRow row{};
        int line_start_pos = 0;
        f32 base_y = 0.0f;
        for (;;)
        {
            if (str->layout_row) str->layout_row(&row, str->user_data, line_start_pos);
            if (line_start_pos >= string_len(str) || y < base_y + row.ymax) break;
            line_start_pos += row.num_chars;
            if (line_start_pos < string_len(str) && get_char(str, line_start_pos) == '\n')
                line_start_pos = next_char(str, line_start_pos);
            base_y += row.baseline_y_delta != 0.0f ? row.baseline_y_delta : (row.ymax - row.ymin);
        }

        cursor.cursor = seek_x(str, line_start_pos, x - row.x0);
        cursor.has_preferred_x = 0;
        clear_selection(cursor);
        state->cursors.resize(1);
    }

    void text_edit_drag(TextEditString *str, TextEditState *state, f32 x, f32 y)
    {
        if (str == nullptr || state == nullptr) return;
        TextEditCursor &cursor = state->primary_cursor();
        const int anchor = cursor.has_selection() ? cursor.select_start : cursor.cursor;
        text_edit_click(str, state, x, y);
        cursor.select_start = anchor;
        cursor.select_end = cursor.cursor;
    }

    bool text_edit_cut(TextEditString *str, TextEditState *state)
    {
        if (str == nullptr || state == nullptr || !state->has_selection()) return false;
        apply_to_cursors(str, state, nullptr, 0);
        return true;
    }

    bool text_edit_paste(TextEditString *str, TextEditState *state, const TextEditChar *text, int text_len)
    {
        if (str == nullptr || state == nullptr || text == nullptr || text_len <= 0) return false;
        apply_to_cursors(str, state, text, text_len);
        return true;
    }

    void text_edit_key(TextEditString *str, TextEditState *state, TextEditKey key)
    {
        if (str == nullptr || state == nullptr) return;
        ensure_cursor(state);

        const bool select = (key & AUIK_TEXT_EDIT_KEY_SHIFT) != 0;
        key &= ~AUIK_TEXT_EDIT_KEY_SHIFT;

        for (auto &cursor : state->cursors)
        {
            clamp_cursor(str, cursor);
            int new_pos = cursor.cursor;

            if (!select && cursor.has_selection() &&
                (key == AUIK_TEXT_EDIT_KEY_LEFT || key == AUIK_TEXT_EDIT_KEY_RIGHT))
            {
                new_pos = key == AUIK_TEXT_EDIT_KEY_LEFT ? selection_min(cursor) : selection_max(cursor);
            }
            else
            {
                switch (key)
                {
                    case AUIK_TEXT_EDIT_KEY_LEFT:
                        new_pos = prev_char(str, cursor.cursor);
                        break;
                    case AUIK_TEXT_EDIT_KEY_RIGHT:
                        new_pos = next_char(str, cursor.cursor);
                        break;
                    case AUIK_TEXT_EDIT_KEY_WORD_LEFT:
                        new_pos = word_left(str, cursor.cursor);
                        break;
                    case AUIK_TEXT_EDIT_KEY_WORD_RIGHT:
                        new_pos = word_right(str, cursor.cursor);
                        break;
                    case AUIK_TEXT_EDIT_KEY_LINE_START:
                        new_pos = line_start(str, cursor.cursor);
                        break;
                    case AUIK_TEXT_EDIT_KEY_LINE_END:
                        new_pos = line_end(str, cursor.cursor);
                        break;
                    case AUIK_TEXT_EDIT_KEY_TEXT_START:
                        new_pos = 0;
                        break;
                    case AUIK_TEXT_EDIT_KEY_TEXT_END:
                        new_pos = string_len(str);
                        break;
                    case AUIK_TEXT_EDIT_KEY_UP:
                        new_pos = vertical_move(str, cursor, -1);
                        break;
                    case AUIK_TEXT_EDIT_KEY_DOWN:
                        new_pos = vertical_move(str, cursor, 1);
                        break;
                    case AUIK_TEXT_EDIT_KEY_BACKSPACE:
                        if (cursor.has_selection()) apply_to_cursors(str, state, nullptr, 0);
                        else
                        {
                            const int prev = prev_char(str, cursor.cursor);
                            replace_range(str, state, prev, cursor.cursor, nullptr, 0);
                            cursor.cursor = prev;
                            clear_selection(cursor);
                        }
                        continue;
                    case AUIK_TEXT_EDIT_KEY_DELETE:
                        if (cursor.has_selection()) apply_to_cursors(str, state, nullptr, 0);
                        else replace_range(str, state, cursor.cursor, next_char(str, cursor.cursor), nullptr, 0);
                        clear_selection(cursor);
                        continue;
                    default:
                        continue;
                }
            }

            if (key != AUIK_TEXT_EDIT_KEY_UP && key != AUIK_TEXT_EDIT_KEY_DOWN) cursor.has_preferred_x = 0;
            move_cursor(str, cursor, new_pos, select);
        }
    }

    void text_edit_text(TextEditString *str, TextEditState *state, const TextEditChar *text, int text_len)
    {
        if (str == nullptr || state == nullptr || text == nullptr || text_len <= 0) return;
        apply_to_cursors(str, state, text, text_len);
    }
} // namespace auik::detail
