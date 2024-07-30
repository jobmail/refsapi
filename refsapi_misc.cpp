#include "precompiled.h"

bool is_number(std::string &s) {
    trim(s);
    if (s.empty())
        return false;
    auto it = s.begin();
    if (*it == '+' || *it == '-')
        if (++it == s.end())
            return false;
    bool dp_found = false;
    bool dp_not_last = false;
    while (it != s.end())
    {
        if (!std::isdigit(*it))
        {
            // Check for decimal point and need to have one more digit after
            if (!dp_found && *it == DECIMAL_POINT && (it + 1) != s.end())
                dp_found = true;
            else
                break;
        }
        it++;
    }
    //UTIL_ServerPrint("[DEBUG] is_number(): s = <%s>, result = %d\n", s.c_str(), it == s.end());
    return it == s.end();
}

size_t set_amx_string(cell* dest, const char* str, size_t max_len)
{
    size_t count = 0;
    if (max_len)
    {
        while (*str && count++ < max_len)
            *dest++ = (cell)*str++;
        *dest = 0;
    }
    return count;
}