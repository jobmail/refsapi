#include "precompiled.h"

void Cvar_DirectSet_RH(IRehldsHook_Cvar_DirectSet *chain, cvar_t *cvar, const char *value)
{
    chain->callNext(cvar, value);
    cvar_list_it cvar_list = g_cvar_mngr.get(cvar);
    UTIL_ServerPrint("[DEBUG] Cvar_DirectSet_RH(): old_value = %s, old_cvar_value = %s, new_value = %s\n", wstos(cvar_list->second.value).c_str(), cvar->string, value);
    // Cvar not register?
    if (check_it_empty(cvar_list))
    {
        // Add cvar to list
        auto result = g_cvar_mngr.add_exists(cvar);
        check_it_empty_r(result);
        // Set new value
        result->second.value = stows(value);
    }
    else
    {
        m_cvar_t* m_cvar = &cvar_list->second;
        // Convert to string
        std::string s = value;
        // Is callback after fix value or samething went wrong?
        if (m_cvar->value.compare(stows(s)) == 0)//(m_cvar->value.size() == s.size() && std::equal( begin(), m_cvar->value.end(), ))
            return;
        // Is number?
        if (is_number(s))
            s = rtrim_zero_c(std::to_string(stod(s, m_cvar)));
        // Do event
        g_cvar_mngr.on_change(cvar_list, s);
        // Set new value
        m_cvar->value = stows(s);
    }
}