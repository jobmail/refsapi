#include "precompiled.h"

// BEGIN
void R_ClientPutInServer_Post(edict_t *pEntity)
{
	CBasePlayer *pPlayer = UTIL_PlayerByIndexSafe(ENTINDEX(pEntity));
	if (!pPlayer->IsBot())
	{
        UTIL_ServerPrint("PutInserver_Post(): %s", pPlayer->pev->netname);
	}

	RETURN_META(MRES_IGNORED);
}