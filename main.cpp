#include "precompiled.h"

edict_t* g_pEdicts;
playermove_t* g_pMove;
char g_szMapName[32] = "";
int gmsgSendAudio, gmsgStatusIcon, gmsgArmorType, gmsgItemStatus, gmsgBarTime, gmsgBarTime2;

struct
{
	const char* pszName;
	int& id;
} g_RegUserMsg[] = {
	{ "SendAudio",  gmsgSendAudio },
	{ "StatusIcon", gmsgStatusIcon },
	{ "ArmorType",  gmsgArmorType },
	{ "ItemStatus", gmsgItemStatus },
	{ "BarTime",    gmsgBarTime },
	{ "BarTime2",   gmsgBarTime2 },
};

void OnAmxxAttach()
{
	// initialize API
	api_cfg.Init();
	g_pEdicts = g_engfuncs.pfnPEntityOfEntIndex(0);

	// If AMXX_Attach been called in a first the event Spawn
	if (g_pEdicts)
	{
		// save true mapname
		strncpy(g_szMapName, STRING(gpGlobals->mapname), sizeof(g_szMapName) - 1);
		g_szMapName[sizeof(g_szMapName) - 1] = '\0';
	}
}

bool OnMetaAttach()
{
	return true;
}

void OnMetaDetach()
{
	// clear all hooks?
	g_hookManager.Clear();

	if (api_cfg.hasReGameDLL()) {
		g_ReGameHookchains->InstallGameRules()->unregisterHook(&InstallGameRules);
	}
}

void ServerActivate_Post(edict_t *pEdictList, int edictCount, int clientMax)
{
	SERVER_PRINT("[DEBUG] SERVER_ACTIVATED\n");

	for (auto& msg : g_RegUserMsg) {

		msg.id = GET_USER_MSG_ID(PLID, msg.pszName, NULL);
	}
	
	r_bMapHasBuyZone = g_Tries.entities.find("func_buyzone") != g_Tries.entities.end();

	g_RehldsHookchains->SV_DropClient()->registerHook(SV_DropClient_RH);
	//g_RehldsHookchains->ED_Alloc()->registerHook(ED_Alloc_RH);
	//g_RehldsHookchains->ED_Free()->registerHook(ED_Free_RH);
	g_RehldsHookchains->CreateFakeClient()->registerHook(CreateFakeClient_RH);
	//g_RehldsHookchains->Cvar_DirectSet()->registerHook(Cvar_DirectSet_RH);

	g_ReGameHookchains->CBasePlayer_Killed()->registerHook(CBasePlayer_Killed_RG);
	g_ReGameHookchains->CSGameRules_CheckMapConditions()->registerHook(CSGameRules_CheckMapConditions_RG);
	g_ReGameHookchains->CBasePlayer_AddPlayerItem()->registerHook(CBasePlayer_AddPlayerItem_RG);
	//g_ReGameHookchains->CBasePlayer_GiveNamedItem()->registerHook(CBasePlayer_GiveNamedItem_RG);
	//g_ReGameHookchains->CSGameRules_CanHavePlayerItem()->registerHook(CSGameRules_CanHavePlayerItem_RG);
	g_ReGameHookchains->CBasePlayer_Spawn()->registerHook(CBasePlayer_Spawn_RG);
	g_ReGameHookchains->CreateWeaponBox()->registerHook(CreateWeaponBox_RG);

	

	SET_META_RESULT(MRES_IGNORED);
}

void ServerDeactivate_Post()
{
	SERVER_PRINT("[DEBUG] SERVER_DEACTIVATED\n");

	g_pEdicts = nullptr;
	api_cfg.ServerDeactivate();
	g_hookManager.Clear();
	EntityCallbackDispatcher().DeleteAllCallbacks();
	g_cvar_mngr.clear_hook_list();
	g_cvar_mngr.clear_bind_list();
	g_cvar_mngr.clear_plugin_all();

	g_pFunctionTable->pfnSpawn = DispatchSpawn;
	g_pFunctionTable->pfnKeyValue = KeyValue;

	g_RehldsHookchains->SV_DropClient()->unregisterHook(SV_DropClient_RH);
	//g_RehldsHookchains->ED_Alloc()->unregisterHook(ED_Alloc_RH);
	//g_RehldsHookchains->ED_Free()->unregisterHook(ED_Free_RH);
	g_RehldsHookchains->CreateFakeClient()->unregisterHook(CreateFakeClient_RH);
	//g_RehldsHookchains->Cvar_DirectSet()->unregisterHook(Cvar_DirectSet_RH);

	g_ReGameHookchains->CBasePlayer_Killed()->unregisterHook(CBasePlayer_Killed_RG);
	g_ReGameHookchains->CSGameRules_CheckMapConditions()->unregisterHook(CSGameRules_CheckMapConditions_RG);
	g_ReGameHookchains->CBasePlayer_AddPlayerItem()->unregisterHook(CBasePlayer_AddPlayerItem_RG);
	//g_ReGameHookchains->CBasePlayer_GiveNamedItem()->unregisterHook(CBasePlayer_GiveNamedItem_RG);
	//g_ReGameHookchains->CSGameRules_CanHavePlayerItem()->registerHook(CSGameRules_CanHavePlayerItem_RG);
	g_ReGameHookchains->CBasePlayer_Spawn()->unregisterHook(CBasePlayer_Spawn_RG);
	g_ReGameHookchains->CreateWeaponBox()->unregisterHook(CreateWeaponBox_RG);
	
	// CLEAR TRIES
	r_bMapHasBuyZone = false;
	memset(g_Clients, 0, sizeof(g_Clients));
	memset(g_PlayersNum, 0, sizeof(g_PlayersNum));
	g_Tries.authids.clear();
	g_Tries.classnames.clear();
	g_Tries.entities.clear();
	g_Tries.names.clear();
	g_Tries.wp_entities.clear();
	for (int i_i = 0; i_i < MAX_PLAYERS + 1; i_i++)
		g_Tries.player_entities[i_i].clear();

	SET_META_RESULT(MRES_IGNORED);
}

void KeyValue(edict_t *pentKeyvalue, KeyValueData *pkvd)
{
	// get the first edict worldspawn
	if (FClassnameIs(pentKeyvalue, "worldspawn"))
	{
		// save true mapname
		strncpy(g_szMapName, STRING(gpGlobals->mapname), sizeof(g_szMapName) - 1);
		g_szMapName[sizeof(g_szMapName) - 1] = '\0';

		g_pEdicts = pentKeyvalue;
		g_pFunctionTable->pfnKeyValue = nullptr;
	}

	SET_META_RESULT(MRES_IGNORED);
}

CGameRules *InstallGameRules(IReGameHook_InstallGameRules *chain)
{
	auto gamerules = chain->callNext();

	// Safe check CGameRules API interface version
	if (!g_ReGameApi->BGetIGameRules(GAMERULES_API_INTERFACE_VERSION))
	{
		api_cfg.FailedReGameDllAPI();
		UTIL_ServerPrint("[%s]: Interface CGameRules API version '%s' not found.\n", Plugin_info.logtag, GAMERULES_API_INTERFACE_VERSION);
	}
	else
	{
		g_pGameRules = gamerules;
	}

	return gamerules;
}

int DispatchSpawn(edict_t *pEntity)
{
	g_pEdicts = g_engfuncs.pfnPEntityOfEntIndex(0);
	if (api_cfg.hasReGameDLL()) {
		g_pMove = g_ReGameApi->GetPlayerMove();
	}

	g_pFunctionTable->pfnSpawn = R_Spawn;
	RETURN_META_VALUE(MRES_IGNORED, 0);
}

void ResetGlobalState()
{
	// restore mapname
	if (strcmp(g_RehldsData->GetName(), g_szMapName) != 0) {
		g_RehldsData->SetName(g_szMapName);
		g_pFunctionTable->pfnResetGlobalState = nullptr;
	}

	SET_META_RESULT(MRES_IGNORED);
}

void OnFreeEntPrivateData(edict_t *pEdict)
{
	CBaseEntity *pEntity = getPrivate<CBaseEntity>(pEdict);
	if (pEntity) {
		Free_EntPrivateData(pEdict);	//RefsAPI
		EntityCallbackDispatcher().DeleteExistingCallbacks(pEntity);
	}

	SET_META_RESULT(MRES_IGNORED);
}

CTempStrings::CTempStrings()
{
	m_current = 0;
}


char* CTempStrings::push(AMX* amx)
{
	if (m_current == STRINGS_MAX) {
		AMXX_LogError(amx, AMX_ERR_NATIVE, "temp strings limit exceeded, contact reapi authors");
		return nullptr;
	}

	return m_strings[m_current++];
}

void CTempStrings::pop(size_t count)
{
	m_current -= count;
}