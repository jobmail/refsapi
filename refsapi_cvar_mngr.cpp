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
        m_cvar_t* m_cvar = &cvar_list->second;
        std::string s = value;
        // Is number?
        if (is_number(s))
        {
            s = rtrim_zero_c(std::to_string(parse_number(s, m_cvar->has_min, m_cvar->min_val, m_cvar->has_max, m_cvar->max_val)));
        }
        // Do event
        g_cvar_mngr.on_change(cvar_list, s);
        // Set new value
        m_cvar->value = stows(s);
    }
}