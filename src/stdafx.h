#pragma once

#include <cassert>
#include <cstdio>

#include <string>
#include <sstream>
#include <fstream>
#include <vector>
#include <map>
#include <unordered_map>
#include <regex>
#include <functional>
#include <algorithm>

#include <windows.h>
#include <guiddef.h>
#include <wmistr.h>
#include <evntrace.h>

#include <json11/json11.hpp>
#include <taowin/src/tw_taowin.h>

extern "C" {
#include <lua/lua.h>
#include <lua/luaconf.h>
#include <lua/lauxlib.h>
}

#ifdef _DEBUG
    #define DBG(f, ...) wprintf(f L"\n", __VA_ARGS__)
#else
    #define DBG(...)
#endif
