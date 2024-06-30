#include "precompiled.h"

void Cvar_RegisterVariable_Post(cvar_t *cvar)
{
    UTIL_ServerPrint("\n[DEBUG] Cvar_RegisterVariable_Post(): cvar = <%s>, string = <%s>, value = %f\n\n", cvar->name, cvar->string, cvar->value);
}

void Cvar_DirectSet_RH(IRehldsHook_Cvar_DirectSet *chain, cvar_t *cvar, const char *value)
{
    chain->callNext(cvar, value);
    //UTIL_ServerPrint("[DEBUG] Cvar_DirectSet_RH(): cvar = <%s>, string = <%s>, value = %f\n", cvar->name, cvar->string, cvar->value);
    g_cvar_mngr.on_direct_set(cvar, stows(value));
}

void CVarRegister_Post(cvar_t *cvar)
{
    //UTIL_ServerPrint("[DEBUG] CVarRegister_Post(): cvar = <%s>, string = <%s>, value = %f\n", cvar->name, cvar->string, cvar->value);
    g_cvar_mngr.on_register(cvar);
}
