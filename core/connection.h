#pragma once

#include <queue>
#include <string>
#include <thread>
#include "mutex.h"

class Connection;

class Connection {
public:
	Connection(const std::string &address, int port);
	~Connection();
	
	bool send(const std::string &data);
	std::string *popRecv();

private:
	const unsigned MAX_RETRIES = 5;
	const unsigned CURL_TIMEOUT = 3000;
	static constexpr unsigned RECEIVE_BUFSIZE = 1024;

	size_t recv(std::string &data);
	static void recvAsync(Connection *con);

	std::thread m_thread;
	std::queue<std::string> m_recv_queue;
	std::mutex m_recv_queue_lock;
	void *m_curl;
	bool m_is_alive = true;
};
