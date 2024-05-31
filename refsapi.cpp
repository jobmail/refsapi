#include "precompiled.h"

int gmsgTeamInfo;
int mState;
int g_PlayersNum[ENUM_COUNT(eRFS_TEAMS)];

sClients g_Clients[MAX_PLAYERS + 1];

funEventCall modMsgsEnd[MAX_REG_MSGS];
funEventCall modMsgs[MAX_REG_MSGS];

void (*function)(void*);
void (*endfunction)(void*);

g_RegUserMsg g_user_msg[] =
{
	{ "TeamInfo", &gmsgTeamInfo, Client_TeamInfo, false },
};

void R_ClientPutInServer(edict_t *pEntity) {

    int id = ENTINDEX(pEntity);

    SERVER_PRINT("[DEBUG] ClientPutInServer() ===>\n");

	CBasePlayer *pPlayer = UTIL_PlayerByIndexSafe(id);
	
    //if (!pPlayer->IsBot())

    g_Clients[id].is_connected = true;

    g_Clients[id].team = TEAM_UNASSIGNED;

    g_PlayersNum[TEAM_UNASSIGNED]++;

    UTIL_ServerPrint("[DEBUG] PutInserver_Post(): id = %d, name = %s, authid = %s, team = %d, is_connected = %d\n", id, STRING(pPlayer->pev->netname), GETPLAYERAUTHID(pPlayer->edict()), g_Clients[id].team, g_Clients[id].is_connected);

    UTIL_ServerPrint("[DEBUG] num_unassigned = %d, num_tt = %d, num_ct = %d, num_spec = %d\n", g_PlayersNum[TEAM_UNASSIGNED], g_PlayersNum[TEAM_TERRORIST], g_PlayersNum[TEAM_CT], g_PlayersNum[TEAM_SPECTRATOR]);

    SET_META_RESULT(MRES_IGNORED);

}

void R_ClientPutInServer_Post(edict_t *pEntity) {

    RETURN_META(MRES_IGNORED);

}

void R_ClientDisconnect(edict_t *pEntity) {

    int id = ENTINDEX(pEntity);

    SERVER_PRINT("[DEBUG] ClientDisconnect() ===>\n");

	Client_Disconnected(id, false, 0);

	RETURN_META(MRES_IGNORED);
}

void SV_DropClient_RH(IRehldsHook_SV_DropClient *chain, IGameClient *cl, bool crash, const char *format) {
	
    char buffer[1024];

    int id = ENTINDEX(cl->GetEdict());    

    SERVER_PRINT("[DEBUG] SV_DropClient_RH() ===>\n");

	Q_strcpy_s(buffer, (char*)format);

    Client_Disconnected(id, crash, buffer);

    chain->callNext(cl, crash, buffer);

}

void Client_Disconnected(int id, bool crash, char *format) {

    CBasePlayer *pPlayer = UTIL_PlayerByIndexSafe(id);

    if (!pPlayer->IsBot())

        UTIL_ServerPrint("[DEBUG] Client_Disconnected(): id = %d, name = %s authid = %s, crash = %d\n", id, STRING(pPlayer->pev->netname), GETPLAYERAUTHID(pPlayer->edict()), crash);

    g_Clients[id].is_connected = false;

    g_PlayersNum[g_Clients[id].team]--;

    UTIL_ServerPrint("[DEBUG] num_unassigned = %d, num_tt = %d, num_ct = %d, num_spec = %d\n", g_PlayersNum[TEAM_UNASSIGNED], g_PlayersNum[TEAM_TERRORIST], g_PlayersNum[TEAM_CT], g_PlayersNum[TEAM_SPECTRATOR]);
}

void Client_TeamInfo(void* mValue) {

    static int id;
    char* msg;
    eRFS_TEAMS new_team;

    switch (mState++) {
    
        case 0:

			id = *(int*)mValue;
		
        	break;

        case 1:
		
        	if (id < 1 || id > gpGlobals->maxClients) break;
		
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

            UTIL_ServerPrint("TeamInfo: id = %d, is_connected = %d, team_old = %d, team_new = %d\n", id, g_Clients[id].is_connected, g_Clients[id].team, new_team);

            if (g_Clients[id].is_connected && g_Clients[id].team != new_team) {

                UTIL_ServerPrint("[DEBUG] Team changed!!!\n");

                g_PlayersNum[g_Clients[id].team]++;

                g_PlayersNum[new_team]++;

                g_Clients[id].team = new_team;

                UTIL_ServerPrint("[DEBUG] num_unassigned = %d, num_tt = %d, num_ct = %d, num_spec = %d\n", g_PlayersNum[TEAM_UNASSIGNED], g_PlayersNum[TEAM_TERRORIST], g_PlayersNum[TEAM_CT], g_PlayersNum[TEAM_SPECTRATOR]);

            }

			//CBasePlayer *pPlayer = UTIL_PlayerByIndexSafe(index);
            //strcpy(pPlayer->m_szTeamName, msg);
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

	//RETURN_META(MRES_IGNORED);
    SET_META_RESULT(MRES_IGNORED);
}

void R_WriteByte_Post(int iValue) {

	if (function) (*function)((void *)&iValue);

	//RETURN_META(MRES_IGNORED);
    SET_META_RESULT(MRES_IGNORED);
}

void R_WriteChar_Post(int iValue) {

	if (function) (*function)((void *)&iValue);

	//RETURN_META(MRES_IGNORED);
    SET_META_RESULT(MRES_IGNORED);
}

void R_WriteShort_Post(int iValue) {

	if (function) (*function)((void *)&iValue);

	//RETURN_META(MRES_IGNORED);
    SET_META_RESULT(MRES_IGNORED);
}

void R_WriteLong_Post(int iValue) {

	if (function) (*function)((void *)&iValue);

	//RETURN_META(MRES_IGNORED);
    SET_META_RESULT(MRES_IGNORED);
}

void R_WriteAngle_Post(float flValue) {

	if (function) (*function)((void *)&flValue);

	RETURN_META(MRES_IGNORED);
}

void R_WriteCoord_Post(float flValue) {

	if (function) (*function)((void *)&flValue);

	//RETURN_META(MRES_IGNORED);
    SET_META_RESULT(MRES_IGNORED);
}

void R_WriteString_Post(const char *sz) {

	if (function) (*function)((void *)sz);

	//RETURN_META(MRES_IGNORED);
    SET_META_RESULT(MRES_IGNORED);
}

void R_WriteEntity_Post(int iValue) {

	if (function) (*function)((void *)&iValue);

	//RETURN_META(MRES_IGNORED);
    SET_META_RESULT(MRES_IGNORED);
}

void R_MessageEnd_Post(void) {

	if (endfunction) (*endfunction)(NULL);

	//RETURN_META(MRES_IGNORED);
    SET_META_RESULT(MRES_IGNORED);
}