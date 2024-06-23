#include <string.h>
#include <extdll.h>
#include "precompiled.h"
#include <meta_api.h>

enginefuncs_t g_engfuncs;
globalvars_t  *gpGlobals;

void (*meta_Cvar_RegisterVariable)(cvar_t *pCvar) = nullptr;

void Cvar_RegisterVariable(cvar_t *pCvar)
{
	UTIL_ServerPrint("\n[DEBUG] [Cvar_RegisterVariable]: meta hook!!!!!!!!\nn");
	if (meta_Cvar_RegisterVariable != nullptr)
		meta_Cvar_RegisterVariable(pCvar);
	Cvar_RegisterVariable_Post(pCvar);
}

// Receive engine function table from engine.
// This appears to be the _first_ DLL routine called by the engine, so we
// do some setup operations here.
C_DLLEXPORT void WINAPI GiveFnptrsToDll(enginefuncs_t *pengfuncsFromEngine, globalvars_t *pGlobals)
{
	memcpy(&g_engfuncs, pengfuncsFromEngine, sizeof(enginefuncs_t));
	gpGlobals = pGlobals;

	// Hook Silent-style, thanks :-))
	//meta_Cvar_RegisterVariable = pengfuncsFromEngine->pfnCVarRegister;
	//pengfuncsFromEngine->pfnCVarRegister = Cvar_RegisterVariable;
	
	meta_Cvar_RegisterVariable = ((enginefuncs_t*)(gpMetaGlobals->orig_ret))->pfnCvar_RegisterVariable; //g_engine.pl_funcs.pfnCVarRegister;
	((enginefuncs_t*)(gpMetaGlobals->orig_ret))->pfnCvar_RegisterVariable = Cvar_RegisterVariable; //g_engine.pl_funcs.pfnCVarRegister
}
