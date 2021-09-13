#pragma once

#include "container.h"
#include "types.h"
#include <set>

#ifdef _WIN32
	#define DLL_EXPORT __declspec(dllexport)
#else
	#define DLL_EXPORT
#endif


class Channel;
class Client;
class ModuleMgr;
class Network;
class UserInstance;
struct ModuleInternal;

class IModule : public ICallbackHandler {
public:
	virtual ~IModule() {}

	void setClient(Client *cli)
	{ m_client = cli; }

	cstr_t &getModulePath()
	{ return *m_path; }

protected:
	// Avoids inclusion of "client.h" in modules
	ModuleMgr *getModuleMgr() const;
	Network *getNetwork() const;

	Client *m_client;
private:
	friend struct ModuleInternal;
	const std::string *m_path = nullptr;
};


// Main class to manage all loaded modules

class ModuleMgr : public ICallbackHandler {
public:
	ModuleMgr(Client *cli) :
		m_client(cli) {}
	~ModuleMgr();

	void addModule(IModule &&module);
	void loadModules();
	bool reloadModule(std::string name, bool keep_data = false);
	void unloadModules();

	// Callback handlers
	void onChannelJoin(Channel *c);
	void onChannelLeave(Channel *c);
	void onUserJoin(Channel *c, UserInstance *ui);
	void onUserLeave(Channel *c, UserInstance *ui);
	void onUserRename(Channel *c, UserInstance *ui, cstr_t &old_name);
	bool onUserSay(Channel *c, ChatInfo info);
private:
	bool loadSingleModule(const std::string &path);

	std::mutex m_lock;
	std::set<ModuleInternal *> m_modules;
	Client *m_client;
};
