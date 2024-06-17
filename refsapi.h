#pragma once
// gets rid of annoying "deprecated conversion from string constant blah blah" warning
#pragma GCC diagnostic ignored "-Wwrite-strings"

#include "precompiled.h"
#include <map>

#define MAX_PLAYERS                 32
#define _QQ                         "\"'`"
#define DECIMAL_POINT               '.'
#define _LOCALE                     std::locale("ru_RU.UTF-8")
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

typedef std::codecvt_utf8<wchar_t> convert_type;

extern bool r_bMapHasBuyZone;
extern char g_fmt_buff[1024];
extern wchar_t g_wfmt_buff[1024];
extern sClients g_Clients[MAX_PLAYERS + 1];
extern sTries g_Tries;
extern cell g_PlayersNum[6];
extern int mState;
extern int gmsgTeamInfo;
extern std::wstring_convert<convert_type, wchar_t> g_converter;
extern funEventCall modMsgsEnd[MAX_REG_MSGS];
extern funEventCall modMsgs[MAX_REG_MSGS];
extern void (*function)(void*);
extern void (*endfunction)(void*);

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
bool is_number(std::string &s);
//float rstof(std::string s, bool has_min = false, float min_val = 0.0f, bool has_max = false, float max_val = 0.0f);
//char* fmt(char *fmt, ...);
//wchar_t * wfmt(wchar_t *fmt, ...);

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

class ws_conv {
    std::wstring* result = new std::wstring;
    public:
        ws_conv(const std::string &s) {
            *result = g_converter.from_bytes(s);
            UTIL_ServerPrint("[DEBUG] ws_conv(): done\n");
            /*
            try {
                *result = g_converter.from_bytes(s);
                UTIL_ServerPrint("[DEBUG] ws_conv(): done\n");
            } catch(std::range_error &e) {
                result->clear();
                size_t length = s.length();
                UTIL_ServerPrint("[DEBUG] ws_conv(): catch !!! length = %d\n", length);
                result->reserve(length);
                for(size_t i = 0; i < length; i++)
                    result->push_back(s[i] & 0xFF);
            }
            */
        }
        std::wstring get() {
            return *result;
        }
        ~ws_conv() {
            delete result;
        }
};



inline float stof(std::string s, bool has_min = false, float min_val = 0.0f, bool has_max = false, float max_val = 0.0f) {
    float result = std::stof(s); //is_number(s) ? std::stof(s) : 0.0f;
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

inline void rtrim_zero(std::string &s) {
    
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
    
        return ch != '0' && ch != '.';
    
    }).base(), s.end());
}

inline std::string rtrim_zero_c(std::string s) {

    rtrim_zero(s);

    return s;
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
            wstoc* p_name = new wstoc(name);
            wstoc* p_value = new wstoc(value);
            cvar_t* p_cvar = CVAR_GET_POINTER(p_name->c_str());
            if (p_cvar == nullptr) {
                UTIL_ServerPrint("[DEBUG] create_cvar(): name = %s, value = %s\n", p_name->c_str(), p_value->c_str());
                cvar_t cvar;
                cvar.name = wstoc(name).c_str();
                cvar.flags = flags;
                cvar.string = wstoc(name).c_str();
                cvar.value = 0.0f;
                cvar.next = nullptr;
                UTIL_ServerPrint("[DEBUG] create_cvar(): before create\n");
                CVAR_REGISTER(&cvar);
                UTIL_ServerPrint("[DEBUG] create_cvar(): after create\n");
                p_cvar = CVAR_GET_POINTER(p_name->c_str());
                UTIL_ServerPrint("[DEBUG] rf_config(): p_cvar = %d, name = <%s>, value = <%s>\n", p_cvar, p_name->c_str(), p_value->c_str());
            }
            delete p_name;
            delete p_value;
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
                std::string num = std::to_string(stof(s, has_min, min_val, has_max, max_val));
                rtrim_zero(num);
                UTIL_ServerPrint("[DEBUG] cvar_mngr::add(): num = %s\n", num.c_str());
                value = ws_conv(num).get();
                //std::wstring test = ws_conv(num).get();
                UTIL_ServerPrint("[DEBUG] cvar_mngr::add(): new_value = %s\n", wstoc(value).c_str());
            }
            // PLUGIN EXIST?
            if ((plugin_it = cvars.plugin.find(plugin->getId())) != cvars.plugin.end()) {
                UTIL_ServerPrint("[DEBUG] cvar_mngr::add(): plugin_it = %d\n", plugin_it);
                p_cvar_list = plugin_it->second;
                // CVAR EXIST?
                if ((cvar_it = p_cvar_list.find(name)) != p_cvar_list.end())
                    return {cvar_it, true};
            }
            // CREATE CVAR
            m_cvar_t m_cvar;
            UTIL_ServerPrint("[DEBUG] cvar_mngr::add(): before create_var()\n");
            if ((m_cvar.cvar = create_cvar(name, value, flags)) != nullptr) {
                m_cvar.value = value;
                m_cvar.desc = desc;
                m_cvar.has_min = has_min;
                m_cvar.min_val = min_val;                
                m_cvar.has_max = has_max;                
                m_cvar.max_val = max_val;
                auto result = p_cvar_list.insert({name, m_cvar});
                UTIL_ServerPrint("[DEBUG] cvar_mngr::add(): result = %d, name = <%s>, value = <%s>, desc = <%s>\n", plugin_it, wstoc(name).c_str(), wstoc(value).c_str(), wstoc(desc).c_str());
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

extern cvar_mngr g_cvar_mngr;