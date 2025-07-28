#include <precompiled.h>

DLL_FUNCTIONS *g_pFunctionTable, *g_pFunctionTable_Post;
DLL_FUNCTIONS g_FunctionTable =
	{
		NULL,				 // pfnGameInit
		&DispatchSpawn,		 // pfnSpawn
		NULL,				 // pfnThink
		NULL,				 // pfnUse
		NULL,				 // pfnTouch
		NULL,				 // pfnBlocked
		&KeyValue,			 // pfnKeyValue
		NULL,				 // pfnSave
		NULL,				 // pfnRestore
		NULL,				 // pfnSetAbsBox
		NULL,				 // pfnSaveWriteFields
		NULL,				 // pfnSaveReadFields
		NULL,				 // pfnSaveGlobalState
		NULL,				 // pfnRestoreGlobalState
		&ResetGlobalState,	 // pfnResetGlobalState
		NULL,				 // pfnClientConnect
		&R_ClientDisconnect, // pfnClientDisconnect
		NULL,				 // pfnClientKill
		NULL,				 // pfnClientPutInServer
		NULL,				 // pfnClientCommand
		NULL,				 // pfnClientUserInfoChanged
		NULL,				 // pfnServerActivate
		NULL,				 // pfnServerDeactivate
		NULL,				 // pfnPlayerPreThink
		NULL,				 // pfnPlayerPostThink
		NULL,	 			 // pfnStartFrame
		NULL,				 // pfnParmsNewLevel
		NULL,				 // pfnParmsChangeLevel
		NULL,				 // pfnGetGameDescription
		NULL,				 // pfnPlayerCustomization
		NULL,				 // pfnSpectatorConnect
		NULL,				 // pfnSpectatorDisconnect
		NULL,				 // pfnSpectatorThink
		NULL,				 // pfnSys_Error
		NULL,				 // pfnPM_Move
		NULL,				 // pfnPM_Init
		NULL,				 // pfnPM_FindTextureType
		NULL,				 // pfnSetupVisibility
		NULL,				 // pfnUpdateClientData
		NULL,				 // pfnAddToFullPack
		NULL,				 // pfnCreateBaseline
		NULL,				 // pfnRegisterEncoders
		NULL,				 // pfnGetWeaponData
		NULL,				 // pfnCmdStart
		NULL,				 // pfnCmdEnd
		NULL,				 // pfnConnectionlessPacket
		NULL,				 // pfnGetHullBounds
		NULL,				 // pfnCreateInstancedBaselines
		NULL,				 // pfnInconsistentFile
		NULL,				 // pfnAllowLagCompensation
};

DLL_FUNCTIONS g_FunctionTable_Post =
	{
		NULL,					   // pfnGameInit
		NULL,					   // pfnSpawn
		NULL,					   // pfnThink
		NULL,					   // pfnUse
		NULL,					   // pfnTouch
		NULL,					   // pfnBlocked
		NULL,					   // pfnKeyValue
		NULL,					   // pfnSave
		NULL,					   // pfnRestore
		NULL,					   // pfnSetAbsBox
		NULL,					   // pfnSaveWriteFields
		NULL,					   // pfnSaveReadFields
		NULL,					   // pfnSaveGlobalState
		NULL,					   // pfnRestoreGlobalState
		NULL,					   // pfnResetGlobalState
		NULL,					   // pfnClientConnect
		NULL,					   // pfnClientDisconnect
		NULL,					   // pfnClientKill
		&R_ClientPutInServer_Post, // pfnClientPutInServer
		NULL,					   // pfnClientCommand
		NULL,					   // pfnClientUserInfoChanged
		&ServerActivate_Post,	   // pfnServerActivate
		&ServerDeactivate_Post,	   // pfnServerDeactivate
		NULL,					   // pfnPlayerPreThink
		NULL,					   // pfnPlayerPostThink
		&R_StartFrame_Post,		   // pfnStartFrame
		NULL,					   // pfnParmsNewLevel
		NULL,					   // pfnParmsChangeLevel
		NULL,					   // pfnGetGameDescription
		NULL,					   // pfnPlayerCustomization
		NULL,					   // pfnSpectatorConnect
		NULL,					   // pfnSpectatorDisconnect
		NULL,					   // pfnSpectatorThink
		NULL,					   // pfnSys_Error
		NULL,					   // pfnPM_Move
		NULL,					   // pfnPM_Init
		NULL,					   // pfnPM_FindTextureType
		NULL,					   // pfnSetupVisibility
		NULL,					   // pfnUpdateClientData
		NULL,					   // pfnAddToFullPack
		NULL,					   // pfnCreateBaseline
		NULL,					   // pfnRegisterEncoders
		NULL,					   // pfnGetWeaponData
		NULL,					   // pfnCmdStart
		CmdEnd_Post,			   // pfnCmdEnd
		NULL,					   // pfnConnectionlessPacket
		NULL,					   // pfnGetHullBounds
		NULL,					   // pfnCreateInstancedBaselines
		NULL,					   // pfnInconsistentFile
		NULL,					   // pfnAllowLagCompensation
};

NEW_DLL_FUNCTIONS g_NewDllFunctionTable =
	{
		&OnFreeEntPrivateData, //! pfnOnFreeEntPrivateData()	Called right before the object's memory is freed.  Calls its destructor.
		NULL,				   //! pfnGameShutdown()
		NULL,				   //! pfnShouldCollide()
		NULL,				   //! pfnCvarValue()
		NULL,				   //! pfnCvarValue2()
};

NEW_DLL_FUNCTIONS g_NewDllFunctionTable_Post =
	{
		NULL, //! pfnOnFreeEntPrivateData()	Called right before the object's memory is freed.  Calls its destructor.
		NULL, //! pfnGameShutdown()
		NULL, //! pfnShouldCollide()
		NULL, //! pfnCvarValue()
		NULL, //! pfnCvarValue2()
};

C_DLLEXPORT int GetEntityAPI2(DLL_FUNCTIONS *pFunctionTable, int *interfaceVersion)
{
	if (!pFunctionTable)
	{
		ALERT(at_logged, "%s called with null pFunctionTable", __FUNCTION__);
		return false;
	}
	if (*interfaceVersion != INTERFACE_VERSION)
	{
		ALERT(at_logged, "%s version mismatch; requested=%d ours=%d", __FUNCTION__, *interfaceVersion, INTERFACE_VERSION);
		*interfaceVersion = INTERFACE_VERSION;
		return false;
	}

	memcpy(pFunctionTable, &g_FunctionTable, sizeof(DLL_FUNCTIONS));
	g_pFunctionTable = pFunctionTable;

	return true;
}

C_DLLEXPORT int GetEntityAPI2_Post(DLL_FUNCTIONS *pFunctionTable, int *interfaceVersion)
{
	if (!pFunctionTable)
	{
		ALERT(at_logged, "%s called with null pFunctionTable", __FUNCTION__);
		return false;
	}
	if (*interfaceVersion != INTERFACE_VERSION)
	{
		ALERT(at_logged, "%s version mismatch; requested=%d ours=%d", __FUNCTION__, *interfaceVersion, INTERFACE_VERSION);
		*interfaceVersion = INTERFACE_VERSION;
		return false;
	}

	memcpy(pFunctionTable, &g_FunctionTable_Post, sizeof(DLL_FUNCTIONS));
	g_pFunctionTable_Post = pFunctionTable;

	return true;
}

C_DLLEXPORT int GetNewDLLFunctions(NEW_DLL_FUNCTIONS *pNewFunctionTable, int *interfaceVersion)
{
	if (!pNewFunctionTable)
	{
		ALERT(at_logged, "%s called with null pNewFunctionTable", __FUNCTION__);
		return false;
	}
	if (*interfaceVersion != NEW_DLL_FUNCTIONS_VERSION)
	{
		ALERT(at_logged, "%s version mismatch; requested=%d ours=%d", __FUNCTION__, *interfaceVersion, NEW_DLL_FUNCTIONS_VERSION);
		*interfaceVersion = NEW_DLL_FUNCTIONS_VERSION;
		return false;
	}

	memcpy(pNewFunctionTable, &g_NewDllFunctionTable, sizeof(NEW_DLL_FUNCTIONS));
	return true;
}

C_DLLEXPORT int GetNewDLLFunctions_Post(NEW_DLL_FUNCTIONS *pNewFunctionTable, int *interfaceVersion)
{
	if (!pNewFunctionTable)
	{
		ALERT(at_logged, "%s called with null pNewFunctionTable", __FUNCTION__);
		return false;
	}
	if (*interfaceVersion != NEW_DLL_FUNCTIONS_VERSION)
	{
		ALERT(at_logged, "%s version mismatch; requested=%d ours=%d", __FUNCTION__, *interfaceVersion, NEW_DLL_FUNCTIONS_VERSION);
		*interfaceVersion = NEW_DLL_FUNCTIONS_VERSION;
		return false;
	}

	memcpy(pNewFunctionTable, &g_NewDllFunctionTable_Post, sizeof(NEW_DLL_FUNCTIONS));
	return true;
}
