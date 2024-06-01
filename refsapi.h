#pragma once

#include <map>
#include <precompiled.h>

#define MAX_REG_MSGS                256 + 16
#define MAX_PLAYERS                 32
#define is_valid_index              __is_valid_edict_index

inline bool __is_valid_edict_index(size_t index) {

    return index > 0 && index <= gpGlobals->maxClients;
}

typedef void (*funEventCall)(void*);

typedef enum {

    TEAM_UNASSIGNED,
    TEAM_TERRORIST,
    TEAM_CT,
    TEAM_SPECTRATOR,
    TEAM_DEAD_TT,
    TEAM_DEAD_CT

} eRFS_TEAMS;

struct g_RegUserMsg {

	const char* name;
	int* id;
	funEventCall func;
    bool endmsg;

};

struct sTries {

    std::map<std::string, int> names;
    std::map<std::string, int> authids;
    std::map<std::string, std::vector<int>> entities;
    std::map<std::string, std::vector<int>> ips;
    std::map<int, std::vector<int>> ips_int;
};

struct sClients {

    bool is_connected;
    bool is_bot;
    eRFS_TEAMS team;

};

extern sClients g_Clients[MAX_PLAYERS + 1];
extern sTries g_Tries;
extern int g_PlayersNum[6];
extern int mState;

edict_t* CreateFakeClient_RH(IRehldsHook_CreateFakeClient *chain, const char *netname);
void SV_DropClient_RH(IRehldsHook_SV_DropClient *chain, IGameClient *cl, bool crash, const char *format);
edict_t* ED_Alloc_RH(IRehldsHook_ED_Alloc* chain);

void CBasePlayer_Killed_RG(IReGameHook_CBasePlayer_Killed *chain, CBasePlayer *pPlayer, entvars_t *pevAttacker, int iGib);
void CSGameRules_CheckMapConditions_RG(IReGameHook_CSGameRules_CheckMapConditions *chain);

qboolean R_ClientConnect(edict_t *pEntity, const char *pszName, const char *pszAddress, char szRejectReason[128]);
void R_ClientPutInServer(edict_t *pEntity);
void R_ClientPutInServer_Post(edict_t *pEntity);
void R_ClientDisconnect(edict_t *pEntity);
int R_Spawn(edict_t *pEntity);

int	 R_RegUserMsg_Post(const char *pszName, int iSize);
void R_MessageBegin_Post(int msg_dest, int msg_type, const float *pOrigin, edict_t *ed);
void R_WriteByte_Post(int iValue);
void R_WriteChar_Post(int iValue);
void R_WriteShort_Post(int iValue);
void R_WriteLong_Post(int iValue);
void R_WriteAngle_Post(float flValue);
void R_WriteCoord_Post(float flValue);
void R_WriteString_Post(const char *sz);
void R_WriteEntity_Post(int iValue);
void R_MessageEnd_Post(void);

void Client_TeamInfo(void*);
void Client_PutInServer(edict_t *pEntity, const char *netname);
void Client_Disconnected(int id, bool crash, char *format);

extern int gmsgTeamInfo;
extern funEventCall modMsgsEnd[MAX_REG_MSGS];
extern funEventCall modMsgs[MAX_REG_MSGS];
extern void (*function)(void*);
extern void (*endfunction)(void*);

