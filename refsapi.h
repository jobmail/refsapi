#pragma once
// gets rid of annoying "deprecated conversion from string constant blah blah" warning
#pragma GCC diagnostic ignored "-Wwrite-strings"


#include <map>
#include <precompiled.h>

#define MAX_PLAYERS                 32
#define WP_CLASS_PREFIX             "weapon_"
#define WP_CLASS_PREFIX_LEN         (sizeof(WP_CLASS_PREFIX) - 1)
#define _COUNT(x)                   (size_t)(sizeof(x)/sizeof(cell))
#define REFSAPI_CVAR                "acs_refsapi_loaded"
#define is_valid_index              __is_valid_edict_index
#define is_valid_entity             __is_valid_edict
#define is_valid_team               __is_valid_team

#define amx_ftoc(f)                 ( * ((cell*)&f) )   /* float to cell */
#define amx_ctof(c)                 ( * ((float*)&c) )  /* cell to float */

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
    std::map<std::string, std::vector<cell>> entities;       // all entities
    std::vector<cell> wp_entities;                           // has classname weapon_*
    std::map<int, std::string> classnames;
    std::vector<cell> player_entities[MAX_PLAYERS + 1];
    //std::map<std::string, std::vector<cell>> ips;
    //std::map<int, std::vector<cell>> ips_int;
};

struct sClients {

    bool is_connected;
    bool is_bot;
    eRFS_TEAMS team;

};

extern bool r_bMapHasBuyZone;

extern sClients g_Clients[MAX_PLAYERS + 1];
extern sTries g_Tries;
extern cell g_PlayersNum[6];
extern int mState;

inline void rm_quote(std::string &s) {

    if ((s.front() == '"' && s.back() == '"') || (s.front() == '\'' && s.back() == '\'') || (s.front() == '`' && (s.back() == '`' || s.back() == '\''))) {
    
        s.erase(s.begin());

        s.erase(s.end() - 1);
    }
}

inline std::string rm_quote_c(std::string &s) {

    if ((s.front() == '"' && s.back() == '"') || (s.front() == '\'' && s.back() == '\'') || (s.front() == '`' && (s.back() == '`' || s.back() == '\''))) {
    
        s.erase(s.begin());

        s.erase(s.end() - 1);
    }

    return s;
}

inline void ltrim(std::string &s) {

    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
    
        return !std::isspace(ch);
    
    }));
}

inline void rtrim(std::string &s) {
    
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
    
        return !std::isspace(ch);
    
    }).base(), s.end());
}

inline void trim(std::string &s) {
    
    rtrim(s);
    
    ltrim(s);
}

inline std::string ltrim_c(std::string s) {
    
    ltrim(s);
    
    return s;
}

inline std::string rtrim_c(std::string s) {
    
    rtrim(s);
    
    return s;
}

inline std::string trim_c(std::string s) {
    
    trim(s);
    
    return s;
}

inline bool __is_valid_edict_index(const size_t index) {

    return index > 0 && index <= gpGlobals->maxClients;
}

inline bool __is_valid_edict(const edict_t *pEdict) {

    return !(pEdict == NULL || pEdict == nullptr || pEdict->pvPrivateData == nullptr || pEdict->free || (pEdict->v.flags & FL_KILLME));
}

inline bool __is_valid_team(const int team) {

    return team >= TEAM_TERRORIST && team <= TEAM_CT;
}

inline bool is_entity_intersects(const edict_t *pEdict_1, const edict_t *pEdict_2) {

    return !(pEdict_1->v.absmin.x > pEdict_2->v.absmax.x ||
            pEdict_1->v.absmin.y > pEdict_2->v.absmax.y ||
            pEdict_1->v.absmin.z > pEdict_2->v.absmax.z ||
            pEdict_1->v.absmax.x < pEdict_2->v.absmin.x ||
            pEdict_1->v.absmax.y < pEdict_2->v.absmin.y ||
            pEdict_1->v.absmax.z < pEdict_2->v.absmin.z);
}

edict_t* CreateFakeClient_RH(IRehldsHook_CreateFakeClient *chain, const char *netname);
void SV_DropClient_RH(IRehldsHook_SV_DropClient *chain, IGameClient *cl, bool crash, const char *format);
edict_t* ED_Alloc_RH(IRehldsHook_ED_Alloc* chain);
void ED_Free_RH(IRehldsHook_ED_Free *chain, edict_t *pEdict);

void CBasePlayer_Killed_RG(IReGameHook_CBasePlayer_Killed *chain, CBasePlayer *pPlayer, entvars_t *pevAttacker, int iGib);
void CSGameRules_CheckMapConditions_RG(IReGameHook_CSGameRules_CheckMapConditions *chain);
qboolean CBasePlayer_AddPlayerItem_RG(IReGameHook_CBasePlayer_AddPlayerItem *chain, CBasePlayer *pPlayer, class CBasePlayerItem *pItem);
CBaseEntity* CBasePlayer_GiveNamedItem_RG(IReGameHook_CBasePlayer_GiveNamedItem *chain, CBasePlayer *pPlayer, const char *classname);
qboolean CSGameRules_CanHavePlayerItem_RG(IReGameHook_CSGameRules_CanHavePlayerItem *chain, CBasePlayer *pPlayer, CBasePlayerItem *pItem);
CWeaponBox* CreateWeaponBox_RG(IReGameHook_CreateWeaponBox *chain, CBasePlayerItem *pItem, CBasePlayer *pPlayer, const char *model, Vector &v_origin, Vector &v_angels, Vector &v_velocity, float life_time, bool pack_ammo);
void CBasePlayer_Spawn_RG(IReGameHook_CBasePlayer_Spawn *chain, CBasePlayer *pPlayer);

qboolean R_ClientConnect(edict_t *pEntity, const char *pszName, const char *pszAddress, char szRejectReason[128]);
void R_ClientPutInServer(edict_t *pEntity);
void R_ClientPutInServer_Post(edict_t *pEntity);
void R_ClientDisconnect(edict_t *pEntity);
int R_Spawn(edict_t *pEntity);
edict_t* R_CreateNamedEntity(string_t className);


void* R_PvAllocEntPrivateData(edict_t *pEdict, int32 cb);
void* R_PvEntPrivateData(edict_t *pEdict);
void* R_PvEntPrivateData_Post(edict_t *pEdict);

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
void Client_PutInServer(edict_t *pEntity, const char *netname, const bool is_bot);
void Client_Disconnected(edict_t *pEdict, bool crash, char *format);
void Alloc_EntPrivateData(edict_t *pEdict);
void Free_EntPrivateData(edict_t *pEdict);

int acs_trie_add(std::map<std::string, std::vector<cell>>* trie, std::string key, int value);
int acs_trie_remove(std::map<std::string, std::vector<cell>>* trie, std::string key, int value);
void acs_trie_transfer(std::map<std::string, std::vector<cell>>* trie, std::string key_from, std::string key_to, int value);
int acs_vector_add(std::vector<cell> *v, int value);
int acs_vector_remove(std::vector<cell> *v, int value);
float acs_roundfloat(float value, int precision);
bool acs_get_user_buyzone(const edict_t *pEdict);
char* fmt(char *fmt, ...);

extern int gmsgTeamInfo;
extern funEventCall modMsgsEnd[MAX_REG_MSGS];
extern funEventCall modMsgs[MAX_REG_MSGS];
extern void (*function)(void*);
extern void (*endfunction)(void*);