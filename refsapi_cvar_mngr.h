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

typedef struct cvar_hook_s
{
    int fwd;
    bool is_enable;
} cvar_hook_t;

typedef std::map<std::wstring, m_cvar_t> cvar_list_t;
typedef cvar_list_t::iterator cvar_list_it;

typedef std::map<int, std::list<cvar_list_it>> plugin_cvar_t;
typedef plugin_cvar_t::iterator plugin_cvar_it;

typedef std::map<cvar_t*, cvar_list_it> p_cvar_t;
typedef p_cvar_t::iterator p_cvar_it;

typedef std::map<cvar_t*, std::list<ptr_bind_t>> cvar_bind_t;
typedef cvar_bind_t::iterator cvar_bind_it;

typedef std::map<int, bool> cvar_hook_state_t;
typedef cvar_hook_state_t::iterator cvar_hook_state_it;

typedef std::map<cvar_t*, std::list<cvar_hook_state_it>> cvar_hook_list_t;
typedef cvar_hook_list_t::iterator cvar_hook_list_it;

typedef struct cvar_mngr_s
{
    cvar_list_t cvar_list;
    plugin_cvar_t plugin;    
    p_cvar_t p_cvar;
    cvar_bind_t bind_list;
    cvar_hook_state_t hook_state;
    cvar_hook_list_t cvar_hook_list;
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
                *bind->ptr = (int)std::stof(cvar->string);
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
        // Value not changed?
        if (m_cvar->value.compare(stows(value)) == 0)
            return;
        // Bind non-exists?
        if (m_cvar->type == CVAR_TYPE_NONE)
        {
            //if (strcmp(cvar->name, "mp_timeleft") != 0)
            //    UTIL_ServerPrint("[DEBUG] on_direct_set(): NOT BIND => type = %d, name = <%s>, string = <%s>, value = %f\n", m_cvar->type, cvar->name, cvar->string, cvar->value);
            // Save new value
            m_cvar->value = stows(value);
            return;
        }
        // Check range
        if (!check_range(m_cvar))
            return;
        UTIL_ServerPrint("[DEBUG] on_direct_set(): cvar_t = %d, cvar_list = %d <===\n", cvar, cvar_list->second.cvar);
        // Do event
        on_change(cvar_list, value);
        // Save new value
        m_cvar->value = stows(value);
    }
    void bind(CPluginMngr::CPlugin *plugin, CVAR_TYPES_t type, cvar_list_it cvar_list, cell *ptr, size_t size = 0)
    {
        check_it_empty_r(cvar_list);
        m_cvar_t* m_cvar = &cvar_list->second;
        // Check previos type of m_cvar
        if (m_cvar->type != CVAR_TYPE_NONE && m_cvar->type != type)
        {
            AMXX_LogError(plugin->getAMX(), AMX_ERR_NATIVE, "%s: cvar <%s> is already binded with type = %d\n", __FUNCTION__, wstos(cvar_list->first).c_str(), m_cvar->type);
            return;
        }
        std::list<ptr_bind_t> bind_list;
        cvar_bind_it bind_it;
        ptr_bind_t ptr_bind;
        bool is_exist;
        // Bind exists?
        if (is_exist = (bind_it = cvars.bind_list.find(m_cvar->cvar)) != cvars.bind_list.end())
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
            cvars.bind_list[m_cvar->cvar] = bind_list;
        UTIL_ServerPrint("[DEBUG] bind(): type = %d, name = %s, value = <%s>, cvar = %d, size = %d\n", m_cvar->type, m_cvar->cvar->name, m_cvar->cvar->string, m_cvar->cvar, size);
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
        if ((bind_it = cvars.bind_list.find(m_cvar->cvar)) != cvars.bind_list.end())
        {
            UTIL_ServerPrint("[DEBUG] on_change(): name = <%s>, old_value = <%s>, new_value = <%s>\n", wstos(m_cvar->name).c_str(), wstos(m_cvar->value).c_str(), new_value.c_str());
            for (auto& bind : bind_it->second)
                copy_bind(&bind, m_cvar->cvar);
        }
        // Hook exists?
        cvar_hook_list_it hook_it;
        if ((hook_it = cvars.cvar_hook_list.find(m_cvar->cvar)) != cvars.cvar_hook_list.end())
        {
            UTIL_ServerPrint("[DEBUG] on_change(): hooook!!!!!!!!!!!!!\n");
            for (auto& h : hook_it->second)
            {
                UTIL_ServerPrint("[DEBUG] on_change(): exec hook = %d, enabled = %d\n", h->first, h->second);
                if (h->second)
                    g_amxxapi.ExecuteForward(h->first);
            }
        } else
            UTIL_ServerPrint("[DEBUG] on_change(): hook for cvar <%d> not found !!!\n", m_cvar->cvar);
    }
    cvar_list_it add_exists(cvar_t *cvar, std::wstring desc = L"", bool has_min = false, float min_val = 0.0f, bool has_max = false, float max_val = 0.0f)
    {
        // Cvar not empty?
        if (cvar != nullptr)
        {
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
        }
        return cvar_list_it{};
    }
    cvar_list_it add(CPluginMngr::CPlugin *plugin, std::wstring name, std::wstring value, int flags = 0, std::wstring desc = L"", bool has_min = false, float min_val = 0.0f, bool has_max = false, float max_val = 0.0f)
    {
        if (!(name.empty() || value.empty()))
        {
            cvar_t *cvar = create_cvar(name, value, flags);
            UTIL_ServerPrint("[DEBUG] add(): cvar = %d\n", cvar);
            cvar_list_it cvar_list = add_exists(cvar, desc, has_min, min_val, has_max, max_val);
            if (!check_it_empty(cvar_list))
            {
                // Set m_cvar
                m_cvar_t* m_cvar = &cvar_list->second;
                //UTIL_ServerPrint("[DEBUG] add(): has_min = %d, min_val = %f, has_max = %d, max_val = %f\n", m_cvar->has_min, m_cvar->min_val, m_cvar->has_max, m_cvar->max_val);
                plugin_cvar_it plugin_cvar;
                // Plugin cvars exist?
                if ((plugin_cvar = cvars.plugin.find(plugin->getId())) != cvars.plugin.end())
                    plugin_cvar->second.push_back(cvar_list);
                // Create plugin cvars
                else
                    cvars.plugin[plugin->getId()].push_back(cvar_list);
                return cvar_list;
            }
        }
        return cvar_list_it{};
    }
    cvar_list_it get(std::wstring name)
    {
        if (!name.empty())
        {
            cvar_list_it cvar_list;
            // Fix caps in name
            ws_convert_tolower(name);
            // Cvar exist?
            if ((cvar_list = cvars.cvar_list.find(name)) != cvars.cvar_list.end())
                return cvar_list;
            // Check global cvar
            cvar_t *p_cvar = CVAR_GET_POINTER(wstos(name).data());
            // Cvar exist?
            if (p_cvar != nullptr)
                return add_exists(p_cvar);
        }
        return cvar_list_it{};
    }
    cvar_list_it get(cvar_t *cvar)
    {
        p_cvar_it p_cvar;
        // Cvar exists?
        return cvar == nullptr ? cvar_list_it{} : (p_cvar = cvars.p_cvar.find(cvar)) != cvars.p_cvar.end() ? p_cvar->second : get(stows(cvar->name));
    }
    cell get(CVAR_TYPES_t type, cvar_list_it cvar_list, void* ptr, size_t ptr_size)
    {
        cell result = FALSE;
        // Cvar exists?
        if (!check_it_empty(cvar_list))
        {   
            m_cvar_t* m_cvar = &cvar_list->second;
            switch (type)
            {
                case CVAR_TYPE_NUM:
                    result = (int)m_cvar->cvar->value; //std::stof(cvar->string);
                    break;
                case CVAR_TYPE_FLT:
                    result = amx_ftoc(m_cvar->cvar->value);
                    break;
                case CVAR_TYPE_STR:
                    result = std::min(ptr_size, m_cvar->value.size());
                    Q_memcpy(ptr, m_cvar->value.data(), result << 2);
                    break;
            }
        }
        return result;
    }
    cell get(CVAR_TYPES_t type, cvar_t* cvar, void* ptr, size_t ptr_size)
    {
        return get(type, get(cvar), ptr, ptr_size);
    }
    cell get(CVAR_TYPES_t type, std::wstring name, void* ptr, size_t ptr_size)
    {
        return get(type, name, ptr, ptr_size);
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
    cvar_hook_state_it create_hook(int fwd, cvar_list_it cvar_list, bool is_enable = true)
    {
        if (!check_it_empty(cvar_list))
        {
            UTIL_ServerPrint("[DEBUG] create_hook(): fwd = %d, cvar = %d\n", fwd, cvar_list->second.cvar);
            auto result = cvars.hook_state.insert({ fwd, is_enable });
            if (result.second)
            {
                cvar_hook_list_it hook_it;
                m_cvar_t* m_cvar = &cvar_list->second;
                if ((hook_it = cvars.cvar_hook_list.find(m_cvar->cvar)) != cvars.cvar_hook_list.end())
                    hook_it->second.push_back(result.first);
                else
                    cvars.cvar_hook_list[m_cvar->cvar].push_back(result.first);
                //UTIL_ServerPrint("\n[DEBUG] create_hook(): fwd = %d, hook = %d, cvar = %d, name = <%s>, value = <%s>, enabled = %d\n\n", fwd, result.first, cvar_list->second.cvar, cvar_list->second.cvar->name, cvar_list->second.cvar->string, is_enable);
                return result.first;
            }
        }
        return cvar_hook_state_it{};
    }
    bool cvar_hook_state(int fwd, bool is_enable)
    {
        bool result;
        cvar_hook_state_it hook_state_it;
        // Hook exists?
        if (result = (hook_state_it = cvars.hook_state.find(fwd)) != cvars.hook_state.end())
            hook_state_it->second = is_enable;
        return result;
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
    void clear_bind_list()
    {
        cvars.bind_list.clear();
    }
    void clear_hook_list()
    {
        cvars.cvar_hook_list.clear();
    }
    void clear_hook_state()
    {
        cvars.hook_state.clear();
    }
    void clear_all()
    {
        clear_plugin_all();
        clear_pcvar_all();
        clear_cvar_list();
        clear_bind_list();
        clear_hook_list();
        clear_hook_state();
    }
};

extern cvar_mngr g_cvar_mngr;