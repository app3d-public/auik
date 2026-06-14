#pragma once

// Based on Dear ImGui's imstb_textedit.h
// (https://github.com/ocornut/imgui/blob/master/imstb_textedit.h)

#include <acul/scalars.hpp>
#include <acul/vector.hpp>

#define AUIK_TEXT_EDIT_UNDO_STATE_COUNT 99
#define AUIK_TEXT_EDIT_UNDO_CHAR_COUNT  999
#define AUIK_TEXT_EDIT_KEY_LEFT         0x200000
#define AUIK_TEXT_EDIT_KEY_RIGHT        0x200001
#define AUIK_TEXT_EDIT_KEY_UP           0x200002
#define AUIK_TEXT_EDIT_KEY_DOWN         0x200003
#define AUIK_TEXT_EDIT_KEY_LINE_START   0x200004
#define AUIK_TEXT_EDIT_KEY_LINE_END     0x200005
#define AUIK_TEXT_EDIT_KEY_TEXT_START   0x200006
#define AUIK_TEXT_EDIT_KEY_TEXT_END     0x200007
#define AUIK_TEXT_EDIT_KEY_DELETE       0x200008
#define AUIK_TEXT_EDIT_KEY_BACKSPACE    0x200009
#define AUIK_TEXT_EDIT_KEY_UNDO         0x20000A
#define AUIK_TEXT_EDIT_KEY_REDO         0x20000B
#define AUIK_TEXT_EDIT_KEY_WORD_LEFT    0x20000C
#define AUIK_TEXT_EDIT_KEY_WORD_RIGHT   0x20000D
#define AUIK_TEXT_EDIT_KEY_PAGE_UP      0x20000E
#define AUIK_TEXT_EDIT_KEY_PAGE_DOWN    0x20000F
#define AUIK_TEXT_EDIT_KEY_SHIFT        0x400000

namespace auik::detail
{
    using TextEditChar = int;
    using TextEditPosition = int;
    using TextEditKey = int;

    struct TextEditUndoRecord
    {
        TextEditPosition where = 0;
        TextEditPosition insert_length = 0;
        TextEditPosition delete_length = 0;
        int char_storage = 0;
    };

    struct TextEditUndoState
    {
        TextEditUndoRecord undo_rec[AUIK_TEXT_EDIT_UNDO_STATE_COUNT]{};
        TextEditChar undo_char[AUIK_TEXT_EDIT_UNDO_CHAR_COUNT]{};
        short undo_point = 0;
        short redo_point = 0;
        int undo_char_point = 0;
        int redo_char_point = 0;
    };

    struct TextEditRow
    {
        f32 x0 = 0.0f;
        f32 x1 = 0.0f;
        f32 baseline_y_delta = 0.0f;
        f32 ymin = 0.0f;
        f32 ymax = 0.0f;
        int num_chars = 0;
    };

    struct TextEditCursor
    {
        int cursor = 0;
        int select_start = 0;
        int select_end = 0;
        unsigned char has_preferred_x = 0;
        unsigned char cursor_at_end_of_line = 0;
        f32 preferred_x = 0.0f;

        bool has_selection() const { return select_start != select_end; }
    };

    struct TextEditState
    {
        acul::vector<TextEditCursor> cursors;
        TextEditUndoState undostate{};
        unsigned char insert_mode = 0;
        unsigned char single_line = 0;
        int row_count_per_page = 0;

        bool has_selection() const;
        void clear_cursors();
        TextEditCursor &primary_cursor();
        const TextEditCursor &primary_cursor() const;
    };

    struct TextEditString
    {
        void *user_data = nullptr;

        int (*string_len)(void *user_data) = nullptr;
        void (*layout_row)(TextEditRow *row, void *user_data, int line_start_idx) = nullptr;
        f32 (*get_width)(void *user_data, int line_start_idx, int char_idx) = nullptr;
        TextEditChar (*get_char)(void *user_data, int char_idx) = nullptr;
        bool (*insert_chars)(void *user_data, int pos, const TextEditChar *text, int text_len) = nullptr;
        void (*delete_chars)(void *user_data, int pos, int count) = nullptr;
        bool (*replace_chars)(void *user_data, int pos, int delete_count, const TextEditChar *text,
                              int text_len) = nullptr;
        int (*key_to_text)(TextEditKey key) = nullptr;
        bool (*is_space)(TextEditChar ch) = nullptr;
        int (*next_char_index)(void *user_data, int idx) = nullptr;
        int (*prev_char_index)(void *user_data, int idx) = nullptr;
    };

    void text_edit_initialize_state(TextEditState *state, bool is_single_line);
    void text_edit_click(TextEditString *str, TextEditState *state, f32 x, f32 y);
    void text_edit_drag(TextEditString *str, TextEditState *state, f32 x, f32 y);
    bool text_edit_cut(TextEditString *str, TextEditState *state);
    bool text_edit_paste(TextEditString *str, TextEditState *state, const TextEditChar *text, int text_len);
    void text_edit_key(TextEditString *str, TextEditState *state, TextEditKey key);
    void text_edit_text(TextEditString *str, TextEditState *state, const TextEditChar *text, int text_len);
} // namespace auik::detail
