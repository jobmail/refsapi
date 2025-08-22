#pragma once

#include "precompiled.h"
#ifdef DEBUG
    #define DEBUG(...)  debug(__VA_ARGS__)    
#else
    #ifdef  NDEBUG
        #define DEBUG(...) ((void) 0)
    #else
    #define DEBUG(...)   debug(__VA_ARGS__)
    #endif
#endif

// gets rid of annoying "deprecated conversion from string constant blah blah" warning
#pragma GCC diagnostic ignored "-Wwrite-strings"

#define MAX_PLAYERS                 32
#define MAX_LOG_BUFFER_SIZE         8192
#define MAX_LOG_STR_SIZE            16384
#define MAX_LOG_WORKERS             4
#define MAX_LOG_MIN_INTERVAL        100U
#define MAX_LOG_MAX_INTERVAL        15000U
#define DECIMAL_POINT               '.'
#define WP_CLASS_PREFIX             "weapon_"
#define WP_CLASS_PREFIX_LEN (sizeof(WP_CLASS_PREFIX) - 1)
#define REFSAPI_CVAR                "acs_refsapi_loaded"
#define _QQ                         "\"'`"
#define _QQ_L                       L"\"'`"
#define _TRIM_CHARS                 " \r\t\n\b\a"
#define _TRIM_CHARS_L               L" \r\t\n\b\a"
#define _BAD_PATH_CHARS             " \\,?`'\"~!@#$%^&*(){}[]-=\r\t\n\b\a"
#define _BAD_PATH_CHARS_L           L" \\,?`'\"~!@#$%^&*(){}[]-=\r\t\n\b\a"
#define _COUNT(x) (size_t)(sizeof(x) / sizeof(cell))

#define amx_ftoc(f) (*((cell *)&f))  /* float to cell */
#define amx_ctof(c) (*((float *)&c)) /* cell to float */

#define gettid()                    std::this_thread::get_id()

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
typedef std::unordered_set<std::wstring> ngram_t;
typedef std::unordered_map<std::wstring, ngram_t> ngrams_t;

extern bool r_bMapHasBuyZone;
extern char g_buff[4096];
extern bool g_SSE4_ENABLE;

extern std::unordered_map<std::wstring, std::unordered_set<std::wstring>> g_cache_ngrams;
extern sClients g_Clients[MAX_PLAYERS + 1];
extern sTries g_Tries;
extern cell g_PlayersNum[6];
extern int mState;
extern int gmsgTeamInfo;
extern std::wstring_convert<convert_type, wchar_t> g_converter;
extern const std::locale _LOCALE;
extern std::mutex std_mutex;

extern funEventCall modMsgsEnd[MAX_REG_MSGS];
extern funEventCall modMsgs[MAX_REG_MSGS];

extern void (*function)(void *);
extern void (*endfunction)(void *);

void debug(const char *fmt, ...);
size_t check_nick_imitation(const size_t id, const float threshold = 0.9f, const float lcs_threshold = 0.7f, const float k_lev = 0.6f, const float k_tan = 0.3f, const float k_lcs = 0.1f);

void R_ChangeLevel(const char* s1, const char* s2);
//bool R_ValidateCommand(IRehldsHook_ValidateCommand *chain, const char* cmd, cmd_source_t src, IGameClient *client);
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
//void CSGameRules_ChangeLevel_RG(IReGameHook_CSGameRules_ChangeLevel *chain);

void R_ExecuteServerStringCmd(IRehldsHook_ExecuteServerStringCmd *chain, const char *cmd, cmd_source_t src, IGameClient *id);
qboolean RF_CheckUserInfo_RH(IRehldsHook_SV_CheckUserInfo *chain, netadr_t *adr, char *userinfo, qboolean bIsReconnecting, int iReconnectSlot, char *name);
void CSGameRules_ClientUserInfoChanged_RG(IReGameHook_CSGameRules_ClientUserInfoChanged *chain, CBasePlayer *pPlayer, char *userinfo);
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

static const std::unordered_map<char, std::wstring> SIMILAR_CHARS_MAP = {
    {'a', L"4@аȺǞ₳Ꭿ凡Ꮨ∀ǺǻαάΆẫẮắẰằẳẴẵÄªäÅÀÁÂåãâàáÃᗩᵰаαⱥａ𝐚𝑎𝒂𝒶𝓪𝔞𝕒𝖆𝖺𝗮𝘢𝙖𝚊𝛂𝛼𝜶𝝰𝞪аᗄ"},
    {'b', L"6бℬᏰβ฿ßЂᗷᗸᗹᗽᗾᗿƁƀხ方ҔҕϬϭচঢ়ƃɓѢѣ৮ӸӹᏰｂ𝐛𝑏𝒃𝒷𝓫𝔟𝕓𝖇𝖻𝗯𝘣𝙗𝚋ᖯ"},
    {'c', L"сℭℂÇÇČċĊĉςĈćĆčḈḉ⊂Ꮸ₡Կկ੫ႡӴӵҸҹᴄｃ𝐜𝑐𝒄𝒸𝓬𝔠𝕔𝖈𝖼𝗰𝘤𝙘𝚌ⅽ"},
    {'d', L"дᗫƊĎďĐđð∂₫ȡℊ∂ԁｄ𝐝𝑑𝒅𝒹𝓭𝔡𝕕𝖉𝖽𝗱𝘥𝙙𝚍ⅾ"},
    {'e', L"3еёℯໂ६Ē℮ēĖėĘěĚęΈêξÊÈÉ∑ẾỀỂỄéèعЄєέεℰℯໂ६Ē℮ēĖėĘěĚęΈêÊÈÉẾỀỂỄéèعЄєέεҾҿｅ𝐞𝑒𝒆𝓮𝔢𝕖𝖊𝖾𝗲𝘦𝙚𝚎𝛆𝜀𝜺𝝴𝞊её"},
    {'f', L"фℱ₣ƒ∮ḞḟჶᶂφՓփႴቁቂቃቄቅቆቇቈᛄꬵｆ𝐟𝑓𝒇𝒻𝓯𝔣𝕗𝖋𝖿𝗳𝘧𝙛𝚏𝟋"},
    {'g', L"9гᏩᎶℊǤǥĜĝĞğĠġĢģפᶃ₲୮┍ℾɡｇ𝐠𝑔𝒈𝓰𝔤𝕘𝖌𝗀𝗴𝘨𝙜𝚐ℊ"},
    {'h', L"#хнℍℋℎℌℏዙᏥĤĦħΉ♅廾ЋђḨҺḩ♄ｈ𝐡𝒉𝒽𝓱𝔥𝕙𝖍𝗁𝗵𝘩𝙝𝚑һհ"},
    {'i', L"1!ийℐℑίιÏΊÎìÌíÍîϊΐĨĩĪīĬĭİįĮᎥนựӤӥŨũŪūŬŭÙúÚùҊҋｉ𝐢𝑖𝒊𝒾𝓲𝔦𝕚𝖎𝗂𝗶𝘪𝙞𝚤𝚒і"},
    {'j', L"жᛤ♅ҖҗӜӝӁӂϳｊ𝐣𝑗𝒋𝒿𝓳𝔧𝕛𝖏𝗃𝗷𝘫𝙟𝚓ј"},
    {'k', L"к₭ᏦЌќķĶҜҝﻸᶄᛕ₭ᏦЌќķĶҜҝᶄҠҡｋ𝐤𝑘𝒌𝓀𝔨𝕜𝖐𝗄𝗸𝘬𝙠𝚔κ"},
    {'l', L"|лℒℓĿŀĹĺĻļλŁłľĽḼḽȴᏝለሉሊሌልሎᏗｌ𝐥𝑙𝒍𝓁𝔩𝕝𝖑𝗅𝗹𝘭𝙡𝚕ⅼ"},
    {'m', L"мℳʍᶆḾḿᗰᙢ爪ጠᛖℳʍᶆḾｍ𝐦𝑚𝒎𝓂𝔪𝕞𝖒𝗆𝗺𝘮𝙢𝚖ⅿ"},
    {'n', L"нℕηñחÑήŋŊŃńŅņŇňŉȵℵਮዘዙዚዛዜዝዞዟℍℋℌℏዙᏥĤĦΉḨӇӈｎ𝐧𝑛𝒏𝓃𝔫𝕟𝖓𝗇𝗻𝘯𝙣𝚗ո"},
    {'o', L"0оℴტ٥ΌóόσǿǾΘòÓÒÔôÖöÕõờớọỌợỢøØΌỞỜỚỔổỢŌō০ℴტ٥ΌóόσǿǾΘòÓÒÔôÖöÕõờớọỌợỢøØΌỞỜỚỔổỢŌōŐőӪӫｏ𝐨𝑜𝒐𝓸𝔬𝕠𝖔𝗈𝗼𝘰𝙤𝚘𝛐𝜊𝝄𝝾𝞸σ"},
    {'p', L"пℙ℘þÞρᎮᎵ尸Ҏҏᶈ₱☧ᖘקァՈগກ⋒Ҧҧｐ𝐩𝑝𝒑𝓅𝔭𝕡𝖕𝗉𝗽𝘱𝙥𝚙ρ"},
    {'q', L"цԱųզｑ𝐪𝑞𝒒𝓆𝔮𝕢𝖖𝗊𝗾𝘲𝙦𝚚գ"},
    {'r', L"гℝℜℛ℟ჩᖇřŘŗŖŕŔᶉᏒ尺թℙ℘ρᎮᎵ尸Ҏҏᶈ₱☧ᖘק₽ǷҎҏｒ𝐫𝑟𝒓𝓇𝔯𝕣𝖗𝗋𝗿𝘳𝙧𝚛г"},
    {'s', L"$сႺℭℂÇÇČċĊĉςĈćĆčḈḉ⊂Ꮸ₡Կկ੫ႡӴӵҸҹｓ𝐬𝑠𝒔𝓈𝔰𝕤𝖘𝗌𝘀𝘴𝙨𝚜ѕ"},
    {'t', L"7т₸†τΐŢţŤťŧŦṪṫナᎿᏆテ₮⍑⍡τŢŤŦṪ₮ｔ𝐭𝑡𝒕𝓉𝕥𝖙𝗍𝘁𝘵𝙩𝚝τ"},
    {'u', L"µию∪ᙀŨ⋒ỦỪỬỮỰύϋÙúÚΰùÛûÜửữựЏüừŨũŪūŬŭųŲűŰůŮɣᎩᎽẎẏϒɤ￥௶Ⴘｕ𝐮𝑢𝒖𝓊𝔲𝕦𝖚𝗎𝘂𝘶𝙪𝚞ս"},
    {'v', L"в∨√ᏉṼṽᶌ℣ʋѵｖ𝐯𝑣𝒗𝓋𝔳𝕧𝖛𝗏𝘃𝘷𝙫𝚟ⅴ"},
    {'w', L"шщ₩ẃẂẁẀẅώωŵŴᏔᎳฬᗯᙡẄѡಎಭᏊᏇผฝพฟᗯᙡωպખｗ𝐰𝑤𝒘𝓌𝔴𝕨𝖜𝗐𝘄𝘸𝙬𝚠ա"},
    {'x', L"+хχ×᙭ჯẌẍᶍᕁｘ𝐱𝑥𝒙𝓍𝔵𝕩𝖝𝗑𝘅𝘹𝙭𝚡х"},
    {'y', L"уɣᎩᎽẎẏϒɤ￥りਠყｙ𝐲𝑦𝒚𝓎𝔶𝕪𝖞𝗒𝘆𝘺𝙮𝚢у"},
    {'z', L"2зℤℨჳ乙ẐẑɀᏃՅℨჳ∋∌∍ヨӬӭ℈ｚ𝐳𝑧𝒛𝓏𝔃𝕫𝖟𝗓𝘇𝘻𝙯𝚣ᴢ"}
};

static const std::unordered_map<char32_t, std::wstring> RU_TO_LAT_MAP = {
    {U'а', L"a"}, {U'б', L"b"}, {U'в', L"v"}, {U'г', L"g"},
    {U'д', L"d"}, {U'е', L"e"}, {U'ё', L"yo"}, {U'ж', L"zh"},
    {U'з', L"z"}, {U'и', L"i"}, {U'й', L"j"}, {U'к', L"k"},
    {U'л', L"l"}, {U'м', L"m"}, {U'н', L"n"}, {U'о', L"o"},
    {U'п', L"p"}, {U'р', L"r"}, {U'с', L"s"}, {U'т', L"t"},
    {U'у', L"L"}, {U'ф', L"f"}, {U'х', L"h"}, {U'ч', L"cz"},
    {U'ш', L"sh"}, {U'щ', L"shc"}, {U'ъ', L"^"}, {U'ы', L"y"},
    {U'ь', L"'"}, {U'э', L"e"}, {U'ю', L"yL"}, {U'я', L"ya"}
};
