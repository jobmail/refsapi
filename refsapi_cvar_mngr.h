#pragma once
#include "precompiled.h"

extern std::wstring_convert<convert_type, wchar_t> g_converter;

void Cvar_DirectSet_RH(IRehldsHook_Cvar_DirectSet *chain, cvar_t *var, const char *value);

typedef enum CVAR_TYPES_e
{
    CVAR_TYPE_NONE,
    CVAR_TYPE_NUM,
    CVAR_TYPE_FLOAT,
    CVAR_TYPE_STRING,
    // Count
    CVAR_TYPES_SIZE
} CVAR_TYPES_t;

typedef struct m_cvar_s
{
    cvar_t *cvar;
    std::wstring name;
    std::wstring value;
    std::wstring desc;
    //CVAR_TYPES_t type;
    CPluginMngr::CPlugin *plugin;
    int flags;
    bool has_min;
    bool has_max;
    float min_val;
    float max_val;
} m_cvar_t;

typedef struct ptr_bind_s
{
    cell* ptr;
    size_t size;
} ptr_bind_t;

typedef std::map<std::wstring, m_cvar_t> cvar_list_t;
typedef cvar_list_t::iterator cvar_list_it;

typedef std::map<int, std::list<cvar_list_it>> plugin_cvar_t;
typedef plugin_cvar_t::iterator plugin_cvar_it;

typedef std::map<cvar_t*, cvar_list_it> p_cvar_t;
typedef p_cvar_t::iterator p_cvar_it;

typedef std::map<cvar_t*, std::list<ptr_bind_t>> cvar_bind_t;
        
typedef cvar_bind_t::iterator cvar_bind_it;

typedef struct cvar_mngr_s
{
    cvar_list_t cvar_list;
    plugin_cvar_t plugin;    
    p_cvar_t p_cvar;
    cvar_bind_t bind;
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
        enum { _name, _value, _count };
        // Copy params
        std::string p[_count] = {wstos(c.name), wstos(c.value)};
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
    void bind(CPluginMngr::CPlugin *plugin, cvar_list_it cvar_it, cell *ptr, size_t size = 0)
    {
        check_it_empty_r(cvar_it);
        std::list<ptr_bind_t> bind_list;
        cvar_bind_it bind_it;
        ptr_bind_t bind;
        bool is_exist;
        // Bind exists?
        if (is_exist = (bind_it = cvars.bind.find(cvar_it->second.cvar)) != cvars.bind.end())
            bind_list = bind_it->second;
        // Fill bind
        bind.ptr = ptr;
        bind.size = size;
        // Bind exist?
        if (!bind_list.empty() && std::find(bind_list.begin(), bind_list.end(), bind) != bind_list.end()) {
            AMXX_LogError(plugin->getAMX(), AMX_ERR_NATIVE, "%s: bind cvar <%s> exists already", __FUNCTION__, wstos(cvar_it->first).c_str());
            return;
        }
        // Push bind
        bind_list.push_back(bind);
        // Update bind
        if (is_exist)
            bind_it->second = bind_list;
        else
            cvars.bind[cvar_it->second.cvar] = bind_list;
    }
    void on_change(cvar_list_it cvar_it, std::string &new_value)
    {
        check_it_empty_r(cvar_it);
        UTIL_ServerPrint("[DEBUG] on_change(): name = %s, old_value = %s, new_value = %s\n", wstos(cvar_it->second.name).c_str(), wstos(cvar_it->second.value).c_str(), new_value.c_str());
        cvar_bind_it bind_it;
        // Bind exists?
        if ((bind_it = cvars.bind.find(cvar_it->second.cvar)) != cvars.bind.end())
        {
            for (auto& bind : bind_it->second)
            {
                // Is number?
                if (bind.size == 0)
                    *bind.ptr = amx_ftoc(cvar_it->second.cvar->value);
                // Copy string
                else
                    Q_memcpy(bind.ptr, cvar_it->second.cvar->string, bind.size);
            }
        }
    }
    cvar_list_it add_exists(cvar_t *p_cvar, std::wstring desc = L"", bool has_min = false, float min_val = 0.0f, bool has_max = false, float max_val = 0.0f)
    {
        if (p_cvar == nullptr)
            return cvar_list_it{};
        // Fill cvar
        m_cvar_t m_cvar;
        m_cvar.name = stows(p_cvar->name);
        m_cvar.value = stows(p_cvar->string);
        //m_cvar.type = CVAR_TYPE_NONE;
        m_cvar.desc = desc;
        m_cvar.flags = p_cvar->flags;
        m_cvar.plugin = nullptr;
        m_cvar.has_min = has_min;
        m_cvar.min_val = min_val;
        m_cvar.has_max = has_max;
        m_cvar.max_val = max_val;
        // Save cvars list
        auto result = cvars.cvar_list.insert({ m_cvar.name, m_cvar });
        if (result.second)
        {
            UTIL_ServerPrint("[DEBUG] add_exists(): cvar = %d, name = <%s>, value = <%s>\n", m_cvar.cvar, wstos(m_cvar.name).c_str(), wstos(m_cvar.value).c_str());
            cvars.p_cvar.insert({ m_cvar.cvar, result.first });
            return result.first;
        }
        return cvar_list_it{};
    }
    cvar_list_it add(CPluginMngr::CPlugin *plugin, std::wstring name, std::wstring value, int flags = 0, std::wstring desc = L"", bool has_min = false, float min_val = 0.0f, bool has_max = false, float max_val = 0.0f)
    {
        if (name.empty() || value.empty() || plugin == nullptr)
            return cvar_list_it{};
        cvar_list_it cvar_it;
        plugin_cvar_it plugin_it;
        std::string s = wstos(value); //g_converter.to_bytes(value);
        // Fix caps in name
        ws_convert_tolower(name);
        // Is number?
        if (is_number(s))
        {
            value = stows(rtrim_zero_c(std::to_string(stof(s, has_min, min_val, has_max, max_val))));
            // std::wstring test = stows(num).get();
            UTIL_ServerPrint("[DEBUG] cvar_mngr::add(): new_value = %s\n", wstos(value).c_str());
        }
        // Cvar exist?
        if (((cvar_it = cvars.cvar_list.find(name)) != cvars.cvar_list.end()))
        {
            UTIL_ServerPrint("[DEBUG] cvar_mngr::add(): cvar exist allready!\n");
            return cvar_it;
        }
        // Fill cvar
        m_cvar_t m_cvar;
        m_cvar.name = name;
        m_cvar.value = value;
        //m_cvar.type = CVAR_TYPE_NONE;
        m_cvar.desc = desc;
        m_cvar.flags = flags;
        m_cvar.plugin = plugin;
        m_cvar.has_min = has_min;
        m_cvar.min_val = min_val;
        m_cvar.has_max = has_max;
        m_cvar.max_val = max_val;
        UTIL_ServerPrint("[DEBUG] cvar_mngr::add(): before create_var()\n");
        // Create cvar
        if ((m_cvar.cvar = create_cvar(m_cvar)) != nullptr)
        {
            // Save cvars list
            auto result = cvars.cvar_list.insert({ m_cvar.name, m_cvar });
            if (result.second)
            {
                UTIL_ServerPrint("[DEBUG] cvar_mngr::add(): is_add = %d, name = <%s>, value = <%s>, desc = <%s>\n", result.second, wstos(m_cvar.name).c_str(), wstos(m_cvar.value).c_str(), wstos(m_cvar.desc).c_str());
                // Plugin cvars exist?
                if ((plugin_it = cvars.plugin.find(plugin->getId())) != cvars.plugin.end())
                {
                    plugin_it->second.push_back(result.first);
                }
                // Create plugin cvars
                else
                {
                    cvars.plugin[plugin->getId()].push_back(result.first);
                }
                // Create p_cvar
                cvars.p_cvar.insert({ m_cvar.cvar, result.first });
                return result.first;
            }
        }
        else
            AMXX_LogError(plugin->getAMX(), AMX_ERR_NATIVE, "%s: cvar creation error <%s> => <%s>", __FUNCTION__, wstos(name).c_str(), wstos(value).c_str());
        return cvar_list_it{};
    }
    cvar_list_it get(std::wstring name)
    {
        cvar_list_it cvar_it;
        // Fix caps in name
        ws_convert_tolower(name);
        // Cvar exist?
        if ((cvar_it = cvars.cvar_list.find(name)) != cvars.cvar_list.end())
        {
            return cvar_it;
        }
        // Check global cvar
        else
        {
            std::string p = wstos(name);
            cvar_t *p_cvar = CVAR_GET_POINTER(p.data());
            // Cvar exist?
            if (p_cvar != nullptr)
            {
                return add_exists(p_cvar);
            }
        }
        return cvar_list_it{};
    }
    cvar_list_it get(cvar_t *p_cvar)
    {
        p_cvar_it cvar_it;
        // Cvar exist?
        if ((cvar_it = cvars.p_cvar.find(p_cvar)) != cvars.p_cvar.end())
        {
            return cvar_it->second;
        }
        return cvar_list_it{};
    }
    void set(std::wstring name, std::wstring value)
    {
        auto cvar_it = get(name);
        check_it_empty_r(cvar_it);
        auto cvar = cvar_it->second.cvar;
        cvar_direct_set(cvar, wstos(value).c_str());
    }
    void set(cvar_list_it cvar_it, std::wstring value)
    {
        check_it_empty_r(cvar_it);
        auto cvar = cvar_it->second.cvar;
        cvar_direct_set(cvar, wstos(value).c_str());
    }
    void clear_plugin(CPluginMngr::CPlugin *plugin)
    {
        plugin_cvar_it plugin_it;
        cvar_list_t p_cvar_list;
        // Plugin cvars exist?
        if ((plugin_it = cvars.plugin.find(plugin->getId())) != cvars.plugin.end())
            plugin_it->second.clear();
    }
    void clear_plugin_all()
    {
        cvars.plugin.clear();
    }
    void clear_cvar_list()
    {
        cvars.cvar_list.clear();
    }
    void clear_pcvar_all()
    {
        cvars.p_cvar.clear();
    }
    void clear_bind_all()
    {
        cvars.bind.clear();
    }
    void clear_all()
    {
        clear_plugin_all();
        clear_pcvar_all();
        clear_cvar_list();
        clear_bind_all();
    }
};

extern cvar_mngr g_cvar_mngr;