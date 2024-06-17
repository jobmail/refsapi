#include "precompiled.h"

int gmsgTeamInfo;
int mState;
int g_PlayersNum[6];

int a = sizeof(eRFS_TEAMS);

bool r_bMapHasBuyZone;

//char g_fmt_buff[1024];
//wchar_t g_wfmt_buff[1024];

//--------------------------------------------------------
class fmt {
    const size_t size = 1024;
    char* buff;
    public:
        fmt(char *fmt, ...) {
            buff = new char[size];
            va_list arg_ptr;
            va_start(arg_ptr, fmt);
            Q_vsnprintf(buff, size, fmt, arg_ptr);
            va_end(arg_ptr);
        }
        ~fmt() {
            delete buff;
        }
        char* c_str() {
            buff[size - 1] = 0;
            return buff;
        }
};

class wfmt {
    const size_t size = 1024;
    wchar_t* buff;
    public:
        wfmt(wchar_t *fmt, ...) {
            buff = new wchar_t[size];
            va_list arg_ptr;
            va_start(arg_ptr, fmt);
            std::vswprintf(buff, size, fmt, arg_ptr);
            va_end(arg_ptr);
        }
        ~wfmt() {
            delete buff;
        }
        wchar_t* c_str() {
            buff[size - 1] = 0;
            return buff;
        }
};

class wstoc {
    const size_t size = 1024;
    char* buff;
    public:
        wstoc(const wchar_t *s) {
            buff = new char[size];
            wcstombs(buff, s, size);
        }
        wstoc(const std::wstring s) {
            buff = new char[size];
            wcstombs(buff, s.c_str(), size);
        }
        wstoc(wfmt c) {
            buff = new char[size];
            wcstombs(buff, c.c_str(), size);
        }
        ~wstoc() {
            delete buff;
        }
        char* c_str() {
            buff[size - 1] = 0;
            return buff;
        }
};

inline float stof(std::string s, bool has_min = false, float min_val = 0.0f, bool has_max = false, float max_val = 0.0f) {
    float result = is_number(s) ? std::stof(s) : 0.0f;
    if (has_min && result < min_val) result = min_val;
    if (has_min && result > max_val) result = max_val;
    return result;
}

inline bool file_exists(const std::wstring &name) {

    struct stat buff;
    
    return (stat(wstoc(name).c_str(), &buff) == 0);
}

inline int rm_quote(std::string &s) {

    int result = 0;

    bool f[2];

    char qq[] = _QQ;

    for (int i = 0; i < sizeof(qq) - 1; i++) {

        f[0] = f[1] = 0;

        if ((f[0] = s.front() == qq[i]) && (f[1] = s.back() == qq[i])) {

            s.erase(s.begin());
        
            s.erase(s.end() - 1);

            result = 1;

            break;

        } else if (f[0] != f[1]) {

            result = -1;

            break;
        }
    }

    return result;
}

inline int rm_quote(std::wstring &s) {

    int result = 0;

    bool f[2];

    char qq[] = _QQ;

    for (int i = 0; i < sizeof(qq) - 1; i++) {

        f[0] = f[1] = 0;

        if ((f[0] = s.front() == qq[i]) && (f[1] = s.back() == qq[i])) {

            s.erase(s.begin());
        
            s.erase(s.end() - 1);

            result = 1;

            break;

        } else if (f[0] != f[1]) {

            result = -1;

            break;
        }
    }

    return result;
}


inline std::string rm_quote_c(std::string &s) {

    char qq[] = _QQ;

    for (int i = 0; i < sizeof(qq) - 1; i++) {

        if (s.front() == qq[i] && s.back() == qq[i]) {

            s.erase(s.begin());
        
            s.erase(s.end() - 1);

            break;
        }
    }

    return s;
}

inline std::wstring rm_quote_c(std::wstring &s) {

    if ((s.front() == '"' && s.back() == '"') || (s.front() == '\'' && s.back() == '\'') || (s.front() == '`' && (s.back() == '`' || s.back() == '\''))) {
    
        s.erase(s.begin());

        s.erase(s.end() - 1);
    }

    return s;
}

inline void ltrim(std::string &s) {

    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
    
        return !std::isspace(ch) && ch != '\t';
    
    }));
}

inline void ltrim(std::wstring &s) {

    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
    
        return !std::isspace(ch) && ch != '\t';
    
    }));
}

inline void rtrim(std::string &s) {
    
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
    
        return !std::isspace(ch) && ch != '\t';
    
    }).base(), s.end());
}

inline void rtrim(std::wstring &s) {
    
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
    
        return !std::isspace(ch) && ch != '\t';
    
    }).base(), s.end());
}

inline void trim(std::string &s) {
    
    rtrim(s);
    
    ltrim(s);
}

inline void trim(std::wstring &s) {
    
    rtrim(s);
    
    ltrim(s);
}

inline std::string ltrim_c(std::string s) {
    
    ltrim(s);
    
    return s;
}

inline std::wstring ltrim_c(std::wstring s) {
    
    ltrim(s);
    
    return s;
}

inline std::string rtrim_c(std::string s) {
    
    rtrim(s);
    
    return s;
}

inline std::wstring rtrim_c(std::wstring s) {
    
    rtrim(s);
    
    return s;
}

inline std::string trim_c(std::string s) {
    
    trim(s);
    
    return s;
}

inline std::wstring trim_c(std::wstring s) {
    
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

typedef struct m_cvar_s {
    cvar_t *cvar;
    std::wstring value;
    std::wstring desc;
    bool has_min;
    bool has_max;
    float min_val;
    float max_val;
} m_cvar_t;

typedef std::map<std::wstring, m_cvar_s> cvar_list_t;
typedef std::map<int, cvar_list_t> plugin_cvar_t;
typedef std::pair<cvar_list_t::iterator, bool> cvar_list_result_t;

typedef struct cvar_mngr_s {
    plugin_cvar_t plugin;
} cvar_mngr_t;

class cvar_mngr {
    cvar_mngr_t cvars;
    private:
        void cvar_direct_set(cvar_t *cvar, char *value) {
            if (cvar != nullptr)
                g_engfuncs.pfnCvar_DirectSet(cvar, value);
        }
        cvar_t* create_cvar(std::wstring name, std::wstring value, int flags = 0) {
            char* name_c = wstoc(name).c_str();
            char* value_c = wstoc(value).c_str();
            cvar_t* p_cvar = CVAR_GET_POINTER(name_c);
            if (p_cvar == nullptr) {
                cvar_t cvar;
                cvar.name = name_c;
                cvar.flags = flags;
                cvar.string = value_c;
                cvar.value = 0.0f;
                cvar.next = nullptr;
                CVAR_REGISTER(&cvar);
                p_cvar = CVAR_GET_POINTER(name_c);
            }
            return p_cvar;
        }
    public:
        cvar_list_result_t add(CPluginMngr::CPlugin *plugin, std::wstring name, std::wstring value, int flags = 0, std::wstring desc = L"", bool has_min = false, float min_val = 0.0f, bool has_max = false, float max_val = 0.0f) {
            std::string s = g_converter.to_bytes(value);
            plugin_cvar_t::iterator plugin_it;
            cvar_list_t::iterator cvar_it;
            cvar_list_t p_cvar_list;
            if (name.empty() || value.empty()) return {cvar_it, false};
            // FIX NAME
            //name = std::tolower(name, _LOCALE);
            // IS NUMBER?
            if (is_number(s)) {
                value = g_converter.from_bytes(std::to_string(stof(s, has_min, min_val, has_max, max_val)));
            }
            // PLUGIN EXIST?
            if ((plugin_it = cvars.plugin.find(plugin->getId())) != cvars.plugin.end()) {
                p_cvar_list = plugin_it->second;
                // CVAR EXIST?
                if ((cvar_it = p_cvar_list.find(name)) != p_cvar_list.end())
                    return {cvar_it, true};
            }
            // CREATE CVAR
            m_cvar_t m_cvar;
            if ((m_cvar.cvar = create_cvar(name, value, flags)) != nullptr) {
                m_cvar.value = value;
                m_cvar.desc = desc;
                m_cvar.has_min = has_min;
                m_cvar.min_val = min_val;                
                m_cvar.has_max = has_max;                
                m_cvar.max_val = max_val;
                auto result = p_cvar_list.insert({name, m_cvar});
                // SAVE cvar_list
                if (result.second) {
                    // PLUGIN EXIST?
                    if (plugin_it != cvars.plugin.end())
                        plugin_it->second = p_cvar_list;
                    // CREATE PLUGINS CVAR
                    else
                        cvars.plugin[plugin->getId()] = p_cvar_list;
                    return result;
                }
            }
            AMXX_LogError(plugin->getAMX(), AMX_ERR_NATIVE, "%s: cvar creation error <%s> => <%s>", __FUNCTION__, wstoc(name).c_str(), wstoc(value).c_str());
            return {cvar_it, false};
        }
        void clear() {
            cvars.plugin.clear();
        }
};
//--------------------------------------------------------

cvar_mngr g_cvar_mngr;

sClients g_Clients[MAX_PLAYERS + 1];
sTries g_Tries;

std::wstring_convert<std::codecvt_utf8<wchar_t>> g_converter;

funEventCall modMsgsEnd[MAX_REG_MSGS];
funEventCall modMsgs[MAX_REG_MSGS];

void (*function)(void*);
void (*endfunction)(void*);

g_RegUserMsg g_user_msg[] =
{
	{ "TeamInfo", &gmsgTeamInfo, Client_TeamInfo, false },
};

edict_t* R_CreateNamedEntity(string_t className) {

    //UTIL_ServerPrint("[DEBUG] R_CreateNamedEntity(): classname = <%s>\n", STRING(className));

    RETURN_META_VALUE(MRES_IGNORED, 0);
}

void* R_PvAllocEntPrivateData(edict_t *pEdict, int32 cb) {

    Alloc_EntPrivateData(pEdict);
    
    RETURN_META_VALUE(MRES_IGNORED, 0);
}

void* R_PvEntPrivateData(edict_t *pEdict) {

    //UTIL_ServerPrint("[DEBUG] ZZZZZZZZZZZZZZZZZZZZ: R_PvEntPrivateData");

    RETURN_META_VALUE(MRES_IGNORED, 0);
}

void* R_PvEntPrivateData_Post(edict_t *pEdict) {

    //UTIL_ServerPrint("[DEBUG] XXXXXXXXXXXXXXXXXXXX: R_PvEntPrivateData_Post");

    RETURN_META_VALUE(MRES_IGNORED, 0);
}

void Alloc_EntPrivateData(edict_t *pEdict) {

    if (FStringNull(pEdict->v.classname)) return;

    //UTIL_ServerPrint("[DEBUG] Alloc_EntPrivateData(): id = %d, classname = <%s>, owner = %d\n", ENTINDEX(pEdict), STRING(pEdict->v.classname), ENTINDEX(pEdict->v.owner));

    int entity_index = ENTINDEX(pEdict);

    std::string key = STRING(pEdict->v.classname);

    // ADD ENTITIES
    int result = acs_trie_add(&g_Tries.entities, key, entity_index);

    //UTIL_ServerPrint("[DEBUG] Alloc_EntPrivateData(): classname = <%s>, new_count = %d\n", key.c_str(), result);

    // ADD CLASSNAMES
    g_Tries.classnames[entity_index] = key;

    // ADD WP_ENTITIES
    if (key.find(WP_CLASS_PREFIX) == 0 && key.length() > WP_CLASS_PREFIX_LEN) {

        acs_vector_add(&g_Tries.wp_entities, entity_index);

        //UTIL_ServerPrint("[DEBUG] Alloc_EntPrivateData(): WEAPONS, new_count = %d\n", result);
    }
}

void Free_EntPrivateData(edict_t *pEdict) {

    int entity_index = ENTINDEX(pEdict);

    int owner_index = ENTINDEX(pEdict->v.owner);

    std::string key;

    // CHECK CREATION CLASSNAME
    if (pEdict == nullptr || pEdict->pvPrivateData == nullptr || FStringNull(pEdict->v.classname)) {

        // WAS REGISTERED?
        if (g_Tries.classnames.find(entity_index) != g_Tries.classnames.end()) {

            key = g_Tries.classnames[entity_index];

            //UTIL_ServerPrint("[DEBUG] Free_EntPrivateData(): found deleted entity = %d with creation_classname = <%s> << WARNING !!!\n", entity_index, key);

            // REMOVE FROM ENTITIES
            acs_vector_remove(&g_Tries.entities[key], entity_index);

            // REMOVE PLAYER_ENTITIES
            if (is_valid_index(owner_index))

                acs_vector_remove(&g_Tries.player_entities[owner_index], entity_index);
            
            // REMOVE WP_ENITITIES
            if (key.find(WP_CLASS_PREFIX) == 0 && key.length() > WP_CLASS_PREFIX_LEN)
                
                acs_vector_remove(&g_Tries.wp_entities, entity_index);

            // REMOVE CLASSNAME
            g_Tries.classnames.erase(entity_index);

            return;
        }
    }

    //UTIL_ServerPrint("[DEBUG] Free_EntPrivateData(): entity = %d, classname = <%s>, owner = %d\n", entity_index, STRING(pEdict->v.classname), owner_index);

    key = STRING(pEdict->v.classname);

    // CHECK ENTITY CREATION CLASS
    if (key != g_Tries.classnames[entity_index]) {

        //UTIL_ServerPrint("[DEBUG] Free_EntPrivateData(): entity = %d, classname = <%s> was changed from <%s> << WARNING !!!\n", entity_index, key.c_str(), g_Tries.classnames[entity_index].c_str());

        key = g_Tries.classnames[entity_index];
    }

    // REMOVE ENTITIES
    int result = acs_trie_remove(&g_Tries.entities, key, entity_index);

    //UTIL_ServerPrint("[DEBUG] Free_EntPrivateData(): remove entity = %d from classname = <%s>, new_count = %d\n", entity_index, key.c_str(), result);

    // REMOVE PLAYER_ENTITIES
    if (is_valid_index(owner_index))

        acs_vector_remove(&g_Tries.player_entities[owner_index], entity_index);

    // REMOVE CLASSNAME
    g_Tries.classnames.erase(entity_index);

    // REMOVE WP_ENTITIES
    if (key.find(WP_CLASS_PREFIX) == 0 && key.length() > WP_CLASS_PREFIX_LEN) {

        acs_vector_remove(&g_Tries.wp_entities, entity_index);

        //UTIL_ServerPrint("[DEBUG] Free_EntPrivateData(): WEAPONS, new_count = %d\n", result);
    }
}

void CBasePlayer_Spawn_RG(IReGameHook_CBasePlayer_Spawn *chain, CBasePlayer *pPlayer) {

    int id = pPlayer->entindex();

    // FIX TEAMS DEAD COUNT
    if (is_valid_index(id) && is_valid_team(g_Clients[id].team) && pPlayer->edict()->v.deadflag != DEAD_NO && g_PlayersNum[TEAM_DEAD_TT + g_Clients[id].team - 1] > 0)

        g_PlayersNum[TEAM_DEAD_TT + g_Clients[id].team - 1]--;

    chain->callNext(pPlayer);
}

CWeaponBox* CreateWeaponBox_RG(IReGameHook_CreateWeaponBox *chain, CBasePlayerItem *pItem, CBasePlayer *pPlayer, const char *model, Vector &v_origin, Vector &v_angels, Vector &v_velocity, float life_time, bool pack_ammo) {
    
    auto origin = chain->callNext(pItem, pPlayer, model, v_origin, v_angels, v_velocity, life_time, pack_ammo);

    int owner_index = pPlayer->entindex();

    int entity_index = pItem->entindex();

    // REMOVE PLAYER_ENTITIES
    if (is_valid_index(owner_index))

        acs_vector_remove(&g_Tries.player_entities[owner_index], entity_index);

    // FIX OWNER
    origin->edict()->v.owner = pPlayer->edict();

    return origin;
}

qboolean CSGameRules_CanHavePlayerItem_RG(IReGameHook_CSGameRules_CanHavePlayerItem *chain, CBasePlayer *pPlayer, CBasePlayerItem *pItem) {

    auto origin = chain->callNext(pPlayer, pItem);

    return origin;
}

CBaseEntity* CBasePlayer_GiveNamedItem_RG(IReGameHook_CBasePlayer_GiveNamedItem *chain, CBasePlayer *pPlayer, const char *classname) {

    auto origin = chain->callNext(pPlayer, classname);

    //UTIL_ServerPrint("[DEBUG] GiveNamedItem_RG(): id = %d, item = %d, classname = <%s>\n", pPlayer->entindex(), origin->entindex(), classname);

    return origin;
}

qboolean CBasePlayer_AddPlayerItem_RG(IReGameHook_CBasePlayer_AddPlayerItem *chain, CBasePlayer *pPlayer, class CBasePlayerItem *pItem) {

    auto origin = chain->callNext(pPlayer, pItem);

    if (origin) {

        std::vector<int> v;

        int id = pPlayer->entindex();

        int entity_index = pItem->entindex();

        int owner_index = ENTINDEX(pItem->pev->owner);

        acs_vector_add(&g_Tries.player_entities[id], entity_index);

        if ((is_valid_index(owner_index) || (owner_index > 0 && is_valid_index(owner_index = ENTINDEX(pItem->pev->owner->v.owner)))) && id != owner_index) {

            acs_vector_remove(&g_Tries.player_entities[owner_index], entity_index);

            //UTIL_ServerPrint("[DEBUG] AddPlayerItem_RG(): remove entity = %d from owner = %d\n", entity_index, owner_index);
        }

        // FIX OWNER
        pItem->pev->owner = pPlayer->edict();

        //UTIL_ServerPrint("[DEBUG] AddPlayerItem_RG(): id = %d, entity = %d, item_classname = <%s>, item_owner = %d\n", pPlayer->entindex(), pItem->entindex(), STRING(pItem->pev->classname), owner_index);
    }

    return origin;
}

void ED_Free_RH(IRehldsHook_ED_Free *chain, edict_t *pEdict) {

    //Free_EntPrivateData(pEdict, "ED_Free_RH");

    chain->callNext(pEdict);
}

edict_t* ED_Alloc_RH(IRehldsHook_ED_Alloc* chain) {

    auto origin = chain->callNext();

	return origin;
}

int R_Spawn(edict_t *pEntity) {

    //int id = ENTINDEX(pEntity);

    //UTIL_ServerPrint("[DEBUG] Spawn(): id = %d, owner = %d\n", ENTINDEX(pEntity), ENTINDEX(pEntity->v.owner));

    RETURN_META_VALUE(MRES_IGNORED, 0);
}

qboolean R_ClientConnect(edict_t *pEntity, const char *pszName, const char *pszAddress, char szRejectReason[ 128 ]) {

    int id = ENTINDEX(pEntity);

    //UTIL_ServerPrint("[DEBUG] R_ClientConnect(): id = %d, name = %s, host = %s\n", id, pszName, pszAddress);

	RETURN_META_VALUE(MRES_IGNORED, true);
}

void R_ClientPutInServer_Post(edict_t *pEntity) {

    //UTIL_ServerPrint("[DEBUG] ClientPutInServer_Post() ===>\n");

    CBasePlayer *pPlayer = UTIL_PlayerByIndexSafe(ENTINDEX(pEntity));

    if (pPlayer != nullptr && !pPlayer->IsBot())

        Client_PutInServer(pEntity, STRING(pEntity->v.netname), false);

    //Client_PutInServer(pEntity, STRING(pEntity->v.netname), false);

    RETURN_META(MRES_IGNORED);
}

void CSGameRules_CheckMapConditions_RG(IReGameHook_CSGameRules_CheckMapConditions *chain) {

    g_PlayersNum[TEAM_DEAD_TT] =
    
        g_PlayersNum[TEAM_DEAD_CT] = 0;

    chain->callNext();
}

void CBasePlayer_Killed_RG(IReGameHook_CBasePlayer_Killed *chain, CBasePlayer *pPlayer, entvars_t *pevAttacker, int iGib) {

    if (g_Clients[pPlayer->entindex()].is_connected && is_valid_team(pPlayer->m_iTeam))

        g_PlayersNum[TEAM_DEAD_TT + pPlayer->m_iTeam - 1]++;
    
    //UTIL_ServerPrint("[DEBUG] KILL: num_unassigned = %d, num_tt = %d, num_ct = %d, num_spec = %d, num_dead_tt = %d, num_dead_ct = %d\n",
    //    g_PlayersNum[TEAM_UNASSIGNED], g_PlayersNum[TEAM_TERRORIST], g_PlayersNum[TEAM_CT], g_PlayersNum[TEAM_SPECTRATOR], g_PlayersNum[TEAM_DEAD_TT], g_PlayersNum[TEAM_DEAD_CT]
    //);

    chain->callNext(pPlayer, pevAttacker, iGib);
}

edict_t* CreateFakeClient_RH(IRehldsHook_CreateFakeClient *chain, const char *netname) {

    edict_t *pEntity = chain->callNext(netname);

    //UTIL_ServerPrint("[DEBUG] CreateFakeClient(): id = %d, name = %s\n", ENTINDEX(pEntity), netname);

    Client_PutInServer(pEntity, netname, true);

    return pEntity;
}

void R_ClientDisconnect(edict_t *pEntity) {

    //UTIL_ServerPrint("[DEBUG] R_ClientDisconnect() ===>\n");

    Client_Disconnected(pEntity, false, 0);

	RETURN_META(MRES_IGNORED);
}

void SV_DropClient_RH(IRehldsHook_SV_DropClient *chain, IGameClient *cl, bool crash, const char *format) {
	
    char buffer[1024];

    //UTIL_ServerPrint("[DEBUG] SV_DropClient_RH() ===>\n");

	Q_strcpy_s(buffer, (char*)format);

    Client_Disconnected(cl->GetEdict(), crash, buffer);

    chain->callNext(cl, crash, format);
}

void Client_PutInServer(edict_t *pEntity, const char *netname, const bool is_bot) {

    int id = ENTINDEX(pEntity);

    if (is_valid_index(id)) {

        g_Clients[id].is_bot = is_bot;

        g_Clients[id].is_connected = true;

        g_Clients[id].team = TEAM_UNASSIGNED;

        g_PlayersNum[TEAM_UNASSIGNED]++;

        //UTIL_ServerPrint("[DEBUG] PutInServer_Post(): id = %d, name = %s, authid = %s, team = %d, is_connected = %d\n", id, netname, GETPLAYERAUTHID(pEntity), g_Clients[id].team, g_Clients[id].is_connected);

        //UTIL_ServerPrint("[DEBUG] num_unassigned = %d, num_tt = %d, num_ct = %d, num_spec = %d\n", g_PlayersNum[TEAM_UNASSIGNED], g_PlayersNum[TEAM_TERRORIST], g_PlayersNum[TEAM_CT], g_PlayersNum[TEAM_SPECTRATOR]);
    }
}

void Client_Disconnected(edict_t *pEdict, bool crash, char *format) {

    int id = ENTINDEX(pEdict);

    if (is_valid_index(id)) {
        
        //UTIL_ServerPrint("[DEBUG] Client_Disconnected(): id = %d, is_connected = %d\n", id, g_Clients[id].is_connected);

        if (g_Clients[id].is_connected) {

            g_Clients[id].is_connected =
                
                g_Clients[id].is_bot = false;

            //if (--g_PlayersNum[g_Clients[id].team] < 0)

                //UTIL_ServerPrint("[DEBUG] Client_Disconnected(): id = %d, team = %d, count = %d << WARNING !!!", id, g_Clients[id].team, g_PlayersNum[g_Clients[id].team]);

            //CBasePlayer *pPlayer = UTIL_PlayerByIndexSafe(id);

            //UTIL_ServerPrint("[DEBUG] Client_Disconnected(): id = %d, name = %s\n", id, STRING(pPlayer->edict()->v.netname));

            g_PlayersNum[g_Clients[id].team]--;
            
            // FIX TEAMS DEAD COUNT
            if (is_valid_entity(pEdict) && pEdict->v.deadflag != DEAD_NO && is_valid_team(g_Clients[id].team) && g_PlayersNum[TEAM_DEAD_TT + g_Clients[id].team - 1] > 0)

                g_PlayersNum[TEAM_DEAD_TT + g_Clients[id].team - 1]--;

            //UTIL_ServerPrint("[DEBUG] DISCONNECT: num_unassigned = %d, num_tt = %d, num_ct = %d, num_spec = %d, num_dead_tt = %d, num_dead_ct = %d\n",
            //    g_PlayersNum[TEAM_UNASSIGNED], g_PlayersNum[TEAM_TERRORIST], g_PlayersNum[TEAM_CT], g_PlayersNum[TEAM_SPECTRATOR], g_PlayersNum[TEAM_DEAD_TT], g_PlayersNum[TEAM_DEAD_CT]
            //);
        }
    }
}

void Client_TeamInfo(void* mValue) {

    static int id;
    char *msg;
    eRFS_TEAMS new_team;

    switch (mState++) {
    
        case 0:

			id = *(int*)mValue;
		
        	break;

        case 1:
		
        	if (!is_valid_index(id)) break;
		
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

            //UTIL_ServerPrint("[DEBUG] id = %d, old_team = %d, new_team = %d\n", id, g_Clients[id].team, new_team);

            if (!g_Clients[id].is_connected) {

                g_Clients[id].team = new_team;

            } else if (new_team != TEAM_UNASSIGNED && g_Clients[id].team != new_team) {

                edict_t* pEdict = INDEXENT(id);

                // FIX TEAMS DEAD COUNT
                if (is_valid_entity(pEdict) && pEdict->v.deadflag != DEAD_NO) {

                    if (is_valid_team(g_Clients[id].team) && g_PlayersNum[TEAM_DEAD_TT + g_Clients[id].team - 1] > 0)

                        g_PlayersNum[TEAM_DEAD_TT + g_Clients[id].team - 1]--;

                    if (is_valid_team(new_team) && g_PlayersNum[TEAM_DEAD_TT + new_team - 1] > 0)

                        g_PlayersNum[TEAM_DEAD_TT + new_team - 1]++;
                } 

                //UTIL_ServerPrint("[DEBUG] Team changed!!!\n");

                g_PlayersNum[g_Clients[id].team]--;

                g_PlayersNum[new_team]++;

                g_Clients[id].team = new_team;

                //UTIL_ServerPrint("[DEBUG] num_unassigned = %d, num_tt = %d, num_ct = %d, num_spec = %d, num_dead_tt = %d, num_dead_ct = %d\n",
                //    g_PlayersNum[TEAM_UNASSIGNED], g_PlayersNum[TEAM_TERRORIST], g_PlayersNum[TEAM_CT], g_PlayersNum[TEAM_SPECTRATOR], g_PlayersNum[TEAM_DEAD_TT], g_PlayersNum[TEAM_DEAD_CT]
                //);
            }

            // FIX TEAM
            edict_t* pEdict = INDEXENT(id);
            
            if (is_valid_entity(pEdict))
            
                pEdict->v.team = new_team;

            break;
    }
}

int	R_RegUserMsg_Post(const char *pszName, int iSize) {

	for (auto& msg : g_user_msg) {

		if (strcmp(msg.name, pszName) == 0) {

			int id = META_RESULT_ORIG_RET(int);

            //UTIL_ServerPrint("[DEBUG] RegUserMsg: id = %d, %s\n", id, pszName);

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

	RETURN_META(MRES_IGNORED);
}

void R_WriteByte_Post(int iValue) {

	if (function) (*function)((void *)&iValue);

	RETURN_META(MRES_IGNORED);
}

void R_WriteChar_Post(int iValue) {

	if (function) (*function)((void *)&iValue);

	RETURN_META(MRES_IGNORED);
}

void R_WriteShort_Post(int iValue) {

	if (function) (*function)((void *)&iValue);

	RETURN_META(MRES_IGNORED);
}

void R_WriteLong_Post(int iValue) {

	if (function) (*function)((void *)&iValue);

	RETURN_META(MRES_IGNORED);
}

void R_WriteAngle_Post(float flValue) {

	if (function) (*function)((void *)&flValue);

	RETURN_META(MRES_IGNORED);
}

void R_WriteCoord_Post(float flValue) {

	if (function) (*function)((void *)&flValue);

	RETURN_META(MRES_IGNORED);
}

void R_WriteString_Post(const char *sz) {

	if (function) (*function)((void *)sz);

	RETURN_META(MRES_IGNORED);
}

void R_WriteEntity_Post(int iValue) {

	if (function) (*function)((void *)&iValue);

	RETURN_META(MRES_IGNORED);
}

void R_MessageEnd_Post(void) {

	if (endfunction) (*endfunction)(NULL);

	RETURN_META(MRES_IGNORED);
}

int acs_trie_add(std::map<std::string, std::vector<int>>* trie, std::string key, int value) {

    std::vector<int> v;

    if (trie->find(key) != trie->end())

        v = (*trie)[key];
    
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

void acs_trie_transfer(std::map<std::string, std::vector<cell>>* trie, std::string key_from, std::string key_to, int value) {

    acs_trie_remove(trie, key_from, value);
    
    g_Tries.classnames[value] = key_to;

    acs_trie_add(trie, key_to, value);
}

int acs_vector_add(std::vector<cell> *v, int value) {

    v->push_back(value);

    return v->size();
}

int acs_vector_remove(std::vector<cell> *v, int value) {

    std::vector<cell>::iterator it_value;

    if ((it_value = std::find(v->begin(), v->end(), value)) != v->end())

        v->erase(it_value);

    return v->size();
}

float acs_roundfloat(float value, int precision) {

    //UTIL_ServerPrint("[DEBUG] acs_roundfloat(): value = %f, precision = %d", value, precision);

    float power = pow(10.0f, -precision);

    return floor(value * power + 0.5) / power;
}

bool acs_get_user_buyzone(const edict_t *pEdict) {

    bool result = false;

    //UTIL_ServerPrint("[DEBUG] get_user_buyzone(): id = %d, team = %d, classname = %s\n", ENTINDEX(pEdict), pEdict->v.team, STRING(pEdict->v.classname));

    if (is_valid_entity(pEdict) && is_valid_team(pEdict->v.team) && pEdict->v.deadflag == DEAD_NO) {

        if (r_bMapHasBuyZone) {

            //UTIL_ServerPrint("[DEBUG] get_user_buyzone(): zone_count = %d\n", g_Tries.entities["func_buyzone"].size());

            for (auto& buyzone : g_Tries.entities["func_buyzone"]) {

                edict_t* pBuyZone = INDEXENT(buyzone);

                //UTIL_ServerPrint("[DEBUG] get_user_buyzone(): entity = %d, team = %d\n", buyzone, pBuyZone->v.team);

                if (is_valid_entity(pBuyZone) && pEdict->v.team == pBuyZone->v.team && is_entity_intersects(pEdict, pBuyZone)) {

                    result = true;

                    break;
                }
            }
            
        } else {

            for (auto& spawn : g_Tries.entities[pEdict->v.team == TEAM_TERRORIST ? "info_player_deathmatch" : "info_player_start"]) {

                edict_t* pSpawn = INDEXENT(spawn);

                //UTIL_ServerPrint("[DEBUG] get_user_buyzone(): spawn = %d, classname = %s, kill = %d\n", spawn, STRING(pSpawn->v.classname), (pSpawn->v.flags & FL_KILLME));

                if (is_valid_entity(pSpawn) && (pSpawn->v.origin - pEdict->v.origin).Length() < 200.0f) {

                    result = true;

                    break;
                }
            }
        }
    }
    return result;
}

bool is_number(std::string &s) {
    trim(s);
    if (s.empty()) return false;
    char* l_decimal_point = localeconv()->decimal_point;
    auto it = s.begin();
    bool need_replace = DECIMAL_POINT != *l_decimal_point;
    if (*it == '+' || *it == '-') it++;
    while (it != s.end()) {
        if (!std::isdigit(*it)) {
            if (*it == DECIMAL_POINT && need_replace) {
                s.replace(it, it + 1, l_decimal_point);
                continue;
            }
            if (*it != *l_decimal_point) break;
        }
        it++;
    }
    return it == s.end();
}
