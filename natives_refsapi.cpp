#include "precompiled.h"

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
    DEBUG("%s(): from %s, START", __func__, findPluginFast(amx)->getName());
    // UTIL_ServerPrint("[DEBUG] rf_get_players_num(): size = %u (%u)\n", params[arg_nums_arr_size], *getAmxAddr(amx, params[arg_nums_arr_size]));
    size_t max_size = std::min(_COUNT(g_PlayersNum), (size_t)params[arg_nums_arr_size]);
    if (max_size)
        Q_memcpy(getAmxAddr(amx, params[arg_nums_arr]), &g_PlayersNum, max_size << 2);
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
    DEBUG("%s(): from %s, START", __func__, findPluginFast(amx)->getName());
    CHECK_ISPLAYER(arg_index);
    auto v = &g_Tries.player_entities[params[arg_index]];
    size_t max_size = std::min(v->size(), (size_t)params[arg_ent_arr_size]);
    if (max_size)
        Q_memcpy(getAmxAddr(amx, params[arg_ent_arr]), v->data(), max_size << 2);
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
    DEBUG("%s(): from %s, START", __func__, findPluginFast(amx)->getName());
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
    DEBUG("%s(): from %s, START", __func__, findPluginFast(amx)->getName());
    std::string key = getAmxString(amx, params[arg_classname], g_buff);
    auto dest = getAmxAddr(amx, params[arg_ent_arr]);
    size_t max_size = params[arg_ent_arr_size];
    if (dest == nullptr || key.empty() || !max_size)
        return 0;
    int owner_index = params[arg_owner];
    if (is_valid_index(owner_index))
        return copy_entities(g_Tries.player_entities[owner_index], key, dest, max_size);
    // CHECK WEAPON
    else if (key.find(WP_CLASS_PREFIX) == 0 && key.length() > sizeof(WP_CLASS_PREFIX))
        return copy_entities(g_Tries.wp_entities, key, dest, max_size);
    // CHECK GLOBAL
    else // if (g_Tries.entities.find(key) != g_Tries.entities.end())
        return copy_entities(g_Tries.entities[key], key, dest, max_size);
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
    DEBUG("%s(): from %s, START", __func__, findPluginFast(amx)->getName());
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
    DEBUG("%s(): from %s, START", __func__, findPluginFast(amx)->getName());
    CHECK_ISPLAYER(arg_index);
    return get_user_buyzone(INDEXENT(params[arg_index]));
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
    DEBUG("%s(): from %s, START", __func__, findPluginFast(amx)->getName());
    int result = FALSE;
    auto plugin = findPluginFast(amx);
    auto list_cvar = g_cvar_mngr.get(plugin->getId());
    if (list_cvar == nullptr)
        return FALSE;
    // Sort list
    g_cvar_mngr.sort(list_cvar);
    std::wstring name = stows(getAmxString(amx, params[arg_name], g_buff));
    std::wstring path = stows(getAmxString(amx, params[arg_folder], g_buff));
    // UTIL_ServerPrint("[DEBUG] rf_config(): path = %s\n", wstos(path).c_str());
    if (name.empty())
        name = stows(plugin->getName());
    size_t pos = name.find(L".amxx");
    // UTIL_ServerPrint("[DEBUG] rf_config(): find pos = %d\n", pos);
    if (pos != std::wstring::npos)
        name.replace(pos, sizeof(L".amxx") - 1, L"");
    getcwd(g_buff, sizeof(g_buff));
    std::wstring root = stows(g_buff);
    path = wfmt(L"%s/%s/%s/plugins/%s", wstos(root).c_str(), g_amxxapi.GetModname(), LOCALINFO("amxx_configsdir"), path.empty() ? "" : wstos(path).c_str()).c_str();
    // UTIL_ServerPrint("[DEBUG] rf_config(): root = %s, dirs = %s\n", wstos(root).c_str(), wstos(path).c_str());
    if (path.back() != L'/')
    {
        // UTIL_ServerPrint("[DEBUG] rf_config(): create dirs = %s\n", wstos(path).c_str());
        std::filesystem::create_directories(wstos(path).c_str());
        path += L'/';
    }
    path += name + L".cfg";
    // UTIL_ServerPrint("[DEBUG] rf_config(): auto_create = %d, path = %s\n", params[arg_auto_create], wstos(path).c_str());

    bool is_exist;
    if ((is_exist = file_exists(path)) || params[arg_auto_create])
    {
        std::wfstream file;
        file.open(wstos(path).c_str(), is_exist ? std::ios::in | std::ios::out | std::ios::binary : std::ios::in | std::ios::out | std::ios::binary | std::ios::trunc);
        // UTIL_ServerPrint("[DEBUG] rf_config(): is_exist = %d, is_open = %d\n", is_exist, file.is_open());
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
                    // UTIL_ServerPrint("[DEBUG] rf_config(): line = <%s>\n", wstos(line).c_str());
                    trim(line);
                    // Is comments?
                    if (line.find(L";") == 0 || line.find(L"#") == 0 || line.find(L"//") == 0 || (pos = line.find(L" ")) == std::string::npos)
                        continue;
                    // Split var
                    var_name = trim_c(line.substr(0, pos++));
                    var_value = trim_c(line.substr(pos, line.size() - pos));
                    if (rm_quote(var_value) == -1)
                        AMXX_LogError(amx, AMX_ERR_NATIVE, "%s: the parameter <%s> has a value <%s> with a wrong quotation mark", __FUNCTION__, wstos(var_name).c_str(), wstos(var_value).c_str());
                    // UTIL_ServerPrint("[DEBUG] rf_config(): name = <%s>, value = <%s>\n", wstos(var_name).c_str(), wstos(var_value).c_str());
                    load_cvars.insert({var_name, var_value});
                }
            }
            // UTIL_ServerPrint("[DEBUG] rf_config(): cvars read %d\n", load_cvars.size());
            bool need_update = g_cvar_mngr.need_update(load_cvars, list_cvar);
            load_cvars_t::iterator load_cvars_it;
            bool need_replace;
            m_cvar_t *m_cvar;
            if (need_update)
            {
                // UTIL_ServerPrint("[DEBUG] rf_config(): the config needs to be updated\n");
                //  Need truncate?
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
            for (auto m_cvar : *list_cvar)
            {
                // Skip cvar witout bind
                // if (m_cvar->type == CVAR_TYPE_NONE)
                //    continue;
                if (need_update)
                {
                    file << L"\n// ";
                    for (auto &ch : m_cvar->desc)
                    {
                        file << ch;
                        if (ch == L'\n')
                            file << L"// ";
                    }
                    file << L"\n// -\n";
                    file << L"// Default: \"" << m_cvar->value << L"\"\n";
                    if (m_cvar->has_min)
                        file << L"// Minimum: \"" << std::to_wstring(m_cvar->min_val) << "\"\n";
                    if (m_cvar->has_max)
                        file << L"// Maximum: \"" << std::to_wstring(m_cvar->max_val) << "\"\n";
                }
                // Is loaded cvar value?
                need_replace = (load_cvars_it = load_cvars.find(m_cvar->name)) != load_cvars.end();
                if (need_update)
                    file << m_cvar->name << " \"" << remove_chars(need_replace ? load_cvars_it->second : m_cvar->value) << "\"\n";
                if (need_replace)
                {
                    // Direct set
                    g_cvar_mngr.direct_set(m_cvar->cvar, wstos(load_cvars_it->second).c_str()); // g_cvar_mngr.set(CVAR_TYPE_STR, m_cvar, (cell *)wstos(load_cvars_it->second).c_str());
                    // Remove from list
                    load_cvars.erase(load_cvars_it);
                }
            }
            // UTIL_ServerPrint("[DEBUG] rf_config(): cvars left %d\n", load_cvars.size());
            //  Unknown cvars left?
            if (need_update && load_cvars.size())
            {
                file << L"\n// - DISABLED\n";
                for (auto &cvar : load_cvars)
                    file << L"// " << cvar.first << L" = \"" << cvar.second << "\"\n";
            }
            result = (list_cvar->size() & 0xFF) | ((load_cvars.size() & 0xFF) << 8) | ((is_exist | ((!is_exist && (bool)params[arg_auto_create]) << 1) | (need_update << 2)) << 16);
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
    DEBUG("%s(): from %s, START", __func__, findPluginFast(amx)->getName());
    auto plugin = findPluginFast(amx);
    std::wstring name = stows(getAmxString(amx, params[arg_name], g_buff));
    std::wstring value = stows(getAmxString(amx, params[arg_value], g_buff));
    std::wstring desc = stows(getAmxString(amx, params[arg_desc], g_buff));
    // UTIL_ServerPrint("[DEBUG] rf_create_cvar(): name = <%s>\n", wstos(name).c_str());
    return (cell)((void *)g_cvar_mngr.add(plugin->getId(), name, value, params[arg_flags], desc, params[arg_has_min], amx_ctof(params[arg_min_val]), params[arg_has_max], amx_ctof(params[arg_max_val])));
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
    DEBUG("%s(): from %s, START", __func__, findPluginFast(amx)->getName());
    auto plugin = findPluginFast(amx);
    // Variable address is not inside global area?
    check_global_r(params[arg_var]);
    check_type_r(params[arg_type]);
    m_cvar_t *m_cvar = (m_cvar_t *)((void *)params[arg_pcvar]);
    g_cvar_mngr.bind(plugin, (CVAR_TYPES_t)params[arg_type], m_cvar, getAmxAddr(amx, params[arg_var]), params[arg_var_size]);
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
    DEBUG("%s(): from %s, START", __func__, findPluginFast(amx)->getName());
    auto plugin = findPluginFast(amx);
    std::wstring name = stows(getAmxString(amx, params[arg_callback], g_buff));
    int fwd = g_amxxapi.RegisterSPForwardByName(amx, wstos(name).c_str(), FP_CELL, FP_STRING, FP_STRING, FP_DONE);
    check_fwd_r(fwd, wstos(name).c_str());
    m_cvar_t *m_cvar = (m_cvar_t *)((void *)params[arg_pcvar]);
    // UTIL_ServerPrint("[DEBUG] rf_hook_cvar_change(): fwd = %d, name = <%s>, cvar = %d\n", fwd, wstos(name).c_str(), cvar);
    return g_cvar_mngr.create_hook(fwd, m_cvar, params[arg_state]);
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
    DEBUG("%s(): from %s, START", __func__, findPluginFast(amx)->getName());
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
    DEBUG("%s(): from %s, START", __func__, findPluginFast(amx)->getName());
    m_cvar_t *m_cvar = (m_cvar_t *)((void *)params[arg_pcvar]);
    return g_cvar_mngr.get((CVAR_TYPES_t)params[arg_type], m_cvar, getAmxAddr(amx, params[arg_var]), params[arg_var_size]);
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
    DEBUG("%s(): from %s, START", __func__, findPluginFast(amx)->getName());
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
    DEBUG("%s(): from %s, START", __func__, findPluginFast(amx)->getName());
    m_cvar_t *m_cvar = (m_cvar_t *)((void *)params[arg_pcvar]);
    cell *ptr;
    switch (params[arg_type])
    {
    case CVAR_TYPE_NUM:
    case CVAR_TYPE_FLT:
        ptr = getAmxAddr(amx, params[arg_var]);
        // UTIL_ServerPrint("[DEBUG] rf_set_pcvar(): value_int = %d, value_float = %f\n", *ptr, *ptr);
        break;
    case CVAR_TYPE_STR:
        ptr = (cell *)getAmxString(amx, params[arg_var], g_buff);
    }
    g_cvar_mngr.set((CVAR_TYPES_t)params[arg_type], m_cvar, ptr);
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
    DEBUG("%s(): from %s, START", __func__, findPluginFast(amx)->getName());
    std::wstring name = stows(getAmxString(amx, params[arg_cvar], g_buff));
    cell *ptr;
    switch (params[arg_type])
    {
    case CVAR_TYPE_NUM:
    case CVAR_TYPE_FLT:
        ptr = getAmxAddr(amx, params[arg_var]);
        // UTIL_ServerPrint("[DEBUG] rf_set_pcvar(): value_int = %d, value_float = %f\n", *ptr, *ptr);
        break;
    case CVAR_TYPE_STR:
        ptr = (cell *)getAmxString(amx, params[arg_var], g_buff);
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
    DEBUG("%s(): from %s, START", __func__, findPluginFast(amx)->getName());
    // UTIL_ServerPrint("[DEBUG] rf_get_cvar_ptr(): start\n");
    std::wstring name = stows(getAmxString(amx, params[arg_cvar], g_buff));
    return (cell)((void *)g_cvar_mngr.get(name));
}

// native rf_recoil_enable(custom_impulse_offset);
cell AMX_NATIVE_CALL rf_recoil_enable(AMX *amx, cell *params)
{
    enum args_e
    {
        arg_count,
        arg_custom_impulse_offset,
    };
    DEBUG("%s(): from %s, START", __func__, findPluginFast(amx)->getName());
    g_recoil_mngr.enable(params[arg_custom_impulse_offset]);
    return TRUE;
}

// native rf_recoil_disable();
cell AMX_NATIVE_CALL rf_recoil_disable(AMX *amx, cell *params)
{
    DEBUG("%s(): from %s, START", __func__, findPluginFast(amx)->getName());
    g_recoil_mngr.disable();
    return TRUE;
}

#ifndef WITHOUT_SQL
// native Handle:rf_sql_tuple(callback[], db_host[], db_user[], db_pass[], db_name[], timeout = 3, db_chrs[] = "utf8");
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
    DEBUG("%s(): from %s, START", __func__, findPluginFast(amx)->getName());
    int fwd = -1;
    auto plugin = findPluginFast(amx);
    std::string callback, db_host, db_user, db_pass, db_name, db_chrs;
    callback = getAmxString(amx, params[arg_callback], g_buff);
    if (!callback.empty())
    {
        // public QueryHandler(failstate, Handle:query, error[], errnum, data[], size, Float:queuetime);
        // fwd = g_amxxapi.RegisterSPForwardByName(amx, callback.c_str(), FP_CELL, FP_CELL, FP_STRING, FP_CELL, FP_ARRAY, FP_CELL, FP_CELL, FP_DONE);
        fwd = g_amxxapi.RegisterSPForwardByName(amx, callback.c_str(), FP_CELL, FP_CELL, FP_CELL, FP_CELL, FP_CELL, FP_CELL, FP_CELL, FP_DONE);
        check_fwd_r(fwd, callback.c_str());
    }
    db_host = getAmxString(amx, params[arg_db_host], g_buff);
    db_user = getAmxString(amx, params[arg_db_user], g_buff);
    db_pass = getAmxString(amx, params[arg_db_pass], g_buff);
    db_name = getAmxString(amx, params[arg_db_name], g_buff);
    db_chrs = getAmxString(amx, params[arg_db_chrs], g_buff);
    return g_mysql_mngr.add_connect(amx, fwd, plugin->isDebug(), db_host, db_user, db_pass, db_name, db_chrs, params[arg_timeout]);
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
    DEBUG("%s(): from %s, START", __func__, findPluginFast(amx)->getName());
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
    DEBUG("%s(): from %s, START", __func__, findPluginFast(amx)->getName());
    return g_mysql_mngr.close((MYSQL *)params[arg_conn]);
}

// native bool:rf_sql_async_query(Handle:tuple, query[], data[] = "", data_size = 0, pri = 3, timeout = 60);
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
    DEBUG("%s(): from %s, START", __func__, findPluginFast(amx)->getName());
    // std::string query = getAmxString(amx, params[arg_query], g_buff);
    return g_mysql_mngr.push_query(params[arg_tuple], getAmxString(amx, params[arg_query], g_buff), getAmxAddr(amx, params[arg_data]), params[arg_data_size], params[arg_pri], params[arg_timeout]);
}

// native bool:rf_sql_get_result(Handle:query, bool:is_buffered = false);
cell AMX_NATIVE_CALL rf_sql_get_result(AMX *amx, cell *params)
{
    enum args_e
    {
        arg_count,
        arg_query,
        arg_buffered,
    };
    DEBUG("%s(): from %s, START", __func__, findPluginFast(amx)->getName());
    return g_mysql_mngr.get_result((m_query_t *)params[arg_query], params[arg_buffered]);
}

// native rf_sql_num_rows(Handle:query);
cell AMX_NATIVE_CALL rf_sql_num_rows(AMX *amx, cell *params)
{
    enum args_e
    {
        arg_count,
        arg_query,
    };
    DEBUG("%s(): from %s, START", __func__, findPluginFast(amx)->getName());
    return g_mysql_mngr.num_rows((m_query_t *)params[arg_query]);
}

// native bool:rf_sql_fetch_row(Handle:query);
cell AMX_NATIVE_CALL rf_sql_fetch_row(AMX *amx, cell *params)
{
    enum args_e
    {
        arg_count,
        arg_query,
    };
    DEBUG("%s(): from %s, START", __func__, findPluginFast(amx)->getName());
    return g_mysql_mngr.fetch_row((m_query_t *)params[arg_query]);
}

// native rf_sql_field_count(Handle:query);
cell AMX_NATIVE_CALL rf_sql_field_count(AMX *amx, cell *params)
{
    enum args_e
    {
        arg_count,
        arg_query,
    };
    DEBUG("%s(): from %s, START", __func__, findPluginFast(amx)->getName());
    return g_mysql_mngr.field_count((m_query_t *)params[arg_query]);
}

// native [RF_MAX_FIELD_SIZE] rf_sql_fetch_field(Handle:query, offset, mode = FT_AUTO, buff[] = "", buff_size = 0);
cell AMX_NATIVE_CALL rf_sql_fetch_field(AMX *amx, cell *params)
{
    enum args_e
    {
        arg_count,
        arg_query,
        arg_offset,
        arg_type,
        arg_buff,
        arg_buff_size,
        arg_ret,
    };
    DEBUG("%s(): from %s, START", __func__, findPluginFast(amx)->getName());
    return g_mysql_mngr.fetch_field((m_query_t *)params[arg_query], params[arg_offset], params[arg_type], getAmxAddr(amx, params[arg_ret]), getAmxAddr(amx, params[arg_buff]), params[arg_buff_size]);
}

// native [RF_MAX_FIELD_SIZE] rf_sql_fetch_nm_field(Handle:query, name[], mode = FT_AUTO, buff[] = "", buff_size = 0);
cell AMX_NATIVE_CALL rf_sql_fetch_nm_field(AMX *amx, cell *params)
{
    enum args_e
    {
        arg_count,
        arg_query,
        arg_name,
        arg_type,
        arg_buff,
        arg_buff_size,
        arg_ret,
    };
    DEBUG("%s(): from %s, START", __func__, findPluginFast(amx)->getName());
    char buff[2 * RF_MAX_FIELD_NAME];
    return g_mysql_mngr.fetch_field((m_query_t *)params[arg_query], getAmxString(amx, params[arg_name], buff), params[arg_type], getAmxAddr(amx, params[arg_ret]), getAmxAddr(amx, params[arg_buff]), params[arg_buff_size]);
}

// native [RF_MAX_FIELD_NAME] rf_sql_field_name(Handle:query, offset, buff[] = 0, buff_size = 0);
cell AMX_NATIVE_CALL rf_sql_field_name(AMX *amx, cell *params)
{
    enum args_e
    {
        arg_count,
        arg_query,
        arg_offset,
        arg_buff,
        arg_buff_size,
        arg_ret,
    };
    DEBUG("%s(): from %s, START", __func__, findPluginFast(amx)->getName());
    return g_mysql_mngr.field_name((m_query_t *)params[arg_query], params[arg_offset], getAmxAddr(amx, params[arg_ret]), getAmxAddr(amx, params[arg_buff]), params[arg_buff_size]);
}

// native rf_sql_insert_id(Handle:query);
cell AMX_NATIVE_CALL rf_sql_insert_id(AMX *amx, cell *params)
{
    enum args_e
    {
        arg_count,
        arg_query,
    };
    DEBUG("%s(): from %s, START", __func__, findPluginFast(amx)->getName());
    return g_mysql_mngr.insert_id((m_query_t *)params[arg_query]);
}

// native rf_sql_affected_rows(Handle:query);
cell AMX_NATIVE_CALL rf_sql_affected_rows(AMX *amx, cell *params)
{
    enum args_e
    {
        arg_count,
        arg_query,
    };
    DEBUG("%s(): from %s, START", __func__, findPluginFast(amx)->getName());
    return g_mysql_mngr.affected_rows((m_query_t *)params[arg_query]);
}

// native [RF_MAX_FIELD_SIZE] rf_sql_query_str(Handle:query, buff[] = "", buff_size = 0);
cell AMX_NATIVE_CALL rf_sql_query_str(AMX *amx, cell *params)
{
    enum args_e
    {
        arg_count,
        arg_query,
        arg_buff,
        arg_buff_size,
        arg_ret,
    };
    DEBUG("%s(): from %s, START", __func__, findPluginFast(amx)->getName());
    return g_mysql_mngr.query_str((m_query_t *)params[arg_query], getAmxAddr(amx, params[arg_ret]), getAmxAddr(amx, params[arg_buff]), params[arg_buff_size]);
}

// native Handle:rf_sql_query(Handle:conn, const fmt[], any:...);
cell AMX_NATIVE_CALL rf_sql_query(AMX *amx, cell *params)
{
    enum args_e
    {
        arg_count,
        arg_conn,
        arg_fmt,
    };
    DEBUG("%s(): from %s, START", __func__, findPluginFast(amx)->getName());
    int len;
    auto fmt = g_amxxapi.FormatAmxString(amx, params, arg_fmt, &len);
    return (cell)g_mysql_mngr.prepare_query((MYSQL *)params[arg_conn], fmt);
}

// native bool:rf_sql_exec(Handle:query);
cell AMX_NATIVE_CALL rf_sql_exec(AMX *amx, cell *params)
{
    enum args_e
    {
        arg_count,
        arg_query,
    };
    DEBUG("%s(): from %s, START", __func__, findPluginFast(amx)->getName());
    return g_mysql_mngr.exec_query((m_query_t *)params[arg_query]);
}

// native rf_sql_free(Handle:query);
cell AMX_NATIVE_CALL rf_sql_free(AMX *amx, cell *params)
{
    enum args_e
    {
        arg_count,
        arg_query,
    };
    DEBUG("%s(): from %s, START", __func__, findPluginFast(amx)->getName());
    g_mysql_mngr.free_query((m_query_t *)params[arg_query]);
    return TRUE;
}
#endif

#ifndef WITHOUT_LOG
// native rf_log(const log_level, const fmt[], any:...);
cell AMX_NATIVE_CALL rf_log(AMX *amx, cell *params)
{
    enum args_e
    {
        arg_count,
        arg_log_level,
        arg_fmt,
    };
    DEBUG("%s(): from %s, START => log_level = %d, level = %d", __func__, findPluginFast(amx)->getName(), g_log_mngr.get_log_level(amx), params[arg_log_level]);
    if (!(g_log_mngr.get_log_level(amx) & params[arg_log_level]))
        return FALSE;
    int len;
    auto fmt = g_amxxapi.FormatAmxString(amx, params, arg_fmt, &len);
    g_log_mngr.write_file(amx, fmt);
    return TRUE;
}
#endif

AMX_NATIVE_INFO Misc_Natives[] = {
    {"rf_get_players_num", rf_get_players_num},
    {"rf_get_user_weapons", rf_get_user_weapons},
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
#ifndef WITHOUT_SQL
    {"rf_sql_tuple", rf_sql_tuple},
    {"rf_sql_connect", rf_sql_connect},
    {"rf_sql_close", rf_sql_close},
    {"rf_sql_get_result", rf_sql_get_result},
    {"rf_sql_async_query", rf_sql_async_query},
    {"rf_sql_num_rows", rf_sql_num_rows},
    {"rf_sql_fetch_row", rf_sql_fetch_row},
    {"rf_sql_field_count", rf_sql_field_count},
    {"rf_sql_fetch_field", rf_sql_fetch_field},
    {"rf_sql_fetch_nm_field", rf_sql_fetch_nm_field},
    {"rf_sql_field_name", rf_sql_field_name},
    {"rf_sql_insert_id", rf_sql_insert_id},
    {"rf_sql_affected_rows", rf_sql_affected_rows},
    {"rf_sql_query", rf_sql_query},
    {"rf_sql_query_str", rf_sql_query_str},
    {"rf_sql_exec", rf_sql_exec},
    {"rf_sql_free", rf_sql_free},
#endif
#ifndef WITHOUT_LOG
    {"rf_log", rf_log},
#endif
    {nullptr, nullptr}};

void RegisterNatives_Misc()
{
    g_amxxapi.AddNatives(Misc_Natives);
    // g_amxxapi.OverrideNatives()
}
