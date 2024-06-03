#include "precompiled.h"

// native rf_get_players_num(nums[], nums_size, bool:teams_only);
cell AMX_NATIVE_CALL rf_get_players_num(AMX *amx, cell *params) {

    enum args_e { arg_count, arg_nums_arr, arg_nums_arr_size, arg_teams_only};

    int max_size = *getAmxAddr(amx, params[arg_nums_arr_size]);

    if (max_size > 0)
    
        Q_memcpy(getAmxAddr(amx, params[arg_nums_arr]), &g_PlayersNum, min(max_size, C_COUNT(g_PlayersNum)));
    
    int total = g_PlayersNum[TEAM_TERRORIST] + g_PlayersNum[TEAM_CT];

    return params[arg_teams_only] ? total : total + g_PlayersNum[TEAM_UNASSIGNED] + g_PlayersNum[TEAM_SPECTRATOR];
}

// native rf_get_user_weapons(const id, ent[], ent_size);
cell AMX_NATIVE_CALL rf_get_user_weapons(AMX *amx, cell *params) {
    
    enum args_e { arg_count, arg_index, arg_ent_arr, arg_ent_arr_size};

    if (params[arg_ent_arr_size] < 1) return 0;

    CHECK_ISPLAYER(arg_index);

    std::vector<int> v = g_Tries.player_entities[params[arg_index]];

    int max_size = min((int)v.size(), *getAmxAddr(amx, params[arg_ent_arr_size]));

    Q_memcpy(getAmxAddr(amx, params[arg_ent_arr]), v.data(), max_size);

    return max_size;
}

// native rf_get_weaponname(const ent, name[], name_len);
cell AMX_NATIVE_CALL rf_get_weaponname(AMX *amx, cell *params) {

    enum args_e { arg_count, arg_entity, arg_name, arg_name_len};

    CHECK_ISENTITY(arg_entity);

    edict_t* pEdict = INDEXENT(params[arg_entity]);

    if (!(pEdict == nullptr || pEdict->pvPrivateData == nullptr)) {

        //UTIL_ServerPrint("[DEBUG] found: ent = %d, classname = %s\n", params[arg_entity], STRING(pEdict->v.classname));

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

    std::vector<int> v;

    std::vector<int>::iterator it_value;

    char classname[256];

    std::string key = getAmxString(amx, params[arg_classname], classname);

    bool is_valid = is_valid_index(owner_index);

    if (g_Tries.entities.find(key) != g_Tries.entities.end()) {

        v = g_Tries.entities[key];

        for (int i = 0; i < v.size(); i++) {

            pEdict = INDEXENT(v[i]);

            if (pEdict == nullptr || pEdict->pvPrivateData == nullptr || is_valid && ENTINDEX(pEdict->v.owner) != owner_index) continue;

            // CHECK CREATION CLASSNAME
            if (key != STRING(pEdict->v.classname)) {

                //g_amxxapi.PrintSrvConsole("[DEBUG] rf_get_ent_by_class(): STEP_1");

                acs_trie_transfer(&g_Tries.entities, key, STRING(pEdict->v.classname), v[i]);

                continue;
            }

            *(getAmxAddr(amx, params[arg_ent_arr]) + result) = v[i];

            if (++result >= params[arg_ent_arr_size]) break;
        }

    } else {

        // CHECK WEAPON
        if (key.find(WP_CLASS_PREFIX) == 0 && key.length() > sizeof(WP_CLASS_PREFIX)) {

            v = g_Tries.wp_entities;

            for (const int& it : v) {

                pEdict = INDEXENT(it);

                if (pEdict == nullptr || pEdict->pvPrivateData == nullptr || is_valid && ENTINDEX(pEdict->v.owner) != owner_index) continue;

                // CHECK CREATION CLASSNAME
                if (key == STRING(pEdict->v.classname)) {

                    //g_amxxapi.PrintSrvConsole("[DEBUG] rf_get_ent_by_class(): found entity = %d, classname = <%s> was changed from <%d>", it, STRING(pEdict->v.classname), g_Tries.classnames[it]);

                    *(getAmxAddr(amx, params[arg_ent_arr]) + result) = it;

                    // TRANSFER CLASSNAME
                    if (key != g_Tries.classnames[it]) {

                        //g_amxxapi.PrintSrvConsole("[DEBUG] rf_get_ent_by_class(): STEP_2");
                        
                        acs_trie_transfer(&g_Tries.entities, g_Tries.classnames[it], key, it);
                    }

                    if (++result >= params[arg_ent_arr_size]) break;
                }
            }
        }
    }

    return result;
}

AMX_NATIVE_INFO Misc_Natives[] = {
    
    { "rf_get_players_num", rf_get_players_num },
    { "rf_get_user_weapons", rf_get_user_weapons },
    { "rf_get_weaponname", rf_get_weaponname },
    { "rf_get_ent_by_class", rf_get_ent_by_class },
    { nullptr, nullptr }
};

void RegisterNatives_Misc() {

	g_amxxapi.AddNatives(Misc_Natives);
}
