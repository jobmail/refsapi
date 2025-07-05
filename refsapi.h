#pragma once

#include "precompiled.h"
// gets rid of annoying "deprecated conversion from string constant blah blah" warning
#pragma GCC diagnostic ignored "-Wwrite-strings"

#define MAX_PLAYERS                 32
#define DECIMAL_POINT               '.'
#define WP_CLASS_PREFIX             "weapon_"
#define WP_CLASS_PREFIX_LEN (sizeof(WP_CLASS_PREFIX) - 1)
#define REFSAPI_CVAR                "acs_refsapi_loaded"
#define check_it_empty(x) ((x)._M_node == nullptr)
#define check_it_empty_r(x) \
    if (check_it_empty(x))  \
    return

#define _QQ                         "\"'`"
#define _TRIM_CHARS                 " \r\t\n"
#define _COUNT(x) (size_t)(sizeof(x) / sizeof(cell))

#define amx_ftoc(f) (*((cell *)&f))  /* float to cell */
#define amx_ctof(c) (*((float *)&c)) /* cell to float */

typedef void (*funEventCall)(void *);

typedef enum TEAMS_e
{

    TEAM_UNASSIGNED,
    TEAM_TERRORIST,
    TEAM_CT,
    TEAM_SPECTRATOR,
    TEAM_DEAD_TT,
    TEAM_DEAD_CT

} TEAMS_t;

struct g_RegUserMsg
{

    const char *name;
    int *id;
    funEventCall func;
    bool endmsg;
};

struct sTries
{

    std::map<std::string, int> names;
    std::map<std::string, int> authids;
    std::map<std::string, std::vector<cell>> entities; // all entities
    std::vector<cell> wp_entities;                     // has classname weapon_*
    std::map<int, std::string> classnames;
    std::vector<cell> player_entities[MAX_PLAYERS + 1];
    // std::map<std::string, std::vector<cell>> ips;
    // std::map<int, std::vector<cell>> ips_int;
};

struct sClients
{
    bool is_connected;
    bool is_bot;
    TEAMS_t team;
};

typedef std::codecvt_utf8<wchar_t> convert_type;

extern bool r_bMapHasBuyZone;
extern char g_buff[4096];
extern sClients g_Clients[MAX_PLAYERS + 1];
extern sTries g_Tries;
extern cell g_PlayersNum[6];
extern int mState;
extern int gmsgTeamInfo;
extern std::wstring_convert<convert_type, wchar_t> g_converter;
extern const std::locale _LOCALE;

extern funEventCall modMsgsEnd[MAX_REG_MSGS];
extern funEventCall modMsgs[MAX_REG_MSGS];

extern void (*function)(void *);
extern void (*endfunction)(void *);

edict_t *CreateFakeClient_RH(IRehldsHook_CreateFakeClient *chain, const char *netname);
void SV_DropClient_RH(IRehldsHook_SV_DropClient *chain, IGameClient *cl, bool crash, const char *format);
// edict_t* ED_Alloc_RH(IRehldsHook_ED_Alloc* chain);
// void ED_Free_RH(IRehldsHook_ED_Free *chain, edict_t *pEdict);

void CBasePlayer_Killed_RG(IReGameHook_CBasePlayer_Killed *chain, CBasePlayer *pPlayer, entvars_t *pevAttacker, int iGib);
void CSGameRules_CheckMapConditions_RG(IReGameHook_CSGameRules_CheckMapConditions *chain);
qboolean CBasePlayer_AddPlayerItem_RG(IReGameHook_CBasePlayer_AddPlayerItem *chain, CBasePlayer *pPlayer, class CBasePlayerItem *pItem);
qboolean CBasePlayer_RemovePlayerItem_RG(IReGameHook_CBasePlayer_RemovePlayerItem *chain, CBasePlayer *pthis, CBasePlayerItem *pItem);
CBaseEntity *CBasePlayer_GiveNamedItem_RG(IReGameHook_CBasePlayer_GiveNamedItem *chain, CBasePlayer *pPlayer, const char *classname);
qboolean CSGameRules_CanHavePlayerItem_RG(IReGameHook_CSGameRules_CanHavePlayerItem *chain, CBasePlayer *pPlayer, CBasePlayerItem *pItem);
CWeaponBox *CreateWeaponBox_RG(IReGameHook_CreateWeaponBox *chain, CBasePlayerItem *pItem, CBasePlayer *pPlayer, const char *model, Vector &v_origin, Vector &v_angels, Vector &v_velocity, float life_time, bool pack_ammo);
void CBasePlayer_Spawn_RG(IReGameHook_CBasePlayer_Spawn *chain, CBasePlayer *pPlayer);
void CSGameRules_ChangeLevel_RG(IReGameHook_CSGameRules_ChangeLevel *chain);

void R_ExecuteServerStringCmd(IRehldsHook_ExecuteServerStringCmd *chain, const char *cmd, cmd_source_t src, IGameClient *id);
void R_StartFrame_Post(void);
void R_ClientPutInServer_Post(edict_t *p);
void R_ClientDisconnect(edict_t *p);
void *R_PvAllocEntPrivateData(edict_t *p, int32 cb);
int R_RegUserMsg_Post(const char *pszName, int iSize);
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

void Client_TeamInfo(void *);
void Client_PutInServer(edict_t *p, const char *netname, const bool is_bot);
void Client_Disconnected(edict_t *p, bool crash, char *format);
void Alloc_EntPrivateData(edict_t *p);
void Free_EntPrivateData(edict_t *p);

int trie_add(std::map<std::string, std::vector<int>> &trie, std::string key, int value);
int trie_remove(std::map<std::string, std::vector<int>> &trie, std::string key, int value);
void trie_transfer(std::map<std::string, std::vector<cell>> &trie, std::string key_from, std::string key_to, int value);
int vector_add(std::vector<cell> &v, int value);
int vector_remove(std::vector<cell> &v, int value);
size_t copy_entities(std::vector<int> &v, std::string &key, cell *dest, size_t max_size);
bool get_user_buyzone(const edict_t *pEdict);