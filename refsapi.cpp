#include "precompiled.h"

int gmsgTeamInfo;
int mState;
int g_PlayersNum[6];

bool r_bMapHasBuyZone;
char g_buff[4096];

cvar_mngr g_cvar_mngr;
sClients g_Clients[MAX_PLAYERS + 1];
sTries g_Tries;

std::wstring_convert<convert_type, wchar_t> g_converter;

funEventCall modMsgsEnd[MAX_REG_MSGS];
funEventCall modMsgs[MAX_REG_MSGS];

void (*function)(void*);
void (*endfunction)(void*);

g_RegUserMsg g_user_msg[] =
{
	{ "TeamInfo", &gmsgTeamInfo, Client_TeamInfo, false },
};

edict_t* R_CreateNamedEntity(string_t className) {

    //UTIL_ServerPrint("[DEBUG] R_CreateNamedEntity(): classname = <%s>\n", STRING(className));

    RETURN_META_VALUE(MRES_IGNORED, 0);
}

void* R_PvAllocEntPrivateData(edict_t *pEdict, int32 cb) {

    Alloc_EntPrivateData(pEdict);
    
    RETURN_META_VALUE(MRES_IGNORED, 0);
}

void* R_PvEntPrivateData(edict_t *pEdict) {

    //UTIL_ServerPrint("[DEBUG] ZZZZZZZZZZZZZZZZZZZZ: R_PvEntPrivateData");

    RETURN_META_VALUE(MRES_IGNORED, 0);
}

void* R_PvEntPrivateData_Post(edict_t *pEdict) {

    //UTIL_ServerPrint("[DEBUG] XXXXXXXXXXXXXXXXXXXX: R_PvEntPrivateData_Post");

    RETURN_META_VALUE(MRES_IGNORED, 0);
}

void Alloc_EntPrivateData(edict_t *pEdict) {

    if (FStringNull(pEdict->v.classname)) return;

    //UTIL_ServerPrint("[DEBUG] Alloc_EntPrivateData(): id = %d, classname = <%s>, owner = %d\n", ENTINDEX(pEdict), STRING(pEdict->v.classname), ENTINDEX(pEdict->v.owner));

    int entity_index = ENTINDEX(pEdict);

    std::string key = STRING(pEdict->v.classname);

    // ADD ENTITIES
    int result = acs_trie_add(&g_Tries.entities, key, entity_index);

    //UTIL_ServerPrint("[DEBUG] Alloc_EntPrivateData(): classname = <%s>, new_count = %d\n", key.c_str(), result);

    // ADD CLASSNAMES
    g_Tries.classnames[entity_index] = key;

    // ADD WP_ENTITIES
    if ((key.find(WP_CLASS_PREFIX) == 0) && (key.length() > WP_CLASS_PREFIX_LEN)) {

        acs_vector_add(&g_Tries.wp_entities, entity_index);

        //UTIL_ServerPrint("[DEBUG] Alloc_EntPrivateData(): WEAPONS, new_count = %d\n", result);
    }
}

void Free_EntPrivateData(edict_t *pEdict) {

    int entity_index = ENTINDEX(pEdict);

    int owner_index = ENTINDEX(pEdict->v.owner);

    std::string key;

    // CHECK CREATION CLASSNAME
    if ((pEdict == nullptr) || (pEdict->pvPrivateData == nullptr) || FStringNull(pEdict->v.classname)) {

        // WAS REGISTERED?
        if (g_Tries.classnames.find(entity_index) != g_Tries.classnames.end()) {

            key = g_Tries.classnames[entity_index];

            //UTIL_ServerPrint("[DEBUG] Free_EntPrivateData(): found deleted entity = %d with creation_classname = <%s> << WARNING !!!\n", entity_index, key);

            // REMOVE FROM ENTITIES
            acs_vector_remove(&g_Tries.entities[key], entity_index);

            // REMOVE PLAYER_ENTITIES
            if (is_valid_index(owner_index))

                acs_vector_remove(&g_Tries.player_entities[owner_index], entity_index);
            
            // REMOVE WP_ENITITIES
            if ((key.find(WP_CLASS_PREFIX) == 0) && (key.length() > WP_CLASS_PREFIX_LEN))
                
                acs_vector_remove(&g_Tries.wp_entities, entity_index);

            // REMOVE CLASSNAME
            g_Tries.classnames.erase(entity_index);

            return;
        }
    }

    //UTIL_ServerPrint("[DEBUG] Free_EntPrivateData(): entity = %d, classname = <%s>, owner = %d\n", entity_index, STRING(pEdict->v.classname), owner_index);

    key = STRING(pEdict->v.classname);

    // CHECK ENTITY CREATION CLASS
    if (key != g_Tries.classnames[entity_index]) {

        //UTIL_ServerPrint("[DEBUG] Free_EntPrivateData(): entity = %d, classname = <%s> was changed from <%s> << WARNING !!!\n", entity_index, key.c_str(), g_Tries.classnames[entity_index].c_str());

        key = g_Tries.classnames[entity_index];
    }

    // REMOVE ENTITIES
    int result = acs_trie_remove(&g_Tries.entities, key, entity_index);

    //UTIL_ServerPrint("[DEBUG] Free_EntPrivateData(): remove entity = %d from classname = <%s>, new_count = %d\n", entity_index, key.c_str(), result);

    // REMOVE PLAYER_ENTITIES
    if (is_valid_index(owner_index))

        acs_vector_remove(&g_Tries.player_entities[owner_index], entity_index);

    // REMOVE CLASSNAME
    g_Tries.classnames.erase(entity_index);

    // REMOVE WP_ENTITIES
    if ((key.find(WP_CLASS_PREFIX) == 0) && (key.length() > WP_CLASS_PREFIX_LEN)) {

        acs_vector_remove(&g_Tries.wp_entities, entity_index);

        //UTIL_ServerPrint("[DEBUG] Free_EntPrivateData(): WEAPONS, new_count = %d\n", result);
    }
}

void CBasePlayer_Spawn_RG(IReGameHook_CBasePlayer_Spawn *chain, CBasePlayer *pPlayer) {

    int id = pPlayer->entindex();

    // FIX TEAMS DEAD COUNT
    if (is_valid_index(id) && is_valid_team(g_Clients[id].team) && (pPlayer->edict()->v.deadflag != DEAD_NO) && (g_PlayersNum[TEAM_DEAD_TT + g_Clients[id].team - 1] > 0))

        g_PlayersNum[TEAM_DEAD_TT + g_Clients[id].team - 1]--;

    chain->callNext(pPlayer);
}

CWeaponBox* CreateWeaponBox_RG(IReGameHook_CreateWeaponBox *chain, CBasePlayerItem *pItem, CBasePlayer *pPlayer, const char *model, Vector &v_origin, Vector &v_angels, Vector &v_velocity, float life_time, bool pack_ammo) {
    
    auto origin = chain->callNext(pItem, pPlayer, model, v_origin, v_angels, v_velocity, life_time, pack_ammo);

    int owner_index = pPlayer->entindex();

    int entity_index = pItem->entindex();

    // REMOVE PLAYER_ENTITIES
    if (is_valid_index(owner_index))

        acs_vector_remove(&g_Tries.player_entities[owner_index], entity_index);

    // FIX OWNER
    origin->edict()->v.owner = pPlayer->edict();

    return origin;
}

qboolean CSGameRules_CanHavePlayerItem_RG(IReGameHook_CSGameRules_CanHavePlayerItem *chain, CBasePlayer *pPlayer, CBasePlayerItem *pItem) {

    auto origin = chain->callNext(pPlayer, pItem);

    return origin;
}

CBaseEntity* CBasePlayer_GiveNamedItem_RG(IReGameHook_CBasePlayer_GiveNamedItem *chain, CBasePlayer *pPlayer, const char *classname) {

    auto origin = chain->callNext(pPlayer, classname);

    //UTIL_ServerPrint("[DEBUG] GiveNamedItem_RG(): id = %d, item = %d, classname = <%s>\n", pPlayer->entindex(), origin->entindex(), classname);

    return origin;
}

qboolean CBasePlayer_AddPlayerItem_RG(IReGameHook_CBasePlayer_AddPlayerItem *chain, CBasePlayer *pPlayer, class CBasePlayerItem *pItem) {

    auto origin = chain->callNext(pPlayer, pItem);

    if (origin) {

        std::vector<int> v;

        int id = pPlayer->entindex();

        int entity_index = pItem->entindex();

        int owner_index = ENTINDEX(pItem->pev->owner);

        acs_vector_add(&g_Tries.player_entities[id], entity_index);

        if ((is_valid_index(owner_index) || ((owner_index > 0) && is_valid_index(owner_index = ENTINDEX(pItem->pev->owner->v.owner)))) && (id != owner_index)) {

            acs_vector_remove(&g_Tries.player_entities[owner_index], entity_index);

            //UTIL_ServerPrint("[DEBUG] AddPlayerItem_RG(): remove entity = %d from owner = %d\n", entity_index, owner_index);
        }

        // FIX OWNER
        pItem->pev->owner = pPlayer->edict();

        //UTIL_ServerPrint("[DEBUG] AddPlayerItem_RG(): id = %d, entity = %d, item_classname = <%s>, item_owner = %d\n", pPlayer->entindex(), pItem->entindex(), STRING(pItem->pev->classname), owner_index);
    }

    return origin;
}

void ED_Free_RH(IRehldsHook_ED_Free *chain, edict_t *pEdict) {

    //Free_EntPrivateData(pEdict, "ED_Free_RH");

    chain->callNext(pEdict);
}

edict_t* ED_Alloc_RH(IRehldsHook_ED_Alloc* chain) {

    auto origin = chain->callNext();

	return origin;
}

int R_Spawn(edict_t *pEntity) {

    //int id = ENTINDEX(pEntity);

    //UTIL_ServerPrint("[DEBUG] Spawn(): id = %d, owner = %d\n", ENTINDEX(pEntity), ENTINDEX(pEntity->v.owner));

    RETURN_META_VALUE(MRES_IGNORED, 0);
}

qboolean R_ClientConnect(edict_t *pEntity, const char *pszName, const char *pszAddress, char szRejectReason[ 128 ]) {

    int id = ENTINDEX(pEntity);

    //UTIL_ServerPrint("[DEBUG] R_ClientConnect(): id = %d, name = %s, host = %s\n", id, pszName, pszAddress);

	RETURN_META_VALUE(MRES_IGNORED, true);
}

void R_ClientPutInServer_Post(edict_t *pEntity) {

    //UTIL_ServerPrint("[DEBUG] ClientPutInServer_Post() ===>\n");

    CBasePlayer *pPlayer = UTIL_PlayerByIndexSafe(ENTINDEX(pEntity));

    if ((pPlayer != nullptr) && !pPlayer->IsBot())

        Client_PutInServer(pEntity, STRING(pEntity->v.netname), false);

    //Client_PutInServer(pEntity, STRING(pEntity->v.netname), false);

    RETURN_META(MRES_IGNORED);
}

void CSGameRules_CheckMapConditions_RG(IReGameHook_CSGameRules_CheckMapConditions *chain) {

    g_PlayersNum[TEAM_DEAD_TT] =
    
        g_PlayersNum[TEAM_DEAD_CT] = 0;

    chain->callNext();
}

void CBasePlayer_Killed_RG(IReGameHook_CBasePlayer_Killed *chain, CBasePlayer *pPlayer, entvars_t *pevAttacker, int iGib) {

    if (g_Clients[pPlayer->entindex()].is_connected && is_valid_team(pPlayer->m_iTeam))

        g_PlayersNum[TEAM_DEAD_TT + pPlayer->m_iTeam - 1]++;
    
    //UTIL_ServerPrint("[DEBUG] KILL: num_unassigned = %d, num_tt = %d, num_ct = %d, num_spec = %d, num_dead_tt = %d, num_dead_ct = %d\n",
    //    g_PlayersNum[TEAM_UNASSIGNED], g_PlayersNum[TEAM_TERRORIST], g_PlayersNum[TEAM_CT], g_PlayersNum[TEAM_SPECTRATOR], g_PlayersNum[TEAM_DEAD_TT], g_PlayersNum[TEAM_DEAD_CT]
    //);

    chain->callNext(pPlayer, pevAttacker, iGib);
}

edict_t* CreateFakeClient_RH(IRehldsHook_CreateFakeClient *chain, const char *netname) {

    edict_t *pEntity = chain->callNext(netname);

    //UTIL_ServerPrint("[DEBUG] CreateFakeClient(): id = %d, name = %s\n", ENTINDEX(pEntity), netname);

    Client_PutInServer(pEntity, netname, true);

    return pEntity;
}

void R_ClientDisconnect(edict_t *pEntity) {

    //UTIL_ServerPrint("[DEBUG] R_ClientDisconnect() ===>\n");

    Client_Disconnected(pEntity, false, 0);

	RETURN_META(MRES_IGNORED);
}

void SV_DropClient_RH(IRehldsHook_SV_DropClient *chain, IGameClient *cl, bool crash, const char *format) {
	
    char buffer[1024];

    //UTIL_ServerPrint("[DEBUG] SV_DropClient_RH() ===>\n");

	Q_strcpy_s(buffer, (char*)format);

    Client_Disconnected(cl->GetEdict(), crash, buffer);

    chain->callNext(cl, crash, format);
}

void Client_PutInServer(edict_t *pEntity, const char *netname, const bool is_bot) {

    int id = ENTINDEX(pEntity);

    if (is_valid_index(id)) {

        g_Clients[id].is_bot = is_bot;

        g_Clients[id].is_connected = true;

        g_Clients[id].team = TEAM_UNASSIGNED;

        g_PlayersNum[TEAM_UNASSIGNED]++;

        //UTIL_ServerPrint("[DEBUG] PutInServer_Post(): id = %d, name = %s, authid = %s, team = %d, is_connected = %d\n", id, netname, GETPLAYERAUTHID(pEntity), g_Clients[id].team, g_Clients[id].is_connected);

        //UTIL_ServerPrint("[DEBUG] num_unassigned = %d, num_tt = %d, num_ct = %d, num_spec = %d\n", g_PlayersNum[TEAM_UNASSIGNED], g_PlayersNum[TEAM_TERRORIST], g_PlayersNum[TEAM_CT], g_PlayersNum[TEAM_SPECTRATOR]);
    }
}

void Client_Disconnected(edict_t *pEdict, bool crash, char *format) {

    int id = ENTINDEX(pEdict);

    if (is_valid_index(id)) {
        
        //UTIL_ServerPrint("[DEBUG] Client_Disconnected(): id = %d, is_connected = %d\n", id, g_Clients[id].is_connected);

        if (g_Clients[id].is_connected) {

            g_Clients[id].is_connected =
                
                g_Clients[id].is_bot = false;

            //if (--g_PlayersNum[g_Clients[id].team] < 0)

                //UTIL_ServerPrint("[DEBUG] Client_Disconnected(): id = %d, team = %d, count = %d << WARNING !!!", id, g_Clients[id].team, g_PlayersNum[g_Clients[id].team]);

            //CBasePlayer *pPlayer = UTIL_PlayerByIndexSafe(id);

            //UTIL_ServerPrint("[DEBUG] Client_Disconnected(): id = %d, name = %s\n", id, STRING(pPlayer->edict()->v.netname));

            g_PlayersNum[g_Clients[id].team]--;
            
            // FIX TEAMS DEAD COUNT
            if (is_valid_entity(pEdict) && (pEdict->v.deadflag != DEAD_NO) && is_valid_team(g_Clients[id].team) && (g_PlayersNum[TEAM_DEAD_TT + g_Clients[id].team - 1] > 0))

                g_PlayersNum[TEAM_DEAD_TT + g_Clients[id].team - 1]--;

            //UTIL_ServerPrint("[DEBUG] DISCONNECT: num_unassigned = %d, num_tt = %d, num_ct = %d, num_spec = %d, num_dead_tt = %d, num_dead_ct = %d\n",
            //    g_PlayersNum[TEAM_UNASSIGNED], g_PlayersNum[TEAM_TERRORIST], g_PlayersNum[TEAM_CT], g_PlayersNum[TEAM_SPECTRATOR], g_PlayersNum[TEAM_DEAD_TT], g_PlayersNum[TEAM_DEAD_CT]
            //);
        }
    }
}

void Client_TeamInfo(void* mValue) {

    static int id;
    char *msg;
    TEAMS_t new_team;

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

            } else if ((new_team != TEAM_UNASSIGNED) && (g_Clients[id].team != new_team)) {

                edict_t* pEdict = INDEXENT(id);

                // FIX TEAMS DEAD COUNT
                if (is_valid_entity(pEdict) && (pEdict->v.deadflag != DEAD_NO)) {

                    if (is_valid_team(g_Clients[id].team) && (g_PlayersNum[TEAM_DEAD_TT + g_Clients[id].team - 1] > 0))

                        g_PlayersNum[TEAM_DEAD_TT + g_Clients[id].team - 1]--;

                    if (is_valid_team(new_team) && (g_PlayersNum[TEAM_DEAD_TT + new_team - 1] > 0))

                        g_PlayersNum[TEAM_DEAD_TT + new_team - 1]++;
                } 

                //UTIL_ServerPrint("[DEBUG] Team changed!!!\n");

                g_PlayersNum[g_Clients[id].team]--;

                g_PlayersNum[new_team]++;

                g_Clients[id].team = new_team;

                //UTIL_ServerPrint("[DEBUG] num_unassigned = %d, num_tt = %d, num_ct = %d, num_spec = %d, num_dead_tt = %d, num_dead_ct = %d\n",
                //    g_PlayersNum[TEAM_UNASSIGNED], g_PlayersNum[TEAM_TERRORIST], g_PlayersNum[TEAM_CT], g_PlayersNum[TEAM_SPECTRATOR], g_PlayersNum[TEAM_DEAD_TT], g_PlayersNum[TEAM_DEAD_CT]
                //);
            }

            // FIX TEAM
            edict_t* pEdict = INDEXENT(id);
            
            if (is_valid_entity(pEdict))
            
                pEdict->v.team = new_team;

            break;
    }
}

int	R_RegUserMsg_Post(const char *pszName, int iSize) {

	for (auto& msg : g_user_msg) {

		if (strcmp(msg.name, pszName) == 0) {

			int id = META_RESULT_ORIG_RET(int);

            //UTIL_ServerPrint("[DEBUG] RegUserMsg: id = %d, %s\n", id, pszName);

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

int acs_trie_add(std::map<std::string, std::vector<int>>* trie, std::string key, int value) {

    std::vector<int> v;

    if (trie->find(key) != trie->end())

        v = (*trie)[key];
    
    if (v.size() < v.max_size()) {

        v.push_back(value);

        (*trie)[key] = v;
    }

    return v.size();
}

int acs_trie_remove(std::map<std::string, std::vector<int>>* trie, std::string key, int value) {

    std::vector<int> v;

    std::vector<int>::iterator it_value;

    if (trie->find(key) != trie->end()) {

        v = (*trie)[key];

        if ((it_value = std::find(v.begin(), v.end(), value)) != v.end()) {

            v.erase(it_value);

            if (v.size() > 0)

                (*trie)[key] = v;                    

            else

               trie->erase(key);
        }
    }

    return v.size();
}

void acs_trie_transfer(std::map<std::string, std::vector<cell>>* trie, std::string key_from, std::string key_to, int value) {

    acs_trie_remove(trie, key_from, value);
    
    g_Tries.classnames[value] = key_to;

    acs_trie_add(trie, key_to, value);
}

int acs_vector_add(std::vector<cell> *v, int value) {

    v->push_back(value);

    return v->size();
}

int acs_vector_remove(std::vector<cell> *v, int value) {

    std::vector<cell>::iterator it_value;

    if ((it_value = std::find(v->begin(), v->end(), value)) != v->end())

        v->erase(it_value);

    return v->size();
}

bool acs_get_user_buyzone(const edict_t *pEdict) {

    bool result = false;

    //UTIL_ServerPrint("[DEBUG] get_user_buyzone(): id = %d, team = %d, classname = %s\n", ENTINDEX(pEdict), pEdict->v.team, STRING(pEdict->v.classname));

    if (is_valid_entity(pEdict) && is_valid_team(pEdict->v.team) && (pEdict->v.deadflag == DEAD_NO)) {

        if (r_bMapHasBuyZone) {

            //UTIL_ServerPrint("[DEBUG] get_user_buyzone(): zone_count = %d\n", g_Tries.entities["func_buyzone"].size());

            for (auto& buyzone : g_Tries.entities["func_buyzone"]) {

                edict_t* pBuyZone = INDEXENT(buyzone);

                //UTIL_ServerPrint("[DEBUG] get_user_buyzone(): entity = %d, team = %d\n", buyzone, pBuyZone->v.team);

                if (is_valid_entity(pBuyZone) && (pEdict->v.team == pBuyZone->v.team) && is_entity_intersects(pEdict, pBuyZone)) {

                    result = true;

                    break;
                }
            }
            
        } else {

            for (auto& spawn : g_Tries.entities[pEdict->v.team == TEAM_TERRORIST ? "info_player_deathmatch" : "info_player_start"]) {

                edict_t* pSpawn = INDEXENT(spawn);

                //UTIL_ServerPrint("[DEBUG] get_user_buyzone(): spawn = %d, classname = %s, kill = %d\n", spawn, STRING(pSpawn->v.classname), (pSpawn->v.flags & FL_KILLME));

                if (is_valid_entity(pSpawn) && ((pSpawn->v.origin - pEdict->v.origin).Length() < 200.0f)) {

                    result = true;

                    break;
                }
            }
        }
    }
    return result;
}
