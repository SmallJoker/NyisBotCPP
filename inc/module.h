#pragma once

#include <string>
#include <unordered_set>
#include <vector>

class Channel;


class ICallbackHandler {
public:
	struct ChatInfo {
		std::string name;
		std::vector<std::string> parts;
		size_t length;
	};

	virtual void onUserJoin(const std::string &name) {}
	virtual void onUserLeave(const std::string &name) {}
	virtual void onUserRename(const std::string &old_name, const std::string &new_name) {}
	virtual void onUserSay(const ChatInfo &info) {}
};

class Module : public ICallbackHandler {
public:
	Module(const std::string &name) :
		m_name(name)
	{}

protected:
	void log(const std::string &what, bool is_error = false);

private:
	const std::string m_name;
};

class BotManager : public ICallbackHandler {
public:
	void addModule(Module &&module);
	void reloadModules();
	void clearModules();
	
	Channel *getChannel(const std::string &name);
private:
	std::unordered_set<Module *>  m_modules;
	std::unordered_set<Channel *> m_channels;
};
