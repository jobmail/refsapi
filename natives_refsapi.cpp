#include "precompiled.h"

// native rf_get_players_num(num_unassigned, num_tt, num_ct, num_spectrator, num_dead_tt, num_dead_ct);
cell AMX_NATIVE_CALL rf_get_players_num(AMX *amx, cell *params) {

    int total = 0;

    for (int i = 0; i < sizeof(g_PlayersNum); i++) {

        if (i < 4)

            total += g_PlayersNum[i];

        if (i < PARAMS_COUNT)
        
            *getAmxAddr(amx, params[i + 1]) = g_PlayersNum[i];
    }
    
    return total;
}

// native rf_get_user_weapons(const id, ent[], ent_size);
cell AMX_NATIVE_CALL rf_get_user_weapons(AMX *amx, cell *params) {
    
    enum args_e { arg_count, arg_index, arg_ent_arr, arg_ent_arr_size};

    CHECK_ISPLAYER(arg_index);

    int id = params[arg_index];

    int i = 0;

    std::vector<int> v = g_Tries.player_entities[id];

    int max_size = min((int)v.size(), *getAmxAddr(amx, params[arg_ent_arr_size]));

    for (; i < max_size; i++)

        *(getAmxAddr(amx, params[arg_ent_arr]) + i) = v[i];

    return i;
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

        int max_size = min((int)v.size(), *getAmxAddr(amx, params[arg_ent_arr_size]));

        for (int i; i < max_size; i++) {

            pEdict = INDEXENT(v[i]);

            if (pEdict == nullptr || pEdict->pvPrivateData == nullptr || is_valid && ENTINDEX(pEdict->v.owner) != owner_index) continue;

            // CHECK CREATION CLASSNAME
            if (key != STRING(pEdict->v.classname)) {

                UTIL_ServerPrint("[DEBUG] rf_get_ent_by_class(): STEP_1");

                //acs_trie_transfer(&g_Tries.entities, key, STRING(pEdict->v.classname), v[i]);

                continue;
            }

            *(getAmxAddr(amx, params[arg_ent_arr]) + result) = v[i];

            result++;
        }
    } else {
        // CHECK WEAPON
        if (key.find(WP_CLASS_PREFIX) == 0 && key.length() > sizeof(WP_CLASS_PREFIX)) {

            std::string wp_key = key.substr(sizeof(WP_CLASS_PREFIX));

            if (g_Tries.wp_entities.find(wp_key) != g_Tries.wp_entities.end()) {

                v = g_Tries.wp_entities[wp_key];

                for (const int& i : v) {

                    pEdict = INDEXENT(v[i]);

                    if (pEdict == nullptr || pEdict->pvPrivateData == nullptr || is_valid && ENTINDEX(pEdict->v.owner) != owner_index) continue;

                    // CHECK CREATION CLASSNAME
                    if (key == STRING(pEdict->v.classname)) {

                        UTIL_ServerPrint("[DEBUG] rf_get_ent_by_class(): found entity = %d, classname = <%s> was changed from <%d>", v[i], STRING(pEdict->v.classname), g_Tries.classnames[v[i]]);

                        *(getAmxAddr(amx, params[arg_ent_arr]) + result) = v[i];

                        result++;

                        // TRANSFER CLASSNAME
                        if (key != g_Tries.classnames[v[i]]) {

                            UTIL_ServerPrint("[DEBUG] rf_get_ent_by_class(): STEP_2");
                            
                            //acs_trie_transfer(&g_Tries.entities, g_Tries.classnames[v[i]], key, v[i]);
                        }
                    }
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
