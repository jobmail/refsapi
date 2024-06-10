#include "precompiled.h"

// native rf_get_players_num(nums[], nums_size, bool:teams_only);
cell AMX_NATIVE_CALL rf_get_players_num(AMX *amx, cell *params) {

    enum args_e { arg_count, arg_nums_arr, arg_nums_arr_size, arg_teams_only};

    size_t max_size = *getAmxAddr(amx, params[arg_nums_arr_size]);

    if (max_size > 0) 
    
        Q_memcpy(getAmxAddr(amx, params[arg_nums_arr]), &g_PlayersNum, min(max_size, _COUNT(g_PlayersNum)) << 2);
    
    int total = g_PlayersNum[TEAM_TERRORIST] + g_PlayersNum[TEAM_CT];

    return params[arg_teams_only] ? total : total + g_PlayersNum[TEAM_UNASSIGNED] + g_PlayersNum[TEAM_SPECTRATOR];
}

// native rf_get_user_weapons(const id, ent[], ent_size);
cell AMX_NATIVE_CALL rf_get_user_weapons(AMX *amx, cell *params) {
    
    enum args_e { arg_count, arg_index, arg_ent_arr, arg_ent_arr_size};

    if (params[arg_ent_arr_size] < 1) return 0;

    CHECK_ISPLAYER(arg_index);

    std::vector<cell> v = g_Tries.player_entities[params[arg_index]];

    size_t max_size = min(v.size(), (size_t)*getAmxAddr(amx, params[arg_ent_arr_size]));

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

    return acs_roundfloat(static_cast<float>(params[arg_value]), params[arg_precision]);
}

// native rf_get_user_buyzone(const id);
cell AMX_NATIVE_CALL rf_get_user_buyzone(AMX *amx, cell *params) {

    enum args_e { arg_count, arg_index};

    CHECK_ISPLAYER(arg_index);

    return (qboolean)acs_get_user_buyzone(INDEXENT(params[arg_index])); 
}


AMX_NATIVE_INFO Misc_Natives[] = {
    
    { "rf_get_players_num", rf_get_players_num },
    { "rf_get_user_weapons", rf_get_user_weapons },
    { "rf_get_weaponname", rf_get_weaponname },
    { "rf_get_ent_by_class", rf_get_ent_by_class },
    { "rf_roundfloat", rf_roundfloat },
    { "rf_get_user_buyzone", rf_get_user_buyzone },
    { nullptr, nullptr }
};

void RegisterNatives_Misc() {

	g_amxxapi.AddNatives(Misc_Natives);
}
