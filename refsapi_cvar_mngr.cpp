#include "precompiled.h"

void Cvar_DirectSet_RH(IRehldsHook_Cvar_DirectSet *chain, cvar_t *cvar, const char *value)
{
    chain->callNext(cvar, value);
    cvar_list_it cvar_list = g_cvar_mngr.get(cvar);
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
        //UTIL_ServerPrint("[DEBUG] Cvar_DirectSet_RH(): old_value = %s, old_cvar_value = %s, new_value = %s\n", wstos(cvar_list->second.value).c_str(), cvar->string, value);
        m_cvar_t* m_cvar = &cvar_list->second;
        // Convert to string
        std::string s = value;
        // Is callback after fix value or samething went wrong?
        //if (m_cvar->value.compare(stows(s)) == 0)
        //    return;
        // Is number?
        if (is_number(s))
        {
            bool was_override = false;
            auto result = std::stod(s);
            UTIL_ServerPrint("[DEBUG] stod(): in = %s, out = %f\n", s.c_str(), result);
            if (was_override |= m_cvar->has_min && result < m_cvar->min_val)
                result = m_cvar->min_val;
            if (was_override |= m_cvar->has_min && result > m_cvar->max_val)
                result = m_cvar->max_val;
            if (was_override) {
                CVAR_SET_FLOAT(wstos(m_cvar->name).c_str(), result);
                UTIL_ServerPrint("[DEBUG] stod(): override = %f, new_string = %s\n", result, m_cvar->cvar->string);
                return;
            }
            s = rtrim_zero_c(std::to_string(result));
        }
        // Do event
        g_cvar_mngr.on_change(cvar_list, s);
        // Set new value
        m_cvar->value = stows(s);
    }
}