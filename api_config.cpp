#include "precompiled.h"

CAPI_Config api_cfg;

CAPI_Config::CAPI_Config() : m_api_rehlds(false), m_api_regame(false)
{
}

void CAPI_Config::FailedReGameDllAPI()
{
	m_api_regame = false;
}

void CAPI_Config::Init()
{
	m_api_rehlds = RehldsApi_Init();
	m_api_regame = RegamedllApi_Init();

	if (m_api_rehlds)
	{
		g_engfuncs.pfnServerPrint("[REFSAPI] ReHLDS API successfully initialized.\n");
		g_RehldsHookchains->Cvar_DirectSet()->registerHook(RF_Cvar_DirectSet_RH);
		g_RehldsHookchains->SV_CheckUserInfo()->registerHook(RF_CheckUserInfo_RH, HC_PRIORITY_UNINTERRUPTABLE);
	}

	if (m_api_regame)
	{
		g_engfuncs.pfnServerPrint("[REFSAPI] ReGAME API successfully initialized.\n");
		g_ReGameHookchains->InstallGameRules()->registerHook(&InstallGameRules);
		g_ReGameHookchains->CSGameRules_ClientUserInfoChanged()->registerHook(CSGameRules_ClientUserInfoChanged_RG, HC_PRIORITY_UNINTERRUPTABLE);
	}

	root_path = std::filesystem::current_path().wstring();
	trim(root_path);
	
	game_path = stows(g_amxxapi.GetModname());
	if (game_path.empty())
		game_path = L"cstrike";

	log_path = stows(LOCALINFO("amxx_logs"));
	if (log_path.empty())
		log_path = L"addons/amxmodx/logs";
	trim(log_path);
	rtrim(log_path, L" /");

	cfg_path = stows(LOCALINFO("amxx_configsdir"));
	if (cfg_path.empty())
		cfg_path = L"addons/amxmodx/logs";
	trim(cfg_path);
	rtrim(cfg_path, L" /");

	// DEBUG("%s() root = %s, game = %s, log_path = %s, cfg_path = %s", __func__, wstos(root_path).c_str(), wstos(game_path).c_str(), wstos(log_path).c_str(), wstos(cfg_path).c_str());

	const int cvar_flags = FCVAR_PROTECTED | FCVAR_SERVER | FCVAR_SPONLY | FCVAR_UNLOGGED;

	g_cvar_mngr.bind(
		CVAR_TYPE_NUM,
		g_cvar_mngr.add(-1,
			L"acs_refsapi_block_status", L"1", cvar_flags,
			L"Блокировка команды status:\n0 - выкл.\n1 - вкл.",
			true, 0.0f, true, 1.0f
		),
		_SS(cvars.block_status)
	);

	g_cvar_mngr.bind(
		CVAR_TYPE_NUM,
		g_cvar_mngr.add(-1,
			L"acs_refsapi_mysql_frame_pooling", L"1000", cvar_flags,
			L"Интервал расчета frame_rate для mysql",
			true, 100.0f, true, 1000.0f
		),
		_SS(cvars.mysql_frame_pooling)
	);
	g_cvar_mngr.bind(
		CVAR_TYPE_FLT,
		g_cvar_mngr.add(-1,
			L"acs_refsapi_mysql_frame_rate_k1", L"53.0", cvar_flags,
			L"Коэффициент k1 расчета frame_rate для mysql",
			true, 1.0f, true, 100.0f
		),
		_SS(cvars.mysql_frame_rate_k1)
	);
	g_cvar_mngr.bind(
		CVAR_TYPE_FLT,
		g_cvar_mngr.add(-1,
			L"acs_refsapi_mysql_frame_rate_k2", L"7.0", cvar_flags,
			L"Коэффициент k2 расчета frame_rate для mysql",
			true, 0.1f, true, 10.0f
		),
		_SS(cvars.mysql_frame_rate_k2)
	);

	g_cvar_mngr.bind(
		CVAR_TYPE_NUM,
		g_cvar_mngr.add(-1,
			L"acs_refsapi_timer_frame_pooling", L"1000", cvar_flags,
			L"Интервал расчета frame_rate для timer",
			true, 100.0f, true, 1000.0f
		),
		_SS(cvars.timer_frame_pooling)
	);
	g_cvar_mngr.bind(
		CVAR_TYPE_FLT,
		g_cvar_mngr.add(-1,
			L"acs_refsapi_timer_frame_rate_k1", L"23.0", cvar_flags,
			L"Коэффициент k1 расчета frame_rate для timer",
			true, 1.0f, true, 100.0f
		),
		_SS(cvars.timer_frame_rate_k1)
	);
	g_cvar_mngr.bind(
		CVAR_TYPE_FLT,
		g_cvar_mngr.add(-1,
			L"acs_refsapi_timer_frame_rate_k2", L"3.0", cvar_flags,
			L"Коэффициент k2 расчета frame_rate для timer",
			true, 0.1f, true, 10.0f
		),
		_SS(cvars.timer_frame_rate_k2)
	);

	g_cvar_mngr.bind(
		CVAR_TYPE_NUM,
		g_cvar_mngr.add(-1,
			REFSAPI_CVAR, L"1", cvar_flags,
			L"",
			true, 0.0f, true, 1.0f
		),
		_SS(cvars.loaded)
	);

	g_cvar_mngr.config(
		-1,
		true,
		L"addons/amxmodx/configs",
		L"refsapi",
		Plugin_info.logtag,
		Plugin_info.author,
		Plugin_info.name,
		Plugin_info.version
	);

#ifndef WITHOUT_LOG
	g_log_mngr.start_main();
#endif

#ifndef WITHOUT_SQL
	g_mysql_mngr.start_main();
#endif

}

void CAPI_Config::make_path(std::wstring &path, std::wstring &name, std::wstring def_path, std::wstring def_name, std::wstring ext)
{
	path = root_path + L'/' + game_path + L'/' + (path.empty() ? def_path : path);
	remove_chars(path, _BAD_PATH_CHARS_L);
	rtrim(path, L" /");
	if (!dir_exists(path) && std::filesystem::create_directories(path))
		DEBUG("%s(): created path = %s", __func__, wstos(path).c_str());
	else
		DEBUG("%s(): path = %s", __func__, wstos(path).c_str());
	if (name.empty())
		name = def_name;
	size_t pos = name.find(L".amxx");
	if (pos != std::wstring::npos)
		name.replace(pos, sizeof(L".amxx") - 1, L"");
	remove_chars(name, _BAD_PATH_CHARS_L);
	name += ext;
}

void CAPI_Config::ServerDeactivate() const
{
	if (m_api_regame)
		g_pGameRules = nullptr;
}