#include "precompiled.h"

// native rf_get_players_num(num_unassigned, num_tt, num_ct, num_spectrator, num_dead_tt, num_dead_ct);
cell AMX_NATIVE_CALL rf_get_players_num(AMX *amx, cell *params) {

    int total = 0;

    for (int i = 0; i < sizeof(g_PlayersNum); i++) {

        if (i < 5)

            total += g_PlayersNum[i];

        if (i < PARAMS_COUNT)
        
            *getAmxAddr(amx, params[i + 1]) = g_PlayersNum[i];
    }
    
    return total;
}

AMX_NATIVE_INFO Misc_Natives[] = {
    
    { "rf_get_players_num", rf_get_players_num },
    { nullptr, nullptr }
};

void RegisterNatives_Misc() {

	g_amxxapi.AddNatives(Misc_Natives);
}
