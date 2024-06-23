#define check_global_r(x)           if ((x) > plugin->getAMX()->hlw) { AMXX_LogError(plugin->getAMX(), AMX_ERR_NATIVE, "%s: Cvars can only be bound to global variables", __FUNCTION__); return FALSE; }
#define check_type_r(x)             if (!((x) > CVAR_TYPE_NONE && x < CVAR_TYPES_SIZE)) { AMXX_LogError(plugin->getAMX(), AMX_ERR_NATIVE, "%s: Cvars type error", __FUNCTION__); return FALSE; }
#define check_fwd_r(x)              if ((x) == -1) { AMXX_LogError(plugin->getAMX(), AMX_ERR_NATIVE, "%s: register callback <%s> failed", __FUNCTION__, wstos(name).c_str()); return FALSE; }

void RegisterNatives_Misc();