#include <string.h>
#include <extdll.h>
#include "precompiled.h"

enginefuncs_t g_engfuncs;
globalvars_t  *gpGlobals;

/*
enginefuncs_t* meta_hack;
void (*meta_Cvar_RegisterVariable)(cvar_t *pCvar) = nullptr;
void (*meta_CvarRegister)(cvar_t *pCvar) = nullptr;

void Cvar_RegisterVariable(cvar_t *pCvar)
{
	UTIL_ServerPrint("\n[DEBUG] [***  Cvar_RegisterVariable ***]: meta hook #1 !!!!!!!!\n\n");
	if (meta_Cvar_RegisterVariable != nullptr)
		meta_Cvar_RegisterVariable(pCvar);
	Cvar_RegisterVariable_Post(pCvar);
}

void CvarRegister(cvar_t *pCvar)
{
	UTIL_ServerPrint("\n[DEBUG] [###  Cvar_Register ###]: meta hook #2!!!!!!!!\n\n");
	if (meta_CvarRegister != nullptr)
		meta_CvarRegister(pCvar);
	Cvar_RegisterVariable_Post(pCvar);
}
*/

// Receive engine function table from engine.
// This appears to be the _first_ DLL routine called by the engine, so we
// do some setup operations here.
C_DLLEXPORT void WINAPI GiveFnptrsToDll(enginefuncs_t *pengfuncsFromEngine, globalvars_t *pGlobals)
{
	memcpy(&g_engfuncs, pengfuncsFromEngine, sizeof(enginefuncs_t));
	gpGlobals = pGlobals;
	/*
	// Hook Silent-style, thanks :-))
	//meta_Cvar_RegisterVariable = pengfuncsFromEngine->pfnCVarRegister;
	//pengfuncsFromEngine->pfnCVarRegister = Cvar_RegisterVariable;

	meta_hack = pengfuncsFromEngine; //(enginefuncs_t*)gpMetaGlobals->orig_ret;
	UTIL_ServerPrint("\n[DEBUG] [Cvar_RegisterVariable]: meta hook %d!!!!!!!!\n", meta_hack);
	meta_Cvar_RegisterVariable = meta_hack->pfnCvar_RegisterVariable; // pfnCvar_RegisterVariable; //g_engine.pl_funcs.pfnCVarRegister;
	meta_CvarRegister = meta_hack->pfnCVarRegister;
	meta_hack->pfnCvar_RegisterVariable = Cvar_RegisterVariable; //g_engine.pl_funcs.pfnCVarRegister
	meta_hack->pfnCVarRegister = CvarRegister;
	meta_hack->pfnCVarRegister = Cvar_RegisterVariable;*/
}
