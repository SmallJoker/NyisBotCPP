#pragma once

#include "utils.h"
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
struct ModuleInternal;

class IModule : public ICallbackHandler {
public:
	virtual ~IModule() {}

protected:
	static Channel *getChannel();
	static ModuleMgr *getManager();
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
	void onUserJoin(UserInstance *ui) {}
	void onUserLeave(UserInstance *ui) {}
	void onUserRename(UserInstance *ui, cstr_t &old_name) {}
	void onUserSay(const ChatInfo &info) {}
private:
	bool loadSingleModule(const std::string &path);

	std::set<ModuleInternal *> m_modules;
	Client *m_client;
};
