#pragma once

//#define WITHOUT_SQL

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
#include <filesystem>
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
//#include "hook_manager.h"
//#include "hook_callback.h"
//#include "entity_callback_dispatcher.h"

// MYSQL
#ifndef WITHOUT_SQL
#include <chrono>
#include <ctime>
#include <future>
#include <thread>
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <signal.h>
#include <poll.h>
#include <mariadb/mysql.h>
#endif

// Refs API
#include "refsapi.h"
#include "refsapi_misc.h"
#include "refsapi_cvar_mngr.h"
#include "refsapi_recoil_mngr.h"
#ifndef WITHOUT_SQL
#include "refsapi_mysql_mngr.h"
#endif
#include "natives_helper.h"
#include "natives_refsapi.h"
#include "sdk_util.h"

// ARENA FIX
//asm(".symver fcntl,fcntl@@GLIBC_2.0");
//#include "force_link_glibc_2.9.h"
//cmake ../ -DWITH_SSL=OFF -DCMAKE_TOOLCHAIN_FILE=../cmake/linux_x86_toolchain.cmake