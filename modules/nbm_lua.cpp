#include "chatcommand.h"
#include "client.h"
#include "logger.h"
#include "module.h"
#include "utils.h"
#include <fstream>
#include <sstream>

#include "lua_channel.cpp"
#include "lua_settings.cpp"

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

#ifdef _WIN32
#  define EXPORT __declspec(dllexport)
#else
#  define EXPORT LUALIB_API
#endif

static const char MODULE_INDEX = '\0';

class SettingsRef;

class nbm_lua : public IModule {
public:
	nbm_lua()
	{
		std::string err = resetLua();
		if (!err.empty())
			ERROR(err);
	}

	~nbm_lua()
	{
		if (m_lua)
			lua_close(m_lua);

		if (m_settings)
			m_settings->syncFileContents(SR_WRITE);
	}

	// ================= Helper functions =================

	void registerFunction(const char *name, lua_CFunction f)
	{
		lua_getglobal(m_lua, "bot");
		int table = lua_gettop(m_lua);
		lua_pushstring(m_lua, name);
		lua_pushcfunction(m_lua, f);
		lua_settable(m_lua, table);
		lua_pop(m_lua, 1); // "bot"
	}

	enum CallbackExecutionMode {
		CBEM_NO_ABORT,
		CBEM_ABORT_ON_TRUE
	};

	void prepareCallback(const char *name, CallbackExecutionMode mode)
	{
		lua_getglobal(m_lua, "bot");
		lua_getfield(m_lua, -1, "run_callbacks");
		lua_pushstring(m_lua, name); // name
		lua_pushnumber(m_lua, (int)mode);    // mode
	}

	void executeCallback(int nresults = 0)
	{
		int nargs = lua_gettop(m_lua);
		lua_call(m_lua, nargs - 2, nresults);
		lua_settop(m_lua, -nresults);
	}

	std::string resetLua()
	{
		if (m_lua)
			lua_close(m_lua);

		// Load environment
		m_lua = lua_open();
		luaL_openlibs(m_lua);

		{
			// Save this instance to retrieve in registered functions
			lua_pushlightuserdata(m_lua, (void *)&MODULE_INDEX);
			lua_pushlightuserdata(m_lua, (void *)this);
			lua_settable(m_lua, LUA_REGISTRYINDEX);
		}
	
		lua_newtable(m_lua);
		lua_setglobal(m_lua, "bot");

		// Set up core functions
		lua_atpanic(m_lua, l_panic);
		lua_pushcfunction(m_lua, l_print);
		lua_setglobal(m_lua, "print");
		lua_pushcfunction(m_lua, l_error);
		lua_setglobal(m_lua, "error");

		{
			// Class initializations
			SettingsRef::initialize(m_lua);
			ChannelRef::initialize(m_lua);
		}

		const std::string filename = "script/init.lua";
		std::ifstream is(filename);
		if (!is.good()) {
			return "Script file not found: " + filename;
		}
		is.close();

		bool ok = !luaL_loadfile(m_lua, filename.c_str());
		ok &= !lua_pcall(m_lua, 0, 0, 0);
		if (!ok) {
			const char *err = lua_tostring(m_lua, -1);
			return std::string("Script failed to execute: ") + (err ? err : "(no information)");
		}
		lua_pop(m_lua, lua_gettop(m_lua));
		return "";
	}

	// ================= Bot interaction =================

	void onClientReady()
	{
		m_settings = getModuleMgr()->getSettings(this);

		lua_getglobal(m_lua, "bot");
		{
			lua_pushstring(m_lua, "settings");
			SettingsRef::create(m_lua, m_settings);
			lua_settable(m_lua, -3);
		}
		lua_pop(m_lua, 1);

		prepareCallback("on_client_ready", CBEM_NO_ABORT);
		executeCallback();
	}

	void onStep(float time)
	{
	}
	
	void onChannelJoin(Channel *c)
	{
		prepareCallback("on_channel_join", CBEM_NO_ABORT);
		lua_pushstring(m_lua, c->getName().c_str());
		executeCallback();
	}

	void onChannelLeave(Channel *c)
	{
		prepareCallback("on_channel_leave", CBEM_NO_ABORT);
		ChannelRef::create(m_lua, m_client->getNetwork(), c);
		executeCallback();
	}

	void onUserJoin(Channel *c, UserInstance *ui)
	{
		prepareCallback("on_user_join", CBEM_NO_ABORT);
		ChannelRef::create(m_lua, m_client->getNetwork(), c);
		lua_pushlightuserdata(m_lua, ui);
		executeCallback();
	}

	void onUserLeave(Channel *c, UserInstance *ui)
	{
		prepareCallback("on_user_leave", CBEM_NO_ABORT);
		ChannelRef::create(m_lua, m_client->getNetwork(), c);
		lua_pushlightuserdata(m_lua, ui);
		executeCallback();
	}

	void onUserRename(UserInstance *ui, const std::string &old_name)
	{
		prepareCallback("on_user_rename", CBEM_NO_ABORT);
		lua_pushlightuserdata(m_lua, ui);
		lua_pushstring(m_lua, old_name.c_str());
		executeCallback();
	}

	bool onUserSay(Channel *c, UserInstance *ui, std::string &msg)
	{
		prepareCallback("on_user_say", CBEM_ABORT_ON_TRUE);
		ChannelRef::create(m_lua, m_client->getNetwork(), c);
		lua_pushlightuserdata(m_lua, ui);
		lua_pushstring(m_lua, msg.c_str());
		executeCallback(1);
		bool ok = lua_toboolean(m_lua, -1);
		lua_settop(m_lua, 0);
		return ok;
	}

	// ================= Lua-exposed functions =================

	static nbm_lua *getModule(lua_State *L)
	{
		// https://www.lua.org/pil/27.3.1.html
		lua_pushlightuserdata(L, (void *)&MODULE_INDEX);
		lua_gettable(L, LUA_REGISTRYINDEX);
		nbm_lua *m = (nbm_lua *)lua_touserdata(L, -1);
		if (!m->m_settings)
			luaL_error(L, "nbm_lua accessed before onClientReady");

		return m;
	}

	static int l_panic(lua_State *L)
	{
		ERROR("Lua panic! unprotected error in " << lua_tostring(L, -1));
		return 0;
	}

	static void doLog(lua_State *L, LogLevel level, int i)
	{
		int nargs = lua_gettop(L);

		std::ostringstream ss;
		for (; i < nargs; ++i) {
			ss << lua_tostring(L, i + 1);
			if (i + 1 < nargs)
				ss << "\t";
		}
		LoggerAssistant(level, "Lua log") << ss.str();
	}

	static int l_print(lua_State *L)
	{
		doLog(L, LL_NORMAL, 0);
		return 0;
	}

	static int l_error(lua_State *L)
	{
		doLog(L, LL_ERROR, 0);
		// TODO: Report error to caller
		lua_error(L);
		return 0;
	}

private:
	Settings *m_settings = nullptr;
	lua_State *m_lua = nullptr;
};

extern "C" {
	DLL_EXPORT void *nbm_init()
	{
		return new nbm_lua();
	}
}
