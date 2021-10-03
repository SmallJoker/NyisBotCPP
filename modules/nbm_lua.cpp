#include "channel.h"
#include "chatcommand.h"
#include "logger.h"
#include "module.h"
#include "settings.h"
#include "utils.h"
#include <fstream>
#include <sstream>

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

	void prepareCallback(const char *name)
	{
		lua_getglobal(m_lua, "bot");
		lua_getfield(m_lua, -1, "run_callbacks");
		lua_pushstring(m_lua, name); // name
		lua_pushnumber(m_lua, 0);    // mode
	}

	void executeCallback()
	{
		int nargs = lua_gettop(m_lua);
		lua_call(m_lua, nargs - 2, 0);
		lua_settop(m_lua, 0);
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
		lua_pushcfunction(m_lua, l_test);
		lua_setglobal(m_lua, "test");

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

		prepareCallback("on_client_ready");
		executeCallback();
	}

	void onStep(float time)
	{
	}
	
	void onChannelJoin(Channel *c)
	{
		prepareCallback("on_channel_join");
		lua_pushstring(m_lua, c->getName().c_str());
		executeCallback();
	}

	void onChannelLeave(Channel *c)
	{
		
	}

	void onUserJoin(Channel *c, UserInstance *ui)
	{
		
	}

	void onUserLeave(Channel *c, UserInstance *ui)
	{
		
	}

	void onUserRename(UserInstance *ui, const std::string &old_name)
	{
		
	}

	bool onUserSay(Channel *c, UserInstance *ui, std::string &msg)
	{
		return false;
		
	}

	// ================= Lua-exposed functions =================

	static nbm_lua *getModule(lua_State *L)
	{
		// https://www.lua.org/pil/27.3.1.html
		lua_pushlightuserdata(L, (void *)&MODULE_INDEX);
		lua_gettable(L, LUA_REGISTRYINDEX);
		return (nbm_lua *)lua_touserdata(L, -1);
	}

	static int l_panic(lua_State *L)
	{
		ERROR("Lua panic! unprotected error in " << lua_tostring(L, -1));
		return 0;
	}

	static int l_print(lua_State *L)
	{
		int nargs = lua_gettop(L);

		std::ostringstream ss;
		for (int i = 0; i < nargs; ++i) {
			ss << lua_tostring(L, i + 1);
			if (i + 1 < nargs)
				ss << "\t";
		}
		LOG(ss.str());
		return 0;
	}

	static int l_error(lua_State *L)
	{
		int nargs = lua_gettop(L);

		std::ostringstream ss;
		for (int i = 0; i < nargs; ++i) {
			ss << lua_tostring(L, i + 1);
			if (i + 1 < nargs)
				ss << "\t";
		}
		ERROR(ss.str());
		// TODO: Report error to caller
		lua_error(L);
		return 0;
	}

	static int l_test(lua_State *L)
	{
		nbm_lua *m = getModule(L);
		if (!m->m_settings)
			return 0; // Client yet not ready

		m->what = lua_tostring(L, 1);
		// This causes SIGSEGV on the next syncFileContents() call. Why?
		//m->m_settings->set("test", lua_tostring(L, 1));
		WARN("Test success: " << m->what);
		return 0;
	}

private:
	std::string what;
	Settings *m_settings = nullptr;
	lua_State *m_lua = nullptr;
};


extern "C" {
	DLL_EXPORT void *nbm_init()
	{
		return new nbm_lua();
	}
}
