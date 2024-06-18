#pragma once
#include "precompiled.h"
#include "refsapi.h"
/*
inline bool is_valid_index(const size_t index);
inline bool is_valid_entity(const edict_t *pEdict);
inline bool is_valid_team(const int team);
inline bool is_entity_intersects(const edict_t *pEdict_1, const edict_t *pEdict_2);
*/

bool is_number(std::string &s);
/*
inline std::string wstoc(const wchar_t *s);
inline std::string wstoc(const std::wstring &s);
inline std::string wstoc(wfmt s);
inline std::wstring stows(const std::string &s);
inline float stof(std::string s, bool has_min = false, float min_val = 0.0f, bool has_max = false, float max_val = 0.0f);
inline bool file_exists(const std::wstring &name);
inline int rm_quote(std::string &s);
inline int rm_quote(std::wstring &s);
inline std::string rm_quote_c(std::string &s);
inline std::wstring rm_quote_c(std::wstring &s);
inline void ltrim(std::string &s);
inline void ltrim(std::wstring &s);
inline void rtrim(std::string &s);
inline void rtrim(std::wstring &s);
inline void trim(std::string &s);
inline void trim(std::wstring &s);
inline std::string ltrim_c(std::string s);
inline std::wstring ltrim_c(std::wstring s);
inline std::string rtrim_c(std::string s);
inline std::wstring rtrim_c(std::wstring s);
inline std::string trim_c(std::string s);
inline std::wstring trim_c(std::wstring s);
inline void rtrim_zero(std::string &s);
inline std::string rtrim_zero_c(std::string s);
*/

class fmt
{
    const size_t size = 1024;
    char *buff;

public:
    fmt(char *fmt, ...)
    {
        buff = new char[size];
        va_list arg_ptr;
        va_start(arg_ptr, fmt);
        Q_vsnprintf(buff, size - 1, fmt, arg_ptr);
        va_end(arg_ptr);
    }
    ~fmt()
    {
        delete buff;
    }
    char *c_str()
    {
        return buff;
    }
};

class wfmt
{
    const size_t size = 1024;
    wchar_t *buff;

public:
    wfmt(wchar_t *fmt, ...)
    {
        buff = new wchar_t[size];
        va_list arg_ptr;
        va_start(arg_ptr, fmt);
        std::vswprintf(buff, size - 1, fmt, arg_ptr);
        va_end(arg_ptr);
    }
    ~wfmt()
    {
        delete buff;
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

inline std::string wstoc(const wchar_t *s)
{
    return g_converter.to_bytes(s);
}

inline std::string wstoc(const std::wstring &s)
{
    return g_converter.to_bytes(s);
}

inline std::string wstoc(wfmt s)
{
    return g_converter.to_bytes(s.c_str());
}

inline std::wstring stows(const std::string &s)
{
    try
    {
        return g_converter.from_bytes(s);
    }
    catch (std::range_error &e)
    {
        std::wstring result;
        size_t length = s.length();
        UTIL_ServerPrint("[DEBUG] stows(): catch !!! length = %d\n", length);
        result.reserve(length);
        for (size_t i = 0; i < length; i++)
            result.push_back(s[i] & 0xFF);
        return result;
    }
}

inline float stof(std::string s, bool has_min = false, float min_val = 0.0f, bool has_max = false, float max_val = 0.0f)
{
    float result = std::stof(s);
    if (has_min && result < min_val)
        result = min_val;
    if (has_min && result > max_val)
        result = max_val;
    return result;
}

inline bool file_exists(const std::wstring &name)
{
    struct stat buff;
    return (stat(wstoc(name).c_str(), &buff) == 0);
}

inline int rm_quote(std::string &s)
{
    int result = 0;
    bool f[2];
    for (auto& ch : _QQ) //(size_t i = 0; i < sizeof(_QQ) - 1; i++)
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

inline int rm_quote(std::wstring &s)
{
    int result = 0;
    bool f[2];
    for (auto& ch : _QQ)
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
    for (auto& ch : _QQ)
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
    for (auto& ch : _QQ)
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

inline void ltrim(std::string &s)
{
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch)
    {
        return !std::isspace(ch) && ch != '\t';
    }));
}

inline void ltrim(std::wstring &s)
{
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch)
    {
        return !std::isspace(ch) && ch != '\t';
    }));
}

inline void rtrim(std::string &s)
{
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch)
    {
        return !std::isspace(ch) && ch != '\t';
    }).base(), s.end());
}

inline void rtrim(std::wstring &s)
{
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch)
    {
        return !std::isspace(ch) && ch != '\t';
    }).base(), s.end());
}

inline void rtrim_zero(std::string &s)
{
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch)
    {
        return ch != '0' && ch != '.';
    }).base(), s.end());
}

inline std::string rtrim_zero_c(std::string s)
{
    rtrim_zero(s);
    return s;
}

inline void trim(std::string &s)
{
    rtrim(s);
    ltrim(s);
}

inline void trim(std::wstring &s)
{
    rtrim(s);
    ltrim(s);
}

inline std::string ltrim_c(std::string s)
{
    ltrim(s);
    return s;
}

inline std::wstring ltrim_c(std::wstring s)
{
    ltrim(s);
    return s;
}

inline std::string rtrim_c(std::string s)
{
    rtrim(s);
    return s;
}

inline std::wstring rtrim_c(std::wstring s)
{
    rtrim(s);
    return s;
}

inline std::string trim_c(std::string s)
{
    trim(s);
    return s;
}

inline std::wstring trim_c(std::wstring s)
{
    trim(s);
    return s;
}