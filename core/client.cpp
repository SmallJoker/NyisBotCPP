#include "client.h"
#include "connection.h"

Client::Client(cstr_t &address, int port)
{
	m_con = new Connection(address, port);
}

Client::~Client()
{
	delete m_con;
}

bool Client::run()
{
	bool ok = true;
	return ok;
}

