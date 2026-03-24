#include <auik/v2/auik.hpp>

namespace auik::v2
{
    static acul::string encode_utf8(u32 cp)
    {
        acul::string out;
        if (cp <= 0x7Fu) out.push_back(static_cast<char>(cp));
        else if (cp <= 0x7FFu)
        {
            out.push_back(static_cast<char>(0xC0u | ((cp >> 6) & 0x1Fu)));
            out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
        }
        else if (cp <= 0xFFFFu)
        {
            out.push_back(static_cast<char>(0xE0u | ((cp >> 12) & 0x0Fu)));
            out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
            out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
        }
        else if (cp <= 0x10FFFFu)
        {
            out.push_back(static_cast<char>(0xF0u | ((cp >> 18) & 0x07u)));
            out.push_back(static_cast<char>(0x80u | ((cp >> 12) & 0x3Fu)));
            out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
            out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
        }
        return out;
    }

    APPLIB_API bool add_char_to_string(acul::string &dst, u32 char_code, TextFlags flags)
    {
        u32 c = char_code;
        if (c == 0) return false;
        if (!(flags & TextFlagBits::allow_tab_input) && c == '\t') return false;
        if ((flags & TextFlagBits::chars_no_blank) && (c == ' ' || c == '\t')) return false;
        if ((flags & TextFlagBits::chars_ascii) && c >= 128) return false;

        const bool is_decimal = flags & TextFlagBits::chars_decimal;
        const bool is_scientific = flags & TextFlagBits::chars_scientific;
        const bool is_hex = flags & TextFlagBits::chars_hexadecimal;

        if (is_decimal || is_scientific)
        {
            const bool is_digit = c >= '0' && c <= '9';
            const bool is_common_op = c == '+' || c == '-' || c == '*' || c == '/' || c == '.';
            const bool is_science_op = (c == 'e' || c == 'E');
            if (!is_digit && !is_common_op && !(is_scientific && is_science_op)) return false;
        }
        else if (is_hex)
        {
            const bool is_digit = c >= '0' && c <= '9';
            const bool is_lower_hex = c >= 'a' && c <= 'f';
            const bool is_upper_hex = c >= 'A' && c <= 'F';
            const bool is_hex_prefix = c == 'x' || c == 'X';
            if (!is_digit && !is_lower_hex && !is_upper_hex && !is_hex_prefix) return false;
        }

        if ((flags & TextFlagBits::chars_uppercase) && c >= 'a' && c <= 'z') c = c - 'a' + 'A';
        dst += encode_utf8(c);
        return true;
    }
} // namespace auik::v2
