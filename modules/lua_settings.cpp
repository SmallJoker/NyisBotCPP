#pragma once

#include "channel.h"
#include "settings.h"

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

class SettingsRef {
public:
	// Userdata: https://www.lua.org/pil/28.1.html
	static void initialize(lua_State *L)
	{
		lua_newtable(L);
		int methods = lua_gettop(L);
		luaL_newmetatable(L, m_classname);
		int meta = lua_gettop(L);

		lua_pushliteral(L, "__index");
		lua_pushvalue(L, methods);
		lua_settable(L, meta);

		lua_pushliteral(L, "__gc");
		lua_pushcfunction(L, gc_object);
		lua_settable(L, meta);

		lua_pop(L, 1); // meta table
		luaL_register(L, nullptr, m_functions);
		lua_pop(L, 1); // methods table
	}

	static void create(lua_State *L, Settings *s)
	{
		SettingsRef *ref = (SettingsRef *)lua_newuserdata(L, sizeof(SettingsRef));
		luaL_getmetatable(L, m_classname);
		lua_setmetatable(L, -2);

		*ref = SettingsRef();
		ref->m_settings = s;
	}

	static int gc_object(lua_State* L)
	{
		// nothing to do
		return 0;
	}

private:
	static SettingsRef *checkClass(lua_State *L)
	{
		SettingsRef *ref = (SettingsRef *)luaL_checkudata(L, 1, m_classname);
		if (!ref->m_settings)
			luaL_error(L, "SettingsRef accessed without valid reference");
		return ref;
	}

	static int l_get(lua_State *L)
	{
		SettingsRef *ref = checkClass(L);
		const char *key = luaL_checkstring(L, 2);
		lua_pushstring(L, ref->m_settings->get(key).c_str());
		return 1;
	}

	static int l_set(lua_State *L)
	{
		SettingsRef *ref = checkClass(L);
		const char *key = luaL_checkstring(L, 2);
		const char *value = luaL_checkstring(L, 3);
		lua_pushboolean(L, ref->m_settings->set(key, value));
		return 1;
	}

	static int l_remove(lua_State *L)
	{
		SettingsRef *ref = checkClass(L);
		const char *key = luaL_checkstring(L, 2);
		lua_pushboolean(L, ref->m_settings->remove(key));
		return 1;
	}

	static int l_save(lua_State *L)
	{
		SettingsRef *ref = checkClass(L);
		ref->m_settings->syncFileContents(SR_WRITE);
		return 0;
	}

	static const char *m_classname;
	static const luaL_reg m_functions[];

	Settings *m_settings;
};

const char *SettingsRef::m_classname = "SettingsRef";
const luaL_reg SettingsRef::m_functions[] = {
	{"get",    l_get},
	{"set",    l_set},
	{"remove", l_remove},
	{"save",   l_save},
	{nullptr, nullptr}
};
