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
            if (!is_valid_entity(pEdict) || (is_valid && ENTINDEX(pEdict->v.owner) != owner_index))
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
                if (!is_valid_entity(pEdict) || (is_valid && ENTINDEX(pEdict->v.owner) != owner_index))
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
    //UTIL_ServerPrint("[DEBUG] rf_config(): START\n");
    CPluginMngr::CPlugin *plugin = findPluginFast(amx);
    auto plugin_cvars = g_cvar_mngr.get(plugin->getId());
    if (check_it_empty(plugin_cvars))
        return FALSE;
    // Sort list
    plugin_cvars->second.sort([](cvar_list_it p1, cvar_list_it p2)
    {
        return p1->first < p2->first;
    });
    //g_cvar_mngr.sort(plugin_cvars);
    std::wstring name = stows(getAmxString(amx, params[arg_name], g_buff));
    std::wstring path = stows(getAmxString(amx, params[arg_folder], g_buff));
    //UTIL_ServerPrint("[DEBUG] rf_config(): path = %s\n", wstos(path).c_str());
    if (name.empty())
        name = stows(plugin->getName());
    size_t pos = name.find(L".amxx");
    //UTIL_ServerPrint("[DEBUG] rf_config(): find pos = %d\n", pos);
    if (pos != -1)
        name.replace(pos, sizeof(L".amxx") - 1, L"");
    getcwd(g_buff, sizeof(g_buff));
    std::wstring root = stows(g_buff);
    path = wfmt(L"%s/%s/%s/plugins/%s", wstos(root).c_str(), g_amxxapi.GetModname(), LOCALINFO("amxx_configsdir"), path.empty() ? "" : wstos(path).c_str()).c_str();
    //UTIL_ServerPrint("[DEBUG] rf_config(): root = %s, dirs = %s\n", wstos(root).c_str(), wstos(path).c_str());
    if (path.back() != L'/')
    {
        //UTIL_ServerPrint("[DEBUG] rf_config(): create dirs = %s\n", wstos(path).c_str());
        std::filesystem::create_directories(wstos(path).c_str());
        path += L'/';
    }
    path += name + L".cfg";
    //UTIL_ServerPrint("[DEBUG] rf_config(): auto_create = %d, path = %s\n", params[arg_auto_create], wstos(path).c_str());

    bool is_exist;
    if ((is_exist = file_exists(path)) || params[arg_auto_create])
    {
        std::wfstream file;
        file.open(wstos(path).c_str(), is_exist ? std::ios::in | std::ios::out | std::ios::binary : std::ios::in | std::ios::out | std::ios::binary | std::ios::trunc);
        //UTIL_ServerPrint("[DEBUG] rf_config(): is_exist = %d, is_open = %d\n", is_exist, file.is_open());
        if (file.is_open())
        {
            file.imbue(_LOCALE);
            load_cvars_t load_cvars;
            if (is_exist)
            {
                size_t pos;
                std::wstring line;
                std::wstring var_name;
                std::wstring var_value;
                while (std::getline(file, line, L'\n'))
                {
                    //UTIL_ServerPrint("[DEBUG] rf_config(): line = <%s>\n", wstos(line).c_str());
                    trim(line);
                    // Is comments?
                    if (line.find(L";") == 0 || line.find(L"#") == 0 || line.find(L"//") == 0 || (pos = line.find(L" ")) == std::string::npos)
                        continue;
                    // Split var
                    var_name = trim_c(line.substr(0, pos++));
                    var_value = trim_c(line.substr(pos, line.size() - pos));
                    if (rm_quote(var_value) == -1)
                        AMXX_LogError(amx, AMX_ERR_NATIVE, "%s: the parameter <%s> has a value <%s> with a wrong quotation mark", __FUNCTION__, wstos(var_name).c_str(), wstos(var_value).c_str());
                    //UTIL_ServerPrint("[DEBUG] rf_config(): name = <%s>, value = <%s>\n", wstos(var_name).c_str(), wstos(var_value).c_str());
                    load_cvars.insert({ var_name, var_value });
                }
            }
            //UTIL_ServerPrint("[DEBUG] rf_config(): cvars read %d\n", load_cvars.size());
            bool need_update = g_cvar_mngr.need_update(load_cvars, plugin_cvars);
            load_cvars_t::iterator load_cvars_it;
            bool need_replace;
            m_cvar_t* m_cvar;
            if (need_update)
            {
                //UTIL_ServerPrint("[DEBUG] rf_config(): the config needs to be updated\n");
                // Need truncate?
                if (is_exist)
                {
                    // Reopen
                    file.close();
                    file.open(wstos(path).c_str(), std::ios::in | std::ios::out | std::ios::binary | std::ios::trunc);
                }
                //"test %d", 10);
                file << L"// This file was auto-generated by REFSAPI\n";
                file << L"// Cvars for plugin \"" << plugin->getTitle() << L"\" by \"" << plugin->getAuthor() << L"\" (" << plugin->getName() << L", v" << plugin->getVersion() << L")\n";
            }
            for (auto& cvar_it : plugin_cvars->second)
            {
                m_cvar = &cvar_it->second;
                if (need_update)
                {
                    file << L"\n// ";
                    for (auto& ch : m_cvar->desc)
                    {
                        file << ch;
                        if (ch == L'\n')
                            file << L"// ";
                    }
                    file << L"\n// -\n";
                    file << L"// Default: \"" << cvar_it->second.value << L"\"\n";
                    if (m_cvar->has_min)
                        file << L"// Minimum: \"" << std::to_wstring(m_cvar->min_val) << "\"\n";
                    if (m_cvar->has_max)
                        file << L"// Maximum: \"" << std::to_wstring(m_cvar->max_val) << "\"\n";
                }
                // Is loaded cvar value?
                need_replace = (load_cvars_it = load_cvars.find(m_cvar->name)) != load_cvars.end();
                if (need_update)
                    file << m_cvar->name << " \"" << (need_replace ? load_cvars_it->second : m_cvar->value) << "\"\n";
                if (need_replace)
                {
                    // Direct set
                    g_cvar_mngr.direct_set(m_cvar->cvar, wstos(load_cvars_it->second).c_str());
                    // Remove from list
                    load_cvars.erase(load_cvars_it);
                }
            }
            //UTIL_ServerPrint("[DEBUG] rf_config(): cvars left %d\n", load_cvars.size());
            // Unknown cvars left?
            if (need_update && load_cvars.size() > 0)
            {
                file << L"\n// - DISABLED\n";
                for (auto& cvar : load_cvars)
                    file << L"// " << cvar.first << L" = \"" << cvar.second << "\"\n";
            }
            result = (plugin_cvars->second.size() & 0xFF) | ((load_cvars.size() & 0xFF) << 8) | ((is_exist | ((!is_exist && (bool)params[arg_auto_create]) << 1) | (need_update << 2)) << 16);
            file.close();
        }
        else
            AMXX_LogError(amx, AMX_ERR_NATIVE, "%s: error opening the file <%s> (check permissions)", __FUNCTION__, wstos(path).c_str());
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
    auto result = g_cvar_mngr.add(plugin->getId(), name, value, params[arg_flags], desc, params[arg_has_min], amx_ctof(params[arg_min_val]), params[arg_has_max], amx_ctof(params[arg_max_val]));
    //UTIL_ServerPrint("[DEBUG] rf_create_cvar(): RESULT = %d\n", result->second.cvar);
    return check_it_empty(result) ? FALSE : (cell)((void*)(result->second.cvar));
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
    cvar_t* cvar = (cvar_t*)((void*)params[arg_pcvar]);
    g_cvar_mngr.bind(plugin, (CVAR_TYPES_t)params[arg_type], g_cvar_mngr.get(cvar), getAmxAddr(amx, params[arg_var]), params[arg_var_size]);
    return TRUE;
}

// native rf_hook_cvar_change(pcvar, const callback[], bool:is_enable = true);
cell AMX_NATIVE_CALL rf_hook_cvar_change(AMX *amx, cell *params)
{
    enum args_e
    {
        arg_count,
        arg_pcvar,
        arg_callback,
        arg_state
    };
    CPluginMngr::CPlugin *plugin = findPluginFast(amx);
    std::wstring name = stows(getAmxString(amx, params[arg_callback], g_buff));
    int fwd = g_amxxapi.RegisterSPForwardByName(amx, wstos(name).c_str(), FP_CELL, FP_STRING, FP_STRING, FP_DONE);
    check_fwd_r(fwd, wstos(name).c_str());
    cvar_t* cvar = (cvar_t*)((void*)params[arg_pcvar]);
    //UTIL_ServerPrint("[DEBUG] rf_hook_cvar_change(): fwd = %d, name = <%s>, cvar = %d\n", fwd, wstos(name).c_str(), cvar);
    auto result = g_cvar_mngr.create_hook(fwd, g_cvar_mngr.get(cvar), params[arg_state]);
    return check_it_empty(result) || fwd != result->first ? FALSE : (cell)result->first;
}

// native rf_cvar_hook_state(phook, bool:is_enbale);
cell AMX_NATIVE_CALL rf_cvar_hook_state(AMX *amx, cell *params)
{
    enum args_e
    {
        arg_count,
        arg_fwd,
        arg_state,
    };
    return g_cvar_mngr.cvar_hook_state(params[arg_fwd], params[arg_state]);
}

// native rf_get_pcvar(type, pcvar, any:value[] = "", size = 0);
cell AMX_NATIVE_CALL rf_get_pcvar(AMX *amx, cell *params)
{
    enum args_e
    {
        arg_count,
        arg_type,
        arg_pcvar,
        arg_var,
        arg_var_size,
    };
    cvar_t* cvar = (cvar_t*)((void*)params[arg_pcvar]);
    return g_cvar_mngr.get((CVAR_TYPES_t)params[arg_type], cvar, getAmxAddr(amx, params[arg_var]), params[arg_var_size]);
}

// native rf_get_cvar(type, cvar[], any:value[] = "", size = 0);
cell AMX_NATIVE_CALL rf_get_cvar(AMX *amx, cell *params)
{
    enum args_e
    {
        arg_count,
        arg_type,
        arg_cvar,
        arg_var,
        arg_var_size,
    };
    std::wstring name = stows(getAmxString(amx, params[arg_cvar], g_buff));
    return g_cvar_mngr.get((CVAR_TYPES_t)params[arg_type], name, getAmxAddr(amx, params[arg_var]), params[arg_var_size]);
}

// native rf_set_pcvar(type, pcvar, any:value[]);
cell AMX_NATIVE_CALL rf_set_pcvar(AMX *amx, cell *params)
{
    enum args_e
    {
        arg_count,
        arg_type,
        arg_pcvar,
        arg_var,
    };
    cvar_t* cvar = (cvar_t*)((void*)params[arg_pcvar]);
    cell* ptr;
    switch (params[arg_type])
    {
        case CVAR_TYPE_NUM:
        case CVAR_TYPE_FLT:
            ptr = getAmxAddr(amx, params[arg_var]);
            //UTIL_ServerPrint("[DEBUG] rf_set_pcvar(): value_int = %d, value_float = %f\n", *ptr, *ptr);
            break;
        case CVAR_TYPE_STR:
            ptr = (cell*)getAmxString(amx, params[arg_var], g_buff);
    }
    g_cvar_mngr.set((CVAR_TYPES_t)params[arg_type], cvar, ptr);
    return TRUE;
}

// native rf_set_cvar(type, cvar[], any:value[]);
cell AMX_NATIVE_CALL rf_set_cvar(AMX *amx, cell *params)
{
    enum args_e
    {
        arg_count,
        arg_type,
        arg_cvar,
        arg_var,
    };
    std::wstring name = stows(getAmxString(amx, params[arg_cvar], g_buff));
    cell* ptr;
    switch (params[arg_type])
    {
        case CVAR_TYPE_NUM:
        case CVAR_TYPE_FLT:
            ptr = getAmxAddr(amx, params[arg_var]);
            //UTIL_ServerPrint("[DEBUG] rf_set_pcvar(): value_int = %d, value_float = %f\n", *ptr, *ptr);
            break;
        case CVAR_TYPE_STR:
            ptr = (cell*)getAmxString(amx, params[arg_var], g_buff);
    }
    g_cvar_mngr.set((CVAR_TYPES_t)params[arg_type], name, ptr);
    return TRUE;
}

// native rf_get_cvar_ptr(type, cvar[]);
cell AMX_NATIVE_CALL rf_get_cvar_ptr(AMX *amx, cell *params)
{
    enum args_e
    {
        arg_count,
        arg_cvar,
    };
    //UTIL_ServerPrint("[DEBUG] rf_get_cvar_ptr(): start\n");
    std::wstring name = stows(getAmxString(amx, params[arg_cvar], g_buff));
    auto result = g_cvar_mngr.get(name);
    return check_it_empty(result) ? FALSE : (cell)((void*)(result->second.cvar));   
}

// native rf_recoil_enable(custom_impulse_offset);
cell AMX_NATIVE_CALL rf_recoil_enable(AMX *amx, cell *params)
{
    enum args_e
    {
        arg_count,
        arg_custom_impulse_offset,
    };
    g_recoil_mngr.enable(params[arg_custom_impulse_offset]);
    return TRUE;   
}

// native rf_recoil_disable();
cell AMX_NATIVE_CALL rf_recoil_disable(AMX *amx, cell *params)
{
    g_recoil_mngr.disable();
    return TRUE;   
}

// native Handle:rf_sql_tuple(callback[], db_host[], db_user[], db_pass[], db_name[], timeout_ms = 250, db_chrs[] = "utf8");
cell AMX_NATIVE_CALL rf_sql_tuple(AMX *amx, cell *params)
{
    enum args_e
    {
        arg_count,
        arg_callback,
        arg_db_host, 
        arg_db_user, 
        arg_db_pass, 
        arg_db_name,
        arg_timeout,
        arg_db_chrs,
    };
    int fwd = -1;
    std::string callback, db_host, db_user, db_pass, db_name, db_chrs;
    callback = getAmxString(amx, params[arg_callback], g_buff);
    if (!callback.empty())
    {
        //public QueryHandler(failstate, Handle:query, error[], errnum, data[], size, Float:queuetime);
        fwd = g_amxxapi.RegisterSPForwardByName(amx, callback.c_str(), FP_CELL, FP_CELL, FP_ARRAY, FP_CELL, FP_ARRAY, FP_CELL, FP_CELL, FP_DONE);
        check_fwd_r(fwd, callback);
    }
    db_host = getAmxString(amx, params[arg_db_host], g_buff);
    db_user = getAmxString(amx, params[arg_db_user], g_buff);
    db_pass = getAmxString(amx, params[arg_db_pass], g_buff);
    db_name = getAmxString(amx, params[arg_db_name], g_buff);
    db_chrs = getAmxString(amx, params[arg_db_chrs], g_buff);
    return g_mysql_mngr.add_connect(fwd, db_host, db_user, db_pass, db_name, db_chrs, params[arg_timeout]);
}

// native Handle:rf_sql_connect(Handle:tuple, &err_num, error[], error_size);
cell AMX_NATIVE_CALL rf_sql_connect(AMX *amx, cell *params)
{
    enum args_e
    {
        arg_count,
        arg_tuple,
        arg_err_num,
        arg_error,
        arg_error_size,
    };
    return (cell)g_mysql_mngr.connect(params[arg_tuple], getAmxAddr(amx, params[arg_err_num]), getAmxAddr(amx, params[arg_error]), params[arg_error_size]);
}

// native bool:rf_sql_close(Handle:conn);
cell AMX_NATIVE_CALL rf_sql_close(AMX *amx, cell *params)
{
    enum args_e
    {
        arg_count,
        arg_conn,
    };
    return (cell)g_mysql_mngr.close((MYSQL*)params[arg_conn]);
}

// native rf_sql_async_query(Handle:tuple, query[], data[] = "", data_size = 0, pri = 3, timeout_ms = 60);
cell AMX_NATIVE_CALL rf_sql_async_query(AMX *amx, cell *params)
{
    enum args_e
    {
        arg_count,
        arg_tuple,
        arg_query,
        arg_data,
        arg_data_size,
        arg_pri,
        arg_timeout,
    };
    std::string query = getAmxString(amx, params[arg_query], g_buff);
    return g_mysql_mngr.push_query(params[arg_tuple], query, getAmxAddr(amx, params[arg_data]), params[arg_data_size], params[arg_pri], params[arg_timeout]);
}

// native bool:rf_sql_get_result(Handle:query, buff[], buff_size, bool:is_buffered = false);
cell AMX_NATIVE_CALL rf_sql_get_result(AMX *amx, cell *params)
{
    enum args_e
    {
        arg_count,
        arg_query,
        arg_buff,
        arg_buff_size,
        arg_buffered,
    };
    return g_mysql_mngr.get_result((m_query_t*)params[arg_query], params[arg_buff], getAmxAddr(amx, params[arg_buff]), params[arg_buff_size], params[arg_buffered]);
}

// native rf_sql_num_rows(Handle:query);
cell AMX_NATIVE_CALL rf_sql_num_rows(AMX *amx, cell *params)
{
    enum args_e
    {
        arg_count,
        arg_query,
    };
    return g_mysql_mngr.num_rows((m_query_t*)params[arg_query]);
}

// native bool:rf_sql_fetch_row(Handle:query);
cell AMX_NATIVE_CALL rf_sql_fetch_row(AMX *amx, cell *params)
{
    enum args_e
    {
        arg_count,
        arg_query,
    };
    return g_mysql_mngr.fetch_row((m_query_t*)params[arg_query]);
}

// native rf_sql_field_count(Handle:query);
cell AMX_NATIVE_CALL rf_sql_field_count(AMX *amx, cell *params)
{
    enum args_e
    {
        arg_count,
        arg_query,
    };
    return g_mysql_mngr.field_count((m_query_t*)params[arg_query]);
}

// native any:rf_sql_fetch_field(Handle:query, offset, type = FT_AUTO);
cell AMX_NATIVE_CALL rf_sql_fetch_field(AMX *amx, cell *params)
{
    enum args_e
    {
        arg_count,
        arg_query,
        arg_offset,
        arg_type,
    };
    return g_mysql_mngr.fetch_field((m_query_t*)params[arg_query], params[arg_offset], params[arg_type]);
}

// native any:rf_sql_field_name(Handle:query, offset);
cell AMX_NATIVE_CALL rf_sql_field_name(AMX *amx, cell *params)
{
    enum args_e
    {
        arg_count,
        arg_query,
        arg_offset,
        arg_return,
    };
    //g_amxxapi.SetAmxStringUTF8Char(amx, )
    std::string ret = g_mysql_mngr.field_name((m_query_t*)params[arg_query], params[arg_offset]);
    UTIL_ServerPrint("[DEBUG] field_name = %s\n", ret.c_str());
    g_amxxapi.SetAmxStringUTF8Char(amx, params[arg_return], ret.c_str(), ret.size(), 256);
    return TRUE;
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
    {"rf_hook_cvar_change", rf_hook_cvar_change},
    {"rf_cvar_hook_state", rf_cvar_hook_state},
    {"rf_get_pcvar", rf_get_pcvar},
    {"rf_get_cvar", rf_get_cvar},
    {"rf_set_pcvar", rf_set_pcvar},
    {"rf_set_cvar", rf_set_cvar},
    {"rf_get_cvar_ptr", rf_get_cvar_ptr},
    {"rf_recoil_enable", rf_recoil_enable},
    {"rf_recoil_disable", rf_recoil_disable},
    {"rf_sql_tuple", rf_sql_tuple},
    {"rf_sql_connect", rf_sql_connect},
    {"rf_sql_close", rf_sql_close}, 
    {"rf_sql_get_result", rf_sql_get_result},
    {"rf_sql_async_query", rf_sql_async_query},
    {"rf_sql_num_rows", rf_sql_num_rows},
    {"rf_sql_fetch_row", rf_sql_fetch_row},
    {"rf_sql_field_count", rf_sql_field_count},
    {"rf_sql_fetch_field", rf_sql_fetch_field},
    {"rf_sql_field_name", rf_sql_field_name},
    {nullptr, nullptr}
};

void RegisterNatives_Misc()
{
    g_amxxapi.AddNatives(Misc_Natives);
    //g_amxxapi.OverrideNatives()
}
