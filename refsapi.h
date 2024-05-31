#pragma once

#include <map>
#include <rehlds_api.h>

#define ENUM_COUNT(e)               (sizeof(e) / sizeof(int))
#define MAX_REG_MSGS                256
#define MAX_PLAYERS                 32

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

struct g_ClientsTrie {

    std::map<std::string, int> name_t;
    std::map<std::string, int> authid_t;

};

struct sClients {

    bool is_connected;
    eRFS_TEAMS team;

};


extern int g_PlayersNum[ENUM_COUNT(eRFS_TEAMS)];
extern int mState;

void R_ClientPutInServer(edict_t *pEntity);
void R_ClientPutInServer_Post(edict_t *pEntity);
void R_ClientDisconnect(edict_t *pEntity);
void SV_DropClient_RH(IRehldsHook_SV_DropClient *chain, IGameClient *cl, bool crash, const char *format);
edict_t* R_ED_Alloc(IRehldsHook_ED_Alloc* chain);

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
void Client_Disconnected(int id, bool crash, char *format);

extern int gmsgTeamInfo;
extern funEventCall modMsgsEnd[MAX_REG_MSGS];
extern funEventCall modMsgs[MAX_REG_MSGS];
extern void (*function)(void*);
extern void (*endfunction)(void*);

