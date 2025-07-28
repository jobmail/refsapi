#include "precompiled.h"

edict_t *g_pEdicts;
playermove_t *g_pMove;
char g_szMapName[32] = "";

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
#ifndef WITHOUT_SQL
	g_mysql_mngr.start_main();
#endif

	return true;
}

void OnMetaDetach()
{
	// clear all hooks?
	// g_hookManager.Clear();
	if (api_cfg.hasReHLDS())
		g_RehldsHookchains->Cvar_DirectSet()->unregisterHook(RF_Cvar_DirectSet_RH);
	if (api_cfg.hasReGameDLL())
		g_ReGameHookchains->InstallGameRules()->unregisterHook(&InstallGameRules);
}

void ServerActivate_Post(edict_t *pEdictList, int edictCount, int clientMax)
{
	SERVER_PRINT("[DEBUG] SERVER_ACTIVATED\n");
#ifndef WITHOUT_SQL
	g_mysql_mngr.start();
#endif
	r_bMapHasBuyZone = g_Tries.entities.find("func_buyzone") != g_Tries.entities.end();

	g_RehldsHookchains->SV_DropClient()->registerHook(SV_DropClient_RH);
	g_RehldsHookchains->CreateFakeClient()->registerHook(CreateFakeClient_RH);
	g_RehldsHookchains->ExecuteServerStringCmd()->registerHook(R_ExecuteServerStringCmd);

	g_ReGameHookchains->CBasePlayer_Killed()->registerHook(CBasePlayer_Killed_RG);
	g_ReGameHookchains->CSGameRules_CheckMapConditions()->registerHook(CSGameRules_CheckMapConditions_RG);
	g_ReGameHookchains->CBasePlayer_AddPlayerItem()->registerHook(CBasePlayer_AddPlayerItem_RG);
	g_ReGameHookchains->CBasePlayer_RemovePlayerItem()->registerHook(CBasePlayer_RemovePlayerItem_RG);
	g_ReGameHookchains->CBasePlayer_Spawn()->registerHook(CBasePlayer_Spawn_RG);
	// g_ReGameHookchains->CreateWeaponBox()->registerHook(CreateWeaponBox_RG);

	//g_ReGameHookchains->CSGameRules_ChangeLevel()->registerHook(CSGameRules_ChangeLevel_RG, HC_PRIORITY_UNINTERRUPTABLE);
	
	SET_META_RESULT(MRES_IGNORED);
}

void ServerDeactivate_Post()
{
	SERVER_PRINT("[DEBUG] SERVER_DEACTIVATED\n");
#ifndef WITHOUT_SQL
	g_mysql_mngr.stop();
#endif
	g_cvar_mngr.clear_all();

	g_pEdicts = nullptr;
	api_cfg.ServerDeactivate();
	// g_hookManager.Clear();
	g_pFunctionTable->pfnSpawn = DispatchSpawn;
	g_pFunctionTable->pfnKeyValue = KeyValue;

	g_RehldsHookchains->SV_DropClient()->unregisterHook(SV_DropClient_RH);
	g_RehldsHookchains->CreateFakeClient()->unregisterHook(CreateFakeClient_RH);
	g_RehldsHookchains->ExecuteServerStringCmd()->unregisterHook(R_ExecuteServerStringCmd);

	g_ReGameHookchains->CBasePlayer_Killed()->unregisterHook(CBasePlayer_Killed_RG);
	g_ReGameHookchains->CSGameRules_CheckMapConditions()->unregisterHook(CSGameRules_CheckMapConditions_RG);
	g_ReGameHookchains->CBasePlayer_AddPlayerItem()->unregisterHook(CBasePlayer_AddPlayerItem_RG);
	g_ReGameHookchains->CBasePlayer_RemovePlayerItem()->unregisterHook(CBasePlayer_RemovePlayerItem_RG);
	g_ReGameHookchains->CBasePlayer_Spawn()->unregisterHook(CBasePlayer_Spawn_RG);
	// g_ReGameHookchains->CreateWeaponBox()->unregisterHook(CreateWeaponBox_RG);
	//g_ReGameHookchains->CSGameRules_ChangeLevel()->unregisterHook(CSGameRules_ChangeLevel_RG);

	//  CLEAR TRIES
	r_bMapHasBuyZone = false;
	memset(g_Clients, 0, sizeof(g_Clients));
	memset(g_PlayersNum, 0, sizeof(g_PlayersNum));
	g_Tries.authids.clear();
	std::map<std::string, int>().swap(g_Tries.authids);
	g_Tries.classnames.clear();
	std::map<int, std::string>().swap(g_Tries.classnames);

	// Free Vectors
	for (auto it = g_Tries.entities.begin(); it != g_Tries.entities.end(); it++)
	{
		it->second.clear();
		it->second.shrink_to_fit();
	}
	g_Tries.entities.clear();
	std::map<std::string, std::vector<int>>().swap(g_Tries.entities);

	g_Tries.names.clear();
	std::map<std::string, int>().swap(g_Tries.names);
	g_Tries.wp_entities.clear();
	g_Tries.wp_entities.shrink_to_fit();
	for (int i_i = 0; i_i < MAX_PLAYERS + 1; i_i++)
	{
		g_Tries.player_entities[i_i].clear();
		g_Tries.player_entities[i_i].shrink_to_fit();
	}
	SET_META_RESULT(MRES_IGNORED);
}

void KeyValue(edict_t *pentKeyvalue, KeyValueData *pkvd)
{
	// Get the first edict worldspawn
	if (FClassnameIs(pentKeyvalue, "worldspawn"))
	{
		// Save true mapname
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
		g_pGameRules = gamerules;
	return gamerules;
}

int DispatchSpawn(edict_t *pEntity)
{
	g_pEdicts = g_engfuncs.pfnPEntityOfEntIndex(0);
	if (api_cfg.hasReGameDLL())
		g_pMove = g_ReGameApi->GetPlayerMove();
	RETURN_META_VALUE(MRES_IGNORED, 0);
}

void ResetGlobalState()
{
	// restore mapname
	if (strcmp(g_RehldsData->GetName(), g_szMapName) != 0)
	{
		g_RehldsData->SetName(g_szMapName);
		g_pFunctionTable->pfnResetGlobalState = nullptr;
	}
	SET_META_RESULT(MRES_IGNORED);
}

void OnFreeEntPrivateData(edict_t *pEdict)
{
	CBaseEntity *pEntity = getPrivate<CBaseEntity>(pEdict);
	if (pEntity)
		Free_EntPrivateData(pEdict); // RefsAPI
	SET_META_RESULT(MRES_IGNORED);
}

CTempStrings::CTempStrings()
{
	m_current = 0;
}

char *CTempStrings::push(AMX *amx)
{
	if (m_current == STRINGS_MAX)
	{
		AMXX_LogError(amx, AMX_ERR_NATIVE, "Temp strings limit exceeded, contact reapi authors");
		return nullptr;
	}
	return m_strings[m_current++];
}

void CTempStrings::pop(size_t count)
{
	m_current -= count;
}