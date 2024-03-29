#pragma once

#include "clientrequest.h"
#include "container.h"
#include "types.h"
#include <set>

#ifdef _WIN32
	#define DLL_EXPORT __declspec(dllexport)
#else
	#define DLL_EXPORT
#endif


class Channel;
class ChatCommand;
class IClient;
class ModuleMgr;
class Network;
class Settings;
class UserInstance;
struct ModuleInternal;

class IModule : public ICallbackHandler {
public:
	virtual ~IModule() {}
	// Extended callbacks
	virtual void onClientReady() {}
	virtual void onStep(float time) {}

	cstr_t &getModulePath()
	{ return *m_path; }

protected:
	// Avoids inclusion of "client.h" in modules
	ModuleMgr *getModuleMgr() const;
	Network *getNetwork() const;
	void sendRaw(cstr_t &what) const; // TODO: remove me
	void addClientRequest(ClientRequest && cr);
	bool checkBotAdmin(Channel *c, UserInstance *ui, bool alert = true) const;

	IClient *m_client = nullptr;

private:
	friend struct ModuleInternal;
	const std::string *m_path = nullptr;
};


// Main class to manage all loaded modules

class ModuleMgr : public ICallbackHandler {
public:
	ModuleMgr(IClient *cli);
	~ModuleMgr();
	DISABLE_COPY(ModuleMgr);

	bool loadModules();
	bool reloadModule(std::string name, bool keep_data = false);
	void unloadModules();

	// Settings object owned by ModuleMgr
	Settings *getSettings(IModule *module) const;
	Settings *getGlobalSettings() const;
	ChatCommand *getChatCommand() const
	{ return m_commands; }

	std::vector<std::string> getModuleList() const;

	// Callback handlers
	void onStep(float time);
	void onChannelJoin(Channel *c);
	void onChannelLeave(Channel *c);
	void onUserJoin(Channel *c, UserInstance *ui);
	void onUserLeave(Channel *c, UserInstance *ui);
	void onUserRename(UserInstance *ui, cstr_t &old_name);
	bool onUserSay(Channel *c, UserInstance *ui, std::string &msg);
	void onUserStatusUpdate(UserInstance *ui, bool is_timeout);

	// Add to status update queue
	void client_privatefunc_1(UserInstance *ui);
private:
	bool loadSingleModule(ModuleInternal *mi);
	void unloadSingleModule(ModuleInternal *mi, bool keep_data = false);

	std::chrono::high_resolution_clock::time_point m_last_step;
	// Lock indicates whether the modules are currently in use
	// do not change to ensure proper module reloading functionality
	mutable std::mutex m_lock;
	std::set<ModuleInternal *> m_modules;
	ChatCommand *m_commands = nullptr;
	IClient *m_client;
	Settings *m_settings;

	std::map<UserInstance *, float> m_status_update_timeout;
};
