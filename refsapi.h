#pragma once

#define ENUM_COUNT(e)               (sizeof(e) / sizeof(int))

enum RFS_TEAMS {
    TEAM_UNASSIGNED,
    TEAM_TERRORIST,
    TEAM_CT,
    TEAM_SPECTRATOR,
    TEAM_DEAD_TT,
    TEAM_DEAD_CT
};

extern int gi_players_num[ENUM_COUNT(RFS_TEAMS)];

void R_ClientPutInServer_Post(edict_t *pEntity);

