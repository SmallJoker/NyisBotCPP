#pragma once

#include "types.h"
#include <queue>
//#include <thread>

class Connection;

class Connection {
public:
	Connection(cstr_t &address, int port);
	~Connection();

	bool send(cstr_t &data) const;
	std::string *popRecv();

private:
	const unsigned MAX_RETRIES = 5;
	const unsigned CURL_TIMEOUT = 3000;
	static constexpr unsigned RECEIVE_BUFSIZE = 1024;

	size_t recv(std::string &data);
	static void *recvAsync(void *con);

	pthread_t m_thread = 0;
	std::queue<std::string> m_recv_queue;
	std::mutex m_recv_queue_lock;
	void *m_curl;
	bool m_is_alive = false;
};
