#pragma once

#define MAX_HOOK_FORWARDS		1024
#define MAX_REGION_RANGE		1024

#define BEGIN_FUNC_REGION(x)		(MAX_REGION_RANGE * hooklist_t::hooks_tables_e::ht_##x)

typedef bool (*reqfunc_t) ();
typedef int  (*regfunc_t) (AMX *, const char *);
typedef void (*regchain_t)();

struct hook_t
{
	std::vector<class CAmxxHookBase *> pre;   // pre forwards
	std::vector<class CAmxxHookBase *> post;  // post forwards

	const char *func_name;                  // function name
	const char *depend_name;                // platform dependency

	reqfunc_t checkRequirements;
	regfunc_t registerForward;              // AMXX forward registration function
	regchain_t registerHookchain;           // register re* API hook
	regchain_t unregisterHookchain;         // unregister re* API hook

	void clear();

	bool wasCalled;
};

extern hook_t hooklist_engine[];
extern hook_t hooklist_gamedll[];
extern hook_t hooklist_animating[];
extern hook_t hooklist_player[];
extern hook_t hooklist_gamerules[];
extern hook_t hooklist_grenade[];
extern hook_t hooklist_weaponbox[];
extern hook_t hooklist_weapon[];
extern hook_t hooklist_gib[];
extern hook_t hooklist_cbaseentity[];
extern hook_t hooklist_botmanager[];

enum
{
	INVALID_HOOKCHAIN = 0
};

struct hooklist_t
{
	hook_t *operator[](size_t hook) const
	{
		#define CASE(h)	case ht_##h: return &hooklist_##h[index];

		const auto table = hooks_tables_e(hook / MAX_REGION_RANGE);
		const auto index = hook & (MAX_REGION_RANGE - 1);

		switch (table) {
			CASE(engine)
			CASE(gamedll)
			CASE(animating)
			CASE(player)
			CASE(gamerules)
			CASE(grenade)
			CASE(weaponbox)
			CASE(weapon)
			CASE(gib)
			CASE(cbaseentity)
			CASE(botmanager)
		}

		#undef CASE

		return nullptr;
	}

	static hook_t *getHookSafe(size_t hook);
	static void clear();

	enum hooks_tables_e
	{
		ht_engine,
		ht_gamedll,
		ht_animating,
		ht_player,
		ht_gamerules,
		ht_grenade,
		ht_weaponbox,
		ht_weapon,
		ht_gib,
		ht_cbaseentity,
		ht_botmanager,
	};
};

enum EngineFunc
{
	RFH_SV_StartSound = BEGIN_FUNC_REGION(engine),
	RFH_SV_DropClient,
	RFH_SV_ActivateServer,
	RFH_Cvar_DirectSet,
	RFH_SV_WriteFullClientUpdate,
	RFH_GetEntityInit,
	RFH_ClientConnected,
	RFH_SV_ConnectClient,
	RFH_SV_EmitPings,
	RFH_ED_Alloc,
	RFH_ED_Free,
	RFH_Con_Printf,
	RFH_SV_CheckUserInfo,
	RFH_PF_precache_generic_I,
	RFH_PF_precache_model_I,
	RFH_PF_precache_sound_I,
	RFH_EV_Precache,
	RFH_SV_AddResource,
	RFH_SV_ClientPrintf,
	RFH_SV_AllowPhysent,
	RFH_ExecuteServerStringCmd,

	// [...]
};

enum GamedllFunc
{
	RFG_GetForceCamera = BEGIN_FUNC_REGION(gamedll),
	RFG_PlayerBlind,
	RFG_RadiusFlash_TraceLine,
	RFG_RoundEnd,

	RFG_PM_Move,
	RFG_PM_AirMove,
	RFG_HandleMenu_ChooseAppearance,
	RFG_HandleMenu_ChooseTeam,
	RFG_ShowMenu,
	RFG_ShowVGUIMenu,

	RFG_BuyGunAmmo,
	RFG_BuyWeaponByWeaponID,

	RFG_ThrowHeGrenade,
	RFG_ThrowFlashbang,
	RFG_ThrowSmokeGrenade,
	RFG_PlantBomb,
	RFG_IsPenetrableEntity,

	RFG_SpawnHeadGib,
	RFG_SpawnRandomGibs,

	RFG_CreateWeaponBox,
	RFG_PM_LadderMove,
	RFG_PM_WaterJump,
	RFG_PM_CheckWaterJump,
	RFG_PM_Jump,
	RFG_PM_Duck,
	RFG_PM_UnDuck,
	RFG_PM_PlayStepSound,
	RFG_PM_AirAccelerate,
	RFG_ClearMultiDamage,
	RFG_AddMultiDamage,
	RFG_ApplyMultiDamage,
	RFG_BuyItem,

	// [...]
};

enum GamedllFunc_CBaseAnimating
{
	RFG_CBaseAnimating_ResetSequenceInfo = BEGIN_FUNC_REGION(animating),

	// [...]
};

enum GamedllFunc_CBasePlayer
{
	// CBasePlayer virtual
	RFG_CBasePlayer_Spawn = BEGIN_FUNC_REGION(player),
	RFG_CBasePlayer_Precache,
	RFG_CBasePlayer_ObjectCaps,
	RFG_CBasePlayer_Classify,
	RFG_CBasePlayer_TraceAttack,
	RFG_CBasePlayer_TakeDamage,
	RFG_CBasePlayer_TakeHealth,
	RFG_CBasePlayer_Killed,
	RFG_CBasePlayer_AddPoints,
	RFG_CBasePlayer_AddPointsToTeam,
	RFG_CBasePlayer_AddPlayerItem,
	RFG_CBasePlayer_RemovePlayerItem,
	RFG_CBasePlayer_GiveAmmo,
	RFG_CBasePlayer_ResetMaxSpeed,
	RFG_CBasePlayer_Jump,
	RFG_CBasePlayer_Duck,
	RFG_CBasePlayer_PreThink,
	RFG_CBasePlayer_PostThink,
	RFG_CBasePlayer_UpdateClientData,
	RFG_CBasePlayer_ImpulseCommands,
	RFG_CBasePlayer_RoundRespawn,
	RFG_CBasePlayer_Blind,

	RFG_CBasePlayer_SetClientUserInfoModel,
	RFG_CBasePlayer_SetClientUserInfoName,
	RFG_CBasePlayer_Observer_IsValidTarget,
	RFG_CBasePlayer_SetAnimation,
	RFG_CBasePlayer_GiveDefaultItems,
	RFG_CBasePlayer_GiveNamedItem,
	RFG_CBasePlayer_AddAccount,
	RFG_CBasePlayer_GiveShield,
	RFG_CBasePlayer_DropPlayerItem,
	RFG_CBasePlayer_HasRestrictItem,

	RFG_CBasePlayer_DropShield,
	RFG_CBasePlayer_OnSpawnEquip,
	RFG_CBasePlayer_Radio,
	RFG_CBasePlayer_Disappear,
	RFG_CBasePlayer_MakeVIP,
	RFG_CBasePlayer_MakeBomber,
	RFG_CBasePlayer_StartObserver,
	RFG_CBasePlayer_GetIntoGame,
	RFG_CBasePlayer_StartDeathCam,
	RFG_CBasePlayer_SwitchTeam,
	RFG_CBasePlayer_CanSwitchTeam,
	RFG_CBasePlayer_ThrowGrenade,
	RFG_CBasePlayer_SetSpawnProtection,
	RFG_CBasePlayer_RemoveSpawnProtection,
	RFG_CBasePlayer_HintMessageEx,
	RFG_CBasePlayer_UseEmpty,
	RFG_CBasePlayer_DropIdlePlayer,

	RFG_CBasePlayer_Observer_SetMode,
	RFG_CBasePlayer_Observer_FindNextPlayer,

	RFG_CBasePlayer_Pain,
	RFG_CBasePlayer_DeathSound,
	RFG_CBasePlayer_JoiningThink,

	RFG_CBasePlayer_CheckTimeBasedDamage,
	RFG_CBasePlayer_EntSelectSpawnPoint,

	// [...]
};

enum GamedllFunc_CGrenade
{
	RFG_CGrenade_DefuseBombStart = BEGIN_FUNC_REGION(grenade),
	RFG_CGrenade_DefuseBombEnd,
	RFG_CGrenade_ExplodeHeGrenade,
	RFG_CGrenade_ExplodeFlashbang,
	RFG_CGrenade_ExplodeSmokeGrenade,
	RFG_CGrenade_ExplodeBomb,

	// [...]
};

enum GamedllFunc_CWeaponBox
{
	RFG_CWeaponBox_SetModel = BEGIN_FUNC_REGION(weaponbox),

	// [...]
};

enum GamedllFunc_CBasePlayerWeapon
{
	RFG_CBasePlayerWeapon_CanDeploy = BEGIN_FUNC_REGION(weapon),
	RFG_CBasePlayerWeapon_DefaultDeploy,
	RFG_CBasePlayerWeapon_DefaultReload,
	RFG_CBasePlayerWeapon_DefaultShotgunReload,
	RFG_CBasePlayerWeapon_ItemPostFrame,
	RFG_CBasePlayerWeapon_KickBack,
	RFG_CBasePlayerWeapon_SendWeaponAnim,

	// [...]
};

enum GamedllFunc_CSGameRules
{
	// CSGameRules virtual
	RFG_CSGameRules_FShouldSwitchWeapon = BEGIN_FUNC_REGION(gamerules),
	RFG_CSGameRules_GetNextBestWeapon,
	RFG_CSGameRules_FlPlayerFallDamage,
	RFG_CSGameRules_FPlayerCanTakeDamage,
	RFG_CSGameRules_PlayerSpawn,
	RFG_CSGameRules_FPlayerCanRespawn,
	RFG_CSGameRules_GetPlayerSpawnSpot,
	RFG_CSGameRules_ClientUserInfoChanged,
	RFG_CSGameRules_PlayerKilled,
	RFG_CSGameRules_DeathNotice,
	RFG_CSGameRules_CanHavePlayerItem,
	RFG_CSGameRules_DeadPlayerWeapons,
	RFG_CSGameRules_ServerDeactivate,
	RFG_CSGameRules_CheckMapConditions,
	RFG_CSGameRules_CleanUpMap,
	RFG_CSGameRules_RestartRound,
	RFG_CSGameRules_CheckWinConditions,
	RFG_CSGameRules_RemoveGuns,
	RFG_CSGameRules_GiveC4,
	RFG_CSGameRules_ChangeLevel,
	RFG_CSGameRules_GoToIntermission,
	RFG_CSGameRules_BalanceTeams,
	RFG_CSGameRules_OnRoundFreezeEnd,
	RFG_CSGameRules_CanPlayerHearPlayer,
	RFG_CSGameRules_Think,
	RFG_CSGameRules_TeamFull,
	RFG_CSGameRules_TeamStacked,
	RFG_CSGameRules_PlayerGotWeapon,

	// [...]
};

enum GamedllFunc_CGib
{
	RFG_CGib_Spawn = BEGIN_FUNC_REGION(gib),
	RFG_CGib_BounceGibTouch,
	RFG_CGib_WaitTillLand,

	// [...]
};

enum GamedllFunc_CBaseEntity
{
	RFG_CBaseEntity_FireBullets = BEGIN_FUNC_REGION(cbaseentity),
	RFG_CBaseEntity_FireBuckshots,
	RFG_CBaseEntity_FireBullets3,

	// [...]
};

enum GamedllFunc_CBotManager
{
	RFG_CBotManager_OnEvent = BEGIN_FUNC_REGION(botmanager),

	// [...]
};
