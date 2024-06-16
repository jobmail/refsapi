#include "precompiled.h"

inline CPluginMngr::CPlugin* findPluginFast(AMX *amx) { return (CPluginMngr::CPlugin*)(amx->userdata[UD_FINDPLUGIN]); }

// native rf_get_players_num(nums[], nums_size, bool:teams_only);
cell AMX_NATIVE_CALL rf_get_players_num(AMX *amx, cell *params) {

    enum args_e { arg_count, arg_nums_arr, arg_nums_arr_size, arg_teams_only};

    size_t max_size = *getAmxAddr(amx, params[arg_nums_arr_size]);

    if (max_size > 0) 
    
        Q_memcpy(getAmxAddr(amx, params[arg_nums_arr]), &g_PlayersNum, std::min(max_size, _COUNT(g_PlayersNum)) << 2);
    
    int total = g_PlayersNum[TEAM_TERRORIST] + g_PlayersNum[TEAM_CT];

    return params[arg_teams_only] ? total : total + g_PlayersNum[TEAM_UNASSIGNED] + g_PlayersNum[TEAM_SPECTRATOR];
}

// native rf_get_user_weapons(const id, ent[], ent_size);
cell AMX_NATIVE_CALL rf_get_user_weapons(AMX *amx, cell *params) {
    
    enum args_e { arg_count, arg_index, arg_ent_arr, arg_ent_arr_size};

    if (params[arg_ent_arr_size] < 1) return 0;

    CHECK_ISPLAYER(arg_index);

    std::vector<cell> v = g_Tries.player_entities[params[arg_index]];

    size_t max_size = std::min(v.size(), (size_t)*getAmxAddr(amx, params[arg_ent_arr_size]));

    if (max_size > 0)
    
        Q_memcpy(getAmxAddr(amx, params[arg_ent_arr]), v.data(), max_size << 2);

    return max_size;
}

// native rf_get_weaponname(const ent, name[], name_len);
cell AMX_NATIVE_CALL rf_get_weaponname(AMX *amx, cell *params) {

    enum args_e { arg_count, arg_entity, arg_name, arg_name_len};

    CHECK_ISENTITY(arg_entity);

    edict_t* pEdict = INDEXENT(params[arg_entity]);

    if (is_valid_entity(pEdict)) {

        g_amxxapi.SetAmxString(amx, params[arg_name], STRING(pEdict->v.classname), params[arg_name_len]);

        return TRUE;
    }
    
    return FALSE;
}

// native rf_get_ent_by_class(const classname[], const owner, ent[], ent_size);
cell AMX_NATIVE_CALL rf_get_ent_by_class(AMX *amx, cell *params) {

    enum args_e { arg_count, arg_classname, arg_owner, arg_ent_arr, arg_ent_arr_size};

    int owner_index = params[arg_owner];

    int result = 0;

    edict_t* pEdict;

    char classname[256];

    std::string key = getAmxString(amx, params[arg_classname], classname);

    bool is_valid = is_valid_index(owner_index);

    if (g_Tries.entities.find(key) != g_Tries.entities.end()) {

        for (auto& entity : g_Tries.entities[key]) {

            pEdict = INDEXENT(entity);

            // CHECK VALID ENTITY & CHECK OWNER
            if (!is_valid_entity(pEdict) || is_valid && ENTINDEX(pEdict->v.owner) != owner_index) continue;

            // CHECK CREATION CLASSNAME
            if (key != STRING(pEdict->v.classname)) {

                acs_trie_transfer(&g_Tries.entities, key, STRING(pEdict->v.classname), entity);

                continue;
            }

            *(getAmxAddr(amx, params[arg_ent_arr]) + result) = entity;

            if (++result >= params[arg_ent_arr_size]) break;
        }

    } else {

        // CHECK WEAPON
        if (key.find(WP_CLASS_PREFIX) == 0 && key.length() > sizeof(WP_CLASS_PREFIX)) {

            for (const int& entity : g_Tries.wp_entities) {

                pEdict = INDEXENT(entity);

                // CHECK VALID ENTITY & CHECK OWNER
                if (!is_valid_entity(pEdict) || is_valid && ENTINDEX(pEdict->v.owner) != owner_index) continue;

                // CHECK CREATION CLASSNAME
                if (key == STRING(pEdict->v.classname)) {

                    *(getAmxAddr(amx, params[arg_ent_arr]) + result) = entity;

                    // TRANSFER CLASSNAME
                    if (key != g_Tries.classnames[entity]) {
                        
                        acs_trie_transfer(&g_Tries.entities, g_Tries.classnames[entity], key, entity);
                    }

                    if (++result >= params[arg_ent_arr_size]) break;
                }
            }
        }
    }

    return result;
}

// native rf_roundfloat(const Float:value, const precision);
cell AMX_NATIVE_CALL rf_roundfloat(AMX *amx, cell *params) {

    enum args_e { arg_count, arg_value, arg_precision};

    float result = acs_roundfloat(amx_ctof(params[arg_value]), params[arg_precision]);

    return amx_ftoc(result);
}

// native rf_get_user_buyzone(const id);
cell AMX_NATIVE_CALL rf_get_user_buyzone(AMX *amx, cell *params) {

    enum args_e { arg_count, arg_index};

    CHECK_ISPLAYER(arg_index);

    return (qboolean)acs_get_user_buyzone(INDEXENT(params[arg_index])); 
}

// native rf_config(const bool:auto_create = true, const name[] = "", const folder[] = "");
cell AMX_NATIVE_CALL rf_config(AMX *amx, cell *params) {

    enum args_e { arg_count, arg_auto_create, arg_name, arg_folder};

    bool result = FALSE;

    UTIL_ServerPrint("[DEBUG] rf_config(): START\n");

    CPluginMngr::CPlugin *plugin = findPluginFast(amx);

    char buff[256];

    std::string name = getAmxString(amx, params[arg_name], buff);

    if (name.empty())

        name =  plugin->getName();

    name.replace(name.find(".amxx"), sizeof(".amxx") - 1, "");

    std::string path = getAmxString(amx, params[arg_folder], buff);

    UTIL_ServerPrint("[DEBUG] rf_config(): plugin = %d, auto_create = %d, name = %s, folder = %s\n", plugin, params[arg_auto_create], name.c_str(), path.c_str());

    getcwd(buff, sizeof(buff));

    std::string root = buff;

    Q_snprintf(buff, sizeof(buff), "%s/%s/%s/plugins/%s.cfg", root.c_str(), g_amxxapi.GetModname(), LOCALINFO("amxx_configsdir"), path.empty() ? name.c_str() : fmt("plugin-%s/%s", path.c_str(), name.c_str()));

    path = buff;

    UTIL_ServerPrint("[DEBUG] rf_config(): name = %s, path = %s, current = %s\n", name.c_str(), path.c_str(), buff);

    if (file_exists(path) || params[arg_auto_create]) {

        std::fstream file;
    
        file.open(path);
    
        UTIL_ServerPrint("[DEBUG] rf_config(): is_open = %d\n", file.is_open());

        if (file.is_open()) {
            
            std::string line;

            size_t pos;

            while (std::getline(file, line)) {

                // COMMENTS
                if (line.find(";") == 0 || line.find("#") == 0 || !line.find("//") == 0) continue;

                if ((pos = line.find("=")) != std::string::npos) {

                    // SPLIT VAR
                    std::string var_name = trim_c(line.substr(0, pos));

                    std::string var_value = trim_c(line.substr(pos + 1, line.size() - pos));

                    rm_quote_c(var_value);

                    UTIL_ServerPrint("[DEBUG] rf_config(): name = %s, value = <%s>\n", var_name.c_str(), var_value.c_str());
                }    
            }

            result = TRUE;
        
        } else {

            file << "TEST\n";
        }

        file.close();
    }
    if (!result)

        AMXX_LogError(amx, AMX_ERR_NATIVE, "%s: error opening the file <%s>", __FUNCTION__, path.c_str());

    return result;
}

AMX_NATIVE_INFO Misc_Natives[] = {
    
    { "rf_get_players_num", rf_get_players_num },
    { "rf_get_user_weapons", rf_get_user_weapons },
    { "rf_get_weaponname", rf_get_weaponname },
    { "rf_get_ent_by_class", rf_get_ent_by_class },
    { "rf_roundfloat", rf_roundfloat },
    { "rf_get_user_buyzone", rf_get_user_buyzone },
    { "rf_config", rf_config },
    { nullptr, nullptr }
};

void RegisterNatives_Misc() {

	g_amxxapi.AddNatives(Misc_Natives);
}
