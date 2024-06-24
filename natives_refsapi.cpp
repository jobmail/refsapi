#include "precompiled.h"

inline CPluginMngr::CPlugin *findPluginFast(AMX *amx) { return (CPluginMngr::CPlugin *)(amx->userdata[UD_FINDPLUGIN]); }

// native rf_get_players_num(nums[], nums_size, bool:teams_only);
cell AMX_NATIVE_CALL rf_get_players_num(AMX *amx, cell *params)
{
    enum args_e
    {
        arg_count,
        arg_nums_arr,
        arg_nums_arr_size,
        arg_teams_only
    };
    size_t max_size = *getAmxAddr(amx, params[arg_nums_arr_size]);
    if (max_size > 0)
        Q_memcpy(getAmxAddr(amx, params[arg_nums_arr]), &g_PlayersNum, std::min(max_size, _COUNT(g_PlayersNum)) << 2);
    int total = g_PlayersNum[TEAM_TERRORIST] + g_PlayersNum[TEAM_CT];
    return params[arg_teams_only] ? total : total + g_PlayersNum[TEAM_UNASSIGNED] + g_PlayersNum[TEAM_SPECTRATOR];
}

// native rf_get_user_weapons(const id, ent[], ent_size);
cell AMX_NATIVE_CALL rf_get_user_weapons(AMX *amx, cell *params)
{
    enum args_e
    {
        arg_count,
        arg_index,
        arg_ent_arr,
        arg_ent_arr_size
    };
    if (params[arg_ent_arr_size] < 1)
        return 0;
    CHECK_ISPLAYER(arg_index);
    std::vector<cell> v = g_Tries.player_entities[params[arg_index]];
    size_t max_size = std::min(v.size(), (size_t)*getAmxAddr(amx, params[arg_ent_arr_size]));
    if (max_size > 0)
        Q_memcpy(getAmxAddr(amx, params[arg_ent_arr]), v.data(), max_size << 2);
    return max_size;
}

// native rf_get_weaponname(const ent, name[], name_len);
cell AMX_NATIVE_CALL rf_get_weaponname(AMX *amx, cell *params)
{
    enum args_e
    {
        arg_count,
        arg_entity,
        arg_name,
        arg_name_len
    };
    CHECK_ISENTITY(arg_entity);
    edict_t *pEdict = INDEXENT(params[arg_entity]);
    if (is_valid_entity(pEdict))
    {
        g_amxxapi.SetAmxString(amx, params[arg_name], STRING(pEdict->v.classname), params[arg_name_len]);
        return TRUE;
    }
    return FALSE;
}

// native rf_get_ent_by_class(const classname[], const owner, ent[], ent_size);
cell AMX_NATIVE_CALL rf_get_ent_by_class(AMX *amx, cell *params)
{
    enum args_e
    {
        arg_count,
        arg_classname,
        arg_owner,
        arg_ent_arr,
        arg_ent_arr_size
    };
    int owner_index = params[arg_owner];
    int result = 0;
    edict_t *pEdict;
    char classname[256];
    std::string key = getAmxString(amx, params[arg_classname], classname);
    bool is_valid = is_valid_index(owner_index);
    if (g_Tries.entities.find(key) != g_Tries.entities.end())
    {
        for (auto &entity : g_Tries.entities[key])
        {
            pEdict = INDEXENT(entity);
            // CHECK VALID ENTITY & CHECK OWNER
            if (!is_valid_entity(pEdict) || is_valid && ENTINDEX(pEdict->v.owner) != owner_index)
                continue;
            // CHECK CREATION CLASSNAME
            if (key != STRING(pEdict->v.classname))
            {
                acs_trie_transfer(&g_Tries.entities, key, STRING(pEdict->v.classname), entity);
                continue;
            }
            *(getAmxAddr(amx, params[arg_ent_arr]) + result) = entity;
            if (++result >= params[arg_ent_arr_size])
                break;
        }
    }
    else
    {
        // CHECK WEAPON
        if (key.find(WP_CLASS_PREFIX) == 0 && key.length() > sizeof(WP_CLASS_PREFIX))
        {
            for (const int &entity : g_Tries.wp_entities)
            {
                pEdict = INDEXENT(entity);
                // CHECK VALID ENTITY & CHECK OWNER
                if (!is_valid_entity(pEdict) || is_valid && ENTINDEX(pEdict->v.owner) != owner_index)
                    continue;
                // CHECK CREATION CLASSNAME
                if (key == STRING(pEdict->v.classname))
                {
                    *(getAmxAddr(amx, params[arg_ent_arr]) + result) = entity;
                    // TRANSFER CLASSNAME
                    if (key != g_Tries.classnames[entity])
                    {
                        acs_trie_transfer(&g_Tries.entities, g_Tries.classnames[entity], key, entity);
                    }
                    if (++result >= params[arg_ent_arr_size])
                        break;
                }
            }
        }
    }
    return result;
}

// native rf_roundfloat(const Float:value, const precision);
cell AMX_NATIVE_CALL rf_roundfloat(AMX *amx, cell *params)
{
    enum args_e
    {
        arg_count,
        arg_value,
        arg_precision
    };
    float result = roundd(amx_ctof(params[arg_value]), params[arg_precision]);
    return amx_ftoc(result);
}

// native rf_get_user_buyzone(const id);
cell AMX_NATIVE_CALL rf_get_user_buyzone(AMX *amx, cell *params)
{
    enum args_e
    {
        arg_count,
        arg_index
    };
    CHECK_ISPLAYER(arg_index);
    return (qboolean)acs_get_user_buyzone(INDEXENT(params[arg_index]));
}

// native rf_config(const bool:auto_create = true, const name[] = "", const folder[] = "");
cell AMX_NATIVE_CALL rf_config(AMX *amx, cell *params)
{
    enum args_e
    {
        arg_count,
        arg_auto_create,
        arg_name,
        arg_folder
    };
    bool result = FALSE;
    UTIL_ServerPrint("[DEBUG] rf_config(): START\n");
    CPluginMngr::CPlugin *plugin = findPluginFast(amx);
    std::wstring name = stows(getAmxString(amx, params[arg_name], g_buff));
    if (name.empty())
        name = stows(plugin->getName());
    name.replace(name.find(L".amxx"), sizeof(L".amxx") - 1, L"");
    std::wstring path = stows(getAmxString(amx, params[arg_folder], g_buff));
    UTIL_ServerPrint("[DEBUG] rf_config(): plugin = %d, auto_create = %d, name = %s, folder = %s\n", plugin, params[arg_auto_create], wstos(name).c_str(), wstos(path).c_str());
    getcwd(g_buff, sizeof(g_buff));
    std::string root = g_buff;
    Q_snprintf(g_buff, sizeof(g_buff), "%s/%s/%s/plugins/%s.cfg", root.c_str(), g_amxxapi.GetModname(), LOCALINFO("amxx_configsdir"), path.empty() ? wstos(name).c_str() : wstos(wfmt(L"plugin-%s/%s", path, name)).c_str());
    path = stows(g_buff);
    UTIL_ServerPrint("[DEBUG] rf_config(): name = %s, path = %s, current = %s\n", wstos(name).c_str(), wstos(path).c_str(), g_buff);
    bool is_exist;
    if ((is_exist = file_exists(path)) || params[arg_auto_create])
    {
        std::wfstream file;
        file.open(wstos(path).c_str(), is_exist ? std::ios::in | std::ios::binary : std::ios::in | std::ios::out | std::ios::trunc | std::ios::binary);
        UTIL_ServerPrint("[DEBUG] rf_config(): is_exist = %d, is_open = %d\n", is_exist, file.is_open());
        if (file.is_open())
        {
            file.imbue(_LOCALE);
            if (is_exist)
            {
                std::wstring line;
                size_t pos;
                while (std::getline(file, line, L'\n'))
                {
                    UTIL_ServerPrint("[DEBUG] rf_config(): line = <%s>\n", wstos(line).c_str());
                    // Is comments?
                    if (line.find(L";") == 0 || line.find(L"#") == 0 || line.find(L"//") == 0 || (pos = line.find(L"=")) == std::string::npos)
                        continue;
                    // Split var
                    std::wstring var_name = trim_c(line.substr(0, pos++));
                    std::wstring var_value = trim_c(line.substr(pos, line.size() - pos));
                    if (rm_quote(var_value) == -1)
                        AMXX_LogError(amx, AMX_ERR_NATIVE, "%s: the parameter <%s> has a value <%s> with a wrong quotation mark", __FUNCTION__, wstos(var_name).c_str(), wstos(var_value).c_str());
                    else
                        trim(var_value);
                    UTIL_ServerPrint("[DEBUG] rf_config(): name = <%s>, value = <%s>\n", wstos(var_name).c_str(), wstos(var_value).c_str());
                    /*
                    g_cvar_mngr.add(plugin, var_name, var_value, FCVAR_SERVER | FCVAR_SPONLY, L"TEST");
                    // CHECK
                    auto cvar_it = g_cvar_mngr.get(var_name);
                    UTIL_ServerPrint("[DEBUG] rf_config(): CHECK ==> exist = %d, name = <%s>, string = <%s>, value = %f\n", cvar_it->second, cvar_it->second.cvar->name, cvar_it->second.cvar->string, cvar_it->second.cvar->value);
                    g_cvar_mngr.set(var_name, L"123.45678900000123");
                    UTIL_ServerPrint("[DEBUG] rf_config(): CHECK ==> exist = %d, name = <%s>, string = <%s>, value = %f\n", cvar_it->second, cvar_it->second.cvar->name, cvar_it->second.cvar->string, cvar_it->second.cvar->value);
                    g_cvar_mngr.set(cvar_it, L"987.654321000111222");
                    UTIL_ServerPrint("[DEBUG] rf_config(): CHECK ==> exist = %d, name = <%s>, string = <%s>, value = %f\n", cvar_it->second, cvar_it->second.cvar->name, cvar_it->second.cvar->string, cvar_it->second.cvar->value);
                    g_cvar_mngr.set(cvar_it, L"LAST_TEST");
                    UTIL_ServerPrint("[DEBUG] rf_config(): CHECK ==> exist = %d, name = <%s>, string = <%s>, value = %f\n", cvar_it->second, cvar_it->second.cvar->name, cvar_it->second.cvar->string, cvar_it->second.cvar->value);
                    */
                }
            }
            else
            {
                file << L"TEST_CVAR = Тестовая строка\n";
            }
            result = TRUE;
            file.close();
        }
        else
            AMXX_LogError(amx, AMX_ERR_NATIVE, "%s: error opening the file <%s>", __FUNCTION__, wstos(path).c_str());
    }
    return result;
}

// native rf_create_cvar(const name[], const value[], flags = FCVAR_NONE, const desc[] = "", bool:has_min = false, Float:min_val = 0.0, bool:has_max = false, Float:max_val = 0.0);
cell AMX_NATIVE_CALL rf_create_cvar(AMX *amx, cell *params)
{
    enum args_e
    {
        arg_count,
        arg_name,
        arg_value,
        arg_flags,
        arg_desc,
        arg_has_min,
        arg_min_val,
        arg_has_max,
        arg_max_val
    };
    CPluginMngr::CPlugin *plugin = findPluginFast(amx);
    std::wstring name = stows(getAmxString(amx, params[arg_name], g_buff));
    std::wstring value = stows(getAmxString(amx, params[arg_value], g_buff));
    std::wstring desc = stows(getAmxString(amx, params[arg_desc], g_buff));
    auto result = g_cvar_mngr.add(plugin, name, value, params[arg_flags], desc, params[arg_has_min], amx_ctof(params[arg_min_val]), params[arg_has_max], amx_ctof(params[arg_max_val]));
    UTIL_ServerPrint("[DEBUG] rf_create_cvar(): RESULT = %d\n", &result->second.cvar);
    return check_it_empty(result) ? FALSE : (cell)((void*)(&result->second.cvar));//(cell)((void*)(&result));
}

// native rf_bind_pcvar(type, pcvar, any:var[], varlen = 0);
cell AMX_NATIVE_CALL rf_bind_pcvar(AMX *amx, cell *params)
{
    enum args_e
    {
        arg_count,
        arg_type,
        arg_pcvar,
        arg_var,
        arg_var_size,
    };
    CPluginMngr::CPlugin *plugin = findPluginFast(amx);
    // Variable address is not inside global area?
    check_global_r(params[arg_var]);
    check_type_r(params[arg_type]);
    g_cvar_mngr.bind(plugin, (CVAR_TYPES_t)params[arg_type], *((cvar_list_it*)((void*)params[arg_pcvar])), getAmxAddr(amx, params[arg_var]), params[arg_var_size]);
    return TRUE;
}

// native rf_bind_pcvar_n(pcvar, &any:var);
cell AMX_NATIVE_CALL rf_bind_pcvar_n(AMX *amx, cell *params)
{
    enum args_e
    {
        arg_count,
        arg_pcvar,
        arg_var
    };
    CPluginMngr::CPlugin *plugin = findPluginFast(amx);
    // Variable address is not inside global area?
    check_global_r(params[arg_var]);
    g_cvar_mngr.bind(plugin, CVAR_TYPE_NUM, *((cvar_list_it*)((void*)params[arg_pcvar])), getAmxAddr(amx, params[arg_var]));
    return TRUE;
}

// native rf_bind_pcvar_f(pcvar, &Float:var);
cell AMX_NATIVE_CALL rf_bind_pcvar_f(AMX *amx, cell *params)
{
    enum args_e
    {
        arg_count,
        arg_pcvar,
        arg_var
    };
    CPluginMngr::CPlugin *plugin = findPluginFast(amx);
    // Variable address is not inside global area?
    check_global_r(params[arg_var]);
    g_cvar_mngr.bind(plugin, CVAR_TYPE_FLT, *((cvar_list_it*)((void*)params[arg_pcvar])), getAmxAddr(amx, params[arg_var]));
    return TRUE;
}

// native rf_bind_pcvar_s(pcvar, any:var[], varlen);
cell AMX_NATIVE_CALL rf_bind_pcvar_s(AMX *amx, cell *params)
{
    enum args_e
    {
        arg_count,
        arg_pcvar,
        arg_var,
        arg_var_size,
    };
    CPluginMngr::CPlugin *plugin = findPluginFast(amx);
    // Variable address is not inside global area?
    check_global_r(params[arg_var]);
    g_cvar_mngr.bind(plugin, CVAR_TYPE_STR, *((cvar_list_it*)((void*)params[arg_pcvar])), getAmxAddr(amx, params[arg_var]), params[arg_var_size]);
    return TRUE;
}

// native rf_hook_cvar_change(pcvar, const callback[]);
cell AMX_NATIVE_CALL rf_hook_cvar_change(AMX *amx, cell *params)
{
    enum args_e
    {
        arg_count,
        arg_pcvar,
        arg_callback,
    };
    CPluginMngr::CPlugin *plugin = findPluginFast(amx);
    std::wstring name = stows(getAmxString(amx, params[arg_callback], g_buff));
    int fwd = g_amxxapi.RegisterSPForwardByName(plugin->getAMX(), wstos(name).c_str(), ET_IGNORE, FP_DONE);
    UTIL_ServerPrint("[DEBUG] rf_hook_cvar_change(): fwd = %d, name = <%s>\n", fwd, wstos(name).c_str());
    check_fwd_r(fwd);
    cvar_t* cvar = (cvar_t*)((void*)&params[arg_pcvar]);
    auto result = g_cvar_mngr.create_hook(fwd, g_cvar_mngr.get(cvar));
    return check_it_empty(result) ? FALSE : (cell)((void*)(&result));
}

AMX_NATIVE_INFO Misc_Natives[] = {
    {"rf_get_players_num", rf_get_players_num},
    {"rf_get_weaponname", rf_get_weaponname},
    {"rf_get_ent_by_class", rf_get_ent_by_class},
    {"rf_roundfloat", rf_roundfloat},
    {"rf_get_user_buyzone", rf_get_user_buyzone},
    {"rf_config", rf_config},
    {"rf_create_cvar", rf_create_cvar},
    {"rf_bind_pcvar", rf_bind_pcvar},
    {"rf_bind_pcvar_n", rf_bind_pcvar_n},
    {"rf_bind_pcvar_f", rf_bind_pcvar_f},
    {"rf_bind_pcvar_s", rf_bind_pcvar_s},
    {"rf_hook_cvar_change", rf_hook_cvar_change},
    {nullptr, nullptr}
};

void RegisterNatives_Misc()
{
    g_amxxapi.AddNatives(Misc_Natives);
}
