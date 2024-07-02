#include "precompiled.h"

void CmdEnd_Post(const edict_t* pEdict)
{
	g_recoil_mngr.cmd_end(pEdict);
}

void RG_CBasePlayer_PostThink(IReGameHook_CBasePlayer_PostThink* chain, CBasePlayer* player)
{
    chain->callNext(player);
    g_recoil_mngr.think_post(player);
}