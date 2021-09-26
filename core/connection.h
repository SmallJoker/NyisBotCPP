#pragma once

#include "types.h"
#include <queue>
//#include <thread>

enum ConnectionType {
	CT_INVALID,
	CT_STREAM,
	CT_HTTP,
	CT_WEBSOCKET
};

struct curl_slist;

class Connection {
public:
	static Connection *createStream(cstr_t &address, int port);
	static Connection *createHTTP(cstr_t &method, cstr_t &url);
	static Connection *createWebsocket(cstr_t &url);

	~Connection();
	DISABLE_COPY(Connection);

	void setHTTP_URL(cstr_t &url);
	void addHTTP_Header(cstr_t &what);
	void enqueueHTTP_Send(std::string && data);
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
	static size_t recvAsyncHTTP(void *buffer, size_t size, size_t nitems, void *con_p);
	static size_t sendAsyncHTTP(void *buffer, size_t size, size_t nitems, void *con_p);
	static size_t recvAsyncWebsocketHeader(void *buffer, size_t size, size_t nitems, void *con_p);

	const ConnectionType m_type;
	void *m_curl;
	curl_slist *m_http_headers = nullptr;
	bool m_is_alive = false;

	// Receive thread
	pthread_t m_thread = 0;
	size_t m_send_index = 0;
	std::queue<std::string> m_recv_queue;
	std::queue<std::string> m_send_queue;
	mutable std::mutex m_recv_queue_lock;
	mutable std::mutex m_send_queue_lock;
};
