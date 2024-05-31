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

		g_RehldsHookchains->SV_DropClient()->registerHook(SV_DropClient_RH);

		g_RehldsHookchains->ED_Alloc()->registerHook(ED_Alloc_RH);
	}	

	if (m_api_regame) {

		g_engfuncs.pfnServerPrint("[REFSAPI] ReGAME API successfully initialized.\n");

		g_ReGameHookchains->InstallGameRules()->registerHook(&InstallGameRules);
	}
}

void CAPI_Config::ServerDeactivate() const {

	if (m_api_rehlds) {

		g_RehldsHookchains->SV_DropClient()->unregisterHook(SV_DropClient_RH);

		g_RehldsHookchains->ED_Alloc()->unregisterHook(ED_Alloc_RH);

	}
	
	if (m_api_regame)

		g_pGameRules = nullptr;
}
