#pragma once

#include "types.h"
#include <queue>
//#include <thread>

enum ConnectionType {
	CT_INVALID,
	CT_STREAM,
	CT_HTTP_GET
};

class Connection {
public:
	static Connection *createStream(cstr_t &address, int port);
	static Connection *createHTTP_GET(cstr_t &url);

	~Connection();
	DISABLE_COPY(Connection);

	void connect();

	bool send(cstr_t &data) const;
	std::string *popRecv();
	std::string *popAll();

private:
	Connection(ConnectionType ct);

	static const unsigned MAX_SEND_RETRIES = 5;
	static const unsigned CURL_TIMEOUT = 2000;
	static const unsigned RECEIVE_BUFSIZE = 1024;

	size_t recv(std::string &data);
	static void *recvAsyncStream(void *con);
	static size_t recvAsyncHTTP_GET(void *contents, size_t size, size_t nmemb, void *con_p);

	const ConnectionType m_type;
	void *m_curl;
	//void *m_multi = nullptr;
	bool m_is_alive = false;

	// Receive thread
	pthread_t m_thread = 0;
	std::queue<std::string> m_recv_queue;
	mutable std::mutex m_recv_queue_lock;
};
