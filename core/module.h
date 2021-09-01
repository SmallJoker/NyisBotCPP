#pragma once

#include <string>
#include <unordered_set>
#include <vector>

#ifdef _WIN32
	#define DLL_EXPORT __declspec(dllexport)
#else
	#define DLL_EXPORT
#endif


class Channel;
class ModuleMgr;
struct ModuleData;


class ICallbackHandler {
public:
	struct ChatInfo {
		std::string nickname;
		std::vector<std::string> parts;
	};

	virtual void onUserJoin(const std::string &name) {}
	virtual void onUserLeave(const std::string &name) {}
	virtual void onUserRename(const std::string &old_name, const std::string &new_name) {}
	virtual void onUserSay(const ChatInfo &info) {}
};

class IModule : public ICallbackHandler {
public:
	virtual ~IModule() {};

protected:
	static Channel *getChannel();
	static ModuleMgr *getManager();
};

class ModuleMgr : public ICallbackHandler {
public:
	~ModuleMgr();

	void addModule(IModule &&module);
	void loadModules();
	bool reloadModule(std::string name);
	void unloadModules();

	Channel *getChannel(const std::string &name);
private:
	bool loadSingleModule(const std::string &path);

	std::unordered_set<ModuleData *> m_modules;
	std::unordered_set<Channel *> m_channels;
};
