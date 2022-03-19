#include "client.h"
#include "channel.h"
#include "logger.h"
#include "module.h"
#include "settings.h"

IClient::IClient(Settings *settings)
{
	m_settings = settings;

	m_network = new Network(this);
	m_module_mgr = new ModuleMgr(this);
}

IClient::~IClient()
{
	delete m_module_mgr;
	m_module_mgr = nullptr;

	delete m_network;
	m_network = nullptr;

	if (m_settings)
		m_settings->syncFileContents(SR_WRITE);
}

IFormatter *IClient::createFormatter() const
{
	return new IFormatter();
}

UserInstance *IClient::findUser(const IUserOwner *iuo, cstr_t &name)
{
	for (UserInstance *ui : iuo->getAllUsers()) {
		if (strequalsi(ui->nickname, name))
			return ui;
	}
	return nullptr;
}


void IClient::addRequest(ClientRequest && cr)
{
	m_requests_lock.lock();
	m_requests.push(std::move(cr));
	m_requests_lock.unlock();
}

void IClient::processRequests()
{
	if (m_requests.empty())
		return;

	ClientRequest cr;
	{
		m_requests_lock.lock();
		std::swap(m_requests.front(), cr);
		m_requests.pop();
		m_requests_lock.unlock();
	}

	VERBOSE("Got request type=" << (int)cr.type);

	if (cr.type == ClientRequest::RT_STATUS_UPDATE)
		m_module_mgr->client_privatefunc_1(cr.status_update);

	processRequest(cr);

	// Clean up data
	switch (cr.type) {
		case ClientRequest::RT_NONE:
			return;
		case ClientRequest::RT_RELOAD_MODULE:
			delete cr.reload_module.path;
			return;
		case ClientRequest::RT_STATUS_UPDATE:
			return;
	}
}

void IClient::processRequest(ClientRequest &cr)
{
	throw std::string("processRequest() must be overloaded by Client class");
}
