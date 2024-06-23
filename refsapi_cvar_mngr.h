#pragma once

#include "precompiled.h"

extern std::wstring_convert<convert_type, wchar_t> g_converter;

extern void Cvar_RegisterVariable_Post(cvar_t *cvar);
extern void Cvar_DirectSet_RH(IRehldsHook_Cvar_DirectSet *chain, cvar_t *var, const char *value);
extern void Cvar_DirectSet_Post(cvar_t *cvar, const char *value);
extern void CVarRegister_Post(cvar_t *pCvar);
extern void CVarSetFloat_Post(const char *szVarName, float flValue);
extern void CVarSetString_Post(const char *szVarName, const char *szValue);

typedef enum CVAR_TYPES_e
{
    CVAR_TYPE_NONE,
    CVAR_TYPE_NUM,
    CVAR_TYPE_FLT,
    CVAR_TYPE_STR,
    // Count
    CVAR_TYPES_SIZE
} CVAR_TYPES_t;

typedef struct m_cvar_s
{
    cvar_t *cvar;
    std::wstring name;
    std::wstring value;
    std::wstring desc;
    CVAR_TYPES_t type;
    //CPluginMngr::CPlugin *plugin;
    int flags;
    bool has_min;
    bool has_max;
    float min_val;
    float max_val;
} m_cvar_t;

typedef struct ptr_bind_s
{
    cell* ptr;
    uint8_t type;
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
    bool check_range(m_cvar_t *m_cvar)
    {
        UTIL_ServerPrint("[DEBUG] check_range(): type = %d, name = <%s>, value = <%s>\n", m_cvar->type, m_cvar->cvar->name, m_cvar->cvar->string);
        if (m_cvar->type == CVAR_TYPE_NONE || m_cvar->type == CVAR_TYPE_STR)
            return true;
        // Is number?
        std::string s = m_cvar->cvar->string;
        bool is_num = is_number(s);
        if (!is_num)
        {
            // Fix wrong value for bind
            UTIL_ServerPrint("[DEBUG] check_range(): wrong non-number value <%s>\n", s.c_str());
            CVAR_SET_STRING(m_cvar->cvar->name, "0");
            return false;
        }
        bool is_override = false;
        auto result = std::stod(s);
        // Check bind type and conver
        if (m_cvar->type == CVAR_TYPE_NUM)
            result = result >= 0.0 ? (int)result : -(int)(-result);
        UTIL_ServerPrint("[DEBUG] check_range(): in = %s, out = %f\n", s.c_str(), result);
        UTIL_ServerPrint("[DEBUG] check_range(): has_min = %d, min_val = %f, has_max = %d, max_val = %f\n", m_cvar->has_min, m_cvar->min_val, m_cvar->has_max, m_cvar->max_val);
        // Check override
        if (is_override |= m_cvar->has_min && result < m_cvar->min_val)
            result = m_cvar->min_val;
        else if (is_override |= m_cvar->has_max && result > m_cvar->max_val)
            result = m_cvar->max_val;
        // Fix overriden
        if (is_override)
        {
            UTIL_ServerPrint("[DEBUG] check_range(): override = %s, new_string = %f\n", s.c_str(), result);
            CVAR_SET_FLOAT(wstos(m_cvar->name).c_str(), result);
        }
        return !is_override;
    }
    void copy_bind(ptr_bind_t *bind, cvar_t *cvar)
    {
        switch (bind->type)
        {
            case CVAR_TYPE_NUM:
                *bind->ptr = (int)roundd(std::stod(cvar->string), 0);
                break;
            case CVAR_TYPE_FLT:
                *bind->ptr = amx_ftoc(cvar->value);
                break;
            case CVAR_TYPE_STR:
                UTIL_ServerPrint("[DEBUG] copy_bind(): from = %s, size = %d\n", cvar->string, bind->size);
                setAmxString(bind->ptr, cvar->string, bind->size);
                break;
        }
    }
    void cvar_direct_set(cvar_t *cvar, const char *value)
    {
        if (cvar != nullptr)
            g_engfuncs.pfnCvar_DirectSet(cvar, value);
    }
    cvar_t* create_cvar(std::wstring name, std::wstring value, int flags = 0)
    {   
        enum { _name, _value, _count };
        // Copy params
        std::string p[_count] = {wstos(name), wstos(value)};
        auto p_name = p[_name].data();
        auto p_value = p[_value].data();
        cvar_t *p_cvar = CVAR_GET_POINTER(p_name);
        // Cvar non-exists?
        if (p_cvar == nullptr)
        {
            // Create new cvar
            cvar_t cvar;
            cvar.name = p_name;
            cvar.string = p_value;
            cvar.value = 0.0f;
            cvar.flags = flags;
            cvar.next = nullptr;
            UTIL_ServerPrint("[DEBUG] create_cvar(): name = <%s>, value = <%s>\n", cvar.name, cvar.string);
            CVAR_REGISTER(&cvar);
            p_cvar = CVAR_GET_POINTER(p_name);
            UTIL_ServerPrint("[DEBUG] create_cvar(): is_created = %d\n", p_cvar != nullptr);
        }
        return p_cvar;
    }

public:
    void on_register(cvar_t *cvar)
    {
        cvar_list_it cvar_list = get(cvar);
        // Cvar not register?
        if (check_it_empty(cvar_list))
            add_exists(cvar);
    }
    void on_direct_set(cvar_t *cvar, std::string value)
    {
        cvar_list_it cvar_list = get(cvar);
        check_it_empty_r(cvar_list);
        // Get m_cvar
        m_cvar_t* m_cvar = &cvar_list->second;
        // Bind non-exists?
        if (m_cvar->type == CVAR_TYPE_NONE)
        {
            //if (strcmp(cvar->name, "mp_timeleft") != 0)
            //    UTIL_ServerPrint("[DEBUG] on_direct_set(): NOT BIND => type = %d, name = <%s>, string = <%s>, value = %f\n", m_cvar->type, cvar->name, cvar->string, cvar->value);
            m_cvar->value = stows(value);
            return;
        }
        // Check range
        if (!check_range(m_cvar))
            return;
        // Do event
        on_change(cvar_list, value);
    }
    void bind(CPluginMngr::CPlugin *plugin, CVAR_TYPES_t type, cvar_list_it cvar_list, cell *ptr, size_t size = 0)
    {
        check_it_empty_r(cvar_list);
        m_cvar_t* m_cvar = &cvar_list->second;
        // Check previos type of m_cvar
        if (m_cvar->type != CVAR_TYPE_NONE && m_cvar->type != type)
        {
            AMXX_LogError(plugin->getAMX(), AMX_ERR_NATIVE, "%s: cvar <%s> is already binded with type = %d, tried to set type = %d\n", __FUNCTION__, wstos(cvar_list->first).c_str(), m_cvar->type, type);
            return;
        }
        std::list<ptr_bind_t> bind_list;
        cvar_bind_it bind_it;
        ptr_bind_t ptr_bind;
        bool is_exist;
        // Bind exists?
        if (is_exist = (bind_it = cvars.bind.find(m_cvar->cvar)) != cvars.bind.end())
            bind_list = bind_it->second;
        // Bind not empty?
        if (!bind_list.empty())
        {
            // Finding exists record with same (ptr & size)
            auto result = std::find_if(bind_list.begin(), bind_list.end(), [&](ptr_bind_t b)
            {
                return (b.ptr == ptr); // && (b.size = size);
            });
            if (result != bind_list.end()) {
                AMXX_LogError(plugin->getAMX(), AMX_ERR_NATIVE, "%s: cvar <%s> is already binded on this global variable\n", __FUNCTION__, wstos(cvar_list->first).c_str());
                return;
            }
        }
        // Fill bind
        ptr_bind.ptr = ptr;
        ptr_bind.size = size;
        ptr_bind.type = type;
        // Push bind
        bind_list.push_back(ptr_bind);
        copy_bind(&ptr_bind, m_cvar->cvar);
        // Update bind
        if (is_exist)
            bind_it->second = bind_list;
        else
            cvars.bind[m_cvar->cvar] = bind_list;
        UTIL_ServerPrint("[DEBUG] bind(): type = %d, name = %s, value = <%s>, size = %d\n", m_cvar->type, m_cvar->cvar->name, m_cvar->cvar->string, size);
        // Set m_cvar type
        m_cvar->type = type;
        // Check range
        check_range(m_cvar);
    }
    void on_change(cvar_list_it cvar_list, std::string &new_value)
    {
        check_it_empty_r(cvar_list);
        m_cvar_t* m_cvar = &cvar_list->second;
        cvar_bind_it bind_it;
        // Bind exists?
        if ((bind_it = cvars.bind.find(m_cvar->cvar)) != cvars.bind.end())
        {
            UTIL_ServerPrint("[DEBUG] on_change(): name = <%s>, old_value = <%s>, new_value = <%s>\n", wstos(m_cvar->name).c_str(), wstos(m_cvar->value).c_str(), new_value.c_str());
            for (auto& bind : bind_it->second)
                copy_bind(&bind, m_cvar->cvar);
        }
    }
    cvar_list_it add_exists(cvar_t *cvar, std::wstring desc = L"", bool has_min = false, float min_val = 0.0f, bool has_max = false, float max_val = 0.0f)
    {
        if (cvar == nullptr)
            return cvar_list_it{};
        // Cvar already exists?
        p_cvar_it p_cvar;
        if ((p_cvar = cvars.p_cvar.find(cvar)) != cvars.p_cvar.end())
            return p_cvar->second;
        // Fill cvar
        m_cvar_t m_cvar;
        m_cvar.cvar = cvar;
        m_cvar.name = stows(cvar->name);
        m_cvar.value = stows(cvar->string);
        m_cvar.type = CVAR_TYPE_NONE;
        m_cvar.desc = desc;
        m_cvar.flags = cvar->flags;
        //m_cvar.plugin = plugin;
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
        cvar_t *cvar = create_cvar(name, value, flags);
        UTIL_ServerPrint("[DEBUG] add(): cvar = %d\n", cvar);
        cvar_list_it cvar_list = add_exists(cvar, desc, has_min, min_val, has_max, max_val);
        if (check_it_empty(cvar_list))
        {
            AMXX_LogError(plugin->getAMX(), AMX_ERR_NATIVE, "%s: m_cvar is empty\n", __FUNCTION__);
            return cvar_list_it{};
        }
        //UTIL_ServerPrint("[DEBUG] add(): check cvar = %d\n", cvar_list->second.cvar);
        // Set m_cvar
        m_cvar_t* m_cvar = &cvar_list->second;
        //UTIL_ServerPrint("[DEBUG] add(): has_min = %d, min_val = %f, has_max = %d, max_val = %f\n", m_cvar->has_min, m_cvar->min_val, m_cvar->has_max, m_cvar->max_val);
        // Plugin cvars exist?
        plugin_cvar_it plugin_cvar;
        if ((plugin_cvar = cvars.plugin.find(plugin->getId())) != cvars.plugin.end())
            plugin_cvar->second.push_back(cvar_list);
        // Create plugin cvars
        else
            cvars.plugin[plugin->getId()].push_back(cvar_list);
        return cvar_list;
    }
    cvar_list_it get(std::wstring name)
    {
        if (name.empty())
            return cvar_list_it{};
        cvar_list_it cvar_list;
        // Fix caps in name
        ws_convert_tolower(name);
        // Cvar exist?
        if ((cvar_list = cvars.cvar_list.find(name)) != cvars.cvar_list.end())
            return cvar_list;
        // Check global cvar
        cvar_t *p_cvar = CVAR_GET_POINTER(wstos(name).data());
        // Cvar exist?
        return p_cvar != nullptr ? add_exists(p_cvar) : cvar_list_it{};
    }
    cvar_list_it get(cvar_t *cvar)
    {
        p_cvar_it p_cvar;
        // Cvar exists?
        return cvar == nullptr ? cvar_list_it{} : (p_cvar = cvars.p_cvar.find(cvar)) != cvars.p_cvar.end() ? p_cvar->second : get(stows(cvar->name));
    }
    void set(std::wstring name, std::wstring value)
    {
        auto cvar_list = get(name);
        check_it_empty_r(cvar_list);
        auto cvar = cvar_list->second.cvar;
        cvar_direct_set(cvar, wstos(value).c_str());
    }
    void set(cvar_list_it cvar_list, std::wstring value)
    {
        check_it_empty_r(cvar_list);
        auto cvar = cvar_list->second.cvar;
        cvar_direct_set(cvar, wstos(value).c_str());
    }
    void clear_plugin(CPluginMngr::CPlugin *plugin)
    {
        plugin_cvar_it plugin_cvar;
        // Plugin cvars exist?
        if ((plugin_cvar = cvars.plugin.find(plugin->getId())) != cvars.plugin.end())
            plugin_cvar->second.clear();
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