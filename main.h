#pragma once

template <typename T, size_t N>
char(&ArraySizeHelper(T(&array)[N]))[N];
#define arraysize(array) (sizeof(ArraySizeHelper(array)))

extern char g_szMapName[32];
extern playermove_t* g_pMove;
extern int gmsgSendAudio;
extern int gmsgStatusIcon;
extern int gmsgArmorType;
extern int gmsgItemStatus;
extern int gmsgBarTime;
extern int gmsgBarTime2;

extern enginefuncs_t* g_pengfuncsTable;
extern DLL_FUNCTIONS *g_pFunctionTable;

void OnAmxxAttach();
bool OnMetaAttach();
void OnMetaDetach();

void OnFreeEntPrivateData(edict_t *pEdict);
void ServerActivate_Post(edict_t *pEdictList, int edictCount, int clientMax);
void ServerDeactivate_Post();
int DispatchSpawn(edict_t* pEntity);
void ResetGlobalState();
void KeyValue(edict_t *pentKeyvalue, KeyValueData *pkvd);

CGameRules *InstallGameRules(IReGameHook_InstallGameRules *chain);

inline void EMESSAGE_BEGIN(int msg_dest, int msg_type, const float *pOrigin = nullptr, edict_t *ed = nullptr) { (*g_pengfuncsTable->pfnMessageBegin)(msg_dest, msg_type, pOrigin, ed); }
inline void EMESSAGE_END() { (*g_pengfuncsTable->pfnMessageEnd)(); }
inline void EWRITE_BYTE(int iValue) { (*g_pengfuncsTable->pfnWriteByte)(iValue); }
inline void EWRITE_CHAR(int iValue) { (*g_pengfuncsTable->pfnWriteChar)(iValue); }
inline void EWRITE_SHORT(int iValue) { (*g_pengfuncsTable->pfnWriteShort)(iValue); }
inline void EWRITE_LONG(int iValue) { (*g_pengfuncsTable->pfnWriteLong)(iValue); }
inline void EWRITE_ANGLE(float flValue) { (*g_pengfuncsTable->pfnWriteAngle)(flValue); }
inline void EWRITE_COORD(float flValue) { (*g_pengfuncsTable->pfnWriteCoord)(flValue); }
inline void EWRITE_STRING(const char *sz) { (*g_pengfuncsTable->pfnWriteString)(sz); }
inline void EWRITE_ENTITY(int iValue) { (*g_pengfuncsTable->pfnWriteEntity)(iValue); }

void UTIL_ServerPrint(const char *fmt, ...);
extern void NORETURN UTIL_SysError(const char *fmt, ...);

enum 

extern int g_players_num[];
