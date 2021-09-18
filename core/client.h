#pragma once

#include "clientrequest.h"
#include "types.h"
#include <queue>

class Channel;
class Connection;
class Logger;
class ModuleMgr;
class Network;
class Settings;
class UserInstance;

class IClient {
public:
	IClient(Settings *settings);
	virtual ~IClient();
	DISABLE_COPY(IClient);

	virtual void initialize() {}

	Settings *getSettings() const
	{ return m_settings; }
	ModuleMgr *getModuleMgr() const
	{ return m_module_mgr; }
	Network *getNetwork() const
	{ return m_network; }

	void addRequest(ClientRequest && ct);
	virtual void processRequests();

	virtual void sendRaw(cstr_t &text) const {} // TODO: remove this

	virtual void actionSay(Channel *c, cstr_t &text) {}
	virtual void actionReply(Channel *c, UserInstance *ui, cstr_t &text) {}
	virtual void actionNotice(Channel *c, UserInstance *ui, cstr_t &text) {}
	virtual void actionJoin(cstr_t &channel) {}
	virtual void actionLeave(Channel *c) {}

	virtual bool run() { return false; }

protected:
	Logger *m_log = nullptr;
	ModuleMgr *m_module_mgr = nullptr;
	Network *m_network = nullptr;
	Settings *m_settings = nullptr;

	std::mutex m_requests_lock;
	std::queue<ClientRequest> m_requests;
};
