#pragma once

#include "types.h"

class Connection;

class Client {
public:
	Client(cstr_t &address, int port);
	~Client();

	bool run();

private:
	Connection *m_con = nullptr;

};
