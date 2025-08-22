#pragma once

#include "refsapi.h"

bool is_number(std::string &s);
size_t set_amx_string(cell *dest, const char *str, size_t max_size);
uint64_t get_next_id(std::atomic<uint64_t> &counter);

bool is_valid_utf8(const uint8_t *data, size_t size);
bool is_valid_utf8(const std::vector<uint8_t> &data);
bool is_valid_utf8(const char *str);
bool is_valid_utf8(const std::string &str);
bool is_valid_utf8(char *str, size_t length);
bool is_valid_utf8(const char *str, size_t length);

#ifdef __SSE4_2__
bool is_valid_utf8_simd(const uint8_t *data, size_t size);
bool is_valid_utf8_simd(const std::vector<uint8_t> &data);
bool is_valid_utf8_simd(const char *str);
bool is_valid_utf8_simd(const std::string &str);
bool is_valid_utf8_simd(const char *str, size_t length);
bool is_valid_utf8_simd(char *str, size_t length);
#endif

std::wstring format_time(const std::chrono::system_clock::time_point &tp, const std::wstring format = L"%d/%m/%Y - %H:%M:%S");
std::wstring cc(const std::wstring &str, bool with_slashes = false, wchar_t cc = L'\\');
bool dir_exists(const std::wstring &path);
bool is_mutex_locked(std::mutex &mutex);
CPluginMngr::CPlugin * get_plugin(AMX *amx);
bool amx_allot(AMX *amx, int cells, cell *amx_addr, cell **phys_addr);
double timespec_diff(const struct timespec *start, const struct timespec *end);
void checkCPUFeatures();
void cpu_test();
int64_t cpu_benchmark();
std::string get_cpu_name();
void get_thread_info(bool boost = false);
bool set_thread_priority(std::thread *t, int pri = 0);
//int safe_poll(struct pollfd *fds, nfds_t nfds, int timeout_ms);
float similarity_score(const std::wstring &nick1, const std::wstring &nick2, const float lcs_threshold = 0.7f, const float k_lev = 0.6f, const float k_tan = 0.3f, const float k_lcs = 0.1f);

class fmt
{
    const size_t size = 4096;
    char *buff;

public:
    fmt(char *fmt, ...)
    {
        buff = new char[size]();
        if (fmt == nullptr)
            return;
        va_list arg_ptr;
        va_start(arg_ptr, fmt);
        vsnprintf(buff, size - 1, fmt, arg_ptr);
        va_end(arg_ptr);
    }
    ~fmt()
    {
        if (buff)
            delete buff;
        buff = nullptr;
    }
    char *c_str()
    {
        return buff;
    }
};

class wfmt
{
    const size_t size = 4096;
    wchar_t *buff;

public:
    wfmt(wchar_t *fmt, ...)
    {
        buff = new wchar_t[size]();
        if (fmt == nullptr)
            return;
        try
        {
            va_list arg_ptr;
            va_start(arg_ptr, fmt);
            std::vswprintf(buff, size - 1, fmt, arg_ptr);
            va_end(arg_ptr);
        }
        catch (...)
        {
            *buff = 0;
            DEBUG("*** CRITICAL *** %s(): fmt = %s", __func__, fmt);
        }
    }
    ~wfmt()
    {
        if (buff)
            delete buff;
        buff = nullptr;
    }
    wchar_t *c_str()
    {
        return buff;
    }
};

inline bool is_valid_index(const size_t index)
{
    return index > 0 && index <= gpGlobals->maxClients;
}

inline bool is_valid_entity(const edict_t *pEdict)
{
    return !(pEdict == NULL || pEdict == nullptr || pEdict->pvPrivateData == nullptr || pEdict->free || (pEdict->v.flags & FL_KILLME));
}

inline bool is_valid_team(const int team)
{
    return team >= TEAM_TERRORIST && team <= TEAM_CT;
}

inline bool is_entity_intersects(const edict_t *pEdict_1, const edict_t *pEdict_2)
{
    return !(pEdict_1->v.absmin.x > pEdict_2->v.absmax.x ||
             pEdict_1->v.absmin.y > pEdict_2->v.absmax.y ||
             pEdict_1->v.absmin.z > pEdict_2->v.absmax.z ||
             pEdict_1->v.absmax.x < pEdict_2->v.absmin.x ||
             pEdict_1->v.absmax.y < pEdict_2->v.absmin.y ||
             pEdict_1->v.absmax.z < pEdict_2->v.absmin.z);
}

inline std::string wstos(const wchar_t *s)
{
    try
    {
        return g_converter.to_bytes(s);
    }
    catch (...)
    {
        return ""; 
    }
}

inline std::string wstos(const std::wstring s)
{
    try
    {
        return g_converter.to_bytes(s);
    }
    catch (...)
    {
        return ""; 
    }
}

inline std::string wstos(wfmt s)
{
    try
    {
        return g_converter.to_bytes(s.c_str());
    }
    catch (...)
    {
        return ""; 
    }
}

/* SIMD version used
inline bool is_valid_utf8(const char *str)
{
    bool result = true;
    try
    {
        g_converter.from_bytes(str);
    }
    catch (...)
    {
        result = false;
    }
    return result;
}
*/

inline std::wstring stows(const std::string s)
{
    try
    {
        return g_converter.from_bytes(s);
    }
    catch (...)
    {
        std::wstring result;
        size_t len = s.size();
        // UTIL_ServerPrint("[DEBUG] stows(): catch !!! str = %s, len = %d\n", s.c_str(), len);
        if (len)
        {
            // result.reserve(len);
            for (size_t i = 0; i < len; i++)
                result.push_back(s[i] & 0xFF);
        }
        return result;
    }
}

inline double stod(std::string s, bool has_min = false, float min_val = 0.0f, bool has_max = false, float max_val = 0.0f)
{
    auto result = std::stod(s); // std::strtof(s.c_str(), 0); //(float)std::stod(s);
    // UTIL_ServerPrint("[DEBUG] stod(): in = %s, out = %f\n", s.c_str(), result);
    if (has_min && result < min_val)
        result = min_val;
    if (has_min && result > max_val)
        result = max_val;
    return result;
}

inline double roundd(double value, int precision = -6)
{
    // UTIL_ServerPrint("[DEBUG] acs_roundfloat(): value = %f, precision = %d", value, precision);
    auto power = pow(10.0f, -precision);
    return floor(value * power + 0.5) / power;
}

inline bool file_exists(const std::wstring &name)
{
    struct stat buff;
    return name.empty() ? false : (stat(wstos(name).c_str(), &buff) == 0);
}

inline std::string remove_chars(std::string &s, std::string chars = _TRIM_CHARS)
{
    s.erase(std::remove_if(s.begin(), s.end(), [&](unsigned char ch) { return chars.find(ch) != std::string::npos; }), s.end());
    return s;
}

inline std::wstring remove_chars(std::wstring &s, std::wstring chars = L"\r\t\n")
{
    s.erase(std::remove_if(s.begin(), s.end(), [&](wchar_t ch) { return chars.find(ch) != std::wstring::npos; }), s.end());
    return s;
}

inline void ltrim(std::string &s, std::string chars = _TRIM_CHARS)
{
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [&](unsigned char ch) { return chars.find(ch) == std::string::npos; }));
}

inline void ltrim(std::wstring &s, std::wstring chars = _TRIM_CHARS_L)
{
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [&](wchar_t ch) { return chars.find(ch) == std::wstring::npos; }));
}

inline void rtrim(std::string &s, std::string chars = _TRIM_CHARS)
{
    s.erase(std::find_if(s.rbegin(), s.rend(), [&](unsigned char ch) { return chars.find(ch) == std::string::npos; }).base(), s.end());
}

inline void rtrim(std::wstring &s, std::wstring chars = _TRIM_CHARS_L)
{
    s.erase(std::find_if(s.rbegin(), s.rend(), [&](wchar_t ch) { return chars.find(ch) == std::wstring::npos; }).base(), s.end());
}

inline void rtrim_zero(std::string &s)
{
    auto it = std::find(s.rbegin(), s.rend(), '.');
    if (it == s.rend())
        return;
    s.erase(std::find_if(s.rbegin(), it, [&](unsigned char ch) { return ch != '0'; }).base(), s.end());
}

inline std::string rtrim_zero_c(std::string s)
{
    rtrim_zero(s);
    return s;
}

inline void trim(std::string &s, std::string chars = _TRIM_CHARS)
{
    rtrim(s, chars);
    ltrim(s, chars);
}

inline void trim(std::wstring &s, std::wstring chars = _TRIM_CHARS_L)
{
    rtrim(s, chars);
    ltrim(s, chars);
}

inline std::string ltrim_c(std::string s, std::string chars = _TRIM_CHARS)
{
    ltrim(s, chars);
    return s;
}

inline std::wstring ltrim_c(std::wstring s, std::wstring chars = _TRIM_CHARS_L)
{
    ltrim(s, chars);
    return s;
}

inline std::string rtrim_c(std::string s, std::string chars = _TRIM_CHARS)
{
    rtrim(s, chars);
    return s;
}

inline std::wstring rtrim_c(std::wstring s, std::wstring chars = _TRIM_CHARS_L)
{
    rtrim(s, chars);
    return s;
}

inline std::string trim_c(std::string s, std::string chars = _TRIM_CHARS)
{
    trim(s, chars);
    return s;
}

inline std::wstring trim_c(std::wstring s, std::wstring chars = _TRIM_CHARS_L)
{
    trim(s, chars);
    return s;
}

inline void ws_convert_tolower(std::wstring &s)
{
    std::transform(s.begin(), s.end(), s.begin(), std::bind(std::tolower<wchar_t>, std::placeholders::_1, _LOCALE));
}

inline int rm_quote(std::string &s)
{
    int result = 0;
    bool f[2];
    for (auto &ch : _QQ)
    {
        f[0] = f[1] = 0;
        if ((f[0] = s.front() == ch) && (f[1] = s.back() == ch))
        {
            s.erase(s.begin());
            s.erase(s.end() - 1);
            trim(s);
            result = 1;
            break;
        }
        else if (f[0] != f[1])
        {
            result = -1;
            break;
        }
    }
    return result;
}

inline int rm_quote(std::wstring &s)
{
    int result = 0;
    bool f[2];
    for (auto &ch : _QQ_L)
    {
        f[0] = f[1] = 0;
        if ((f[0] = s.front() == ch) && (f[1] = s.back() == ch))
        {
            s.erase(s.begin());
            s.erase(s.end() - 1);
            result = 1;
            break;
        }
        else if (f[0] != f[1])
        {
            result = -1;
            break;
        }
    }
    return result;
}

inline std::string rm_quote_c(std::string &s)
{
    for (auto &ch : _QQ)
    {
        if (s.front() == ch && s.back() == ch)
        {
            s.erase(s.begin());
            s.erase(s.end() - 1);
            break;
        }
    }
    return s;
}

inline std::wstring rm_quote_c(std::wstring &s)
{
    for (auto &ch : _QQ_L)
    {
        if (s.front() == ch && s.back() == ch)
        {
            s.erase(s.begin());
            s.erase(s.end() - 1);
            break;
        }
    }
    return s;
}