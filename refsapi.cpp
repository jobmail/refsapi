#include "precompiled.h"

// BEGIN
void R_ClientPutInServer_Post(edict_t *pEntity) {

	CBasePlayer *pPlayer = UTIL_PlayerByIndexSafe(ENTINDEX(pEntity));
	
    if (!pPlayer->IsBot()) {
    
        SERVER_PRINT("[DEBUG] R_ClientPutInServer_Post() ===>\n");

        UTIL_ServerPrint("PutInserver_Post(): %s <%s>\n", STRING(pPlayer->pev->netname), GETPLAYERAUTHID(pPlayer->edict()));

    }
    SET_META_RESULT(MRES_IGNORED);
}