#include "precompiled.h"

int gmsgTeamInfo;
int m_state;
int g_PlayersNum[6];
bool r_bMapHasBuyZone;
char g_buff[4096];
bool g_SSE4_ENABLE;

ngrams_t g_cache_ngrams;
const std::locale _LOCALE = std::locale("ru_RU.UTF-8");
cvar_mngr g_cvar_mngr;
#ifndef WITHOUT_SQL
mysql_mngr g_mysql_mngr;
#endif
#ifndef WITHOUT_LOG
log_mngr g_log_mngr;
#endif
#ifndef WITHOUT_TIMER
timer_mngr g_timer_mngr;
#endif
recoil_mngr g_recoil_mngr;
sClients g_Clients[MAX_PLAYERS + 1];
sTries g_Tries;
std::wstring_convert<convert_type, wchar_t> g_converter;
std::mutex std_mutex;
funEventCall modMsgsEnd[MAX_REG_MSGS];
funEventCall modMsgs[MAX_REG_MSGS];
void (*function)(void *);
void (*endfunction)(void *);

g_RegUserMsg g_user_msg[] =
    {
        {"TeamInfo", &gmsgTeamInfo, Client_TeamInfo, false},
};

void CSGameRules_ClientUserInfoChanged_RG(IReGameHook_CSGameRules_ClientUserInfoChanged *chain, CBasePlayer *pPlayer, char *userinfo)
{
    DEBUG("*** change userinfo = %s", userinfo);
    // UTIL_ServerPrint("[DEBUG] *** userinfo = %s\n", userinfo);
#ifdef __SSE4_2__
    if ((g_SSE4_ENABLE ? is_valid_utf8_simd(userinfo) : is_valid_utf8(userinfo)))
#else
    if (is_valid_utf8(userinfo))
#endif
    
        chain->callNext(pPlayer, userinfo);
    else if (pPlayer != nullptr)
    {
        auto cl = clientByIndex(pPlayer->entindex());
        if (cl->IsConnected())
            g_RehldsFuncs->DropClient(cl, false, "[REFSAPI] Invalid userinfo");
    }
}

size_t check_nick_imitation(const size_t id, const float threshold, const float lcs_threshold, const float k_lev, const float k_tan, const float k_lcs)
{
    if (is_valid_index(id))
    {
        auto cl = clientByIndex(id);
        std::wstring name = stows(cl->GetName());
        for (size_t i = gpGlobals->maxClients; i > 0; i--)
        {
            if (i == id)
                continue;
            cl = clientByIndex(i);
            if (!cl->IsConnected())
                continue;
            if ((float)similarity_score(name, stows(cl->GetName())) >= threshold)
                return i;
        }
    }
    return 0;
}

qboolean RF_CheckUserInfo_RH(IRehldsHook_SV_CheckUserInfo *chain, netadr_t *adr, char *userinfo, qboolean bIsReconnecting, int iReconnectSlot, char *name)
{
    DEBUG("*** check userinfo = %s", userinfo);
    // UTIL_ServerPrint("\n[DEBUG] **** check userinfo = %s, name = %s\n\n", userinfo, name);
#ifdef __SSE4_2__
    return !(g_SSE4_ENABLE ? is_valid_utf8_simd(userinfo) : is_valid_utf8(userinfo)) ? FALSE : chain->callNext(adr, userinfo, bIsReconnecting, iReconnectSlot, name);
#else
    return !is_valid_utf8(userinfo) ? FALSE : chain->callNext(adr, userinfo, bIsReconnecting, iReconnectSlot, name);
#endif
}

/*
bool R_ValidateCommand(IRehldsHook_ValidateCommand *chain, const char* cmd, cmd_source_t src, IGameClient *client)
{
    std::string str = cmd;
    if (str.find("changelevel") != std::string::npos)
    {
        UTIL_ServerPrint("CMD = %s\n", cmd);
        assert(false);
    }
    return chain->callNext(cmd, src, client);
}
*/

void R_ChangeLevel(const char* s1, const char* s2)
{
    SERVER_PRINT("[DEBUG] CHANGELEVEL\n");
    DEBUG("%s(): s1 = %s, s2 = %s", __func__, s1, s2);
    /*
    #ifndef WITHOUT_SQL
        while (g_mysql_mngr.block_changelevel.load())
            std::this_thread::sleep_for(std::chrono::milliseconds(QUERY_POOLING_INTERVAL));
    #endif
    */
}

void R_ExecuteServerStringCmd(IRehldsHook_ExecuteServerStringCmd *chain, const char *cmd, cmd_source_t src, IGameClient *client)
{
    if (src == src_client && !strcmp(cmd, "status"))
    {
        char flags_str[32];
        auto ip = client->GetNetChan()->GetRemoteAdr()->ip;
        UTIL_GetFlags(flags_str, g_amxxapi.GetPlayerFlags(ENTINDEX(client->GetEdict())));
        int len = snprintf(g_buff, sizeof(g_buff), "[ACS] Имя: %s\n[ACS] Стим: %s\n[ACS] IP: %d.%d.%d.%d\n[ACS] Флаги: %s\n", client->GetName(), GETPLAYERAUTHID(client->GetEdict()), ip[0], ip[1], ip[2], ip[3], flags_str);
        // UTIL_ServerPrint("[DEBUG] R_ExecuteServerStringCmd(): id = %d, cmd = %s\n", client->GetId(), cmd);
        if (len >= 0)
            g_engfuncs.pfnClientPrintf(client->GetEdict(), print_console, g_buff);
        return;
    }
    else if (src == src_command && !strcmp(cmd, "cpu"))
    {
        cpu_test();
        return;
    }
    else if (src == src_command && !strcmp(cmd, "changelevel"))
    {
        SERVER_PRINT("[DEBUG] CHANGELEVEL\n");
        /*
#ifndef WITHOUT_SQL
        while (g_mysql_mngr.block_changelevel.load())
            std::this_thread::sleep_for(std::chrono::milliseconds(QUERY_POOLING_INTERVAL));
#endif
        */
    }
    else if (src == src_command && !strcmp(cmd, "stats"))
    {
#ifndef WITHOUT_SQL
        if (snprintf(g_buff, sizeof(g_buff), "\n[REFSAPI] FPS = %.1f, frame_delay = %.3f, frame_rate = %d (%d), threads = %d/%d(%d), query_nums = %lld\n",
            g_mysql_mngr.frame_delay > 0.0 ? 1000.0 / g_mysql_mngr.frame_delay : 0.0, g_mysql_mngr.frame_delay, g_mysql_mngr.frame_rate, g_mysql_mngr.frame_rate_max,
            g_mysql_mngr.num_threads.load(), g_mysql_mngr.num_finished.load(), g_mysql_mngr.m_threads.size(), g_mysql_mngr.m_query_nums.load()) >= 0)
            SERVER_PRINT(g_buff);
#endif
#ifndef WITHOUT_TIMER
        if (snprintf(g_buff, sizeof(g_buff), "[REFSAPI] std_delay = %.6f, frame_rate = %d (%d), timer_nums = %lld\n",
            g_timer_mngr.total_std > 0.0 ? g_timer_mngr.total_std / g_timer_mngr.m_timer_nums : 0.0, g_timer_mngr.frame_rate, g_timer_mngr.frame_rate_max, g_timer_mngr.m_timer_nums.load()) >= 0)
            SERVER_PRINT(g_buff);
#endif
    }
    chain->callNext(cmd, src, client);
}

void R_StartFrame_Post(void)
{

#ifndef WITHOUT_SQL
    g_mysql_mngr.start_frame();
#endif

#ifndef WITHOUT_TIMER
	g_timer_mngr.start_frame();
#endif

    RETURN_META(MRES_IGNORED);
}

void *R_PvAllocEntPrivateData(edict_t *p, int32 cb)
{
    Alloc_EntPrivateData(p);
    RETURN_META_VALUE(MRES_IGNORED, 0);
}

void Alloc_EntPrivateData(edict_t *p)
{
    if (FStringNull(p->v.classname))
        return;
    // UTIL_ServerPrint"[DEBUG] Alloc_EntPrivateData(): id = %d, classname = <%s>, owner = %d\n", ENTINDEX(p), STRING(p->v.classname), ENTINDEX(p->v.owner));
    auto index = ENTINDEX(p);
    std::string key = STRING(p->v.classname);
    trie_add(g_Tries.entities, key, index);
    // UTIL_ServerPrint"[DEBUG] Alloc_EntPrivateData(): classname = <%s>, new_count = %d\n", key.c_str(), result);
    g_Tries.classnames[index] = key;
    if (key.find(WP_CLASS_PREFIX) == 0 && key.length() > WP_CLASS_PREFIX_LEN)
    {
        vector_add(g_Tries.wp_entities, index);
        // UTIL_ServerPrint"[DEBUG] Alloc_EntPrivateData(): WEAPONS, new_count = %d\n", result);
    }
}

void Free_EntPrivateData(edict_t *p)
{
    auto index = ENTINDEX(p);
    auto owner = ENTINDEX(p->v.owner);
    // UTIL_ServerPrint"[DEBUG] Free_EntPrivateData(): p = %p, privdata = %p, free = %d, kill = %d, entity = %d, classname = <%s>, owner = %d\n", p, p->pvPrivateData, p->free, p->v.flags & FL_KILLME, index, STRING(p->v.classname), owner);
    auto key_old = g_Tries.classnames[index];
    auto key = FStringNull(p->v.classname) ? key_old : STRING(p->v.classname);
    if (key_old != key)
        trie_transfer(g_Tries.entities, key_old, key, index);
    trie_remove(g_Tries.entities, key, index);
    if (key.find(WP_CLASS_PREFIX) == 0 && key.length() > WP_CLASS_PREFIX_LEN)
        vector_remove(g_Tries.wp_entities, index);
    if (is_valid_index(owner))
        vector_remove(g_Tries.player_entities[owner], index);
    g_Tries.classnames.erase(index);
}

void CBasePlayer_Spawn_RG(IReGameHook_CBasePlayer_Spawn *chain, CBasePlayer *pPlayer)
{
    auto id = pPlayer->entindex();
    // FIX TEAMS DEAD COUNT
    if (is_valid_index(id) && is_valid_team(g_Clients[id].team) && pPlayer->edict()->v.deadflag != DEAD_NO && g_PlayersNum[TEAM_DEAD_TT + g_Clients[id].team - 1] > 0)
        g_PlayersNum[TEAM_DEAD_TT + g_Clients[id].team - 1]--;
    chain->callNext(pPlayer);
}

/*
void CSGameRules_ChangeLevel_RG(IReGameHook_CSGameRules_ChangeLevel *chain)
{
#ifndef WITHOUT_SQL
    //SERVER_PRINT("[REFSAPI_SQL] *** CHANGELEVEL ***\n");
    while (g_mysql_mngr.block_changelevel.load())
        std::this_thread::sleep_for(std::chrono::milliseconds(QUERY_POOLING_INTERVAL));
#endif
    chain->callNext();
}
*/

qboolean CBasePlayer_RemovePlayerItem_RG(IReGameHook_CBasePlayer_RemovePlayerItem *chain, CBasePlayer *pPlayer, CBasePlayerItem *pItem)
{
    auto origin = chain->callNext(pPlayer, pItem);
    if (origin)
    {
        auto index = pItem->entindex();
        auto owner = ENTINDEX(pItem->pev->owner);
        if (is_valid_index(owner))
        {
            vector_remove(g_Tries.player_entities[owner], index);
            // UTIL_ServerPrint"[DEBUG] CBasePlayer_RemovePlayerItem_RG(): remove entity = %d from owner = %d\n", index, owner);
        }
    }
    return origin;
}

qboolean CBasePlayer_AddPlayerItem_RG(IReGameHook_CBasePlayer_AddPlayerItem *chain, CBasePlayer *pPlayer, class CBasePlayerItem *pItem)
{
    auto origin = chain->callNext(pPlayer, pItem);
    if (origin)
    {
        auto id = pPlayer->entindex();
        auto index = pItem->entindex();
        // int owner = ENTINDEX(pItem->pev->owner);
        vector_add(g_Tries.player_entities[id], index);
        // UTIL_ServerPrint"[DEBUG] AddPlayerItem_RG(): id = %d, entity = %d prev_owner = %d\n", id, index, owner);
        //  FIX OWNER
        pItem->pev->owner = pPlayer->edict();
        // UTIL_ServerPrint("[DEBUG] AddPlayerItem_RG(): id = %d, entity = %d, item_classname = <%s>, item_owner = %d\n", pPlayer->entindex(), pItem->entindex(), STRING(pItem->pev->classname), owner);
    }
    return origin;
}

void R_ClientPutInServer_Post(edict_t *p)
{
    // UTIL_ServerPrint("[DEBUG] ClientPutInServer_Post() ===>\n");
    auto *pPlayer = UTIL_PlayerByIndexSafe(ENTINDEX(p));
    if (pPlayer != nullptr && !pPlayer->IsBot())
        Client_PutInServer(p, STRING(p->v.netname), false);
    // Client_PutInServer(p, STRING(p->v.netname), false);
    RETURN_META(MRES_IGNORED);
}

void CSGameRules_CheckMapConditions_RG(IReGameHook_CSGameRules_CheckMapConditions *chain)
{
    g_PlayersNum[TEAM_DEAD_TT] = 0;
    g_PlayersNum[TEAM_DEAD_CT] = 0;
    chain->callNext();
}

void CBasePlayer_Killed_RG(IReGameHook_CBasePlayer_Killed *chain, CBasePlayer *pPlayer, entvars_t *pevAttacker, int iGib)
{
    if (g_Clients[pPlayer->entindex()].is_connected && is_valid_team(pPlayer->m_iTeam))
        g_PlayersNum[TEAM_DEAD_TT + pPlayer->m_iTeam - 1]++;
    // UTIL_ServerPrint("[DEBUG] KILL: num_unassigned = %d, num_tt = %d, num_ct = %d, num_spec = %d, num_dead_tt = %d, num_dead_ct = %d\n",
    //     g_PlayersNum[TEAM_UNASSIGNED], g_PlayersNum[TEAM_TERRORIST], g_PlayersNum[TEAM_CT], g_PlayersNum[TEAM_SPECTRATOR], g_PlayersNum[TEAM_DEAD_TT], g_PlayersNum[TEAM_DEAD_CT]
    //);
    chain->callNext(pPlayer, pevAttacker, iGib);
}

edict_t *CreateFakeClient_RH(IRehldsHook_CreateFakeClient *chain, const char *netname)
{
    auto p = chain->callNext(netname);
    // UTIL_ServerPrint("[DEBUG] CreateFakeClient(): id = %d, name = %s\n", ENTINDEX(p), netname);
    Client_PutInServer(p, netname, true);
    return p;
}

void R_ClientDisconnect(edict_t *p)
{
    // UTIL_ServerPrint("[DEBUG] R_ClientDisconnect() ===>\n");
    Client_Disconnected(p, false, 0);
#ifndef WITHOUT_TIMER
    g_timer_mngr.on_client_disconnected(p);
#endif
    RETURN_META(MRES_IGNORED);
}

void SV_DropClient_RH(IRehldsHook_SV_DropClient *chain, IGameClient *cl, bool crash, const char *format)
{

    char buffer[1024];
    // UTIL_ServerPrint("[DEBUG] SV_DropClient_RH() ===>\n");
    Q_strcpy_s(buffer, (char *)format);
    Client_Disconnected(cl->GetEdict(), crash, buffer);
    chain->callNext(cl, crash, format);
}

void Client_PutInServer(edict_t *p, const char *netname, const bool is_bot)
{
    auto id = ENTINDEX(p);
    if (is_valid_index(id))
    {
        g_Clients[id].is_bot = is_bot;
        g_Clients[id].is_connected = true;
        g_Clients[id].team = TEAM_UNASSIGNED;
        g_PlayersNum[TEAM_UNASSIGNED]++;
        // UTIL_ServerPrint("[DEBUG] PutInServer_Post(): id = %d, name = %s, authid = %s, team = %d, is_connected = %d\n", id, netname, GETPLAYERAUTHID(p), g_Clients[id].team, g_Clients[id].is_connected);
        // UTIL_ServerPrint("[DEBUG] num_unassigned = %d, num_tt = %d, num_ct = %d, num_spec = %d\n", g_PlayersNum[TEAM_UNASSIGNED], g_PlayersNum[TEAM_TERRORIST], g_PlayersNum[TEAM_CT], g_PlayersNum[TEAM_SPECTRATOR]);
    }
}

void Client_Disconnected(edict_t *p, bool crash, char *format)
{
    auto id = ENTINDEX(p);
    if (is_valid_index(id))
    {
        // UTIL_ServerPrint("[DEBUG] Client_Disconnected(): id = %d, is_connected = %d\n", id, g_Clients[id].is_connected);
        if (g_Clients[id].is_connected)
        {
            g_Clients[id].is_connected = false;
            g_Clients[id].is_bot = false;
            ////UTIL_ServerPrint("[DEBUG] Client_Disconnected(): id = %d, name = %s\n", id, STRING(pPlayer->edict()->v.netname));
            g_PlayersNum[g_Clients[id].team]--;
            // FIX TEAMS DEAD COUNT
            if (is_valid_entity(p) && p->v.deadflag != DEAD_NO && is_valid_team(g_Clients[id].team) && g_PlayersNum[TEAM_DEAD_TT + g_Clients[id].team - 1] > 0)
                g_PlayersNum[TEAM_DEAD_TT + g_Clients[id].team - 1]--;
            // UTIL_ServerPrint("[DEBUG] DISCONNECT: num_unassigned = %d, num_tt = %d, num_ct = %d, num_spec = %d, num_dead_tt = %d, num_dead_ct = %d\n",
            //     g_PlayersNum[TEAM_UNASSIGNED], g_PlayersNum[TEAM_TERRORIST], g_PlayersNum[TEAM_CT], g_PlayersNum[TEAM_SPECTRATOR], g_PlayersNum[TEAM_DEAD_TT], g_PlayersNum[TEAM_DEAD_CT]
            //);
        }
    }
}

void Client_TeamInfo(void *m_value)
{
    static int id;
    char *msg;
    TEAMS_t new_team;
    switch (m_state++)
    {
    case 0:
        id = *(int *)m_value;
        break;
    case 1:
        if (!is_valid_index(id))
            break;
        msg = (char *)m_value;
        if (!msg)
            break;
        switch (msg[0])
        {
        case 'T':
            new_team = TEAM_TERRORIST;
            break;
        case 'C':
            new_team = TEAM_CT;
            break;
        case 'S':
            new_team = TEAM_SPECTRATOR;
            break;
        default:
            new_team = TEAM_UNASSIGNED;
        }
        // UTIL_ServerPrint("[DEBUG] id = %d, old_team = %d, new_team = %d\n", id, g_Clients[id].team, new_team);
        auto p = INDEXENT(id);
        if (!g_Clients[id].is_connected)
        {
            g_Clients[id].team = new_team;
        }
        else if (new_team != TEAM_UNASSIGNED && g_Clients[id].team != new_team)
        {
            // FIX TEAMS DEAD COUNT
            if (is_valid_entity(p) && p->v.deadflag != DEAD_NO)
            {
                if (is_valid_team(g_Clients[id].team) && g_PlayersNum[TEAM_DEAD_TT + g_Clients[id].team - 1] > 0)
                    g_PlayersNum[TEAM_DEAD_TT + g_Clients[id].team - 1]--;
                if (is_valid_team(new_team) && g_PlayersNum[TEAM_DEAD_TT + new_team - 1] > 0)
                    g_PlayersNum[TEAM_DEAD_TT + new_team - 1]++;
            }
            // UTIL_ServerPrint("[DEBUG] Team changed!!!\n");
            g_PlayersNum[g_Clients[id].team]--;
            g_PlayersNum[new_team]++;
            g_Clients[id].team = new_team;
            // UTIL_ServerPrint("[DEBUG] num_unassigned = %d, num_tt = %d, num_ct = %d, num_spec = %d, num_dead_tt = %d, num_dead_ct = %d\n",
            //     g_PlayersNum[TEAM_UNASSIGNED], g_PlayersNum[TEAM_TERRORIST], g_PlayersNum[TEAM_CT], g_PlayersNum[TEAM_SPECTRATOR], g_PlayersNum[TEAM_DEAD_TT], g_PlayersNum[TEAM_DEAD_CT]
            //);
        }
        // FIX TEAM
        if (is_valid_entity(p))
            p->v.team = new_team;
    }
}

int R_RegUserMsg_Post(const char *pszName, int iSize)
{
    for (auto &msg : g_user_msg)
    {
        if (strcmp(msg.name, pszName) == 0)
        {
            int id = META_RESULT_ORIG_RET(int);
            // UTIL_ServerPrint("[DEBUG] RegUserMsg: id = %d, %s\n", id, pszName);
            *msg.id = id;
            if (msg.endmsg)
                modMsgsEnd[id] = msg.func;
            else
                modMsgs[id] = msg.func;
            break;
        }
    }
    RETURN_META_VALUE(MRES_IGNORED, 0);
}

void R_MessageBegin_Post(int msg_dest, int msg_type, const float *pOrigin, edict_t *ed)
{
    m_state = 0;
    function = modMsgs[msg_type];
    endfunction = modMsgsEnd[msg_type];
    RETURN_META(MRES_IGNORED);
}

void R_WriteByte_Post(int iValue)
{
    if (function)
        (*function)((void *)&iValue);
    RETURN_META(MRES_IGNORED);
}

void R_WriteChar_Post(int iValue)
{
    if (function)
        (*function)((void *)&iValue);
    RETURN_META(MRES_IGNORED);
}

void R_WriteShort_Post(int iValue)
{
    if (function)
        (*function)((void *)&iValue);
    RETURN_META(MRES_IGNORED);
}

void R_WriteLong_Post(int iValue)
{
    if (function)
        (*function)((void *)&iValue);
    RETURN_META(MRES_IGNORED);
}

void R_WriteAngle_Post(float flValue)
{
    if (function)
        (*function)((void *)&flValue);
    RETURN_META(MRES_IGNORED);
}

void R_WriteCoord_Post(float flValue)
{
    if (function)
        (*function)((void *)&flValue);
    RETURN_META(MRES_IGNORED);
}

void R_WriteString_Post(const char *sz)
{
    if (function)
        (*function)((void *)sz);
    RETURN_META(MRES_IGNORED);
}

void R_WriteEntity_Post(int iValue)
{
    if (function)
        (*function)((void *)&iValue);
    RETURN_META(MRES_IGNORED);
}

void R_MessageEnd_Post(void)
{
    if (endfunction)
        (*endfunction)(NULL);
    RETURN_META(MRES_IGNORED);
}

int trie_add(std::map<std::string, std::vector<int>> &trie, std::string key, int value)
{
    auto &v = trie[key];
    if (v.size() < v.max_size())
        v.push_back(value);
    return v.size();
}

int trie_remove(std::map<std::string, std::vector<int>> &trie, std::string key, int value)
{
    auto &v = trie[key];
    if (v.size())
    {
        auto it = std::find(v.begin(), v.end(), value);
        if (it != v.end())
        {
            v.erase(it);
            if (!v.size())
                trie.erase(key);
        }
    }
    return v.size();
}

void trie_transfer(std::map<std::string, std::vector<cell>> &trie, std::string key_from, std::string key_to, int value)
{
    trie_remove(trie, key_from, value);
    g_Tries.classnames[value] = key_to;
    trie_add(trie, key_to, value);
}

int vector_add(std::vector<cell> &v, int value)
{
    v.push_back(value);
    return v.size();
}

int vector_remove(std::vector<cell> &v, int value)
{
    auto it = std::find(v.begin(), v.end(), value);
    if (it != v.end())
        v.erase(it);
    return v.size();
}

size_t copy_entities(std::vector<int> &v, std::string &key, cell *dest, size_t max_size)
{
    size_t result = 0;
    for (auto &entity : v)
    {
        auto p = INDEXENT(entity);
        if (!is_valid_entity(p))
        {
            vector_remove(v, entity);
            continue;
        }
        if (g_Tries.classnames[entity] != STRING(p->v.classname))
            trie_transfer(g_Tries.entities, g_Tries.classnames[entity], STRING(p->v.classname), entity);
        if (key != STRING(p->v.classname))
            continue;
        *(dest + result) = entity;
        if (++result >= max_size)
            break;
    }
    return result;
}

bool get_user_buyzone(const edict_t *p)
{
    bool result = false;
    // UTIL_ServerPrint("[DEBUG] get_user_buyzone(): id = %d, team = %d, classname = %s\n", ENTINDEX(p), p->v.team, STRING(p->v.classname));
    if (is_valid_entity(p) && is_valid_team(p->v.team) && p->v.deadflag == DEAD_NO)
    {
        if (r_bMapHasBuyZone)
        {
            // UTIL_ServerPrint("[DEBUG] get_user_buyzone(): zone_count = %d\n", g_Tries.entities["func_buyzone"].size());
            for (auto &buyzone : g_Tries.entities["func_buyzone"])
            {
                edict_t *pBuyZone = INDEXENT(buyzone);
                // UTIL_ServerPrint("[DEBUG] get_user_buyzone(): entity = %d, team = %d\n", buyzone, pBuyZone->v.team);
                if (is_valid_entity(pBuyZone) && p->v.team == pBuyZone->v.team && is_entity_intersects(p, pBuyZone))
                {
                    result = true;
                    break;
                }
            }
        }
        else
        {
            for (auto &spawn : g_Tries.entities[p->v.team == TEAM_TERRORIST ? "info_player_deathmatch" : "info_player_start"])
            {
                edict_t *pSpawn = INDEXENT(spawn);
                // UTIL_ServerPrint("[DEBUG] get_user_buyzone(): spawn = %d, classname = %s, kill = %d\n", spawn, STRING(pSpawn->v.classname), (pSpawn->v.flags & FL_KILLME));
                if (is_valid_entity(pSpawn) && (pSpawn->v.origin - p->v.origin).Length() < 200.0f)
                {
                    result = true;
                    break;
                }
            }
        }
    }
    return result;
}

void debug(const char *fmt, ...)
{
    static std::string f;
    if (fmt == nullptr)
        return;
    std::lock_guard lock(std_mutex);
    char str[1014]{0};
    va_list arg_ptr;
    va_start(arg_ptr, fmt);
    int len = vsnprintf(str, sizeof(str), fmt, arg_ptr);
    va_end(arg_ptr);
    if (len < 0)
        return;
    //////////////////////////////////////////////////
    //AMXX_Log(str);
    f = str;
    f = "[DEBUG] " + f + "\n";
    SERVER_PRINT(f.c_str());
}