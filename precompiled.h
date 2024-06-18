#pragma once

// C
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>			    // for strncpy(), etc
#include <unistd.h>             
#include <fcntl.h>
#include <wchar.h>

// C++
#include <vector>				// std::vector
#include <list>					// std::list
#include <fstream>
#include <codecvt>
#include <cstdio>
#include <bits/stdc++.h>
#include <map>

// platform defs
#include "platform.h"

// cssdk
#include <extdll.h>
#include <interface.h>
#include <eiface.h>

// rewrite on own custom preprocessor definitions INDEXENT and ENTINDEX from cbase.h
#include "type_conversion.h"

#include <pm_defs.h>
#include <pm_movevars.h>
#include <com_progdefs.h>

// metamod SDK
#include <meta_api.h>

// regamedll API
#include <regamedll_api.h>
#include "regamedll_api.h"
#include "ex_regamedll_api.h"
#include <mapinfo.h>
#include <studio.h>

// rehlds API
#include <rehlds_api.h>
#include "rehlds_api.h"
#include <ex_rehlds_api.h>

// AmxModX API
//#include "amxmodx.h"
#include "CPlugin.h"
#include <amxxmodule.h>

#include "info.h"
#include "com_client.h"

// Other
#include <main.h>
#include "api_config.h"
#include "api_config.h"
#include "hook_manager.h"
#include "hook_callback.h"
#include "entity_callback_dispatcher.h"

// Refs API
#include "refsapi.h"
#include "refsapi_misc.h"
#include "refsapi_cvar_mngr.h"
#include "natives_helper.h"
#include "natives_refsapi.h"
#include "sdk_util.h"

#define MAX_PLAYERS                 32
#define DECIMAL_POINT               '.'
#define WP_CLASS_PREFIX             "weapon_"
#define WP_CLASS_PREFIX_LEN         (sizeof(WP_CLASS_PREFIX) - 1)
#define REFSAPI_CVAR                "acs_refsapi_loaded"
#define check_it_empty(x)           if (x._M_node == nullptr) return

#define _QQ                         "\"'`"
#define _LOCALE                     std::locale("ru_RU.UTF-8")
#define _COUNT(x)                   (size_t)(sizeof(x)/sizeof(cell))

#define amx_ftoc(f)                 ( * ((cell*)&f) )   /* float to cell */
#define amx_ctof(c)                 ( * ((float*)&c) )  /* cell to float */