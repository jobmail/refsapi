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

    //UTIL_ServerPrint("[DEBUG] rf_get_user_weapons(): max_size = %d\n", max_size);

    for (; i < max_size; i++)

        *(getAmxAddr(amx, params[arg_ent_arr]) + i) = v[i];

    return i;
}

// native rf_get_weaponname(const ent, name[], name_len);
cell AMX_NATIVE_CALL rf_get_weaponname(AMX *amx, cell *params) {

    enum args_e { arg_count, arg_entity, arg_name, arg_name_len};

    CHECK_ISENTITY(arg_entity);

    int entity_index = params[arg_entity];

    edict_t* pEdict = INDEXENT(entity_index);

    if (!(pEdict == nullptr || pEdict->pvPrivateData == nullptr)) {

        Q_strcpy_s((char*)getAmxAddr(amx, params[arg_name]), (char*)STRING(pEdict->v.classname));

        return TRUE;
    }

    return FALSE;
}

// native rf_get_ent_by_class(const classname[], const id, ent[], ent_size);
cell AMX_NATIVE_CALL rf_get_ent_by_class(AMX *amx, cell *params) {

    enum args_e { arg_count, arg_classname, arg_owner, arg_ent_arr, arg_ent_arr_size};

    int owner_index = params[arg_owner];

    int result = 0;

    char* key = (char*)getAmxAddr(amx, params[arg_classname]);

    if (g_Tries.entities.find(key) != g_Tries.entities.end()) {

        std::vector<int> v = g_Tries.entities[key];

        int max_size = min((int)v.size(), *getAmxAddr(amx, params[arg_ent_arr_size]));

        for (int i; i < max_size; i++) {

            if (is_valid_index(owner_index)) {

                edict_t* pEdict = INDEXENT(v[i]);

                if (!(pEdict == nullptr || pEdict->pvPrivateData == nullptr) && ENTINDEX(pEdict->v.owner) != owner_index)

                    continue;
            }

            *(getAmxAddr(amx, params[arg_ent_arr]) + i) = v[i];

            result++;
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
