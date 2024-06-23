#include "precompiled.h"

CAPI_Config api_cfg;

CAPI_Config::CAPI_Config() : m_api_rehlds(false), m_api_regame(false) {

}

void CAPI_Config::FailedReGameDllAPI() {

	m_api_regame = false;
}

void CAPI_Config::Init() {

	m_api_rehlds = RehldsApi_Init();

	m_api_regame = RegamedllApi_Init();

	if (m_api_rehlds) {

		g_engfuncs.pfnServerPrint("[REFSAPI] ReHLDS API successfully initialized.\n");

		g_RehldsHookchains->Cvar_DirectSet()->registerHook(Cvar_DirectSet_RH);
	}	

	if (m_api_regame) {

		g_engfuncs.pfnServerPrint("[REFSAPI] ReGAME API successfully initialized.\n");

		g_ReGameHookchains->InstallGameRules()->registerHook(&InstallGameRules);

	}

	cvar_t* pCvar = CVAR_GET_POINTER(REFSAPI_CVAR);
	
	if (pCvar == nullptr) {

		cvar_t cvar;

		cvar.name = REFSAPI_CVAR;
		
		cvar.flags = (FCVAR_SERVER | FCVAR_SPONLY | FCVAR_UNLOGGED);
		
		cvar.string = "1";

		CVAR_REGISTER(&cvar);

		pCvar = CVAR_GET_POINTER(REFSAPI_CVAR);

		if (pCvar != nullptr)

			g_engfuncs.pfnCvar_DirectSet(pCvar, "1");
	}

	CVAR_SET_STRING("zzz1", "123.456");
}

void CAPI_Config::ServerDeactivate() const {
	
	if (m_api_regame)

		g_pGameRules = nullptr;
}
