#include "precompiled.h"

int gmsgTeamInfo;
int mState;

funEventCall modMsgsEnd[MAX_REG_MSGS];
funEventCall modMsgs[MAX_REG_MSGS];

void (*function)(void*);
void (*endfunction)(void*);

struct g_RegUserMsg
{
	const char* name;
	int* id;
	funEventCall func;
    bool endmsg;
} g_user_msg[] =
{
	{ "TeamInfo", &gmsgTeamInfo, Client_TeamInfo, false },
};

// BEGIN ===>

void R_ClientPutInServer_Post(edict_t *pEntity) {

	CBasePlayer *pPlayer = UTIL_PlayerByIndexSafe(ENTINDEX(pEntity));
	
    if (!pPlayer->IsBot()) {
    
        SERVER_PRINT("[DEBUG] R_ClientPutInServer_Post() ===>\n");

        UTIL_ServerPrint("PutInserver_Post(): %s <%s>\n", STRING(pPlayer->pev->netname), GETPLAYERAUTHID(pPlayer->edict()));

    }

    SET_META_RESULT(MRES_IGNORED);
}

int	R_RegUserMsg_Post(const char *pszName, int iSize) {

    SERVER_PRINT("[DEBUG] R_RegUserMsg_Post() ===>\n");

	for (auto& msg : g_user_msg) {      //(int i = 0; g_user_msg[i].name;	i++) {

		if (strcmp(msg.name, pszName) == 0) {

			int id = META_RESULT_ORIG_RET(int);

            UTIL_ServerPrint("RegUserMsg: id = %d, %s", id, pszName);


            *msg.id = id;

            if (msg.endmsg) {
		
        		modMsgsEnd[id] = msg.func;
		
            } else {
		
        		modMsgs[id] = msg.func;
            }

			break;

        }

	}

    SERVER_PRINT("[DEBUG] R_RegUserMsg_Post() <===\n");

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

void Client_TeamInfo(void* mValue) {

    static int index;
    char* msg;

    switch (mState++) {
    
        case 0:

			index = *(int*)mValue;
		
        	break;

        case 1:
		
        	if (index < 1 || index > gpGlobals->maxClients) break;
		
        	msg = (char*)mValue;
			
            if (!msg) break;

            SERVER_PRINT("[DEBUG] Client_TeamInfo() ===>\n");

            //UTIL_ServerPrint("TeamInfo: id = %d, team = %s\n", index, STRING(msg));

			//CBasePlayer *pPlayer = UTIL_PlayerByIndexSafe(index);
            //strcpy(pPlayer->m_szTeamName, msg);
    
    }

}