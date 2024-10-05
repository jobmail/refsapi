#pragma once

#include "precompiled.h"

extern std::wstring_convert<convert_type, wchar_t> g_converter;

extern void Cvar_DirectSet_RH(IRehldsHook_Cvar_DirectSet *chain, cvar_t *var, const char *value);
extern void CVarRegister_Post(cvar_t *pCvar);

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
    int flags;
    bool has_min;
    bool has_max;
    float min_val;
    float max_val;
} m_cvar_t;

typedef struct ptr_bind_s
{
    cell *ptr;
    uint8_t type;
    size_t size;
} ptr_bind_t;

typedef struct cvar_hook_s
{
    int fwd;
    bool is_enable;
} cvar_hook_t;

typedef std::map<std::wstring, std::wstring> load_cvars_t;

typedef std::map<std::wstring, m_cvar_t *> cvar_list_t;
typedef cvar_list_t::iterator cvar_list_it;

typedef std::map<int, std::list<m_cvar_t *>> plugin_cvar_t;
typedef plugin_cvar_t::iterator plugin_cvars_it;

typedef std::map<cvar_t *, m_cvar_t *> p_cvar_t;
typedef p_cvar_t::iterator p_cvar_it;

typedef std::map<m_cvar_t *, std::list<ptr_bind_t>> cvar_bind_t;
typedef cvar_bind_t::iterator cvar_bind_it;

typedef std::map<int, bool> cvar_hook_state_t;
typedef cvar_hook_state_t::iterator cvar_hook_state_it;

typedef std::map<cvar_t *, std::list<cvar_hook_state_it>> cvar_hook_list_t;
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
        // UTIL_ServerPrint("[DEBUG] check_range(): type = %d, name = <%s>, value = <%s>\n", m_cvar->type, m_cvar->cvar->name, m_cvar->cvar->string);
        if (m_cvar->type == CVAR_TYPE_NONE || m_cvar->type == CVAR_TYPE_STR)
            return true;
        // Is number?
        std::string s = m_cvar->cvar->string;
        if (!is_number(s))
        {
            // Fix wrong value for bind
            // UTIL_ServerPrint("[DEBUG] check_range(): wrong non-number value <%s>\n", s.c_str());
            CVAR_SET_STRING(m_cvar->cvar->name, "0");
            return false;
        }
        bool is_override = false;
        auto result = std::stod(s);
        // Check bind type and conver
        if (m_cvar->type == CVAR_TYPE_NUM)
            result = result >= 0.0 ? (int)result : -(int)(-result);
        // UTIL_ServerPrint("[DEBUG] check_range(): in = %s, out = %f\n", s.c_str(), result);
        // UTIL_ServerPrint("[DEBUG] check_range(): has_min = %d, min_val = %f, has_max = %d, max_val = %f\n", m_cvar->has_min, m_cvar->min_val, m_cvar->has_max, m_cvar->max_val);
        //  Check override
        if ((is_override |= (m_cvar->has_min && result < m_cvar->min_val)))
            result = m_cvar->min_val;
        else if ((is_override |= (m_cvar->has_max && result > m_cvar->max_val)))
            result = m_cvar->max_val;
        // Fix overriden
        if (is_override)
        {
            // UTIL_ServerPrint("[DEBUG] check_range(): override = %s, new_string = %f\n", s.c_str(), result);
            CVAR_SET_FLOAT(wstos(m_cvar->name).c_str(), result);
        }
        return !is_override;
    }
    void copy_bind(ptr_bind_t *bind, cvar_t *cvar)
    {
        // UTIL_ServerPrint("[DEBUG] copy_bind(): from = %s, type = %d, size = %d\n", cvar->string, bind->type, bind->size);
        switch (bind->type)
        {
        case CVAR_TYPE_NUM:
            *bind->ptr = (int)std::stod(cvar->string);
            break;
        case CVAR_TYPE_FLT:
            *bind->ptr = amx_ftoc(cvar->value);
            break;
        case CVAR_TYPE_STR:
            setAmxString(bind->ptr, cvar->string, bind->size);
        }
    }
    cvar_t *create_cvar(std::wstring name, std::wstring value, int flags = 0)
    {
        enum
        {
            _name,
            _value,
            _count
        };
        // Copy params
        std::string p[_count] = {wstos(name), wstos(value)};
        auto p_name = p[_name].data();
        auto p_value = p[_value].data();
        cvar_t *cvar = CVAR_GET_POINTER(p_name);
        // Cvar non-exists?
        if (cvar == nullptr)
        {
            // Create new cvar
            cvar_t c;
            c.name = p_name;
            c.string = p_value;
            c.value = 0.0f;
            c.flags = flags;
            c.next = nullptr;
            // UTIL_ServerPrint("[DEBUG] create_cvar(): name = <%s>, value = <%s>\n", c.name, c.string);
            CVAR_REGISTER(&c);
            cvar = CVAR_GET_POINTER(p_name);
            // UTIL_ServerPrint("[DEBUG] create_cvar(): is_created = %d\n", cvar != nullptr);
        }
        return cvar;
    }

public:
    bool need_update(load_cvars_t &load_cvars, plugin_cvars_it plugin_cvars)
    {
        // UTIL_ServerPrint("[DEBUG] need_update(): plugin_cvars = %d, load_cvars = %d\n", plugin_cvars->second.size(), load_cvars.size());
        for (auto m_cvar : plugin_cvars->second)
        {
            if (load_cvars.find(m_cvar->name) == load_cvars.end())
                return true;
        }
        return plugin_cvars->second.size() == 0 || plugin_cvars->second.size() != load_cvars.size();
    }
    void on_direct_set(cvar_t *cvar, std::wstring value)
    {
        auto m_cvar = get(cvar);
        // ("[DEBUG] on_direct_set(): cvar = %p, m_cvar = %p\n", cvar, m_cvar);
        if (m_cvar == nullptr)
            return;
        // Value not changed?
        if (m_cvar->value.compare(value) == 0)
            return;
        // Bind non-exists?
        if (m_cvar->type == CVAR_TYPE_NONE)
        {
            // if (strcmp(cvar->name, "mp_timeleft") != 0)
            //     UTIL_ServerPrint("[DEBUG] on_direct_set(): NOT BIND => type = %d, name = <%s>, string = <%s>, value = %f\n", m_cvar->type, cvar->name, cvar->string, cvar->value);
            //   Save new value
            m_cvar->value = value;
            return;
        }
        // Check range
        if (!check_range(m_cvar))
            return;
        //  Do event
        on_change(m_cvar, value);
        // Save new value
        m_cvar->value = value;
    }
    void bind(CPluginMngr::CPlugin *plugin, CVAR_TYPES_t type, m_cvar_t *m_cvar, cell *ptr, size_t size = 0)
    {
        if (m_cvar == nullptr)
            return;
        // Check previos type of m_cvar
        if (m_cvar->type != CVAR_TYPE_NONE && m_cvar->type != type)
        {
            AMXX_LogError(plugin->getAMX(), AMX_ERR_NATIVE, "%s: cvar <%s> is already binded with type = %d\n", __FUNCTION__, wstos(m_cvar->name).c_str(), m_cvar->type);
            return;
        }
        bool is_exist;
        ptr_bind_t ptr_bind;
        std::list<ptr_bind_t> bind_list;
        auto bind_it = cvars.bind_list.find(m_cvar);
        // Bind exists?
        if (is_exist = bind_it != cvars.bind_list.end())
            bind_list = bind_it->second;
        // Bind not empty?
        if (is_exist && !bind_list.empty())
        {
            // Finding exists record with same (ptr & size)
            auto result = std::find_if(bind_list.begin(), bind_list.end(), [&](ptr_bind_t b)
                                       {
                                           return (b.ptr == ptr); // && (b.size = size);
                                       });
            if (result != bind_list.end())
            {
                AMXX_LogError(plugin->getAMX(), AMX_ERR_NATIVE, "%s: cvar <%s> is already binded on this global variable\n", __FUNCTION__, wstos(m_cvar->name).c_str());
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
        // Update binds
        if (is_exist)
            bind_it->second = bind_list;
        else
            cvars.bind_list[m_cvar] = bind_list;
        // UTIL_ServerPrint("[DEBUG] bind(): type = %d, name = %s, value = <%s>, cvar = %d, size = %d\n", m_cvar->type, m_cvar->cvar->name, m_cvar->cvar->string, m_cvar->cvar, size);
        //  Set m_cvar type
        m_cvar->type = type;
        // Check range
        check_range(m_cvar);
    }
    void on_change(m_cvar_t *m_cvar, std::wstring &new_value)
    {
        auto bind_it = cvars.bind_list.find(m_cvar);
        // Bind exists?
        if (bind_it != cvars.bind_list.end())
        {
            // UTIL_ServerPrint("[DEBUG] on_change(): name = <%s>, old_value = <%s>, new_value = <%s>\n", wstos(m_cvar->name).c_str(), wstos(m_cvar->value).c_str(), wstos(new_value).c_str());
            for (auto &bind : bind_it->second)
                copy_bind(&bind, m_cvar->cvar);
        }
        // Hook exists?
        auto hook_it = cvars.cvar_hook_list.find(m_cvar->cvar);
        if (hook_it != cvars.cvar_hook_list.end())
        {
            for (auto &h : hook_it->second)
            {
                // UTIL_ServerPrint("[DEBUG] on_change(): exec hook = %d, enabled = %d\n", h->first, h->second);
                if (h->second)
                    g_amxxapi.ExecuteForward(h->first, (cell)((void *)(m_cvar)), wstos(m_cvar->value).c_str(), wstos(new_value).c_str());
            }
        }
    }
    m_cvar_t *add_exists(cvar_t *cvar, std::wstring desc = L"", bool has_min = false, float min_val = 0.0f, bool has_max = false, float max_val = 0.0f)
    {
        // Cvar not empty?
        if (cvar != nullptr)
        {
            // Cvar already exists?
            auto cvar_it = cvars.p_cvar.find(cvar);
            if (cvar_it != cvars.p_cvar.end())
                return cvar_it->second;
            // Fill cvar
            m_cvar_t *m_cvar = new m_cvar_t;
            m_cvar->cvar = cvar;
            m_cvar->name = stows(cvar->name);
            m_cvar->value = stows(cvar->string);
            m_cvar->type = CVAR_TYPE_NONE;
            m_cvar->desc = desc;
            m_cvar->flags = cvar->flags;
            m_cvar->has_min = has_min;
            m_cvar->min_val = min_val;
            m_cvar->has_max = has_max;
            m_cvar->max_val = max_val;
            // Save cvars list
            cvars.cvar_list.insert({m_cvar->name, m_cvar});
            cvars.p_cvar.insert({m_cvar->cvar, m_cvar});
            // UTIL_ServerPrint("[DEBUG] add_exists(): cvar = %d, name = <%s>, value = <%s>\n", m_cvar->cvar, wstos(m_cvar->name).c_str(), wstos(m_cvar->value).c_str());
            return m_cvar;
        }
        return nullptr;
    }
    m_cvar_t *add(int plugin_id, std::wstring name, std::wstring value, int flags = 0, std::wstring desc = L"", bool has_min = false, float min_val = 0.0f, bool has_max = false, float max_val = 0.0f)
    {
        if (!name.empty())
        {
            auto m_cvar = get(name);
            if (m_cvar == nullptr)
                m_cvar = add_exists(create_cvar(name, value, flags), desc, has_min, min_val, has_max, max_val);
            if (m_cvar != nullptr && !desc.empty())
            {
                auto plugin_cvars = cvars.plugin.find(plugin_id);
                if (plugin_cvars != cvars.plugin.end())
                {
                    auto pc = &plugin_cvars->second;
                    auto result = std::find(pc->begin(), pc->end(), m_cvar);
                    // If cvar not in list
                    if (result == pc->end())
                        pc->push_back(m_cvar);
                }
                else
                    cvars.plugin[plugin_id].push_back(m_cvar);
            }
            return m_cvar;
        }
        return nullptr;
    }
    plugin_cvars_it get(int plugin_id)
    {
        auto plugin_cvars = cvars.plugin.find(plugin_id);
        // Plugin cvars exist?
        return plugin_cvars != cvars.plugin.end() ? plugin_cvars : plugin_cvars_it{};
    }
    m_cvar_t *get(cvar_t *cvar)
    {
        auto m_cvar_it = cvars.p_cvar.find(cvar);
        return m_cvar_it != cvars.p_cvar.end() ? m_cvar_it->second : add_exists(cvar);
    }
    m_cvar_t *get(std::wstring name)
    {
        if (!name.empty())
        {
            // Fix caps in name
            ws_convert_tolower(name);
            auto m_cvar_it = cvars.cvar_list.find(name);
            // Cvar exist?
            if (m_cvar_it != cvars.cvar_list.end())
                return m_cvar_it->second;
            // Check global cvar
            cvar_t *cvar = CVAR_GET_POINTER(wstos(name).c_str());
            // Cvar exist?
            if (cvar != nullptr)
                return add_exists(cvar);
        }
        return nullptr;
    }
    cell get(CVAR_TYPES_t type, m_cvar_t *m_cvar, cell *ptr, size_t ptr_size)
    {
        cell result = 0;
        // Cvar exists?
        if (m_cvar != nullptr)
        {
            switch (type)
            {
            case CVAR_TYPE_NUM:
                result = (int)m_cvar->cvar->value;
                break;
            case CVAR_TYPE_FLT:
                result = amx_ftoc(m_cvar->cvar->value);
                break;
            case CVAR_TYPE_STR:
                if (!ptr_size)
                    break;
                std::string s = wstos(m_cvar->value);
                result = std::min(ptr_size, s.size() + 1);
                setAmxString(ptr, s.c_str(), result);
            }
        }
        return result;
    }
    cell get(CVAR_TYPES_t type, std::wstring name, cell *ptr, size_t ptr_size)
    {
        return get(type, get(name), ptr, ptr_size);
    }
    void direct_set(cvar_t *cvar, const char *value)
    {
        if (cvar != nullptr)
            g_engfuncs.pfnCvar_DirectSet(cvar, value);
    }
    void set(CVAR_TYPES_t type, m_cvar_t *m_cvar, cell *ptr)
    {
        if (m_cvar == nullptr)
            return;
        switch (type)
        {
        case CVAR_TYPE_NUM:
            direct_set(m_cvar->cvar, std::to_string((int)*ptr).c_str());
            break;
        case CVAR_TYPE_FLT:
            direct_set(m_cvar->cvar, std::to_string(amx_ctof(*ptr)).c_str());
            break;
        case CVAR_TYPE_STR:
            direct_set(m_cvar->cvar, (char *)ptr);
        }
    }
    void set(CVAR_TYPES_t type, std::wstring name, cell *ptr)
    {
        set(type, get(name), ptr);
    }
    cell create_hook(int fwd, m_cvar_t *m_cvar, bool is_enable = true)
    {
        if (m_cvar != nullptr)
        {
            // UTIL_ServerPrint("[DEBUG] create_hook(): fwd = %d, cvar = %d\n", fwd, cvar_list->second.cvar);
            auto result = cvars.hook_state.insert({fwd, is_enable});
            if (result.second)
            {
                auto hook_it = cvars.cvar_hook_list.find(m_cvar->cvar);
                if (hook_it != cvars.cvar_hook_list.end())
                    hook_it->second.push_back(result.first);
                else
                    cvars.cvar_hook_list[m_cvar->cvar].push_back(result.first);
                // UTIL_ServerPrint("\n[DEBUG] create_hook(): fwd = %d, hook = %d, cvar = %d, name = <%s>, value = <%s>, enabled = %d\n\n", fwd, result.first, cvar_list->second.cvar, cvar_list->second.cvar->name, cvar_list->second.cvar->string, is_enable);
                return fwd;
            }
        }
        return -1;
    }
    bool cvar_hook_state(int fwd, bool is_enable)
    {
        bool result;
        auto hook_state_it = cvars.hook_state.find(fwd);
        // Hook exists?
        if (result = hook_state_it != cvars.hook_state.end())
            hook_state_it->second = is_enable;
        return result;
    }
    void sort(plugin_cvars_it plugin_cvars)
    {
        auto list = &plugin_cvars->second;
        list->sort([](m_cvar_t *p1, m_cvar_t *p2)
                   { return p1->name.compare(p2->name) <= 0; });
    }
    void clear_plugin(CPluginMngr::CPlugin *plugin)
    {
        auto cvars_it = cvars.plugin.find(plugin->getId());
        // Plugin cvars exist?
        if (cvars_it != cvars.plugin.end())
            cvars_it->second.clear();
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
        for (auto &h : cvars.hook_state)
            if (h.first != -1)
                g_amxxapi.UnregisterSPForward(h.first);
        cvars.hook_state.clear();
    }
    void clear()
    {
        clear_hook_list();
        clear_hook_state();
        clear_bind_list();
        clear_plugin_all();
    }
    void clear_all()
    {
        clear_hook_list();
        clear_hook_state();
        clear_bind_list();
        clear_plugin_all();
        clear_cvar_list();
        clear_pcvar_all();
    }
};

extern cvar_mngr g_cvar_mngr;