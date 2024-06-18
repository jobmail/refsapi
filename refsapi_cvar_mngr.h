#pragma once
#include "precompiled.h"

extern std::wstring_convert<convert_type, wchar_t> g_converter;

typedef enum CVAR_TYPES_e {
    CVAR_NONE,
    CVAR_NUM,
    CVAR_FLOAT,
    CVAR_STRING,
    CVAR_TYPES_SIZE
} CVAR_TYPES_t;

typedef struct m_cvar_s
{
    cvar_t *cvar;
    std::wstring name;
    std::wstring value;
    std::wstring desc;
    int flags;
    bool has_min;
    bool has_max;
    float min_val;
    float max_val;
} m_cvar_t;

typedef std::map<std::wstring, m_cvar_t> cvar_list_t;
typedef std::map<int, cvar_list_t> plugin_cvar_t;
typedef cvar_list_t::iterator cvar_list_it;
typedef plugin_cvar_t::iterator plugin_cvar_it;

typedef struct cvar_mngr_s
{
    plugin_cvar_t plugin;
} cvar_mngr_t;

class cvar_mngr
{
    cvar_mngr_t cvars;

private:
    void cvar_direct_set(cvar_t *cvar, const char *value)
    {
        if (cvar != nullptr)
            g_engfuncs.pfnCvar_DirectSet(cvar, value);
    }
    cvar_t *create_cvar(m_cvar_t &c)
    {
        enum { _name, _value, _count};
        // Copy params
        std::string p[_count] = { wstos(c.name), wstos(c.value) };
        auto p_name = p[_name].data();
        auto p_value = p[_value].data();
        cvar_t *p_cvar = CVAR_GET_POINTER(p_name);
        if (p_cvar == nullptr)
        {
            cvar_t cvar;
            cvar.name = p_name;
            cvar.string = p_value;
            cvar.value = 0.0f;
            cvar.flags = c.flags;
            cvar.next = nullptr;
            UTIL_ServerPrint("[DEBUG] create_cvar(): name = <%s>, value = <%s>\n", cvar.name, cvar.string);
            CVAR_REGISTER(&cvar);
            p_cvar = CVAR_GET_POINTER(p_name);
            UTIL_ServerPrint("[DEBUG] create_cvar(): is_created = %d, name = <%s>, value = <%s>\n", p_cvar != nullptr, p_cvar->name, p_cvar->string);
        }
        return p_cvar;
    }

public:
    cvar_list_it add(CPluginMngr::CPlugin *plugin, std::wstring name, std::wstring value, int flags = 0, std::wstring desc = L"", bool has_min = false, float min_val = 0.0f, bool has_max = false, float max_val = 0.0f)
    {
        if (name.empty() || value.empty())
            return cvar_list_it{};
        cvar_list_it cvar_it;
        cvar_list_t p_cvar_list;
        plugin_cvar_it plugin_it;
        std::string s = g_converter.to_bytes(value);
        std::transform(name.begin(), name.end(), name.begin(), std::bind(std::ptr_fun(&std::tolower<wchar_t>), std::locale("ru_RU.UTF-8")));
        
        // Is number?
        if (is_number(s))
        {
            value = stows(rtrim_zero_c(std::to_string(stof(s, has_min, min_val, has_max, max_val))));
            // std::wstring test = stows(num).get();
            UTIL_ServerPrint("[DEBUG] cvar_mngr::add(): new_value = %s\n", wstos(value).c_str());
        }
        // Plugin cvars exist?
        if ((plugin_it = cvars.plugin.find(plugin->getId())) != cvars.plugin.end())
        {
            UTIL_ServerPrint("[DEBUG] cvar_mngr::add(): plugin_it = %d\n", plugin_it);
            p_cvar_list = plugin_it->second;
            // Cvar exist?
            if ((cvar_it = p_cvar_list.find(name)) != p_cvar_list.end())
            {
                UTIL_ServerPrint("[DEBUG] cvar_mngr::add(): cvar exist allready!\n");
                return cvar_it;
            }
        }
        // Fill cvar
        m_cvar_t m_cvar;
        m_cvar.name = name;
        m_cvar.value = value;
        m_cvar.desc = desc;
        m_cvar.flags = flags;
        m_cvar.has_min = has_min;
        m_cvar.min_val = min_val;
        m_cvar.has_max = has_max;
        m_cvar.max_val = max_val;
        UTIL_ServerPrint("[DEBUG] cvar_mngr::add(): before create_var()\n");
        if ((m_cvar.cvar = create_cvar(m_cvar)) != nullptr)
        {
            auto result = p_cvar_list.insert({name, m_cvar});
            UTIL_ServerPrint("[DEBUG] cvar_mngr::add(): is_add = %d, name = <%s>, value = <%s>, desc = <%s>\n", result.second, m_cvar.name.c_str(), m_cvar.value.c_str(), wstos(m_cvar.desc).c_str());
            // Save cvars list
            if (result.second)
            {
                // Plugin cvars exist?
                if (plugin_it != cvars.plugin.end())
                    plugin_it->second = p_cvar_list;
                // Create plugin cvars
                else
                    cvars.plugin[plugin->getId()] = p_cvar_list;
                return result.first;
            }
        }
        else
            AMXX_LogError(plugin->getAMX(), AMX_ERR_NATIVE, "%s: cvar creation error <%s> => <%s>", __FUNCTION__, wstos(name).c_str(), wstos(value).c_str());
        return cvar_list_it{};
    }
    cvar_list_it get(CPluginMngr::CPlugin *plugin, std::wstring name)
    {
        plugin_cvar_it plugin_it;
        cvar_list_it cvar_it;
        cvar_list_t p_cvar_list;
        // Plugin cvars exist?
        if ((plugin_it = cvars.plugin.find(plugin->getId())) != cvars.plugin.end())
        {
            UTIL_ServerPrint("[DEBUG] cvar_mngr::get(): plugin_it = %d\n", plugin_it);
            p_cvar_list = plugin_it->second;
            // Cvar exist?
            if ((cvar_it = p_cvar_list.find(name)) != p_cvar_list.end())
                return cvar_it;
        }
        return cvar_list_it{};
    }
    void set(CPluginMngr::CPlugin *plugin, std::wstring name, std::wstring value)
    {
        auto cvar_it = get(plugin, name);
        check_it_empty(cvar_it);
        auto cvar = cvar_it->second.cvar;
        cvar_direct_set(cvar, wstos(value).c_str());
    }
    void set(CPluginMngr::CPlugin *plugin, cvar_list_it cvar_it, std::wstring value)
    {
        check_it_empty(cvar_it);
        auto cvar = cvar_it->second.cvar;
        cvar_direct_set(cvar, wstos(value).c_str());
    }
    void clear(CPluginMngr::CPlugin *plugin)
    {
        plugin_cvar_it plugin_it;
        cvar_list_t p_cvar_list;
        // Plugin cvars exist?
        if ((plugin_it = cvars.plugin.find(plugin->getId())) != cvars.plugin.end())
            plugin_it->second.clear();
    }
    void clear_all()
    {
        cvars.plugin.clear();
    }
};

extern cvar_mngr g_cvar_mngr;