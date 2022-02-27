#pragma once

#include "channel.h"

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

class ChannelRef {
public:
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

	static void create(lua_State *L, Network *n, Channel *c)
	{
		ChannelRef *ref = (ChannelRef *)lua_newuserdata(L, sizeof(ChannelRef));
		luaL_getmetatable(L, m_classname);
		lua_setmetatable(L, -2);

		*ref = ChannelRef();
		ref->m_network = n;
		ref->m_channel = c;
	}

	static ChannelRef *checkClass(lua_State *L, int index = 1)
	{
		ChannelRef *ref = (ChannelRef *)luaL_checkudata(L, index, m_classname);
		if (!ref->m_network->contains(ref->m_channel))
			luaL_error(L, "ChannelRef points to invalid channel");
		return ref;
	}

	Channel *get() const { return m_channel; }

private:
	static int gc_object(lua_State* L)
	{
		// nothing to do
		return 0;
	}

	static int l_get_name(lua_State *L)
	{
		ChannelRef *ref = checkClass(L);
		lua_pushstring(L, ref->m_channel->getName().c_str());
		return 1;
	}

	static int l_say(lua_State *L)
	{
		ChannelRef *ref = checkClass(L);
		const char *text = luaL_checkstring(L, 2);
		ref->m_channel->say(text);
		return 0;
	}

	static int l_reply(lua_State *L)
	{
		ChannelRef *ref = checkClass(L);
		UserInstance *ui = (UserInstance *)lua_touserdata(L, 2);
		const char *text = luaL_checkstring(L, 3);
		bool ok = ref->m_channel->contains(ui);
		lua_pushboolean(L, ok);
		if (ok)
			ref->m_channel->reply(ui, text);
		return 1;
	}

	static int l_notice(lua_State *L)
	{
		ChannelRef *ref = checkClass(L);
		const char *text = luaL_checkstring(L, 2);
		UserInstance *ui = (UserInstance *)lua_touserdata(L, 2);
		bool ok = ref->m_channel->contains(ui);
		lua_pushboolean(L, ok);
		if (ok)
			ref->m_channel->notice(ui, text);
		return 1;
	}

	static int l_get_nickname(lua_State *L)
	{
		ChannelRef *ref = checkClass(L);
		UserInstance *ui = (UserInstance *)lua_touserdata(L, 2);
		if (ref->m_channel->contains(ui))
			lua_pushstring(L, ui->nickname.c_str());
		else
			lua_pushnil(L);
		return 1;
	}

	static int l_get_all_users(lua_State *L)
	{
		ChannelRef *ref = checkClass(L);
		auto &users = ref->m_channel->getAllUsers();
		lua_createtable(L, users.size(), 0);
		int i = 1;
		for (UserInstance *ui : users) {
			lua_pushlightuserdata(L, ui);
			lua_rawseti(L, -2, i++);
		}
		return 1;
	}

	static const char *m_classname;
	static const luaL_reg m_functions[];

	Network *m_network;
	Channel *m_channel;
};

const char *ChannelRef::m_classname = "ChannelRef";
const luaL_reg ChannelRef::m_functions[] = {
	{"get_name",  l_get_name},
	{"say",    l_say},
	{"reply",  l_reply},
	{"notice", l_notice},
	{"get_nickname", l_get_nickname},
	{"get_all_users", l_get_all_users},
	{nullptr, nullptr}
};
