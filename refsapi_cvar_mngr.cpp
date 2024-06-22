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
        // Bind exists?
        if (m_cvar->type == CVAR_TYPE_NONE)
            return;
        std::string s = value;
        bool is_num = is_number(s);
        // Fix wrong value
        if (!is_num && (m_cvar->type == CVAR_TYPE_NUM || m_cvar->type == CVAR_TYPE_FLT))
        {
            UTIL_ServerPrint("[DEBUG] Cvar_DirectSet_RH(): wrong non-number value <%s>\n", value);
            CVAR_SET_STRING(cvar->name, "0");
            return;
        }
        // Is number?
        if (is_num)
        {
            bool is_override = false;
            auto result = std::stod(s);
            // Check bind type and conver
            if (m_cvar->type == CVAR_TYPE_NUM)
                result = result >= 0.0 ? (int)result : -(int)(-result);
            UTIL_ServerPrint("[DEBUG] Cvar_DirectSet_RH(): in = %s, out = %f, type = %d\n", s.c_str(), result, m_cvar->type);
            if (is_override |= m_cvar->has_min && result < m_cvar->min_val)
                result = m_cvar->min_val;
            if (is_override |= m_cvar->has_min && result > m_cvar->max_val)
                result = m_cvar->max_val;
            if (is_override) {
                CVAR_SET_FLOAT(wstos(m_cvar->name).c_str(), result);
                UTIL_ServerPrint("[DEBUG] Cvar_DirectSet_RH(): override = %f, new_string = %s\n", result, m_cvar->cvar->string);
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

void CVarRegister_Post(cvar_t *pCvar)
{
    UTIL_ServerPrint("[DEBUG] CVarRegister_Post(): cvar = <%s>, string = <%s>, value = %f\n", pCvar->name, pCvar->string, pCvar->value);
}

void CVarSetFloat_Post(const char *szVarName, float flValue)
{
    UTIL_ServerPrint("[DEBUG] CVarRegister_Post(): cvar = <%s>, value = %f\n", szVarName, flValue);
}

void CVarSetString_Post(const char *szVarName, const char *szValue)
{
    UTIL_ServerPrint("[DEBUG] CVarRegister_Post(): cvar = <%s>, value = <%s>\n", szVarName, szValue);
}
