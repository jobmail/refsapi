#pragma once

typedef struct api_cvars_s
{
	cell loaded;
	cell block_status;
	cell mysql_frame_pooling;
	float mysql_frame_rate_k1;
	float mysql_frame_rate_k2;
	cell timer_frame_pooling;
	float timer_frame_rate_k1;
	float timer_frame_rate_k2;
} api_cvars_t;

class CAPI_Config {
public:
	api_cvars_t cvars;
	std::wstring root_path, game_path, log_path, cfg_path;
	CAPI_Config();
	void Init();
	void FailedReGameDllAPI();

	bool hasReHLDS() const { return m_api_rehlds; }
	bool hasReGameDLL() const { return m_api_regame; }

	void make_path(std::wstring &path, std::wstring &name, std::wstring def_path, std::wstring def_name, std::wstring ext);

	void ServerDeactivate() const;

private:
	// to provide API functions
	bool m_api_rehlds;		// some useful functions
	bool m_api_regame;		// some useful functions #2
};

extern CAPI_Config api_cfg;
