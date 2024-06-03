#pragma once
// gets rid of annoying "deprecated conversion from string constant blah blah" warning
#pragma GCC diagnostic ignored "-Wwrite-strings"


#include <map>
#include <precompiled.h>

#define MAX_REG_MSGS                256 + 16
#define MAX_PLAYERS                 32
#define WP_CLASS_PREFIX             "weapon_"
#define REFSAPI_CVAR                "acs_refsapi_loaded"
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
    std::map<std::string, std::vector<int>> entities;       // all entities
    std::map<std::string, std::vector<int>> wp_entities;    // has classname weapon_*
    std::map<int, std::string> classnames;
    std::vector<int> player_entities[MAX_PLAYERS + 1];
    //std::map<std::string, std::vector<int>> ips;
    //std::map<int, std::vector<int>> ips_int;
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
void ED_Free_RH(IRehldsHook_ED_Free *chain, edict_t *pEdict);

void CBasePlayer_Killed_RG(IReGameHook_CBasePlayer_Killed *chain, CBasePlayer *pPlayer, entvars_t *pevAttacker, int iGib);
void CSGameRules_CheckMapConditions_RG(IReGameHook_CSGameRules_CheckMapConditions *chain);
qboolean CBasePlayer_AddPlayerItem_RG(IReGameHook_CBasePlayer_AddPlayerItem *chain, CBasePlayer *pPlayer, class CBasePlayerItem *pItem);
CBaseEntity* CBasePlayer_GiveNamedItem_RG(IReGameHook_CBasePlayer_GiveNamedItem *chain, CBasePlayer *pPlayer, const char *classname);
qboolean CSGameRules_CanHavePlayerItem_RG(IReGameHook_CSGameRules_CanHavePlayerItem *chain, CBasePlayer *pPlayer, CBasePlayerItem *pItem);
CWeaponBox* CreateWeaponBox_RG(IReGameHook_CreateWeaponBox *chain, CBasePlayerItem *pItem, CBasePlayer *pPlayer, const char *model, Vector &v_origin, Vector &v_angels, Vector &v_velocity, float life_time, bool pack_ammo);

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
void Client_PutInServer(edict_t *pEntity, const char *netname);
void Client_Disconnected(int id, bool crash, char *format);
void Alloc_EntPrivateData(edict_t *pEdict);
void Free_EntPrivateData(edict_t *pEdict);

extern int gmsgTeamInfo;
extern funEventCall modMsgsEnd[MAX_REG_MSGS];
extern funEventCall modMsgs[MAX_REG_MSGS];
extern void (*function)(void*);
extern void (*endfunction)(void*);

int acs_trie_add(std::map<std::string, std::vector<int>>* trie, std::string key, int value) {

    std::vector<int> v;

    if (trie->find(key) != trie->end())

        v = (*trie)[key];

    else

        v.clear();
    
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

int acs_vector_remove(std::vector<int> *v, int value) {

    std::vector<int>::iterator it_value;

    if ((it_value = std::find(v->begin(), v->end(), value)) != v->end())

        v->erase(it_value);

    return v->size();
}