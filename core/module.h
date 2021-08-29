#pragma once

#include <string>
#include <unordered_set>
#include <vector>

#ifdef _WIN32
	#define DLL_EXPORT __declspec(dllexport)
#else
	#define DLL_EXPORT [[gnu::visibility("default")]]
#endif


class Channel;
class BotManager;
struct ModuleInternal;


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
	virtual ~IModule();
	virtual void onInitialize();

protected:
	static Channel *getChannel();
	static BotManager *getManager();
	void log(const std::string &what, bool is_error = false);
};

class BotManager : public ICallbackHandler {
public:
	~BotManager();

	void addModule(IModule &&module);
	void reloadModules();
	void clearModules();
	
	Channel *getChannel(const std::string &name);
private:
	std::unordered_set<ModuleInternal *> m_modules;
	std::unordered_set<Channel *> m_channels;
};
