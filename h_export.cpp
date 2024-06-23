#include <string.h>
#include <extdll.h>
#include <meta_api.h>
#include "precompiled.h"
#include "engine_t.h"

enginefuncs_t g_engfuncs;
globalvars_t  *gpGlobals;

void (*meta_Cvar_RegisterVariable)(cvar_t *pCvar) = nullptr;

void Cvar_RegisterVariable(cvar_t *pCvar)
{
	UTIL_ServerPrint("[DEBUG] [Cvar_RegisterVariable]: meta hook!!!!!!!!");
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

	meta_Cvar_RegisterVariable = g_engine.pl_funcs.pfnCVarRegister;
	g_engine.pl_funcs.pfnCVarRegister = Cvar_RegisterVariable;
}
