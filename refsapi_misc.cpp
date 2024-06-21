#include "precompiled.h"

bool is_number(std::string &s) {
    remove_chars(s); // trim(s);
    if (s.empty()) return false;
    char* l_decimal_point = localeconv()->decimal_point;
    auto it = s.begin();
    bool need_replace = DECIMAL_POINT != *l_decimal_point;
    if (*it == '+' || *it == '-')
        if (s.size() == 1)
            return false;
        else
            it++;
    while (it != s.end()) {
        if (!std::isdigit(*it)) {
            if (*it == DECIMAL_POINT && need_replace) {
                s.replace(it, it + 1, l_decimal_point);
                continue;
            }
            if (*it != *l_decimal_point)
                break;
        }
        it++;
    }
    //UTIL_ServerPrint("[DEBUG] is_number(): s = <%s>, result = %d\n", s.c_str(), it == s.end());
    return it == s.end();
}