#include "precompiled.h"

void Cvar_RegisterVariable_Post(cvar_t *cvar)
{
    UTIL_ServerPrint("\n[DEBUG] Cvar_RegisterVariable_Post(): cvar = <%s>, string = <%s>, value = %f\n\n", cvar->name, cvar->string, cvar->value);
}
//void Cvar_DirectSet_RH(IRehldsHook_Cvar_DirectSet *chain, cvar_t *cvar, const char *value)
void Cvar_DirectSet_Post(cvar_t *cvar, const char *value)
{
    //UTIL_ServerPrint("[DEBUG] Cvar_DirectSet_Post(): cvar = <%s>, string = <%s>, value = %f\n", cvar->name, cvar->string, cvar->value);
}

void Cvar_DirectSet_RH(IRehldsHook_Cvar_DirectSet *chain, cvar_t *cvar, const char *value)
{
    chain->callNext(cvar, value);
    UTIL_ServerPrint("[DEBUG] Cvar_DirectSet_RH(): cvar = <%s>, string = <%s>, value = %f\n", cvar->name, cvar->string, cvar->value);
    g_cvar_mngr.on_direct_set(cvar, stows(value));
}

void CVarRegister_Post(cvar_t *cvar)
{
    UTIL_ServerPrint("[DEBUG] CVarRegister_Post(): cvar = <%s>, string = <%s>, value = %f\n", cvar->name, cvar->string, cvar->value);
    g_cvar_mngr.on_register(cvar);
}

void CVarSetFloat_Post(const char *name, float value)
{
    UTIL_ServerPrint("[DEBUG] CVarSetFloat_Post(): cvar = <%s>, value = %f\n", name, value);
    //g_cvar_mngr.on_direct_set(g_cvar_mngr.get(stows(name))->, std::to_string(value)); 
    cvar_t* cvar = CVAR_GET_POINTER(name);
    if (cvar != nullptr)
    {
        UTIL_ServerPrint("[DEBUG] CVarSetFloat_Post(): VALUE = %f\n", cvar->value);
        g_cvar_mngr.on_direct_set(cvar, std::to_wstring(value));
    }
        
}

void CVarSetString_Post(const char *name, const char *value)
{
    UTIL_ServerPrint("[DEBUG] CVarSetString_Post(): cvar = <%s>, value = <%s>\n", name, value);
    cvar_t* cvar = CVAR_GET_POINTER(name);
    if (cvar != nullptr)
    {
        UTIL_ServerPrint("[DEBUG] CVarSetString_Post(): VALUE = %f\n", cvar->value);
        g_cvar_mngr.on_direct_set(cvar, stows(value));
    }
}
