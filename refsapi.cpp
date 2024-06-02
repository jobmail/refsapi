#include "precompiled.h"

int gmsgTeamInfo;
int mState;
int g_PlayersNum[6];

sClients g_Clients[MAX_PLAYERS + 1];
sTries g_Tries;

funEventCall modMsgsEnd[MAX_REG_MSGS];
funEventCall modMsgs[MAX_REG_MSGS];

void (*function)(void*);
void (*endfunction)(void*);

g_RegUserMsg g_user_msg[] =
{
	{ "TeamInfo", &gmsgTeamInfo, Client_TeamInfo, false },
};

edict_t* R_CreateNamedEntity(string_t className) {

    //UTIL_ServerPrint("[DEBUG] R_CreateNamedEntity(): classname = %s\n", STRING(className));

    RETURN_META_VALUE(MRES_IGNORED, 0);
}

void* R_PvAllocEntPrivateData(edict_t *pEdict, int32 cb) {

    UTIL_ServerPrint("[DEBUG] R_PvAllocEntPrivateData(): id = %d, classname = %s, owner = %d\n", ENTINDEX(pEdict), STRING(pEdict->v.classname), ENTINDEX(pEdict->v.owner));

    char key[128];
    
    Q_strcpy_s(key, (char*)STRING(pEdict->v.classname));

    if (key[0]) {

        std::vector<int> v;

        if (g_Tries.entities.find(key) != g_Tries.entities.end())

            v = g_Tries.entities[key];

        else

            v.clear();
        
        if (v.size() < v.max_size()) {

            v.push_back(ENTINDEX(pEdict));

            g_Tries.entities[key] = v;
        }

        UTIL_ServerPrint("[DEBUG] R_PvAllocEntPrivateData(): classname = %s, count = %d\n", key, v.size());
    }

    RETURN_META_VALUE(MRES_IGNORED, 0);
}

void* R_PvEntPrivateData(edict_t *pEdict) {

    UTIL_ServerPrint("[DEBUG] ZZZZZZZZZZZZZZZZZZZZ: R_PvEntPrivateData");

    RETURN_META_VALUE(MRES_IGNORED, 0);
}

void* R_PvEntPrivateData_Post(edict_t *pEdict) {

    UTIL_ServerPrint("[DEBUG] XXXXXXXXXXXXXXXXXXXX: R_PvEntPrivateData_Post");

    RETURN_META_VALUE(MRES_IGNORED, 0);
}

void Free_EntPrivateData(edict_t *pEdict) {

    UTIL_ServerPrint("[DEBUG] R_FreeEntPrivateData(): id = %d, classname = %s, owner = %d\n", ENTINDEX(pEdict), STRING(pEdict->v.classname), ENTINDEX(pEdict->v.owner));

    char key[128];
    
    Q_strcpy_s(key, (char*)STRING(pEdict->v.classname));

    if (key[0]) {

        std::vector<int> v;

        std::vector<int>::iterator it_value;
    
        if (g_Tries.entities.find(key) != g_Tries.entities.end()) {

            v = g_Tries.entities[key];

            if ((it_value = std::find(v.begin(), v.end(), ENTINDEX(pEdict))) != v.end()) {

                v.erase(it_value);

                UTIL_ServerPrint("[DEBUG] R_FreeEntPrivateData(): classname = %s, count = %d\n", key, v.size());

                if (v.size() > 0)

                    g_Tries.entities[key] = v;                    

                else

                    g_Tries.entities.erase(key);
            }
        }
    }
}

void ED_Free_RH(IRehldsHook_ED_Free *chain, edict_t *pEdict) {

    chain->callNext(pEdict);
}

edict_t* ED_Alloc_RH(IRehldsHook_ED_Alloc* chain) {

    auto origin = chain->callNext();

	return origin;
}

int R_Spawn(edict_t *pEntity) {

    //int id = ENTINDEX(pEntity);

    UTIL_ServerPrint("[DEBUG] Spawn(): id = %d, owner = %d\n", ENTINDEX(pEntity), ENTINDEX(pEntity->v.owner));

    RETURN_META_VALUE(MRES_IGNORED, 0);
}

qboolean CBasePlayer_AddPlayerItem_RG(IReGameHook_CBasePlayer_AddPlayerItem *chain, CBasePlayer *pPlayer, class CBasePlayerItem *pItem) {

    auto result = chain->callNext(pPlayer, pItem);

    UTIL_ServerPrint("[DEBUG] AddPlayerItem_RG(): id = %d, result = %d, entity = %d, item_classname = %s, item_owner = %d\n", pPlayer->entindex(), result, pItem->entindex(), STRING(pItem->pev->classname), ENTINDEX(pItem->pev->owner));

    return result;
}

qboolean R_ClientConnect(edict_t *pEntity, const char *pszName, const char *pszAddress, char szRejectReason[ 128 ]) {

    int id = ENTINDEX(pEntity);

    UTIL_ServerPrint("[DEBUG] R_ClientConnect(): id = %d, name = %s, host = %s\n", id, pszName, pszAddress);

	RETURN_META_VALUE(MRES_IGNORED, true);
}

void R_ClientPutInServer_Post(edict_t *pEntity) {

    UTIL_ServerPrint("[DEBUG] ClientPutInServer_Post() ===>\n");

    CBasePlayer *pPlayer = UTIL_PlayerByIndexSafe(ENTINDEX(pEntity));

    if (pPlayer != nullptr && !pPlayer->IsBot())

        Client_PutInServer(pEntity, STRING(pPlayer->pev->netname));

    RETURN_META(MRES_IGNORED);
}

void CSGameRules_CheckMapConditions_RG(IReGameHook_CSGameRules_CheckMapConditions *chain) {

    g_PlayersNum[TEAM_DEAD_TT] =
    
        g_PlayersNum[TEAM_DEAD_CT] = 0;

    chain->callNext();
}

void CBasePlayer_Killed_RG(IReGameHook_CBasePlayer_Killed *chain, CBasePlayer *pPlayer, entvars_t *pevAttacker, int iGib) {

    if (g_Clients[pPlayer->entindex()].is_connected && pPlayer->m_iTeam >= 1 && pPlayer->m_iTeam <=2)

        g_PlayersNum[TEAM_DEAD_TT + pPlayer->m_iTeam - 1]++;

    chain->callNext(pPlayer, pevAttacker, iGib);
}

edict_t* CreateFakeClient_RH(IRehldsHook_CreateFakeClient *chain, const char *netname) {

    edict_t *pEntity = chain->callNext(netname);

    UTIL_ServerPrint("[DEBUG] CreateFakeClient(): id = %d, name = %s\n", ENTINDEX(pEntity), netname);

    Client_PutInServer(pEntity, netname);

    return pEntity;
}

void R_ClientDisconnect(edict_t *pEntity) {

    SERVER_PRINT("[DEBUG] R_ClientDisconnect() ===>\n");

    Client_Disconnected(ENTINDEX(pEntity), false, 0);

	RETURN_META(MRES_IGNORED);
}

void SV_DropClient_RH(IRehldsHook_SV_DropClient *chain, IGameClient *cl, bool crash, const char *format) {
	
    char buffer[1024];

    SERVER_PRINT("[DEBUG] SV_DropClient_RH() ===>\n");

	Q_strcpy_s(buffer, (char*)format);

    Client_Disconnected(ENTINDEX(cl->GetEdict()), crash, buffer);

    chain->callNext(cl, crash, format);
}

void Client_PutInServer(edict_t *pEntity, const char *netname) {

    int id = ENTINDEX(pEntity);

    if (is_valid_index(id)) {

        g_Clients[id].is_connected = true;

        g_Clients[id].team = TEAM_UNASSIGNED;

        g_PlayersNum[TEAM_UNASSIGNED]++;

        UTIL_ServerPrint("[DEBUG] PutInServer_Post(): id = %d, name = %s, authid = %s, team = %d, is_connected = %d\n", id, netname, GETPLAYERAUTHID(pEntity), g_Clients[id].team, g_Clients[id].is_connected);

        UTIL_ServerPrint("[DEBUG] num_unassigned = %d, num_tt = %d, num_ct = %d, num_spec = %d\n", g_PlayersNum[TEAM_UNASSIGNED], g_PlayersNum[TEAM_TERRORIST], g_PlayersNum[TEAM_CT], g_PlayersNum[TEAM_SPECTRATOR]);
    }
}

void Client_Disconnected(int id, bool crash, char *format) {

    if (is_valid_index(id)) {
        
        UTIL_ServerPrint("[DEBUG] Client_Disconnected(): id = %d, is_connected = %d\n", id, g_Clients[id].is_connected);

        if (g_Clients[id].is_connected) {

            g_Clients[id].is_connected =
                
                g_Clients[id].is_bot = false;

            g_PlayersNum[g_Clients[id].team]--;

            UTIL_ServerPrint("[DEBUG] num_unassigned = %d, num_tt = %d, num_ct = %d, num_spec = %d\n", g_PlayersNum[TEAM_UNASSIGNED], g_PlayersNum[TEAM_TERRORIST], g_PlayersNum[TEAM_CT], g_PlayersNum[TEAM_SPECTRATOR]);
        }
    }
}

void Client_TeamInfo(void* mValue) {

    static int id;
    char *msg;
    eRFS_TEAMS new_team;

    switch (mState++) {
    
        case 0:

			id = *(int*)mValue;
		
        	break;

        case 1:
		
        	if (!is_valid_index(id)) break;
		
        	msg = (char*)mValue;
			
            if (!msg) break;

            switch (msg[0]) {
                
                case 'T': new_team = TEAM_TERRORIST;
                
                    break;

                case 'C': new_team = TEAM_CT;
                
                    break;

                case 'S': new_team = TEAM_SPECTRATOR;
                    
                    break;

                default: new_team = TEAM_UNASSIGNED;
            }

            //UTIL_ServerPrint("[DEBUG] id = %d, old_team = %d, new_team = %d\n", id, g_Clients[id].team, new_team);

            if (!g_Clients[id].is_connected) {

                g_Clients[id].team = new_team;

            } else if (new_team != TEAM_UNASSIGNED && g_Clients[id].team != new_team) {

                UTIL_ServerPrint("[DEBUG] Team changed!!!\n");

                g_PlayersNum[g_Clients[id].team]--;

                g_PlayersNum[new_team]++;

                g_Clients[id].team = new_team;

                UTIL_ServerPrint("[DEBUG] num_unassigned = %d, num_tt = %d, num_ct = %d, num_spec = %d\n", g_PlayersNum[TEAM_UNASSIGNED], g_PlayersNum[TEAM_TERRORIST], g_PlayersNum[TEAM_CT], g_PlayersNum[TEAM_SPECTRATOR]);
            }

            break;
    }
}

int	R_RegUserMsg_Post(const char *pszName, int iSize) {

	for (auto& msg : g_user_msg) {

		if (strcmp(msg.name, pszName) == 0) {

			int id = META_RESULT_ORIG_RET(int);

            UTIL_ServerPrint("[DEBUG] RegUserMsg: id = %d, %s\n", id, pszName);

            *msg.id = id;

            if (msg.endmsg) {
		
        		modMsgsEnd[id] = msg.func;
		
            } else {
		
        		modMsgs[id] = msg.func;
            }

			break;
        }
	}
	RETURN_META_VALUE(MRES_IGNORED, 0);
}

void R_MessageBegin_Post(int msg_dest, int msg_type, const float *pOrigin, edict_t *ed) {

    mState = 0;

	function = modMsgs[msg_type];
	
    endfunction = modMsgsEnd[msg_type];

	RETURN_META(MRES_IGNORED);
}

void R_WriteByte_Post(int iValue) {

	if (function) (*function)((void *)&iValue);

	RETURN_META(MRES_IGNORED);
}

void R_WriteChar_Post(int iValue) {

	if (function) (*function)((void *)&iValue);

	RETURN_META(MRES_IGNORED);
}

void R_WriteShort_Post(int iValue) {

	if (function) (*function)((void *)&iValue);

	RETURN_META(MRES_IGNORED);
}

void R_WriteLong_Post(int iValue) {

	if (function) (*function)((void *)&iValue);

	RETURN_META(MRES_IGNORED);
}

void R_WriteAngle_Post(float flValue) {

	if (function) (*function)((void *)&flValue);

	RETURN_META(MRES_IGNORED);
}

void R_WriteCoord_Post(float flValue) {

	if (function) (*function)((void *)&flValue);

	RETURN_META(MRES_IGNORED);
}

void R_WriteString_Post(const char *sz) {

	if (function) (*function)((void *)sz);

	RETURN_META(MRES_IGNORED);
}

void R_WriteEntity_Post(int iValue) {

	if (function) (*function)((void *)&iValue);

	RETURN_META(MRES_IGNORED);
}

void R_MessageEnd_Post(void) {

	if (endfunction) (*endfunction)(NULL);

	RETURN_META(MRES_IGNORED);
}