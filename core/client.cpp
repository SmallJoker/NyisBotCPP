#include "client.h"
#include "channel.h"
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
	if (m_settings)
		m_settings->syncFileContents();

	delete m_module_mgr;
	m_module_mgr = nullptr;

	delete m_network;
	m_network = nullptr;
}

void IClient::addTodo(ClientTodo && ct)
{
	m_todo_lock.lock();
	m_todo.push(std::move(ct));
	m_todo_lock.unlock();
}
