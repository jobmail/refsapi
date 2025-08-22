#include <precompiled.h>

meta_globals_t *gpMetaGlobals;
gamedll_funcs_t *gpGamedllFuncs;
mutil_funcs_t *gpMetaUtilFuncs;
enginefuncs_t *g_pengfuncsTable;

plugin_info_t Plugin_info =
	{
		META_INTERFACE_VERSION,				  // ifvers
		"RefsAPI",							  // name
		"1.0.7.6",							  // version
		__DATE__,							  // date
		"Refresh",							  // author
		"https://github.com/jobmail/refsapi", // url
		"REFSAPI",							  // logtag
		PT_ANYTIME,							  // (when) loadable
		PT_NEVER,							  // (when) unloadable
};

C_DLLEXPORT int Meta_Query(char *interfaceVersion, plugin_info_t **plinfo, mutil_funcs_t *pMetaUtilFuncs)
{
	*plinfo = &Plugin_info;
	gpMetaUtilFuncs = pMetaUtilFuncs;
	return true;
}

META_FUNCTIONS gMetaFunctionTable =
	{
		NULL,					 // pfnGetEntityAPI		HL SDK; called before game DLL
		NULL,					 // pfnGetEntityAPI_Post		META; called after game DLL
		GetEntityAPI2,			 // pfnGetEntityAPI2		HL SDK2; called before game DLL
		GetEntityAPI2_Post,		 // pfnGetEntityAPI2_Post	META; called after game DLL
		GetNewDLLFunctions,		 // pfnGetNewDLLFunctions	HL SDK2; called before game DLL
		GetNewDLLFunctions_Post, // pfnGetNewDLLFunctions_Post	META; called after game DLL
		GetEngineFunctions,		 // pfnGetEngineFunctions	META; called before HL engine
		GetEngineFunctions_Post, // pfnGetEngineFunctions_Post	META; called after HL engine
};

C_DLLEXPORT int Meta_Attach(PLUG_LOADTIME now, META_FUNCTIONS *pFunctionTable, meta_globals_t *pMGlobals, gamedll_funcs_t *pGamedllFuncs)
{
	gpMetaGlobals = pMGlobals;
	gpGamedllFuncs = pGamedllFuncs;

	if (!OnMetaAttach())
		return false;

	DEBUG("*** Hello World! ***");
	DEBUG("mysql_thread_safe = %d", mysql_thread_safe());
	const char test_utf8[] = { '\xFF', '\xFF', '\xFF', 20, 0 };
#ifdef __SSE4_2__
	DEBUG("Check is_valid_utf8(%s) = %d", "", g_SSE4_ENABLE ? is_valid_utf8_simd("") : is_valid_utf8(""));
	DEBUG("Check is_valid_utf8(%s) = %d", "ʰᵒᵐᵉʳ", is_valid_utf8_simd("ʰᵒᵐᵉʳ"));
	DEBUG("Check is_valid_utf8(%s) = %d", "test", g_SSE4_ENABLE ? is_valid_utf8_simd("test") : is_valid_utf8("test"));
	DEBUG("Check is_valid_utf8(%s) = %d", "\xC3\x28", g_SSE4_ENABLE ? is_valid_utf8_simd("\xC3\x28") : is_valid_utf8("\xC3\x28"));
	DEBUG("Check is_valid_utf8(%s) = %d", "\xED\xA0\x80", g_SSE4_ENABLE ? is_valid_utf8_simd("\xED\xA0\x80") : is_valid_utf8("\xED\xA0\x80"));
	DEBUG("Check is_valid_utf8(%s) = %d\n", "\xFF\xFF\xFF\x20\x00", g_SSE4_ENABLE ? is_valid_utf8_simd("\xFF\xFF\xFF\x20\x00") : is_valid_utf8("\xFF\xFF\xFF\x20\x00"));
#else
	DEBUG("Check is_valid_utf8(%s) = %d", "", is_valid_utf8(""));
	DEBUG("Check is_valid_utf8(%s) = %d", "ʰᵒᵐᵉʳ", is_valid_utf8("ʰᵒᵐᵉʳ"));
	DEBUG("Check is_valid_utf8(%s) = %d", "test", is_valid_utf8("test"));
	DEBUG("Check is_valid_utf8(%s) = %d", "\xC3\x28", is_valid_utf8("\xC3\x28"));
	DEBUG("Check is_valid_utf8(%s) = %d", "\xED\xA0\x80", is_valid_utf8("\xED\xA0\x80"));
	DEBUG("Check is_valid_utf8(%s) = %d\n", "\xFF\xFF\xFF\x20\x00", is_valid_utf8("\xFF\xFF\xFF\x20\x00"));
#endif
	DEBUG("Similar test: %s / %s = %f", "Refresh", "Рефрешь", similarity_score(L"Refresh", L"Рефрешь"));
	DEBUG("Similar test: %s / %s = %f", "Refresh", "Reefrеsh", similarity_score(L"Refresh", L"Reefrеsh"));
	DEBUG("Similar test: %s / %s = %f", "Refresh", "Peеflешь", similarity_score(L"Refresh", L"Peеflешь"));

	DEBUG("Similar test: %s / %s = %f", "Refresh", "NeRefresh", similarity_score(L"Refresh", L"NeRefresh"));
	DEBUG("Similar test: %s / %s = %f", "Refresh", "Reresh", similarity_score(L"Refresh", L"Reresh"));

	DEBUG("Similar test: %s / %s = %f", "Liverpool.", "Liverpool     '>>", similarity_score(L"Liverpool", L"Liverpool     '>>"));

	DEBUG("Similar test: %s / %s = %f", "dusst", "dust2", similarity_score(L"dusst", L"dust2"));
	DEBUG("Similar test: %s / %s = %f", "dust2", "aztec", similarity_score(L"dust2", L"aztec"));
	DEBUG("Similar test: %s / %s = %f", "berserker", "inferno", similarity_score(L"berserker", L"inferno"));
	DEBUG("Similar test: %s / %s = %f", "barselona", "barcelona", similarity_score(L"barselona", L"barcelona"));


	DEBUG("Similar test: %s / %s = %f", "Oki", "Okkki", similarity_score(L"Oki", L"Okkki"));
	DEBUG("Similar test: %s / %s = %f", "Okidokki", "Okkkidooookkkii", similarity_score(L"Okidokki", L"Okkkidooookkkii"));

	DEBUG("Similar test: %s / %s = %f", "M4LVIN4", "Malvina", similarity_score(L"M4LVIN4", L"Malvina"));
	DEBUG("Similar test: %s / %s = %f", "M4LVIN4", "~~~MalВina~~~", similarity_score(L"M4LVIN4", L"~~~MalВina~~~"));
	DEBUG("Similar test: %s / %s = %f", "M4LVIN4", "МАЛЬВИНА", similarity_score(L"M4LVIN4", L"МАЛЬВИНА"));
	DEBUG("Similar test: %s / %s = %f", "M4LVIN4", "БУРАТИНА", similarity_score(L"M4LVIN4", L"БУРАТИНА"));
	DEBUG("Similar test: %s / %s = %f", "M4LVIN4", "B4RVINa", similarity_score(L"M4LVIN4", L"B4RVINa"));
	DEBUG("Similar test: %s / %s = %f", "M4LVIN4", "S4BRIN4", similarity_score(L"M4LVIN4", L"S4BRIN4"));

	cpu_test();
	get_thread_info(true);

	GET_HOOK_TABLES(PLID, &g_pengfuncsTable, nullptr, nullptr);
	memcpy(pFunctionTable, &gMetaFunctionTable, sizeof(META_FUNCTIONS));

	return true;
}

C_DLLEXPORT int Meta_Detach(PLUG_LOADTIME now, PL_UNLOAD_REASON reason)
{
	OnMetaDetach();
	return true;
}
