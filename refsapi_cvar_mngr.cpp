#include "precompiled.h"

void Cvar_DirectSet_RH(IRehldsHook_Cvar_DirectSet *chain, cvar_t *cvar, const char *value)
{
    chain->callNext(cvar, value);
    // UTIL_ServerPrint("[DEBUG] Cvar_DirectSet_RH(): cvar = <%s>, string = <%s>, value = %f\n", cvar->name, cvar->string, cvar->value);
    g_cvar_mngr.on_direct_set(cvar, stows(value));
}
