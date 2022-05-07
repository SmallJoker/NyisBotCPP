#include "connection.h"
#include "logger.h"
#include "utils.h" // strtrim
#include <curl/curl.h>
#include <pthread.h>
#include <iostream>
#include <sstream>
#include <string.h>

// Init on program start, destruct on close
struct curl_init {
	curl_init()
	{
		curl_global_init(CURL_GLOBAL_ALL);
	}
	~curl_init()
	{
		curl_global_cleanup();
	}
} CURL_INIT;

Connection *Connection::createStream(cstr_t &address, int port)
{
	Connection *con = new Connection(CT_STREAM);
	curl_easy_setopt(con->m_curl, CURLOPT_URL, address.c_str());
	curl_easy_setopt(con->m_curl, CURLOPT_PORT, (long)port);
	curl_easy_setopt(con->m_curl, CURLOPT_CONNECT_ONLY, 1L);
	curl_easy_setopt(con->m_curl, CURLOPT_TCP_KEEPALIVE, 1L);
	curl_easy_setopt(con->m_curl, CURLOPT_USE_SSL, (long)CURLUSESSL_ALL);

	curl_easy_setopt(con->m_curl, CURLOPT_VERBOSE, 1L);
	return con;
}

Connection *Connection::createHTTP(cstr_t &method, cstr_t &url)
{
	Connection *con = new Connection(CT_HTTP);
	curl_easy_setopt(con->m_curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_0);
	curl_easy_setopt(con->m_curl, CURLOPT_URL, url.c_str());

	if (method == "GET")
		curl_easy_setopt(con->m_curl, CURLOPT_HTTPGET, 1L);
	else if (method == "POST")
		curl_easy_setopt(con->m_curl, CURLOPT_POST, 1L);
	else if (method == "PUT") {
		curl_easy_setopt(con->m_curl, CURLOPT_CUSTOMREQUEST, method.c_str());
		curl_easy_setopt(con->m_curl, CURLOPT_UPLOAD, 1L);
	} else
		curl_easy_setopt(con->m_curl, CURLOPT_CUSTOMREQUEST, method.c_str());

	curl_easy_setopt(con->m_curl, CURLOPT_NOBODY, 0L);
	curl_easy_setopt(con->m_curl, CURLOPT_USERAGENT, "curl/" LIBCURL_VERSION);
	curl_easy_setopt(con->m_curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(con->m_curl, CURLOPT_PIPEWAIT, 1L);

	curl_easy_setopt(con->m_curl, CURLOPT_WRITEFUNCTION, recvAsyncHTTP);
	curl_easy_setopt(con->m_curl, CURLOPT_WRITEDATA, con);
	curl_easy_setopt(con->m_curl, CURLOPT_READFUNCTION, sendAsyncHTTP);
	curl_easy_setopt(con->m_curl, CURLOPT_READDATA, con);
	//curl_easy_setopt(con->m_curl, CURLOPT_VERBOSE, 1L);
	return con;
}

Connection *Connection::createWebsocket(cstr_t &url)
{
	Connection *con = new Connection(CT_WEBSOCKET);
	curl_easy_setopt(con->m_curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(con->m_curl, CURLOPT_FOLLOWLOCATION, 1L);

    curl_easy_setopt(con->m_curl, CURLOPT_HEADERFUNCTION, recvAsyncWebsocketHeader);
    curl_easy_setopt(con->m_curl, CURLOPT_HEADERDATA, con);
    curl_easy_setopt(con->m_curl, CURLOPT_WRITEFUNCTION, recvAsyncHTTP);
    curl_easy_setopt(con->m_curl, CURLOPT_WRITEDATA, con);
	curl_easy_setopt(con->m_curl, CURLOPT_READFUNCTION, sendAsyncHTTP);
	curl_easy_setopt(con->m_curl, CURLOPT_READDATA, con);

    con->addHTTP_Header("Connection: Upgrade");
	con->addHTTP_Header("Upgrade: websocket");
	con->addHTTP_Header("Origin: " + url);
    con->addHTTP_Header("Sec-WebSocket-Key: SGVsbG8sIHdvcmxkIQ==");
    con->addHTTP_Header("Sec-WebSocket-Version: 13");

	curl_easy_setopt(con->m_curl, CURLOPT_VERBOSE, 1L);
	return con;
}

Connection::Connection(ConnectionType ct) :
	m_type(ct)
{
	// Open connection
	m_curl = curl_easy_init();
	ASSERT(m_curl, "CURL init failed");

	curl_easy_setopt(m_curl, CURLOPT_TIMEOUT, CURL_TIMEOUT_MS);
}

Connection::~Connection()
{
	m_connected = false;

	if (m_thread) {
		pthread_join(m_thread, nullptr);
		m_thread = 0;
	}

	if (m_http_headers)
		curl_slist_free_all(m_http_headers);

	// Disconnect
	curl_easy_cleanup(m_curl);
}

void Connection::setHTTP_URL(cstr_t &url)
{
	if (m_type != CT_HTTP)
		return;

	curl_easy_setopt(m_curl, CURLOPT_URL, url.c_str());
}

void Connection::addHTTP_Header(cstr_t &what)
{
	if (m_type != CT_HTTP && m_type != CT_WEBSOCKET)
		return;

	m_http_headers = curl_slist_append(m_http_headers, what.c_str());
}

void Connection::enqueueHTTP_Send(std::string && data)
{
	if (m_type != CT_HTTP)
		return;

	MutexLock _(m_send_queue_lock);
	m_send_queue.push(std::move(data));

	/*
	curl_easy_setopt(m_curl, CURLOPT_POSTFIELDSIZE, data.size());
	curl_easy_setopt(m_curl, CURLOPT_POSTFIELDS, data.c_str()); // not copied!
	*/
}

bool Connection::connect()
{
	curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, m_http_headers);
	CURLcode res = curl_easy_perform(m_curl);

	m_connected = (res == CURLE_OK);
	if (!m_connected) {
		ERROR("CURL failed: " << curl_easy_strerror(res));
		return false;
	}

	if (m_type == CT_STREAM) {
		// Start async receive thread
		int status = pthread_create(&m_thread, nullptr, &recvAsyncStream, this);

		if (status != 0) {
			m_connected = false;
			ERROR("pthread failed: " << strerror(status));
		}
	}

	// HTTP connection: the request is already done now.

	// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=67791
	//m_thread = new std::thread(recvAsync, this);
	return m_connected;
}

bool Connection::send(cstr_t &data) const
{
	if (!m_connected) {
		WARN("Connection is dead.");
		return false;
	}

	if (data.size() < 255)
		VERBOSE("<< Sending: " << strtrim(data));
	else
		VERBOSE("<< Sending " << data.size() << " bytes");

	CURLcode res;
	int retries = 0;
	for (size_t sent_total = 0; sent_total < data.size();) {
		if (retries == MAX_SEND_RETRIES) {
			WARN(curl_easy_strerror(res));
			return false;
		}

		size_t nsent = 0;
		res = curl_easy_send(m_curl, &data[sent_total], data.size() - sent_total, &nsent);
		sent_total += nsent;

		if (res == CURLE_OK)
			continue;

		retries++;

		// Try waiting for socket
		if (res == CURLE_AGAIN)
			SLEEP_MS(10);
	}
	return true;
}

std::string *Connection::popRecv()
{
	MutexLock _(m_recv_queue_lock);
	if (m_recv_queue.size() == 0)
		return nullptr;

	// std::swap to avoid memory copy
	std::string *data = new std::string();
	std::swap(m_recv_queue.front(), *data);
	m_recv_queue.pop();
	return data;
}

std::string *Connection::popAll()
{
	MutexLock _(m_recv_queue_lock);
	if (m_recv_queue.size() == 0)
		return nullptr;

	std::string *data = new std::string();
	data->reserve(m_recv_queue.front().size() * m_recv_queue.size());

	while (!m_recv_queue.empty()) {
		data->append(m_recv_queue.front());
		m_recv_queue.pop();
	}
	return data;
}

size_t Connection::recv(std::string &data)
{
	// Re-use memory wherever possible
	thread_local char buf[RECEIVE_BUFSIZE];

	size_t nread = 0;
	CURLcode res = curl_easy_recv(m_curl, buf, sizeof(buf), &nread);

	if (res != CURLE_OK && res != CURLE_AGAIN)
		WARN(curl_easy_strerror(res));

	if (nread == 0)
		return nread;

	data.append(buf, nread);

	return nread;
}

void *Connection::recvAsyncStream(void *con_p)
{
	Connection *con = (Connection *)con_p;
	// Receive thread function
	std::string data;

	LOG("Start!");
	while (con->m_connected) {
		size_t nread = con->recv(data);
		if (nread == 0) {
			SLEEP_MS(100);
			continue;
		}

		size_t offset = 0;
		while (true) {
			// Search for newlines and split
			size_t index = data.find('\n', offset);
			if (index == std::string::npos)
				break;

			if (index == offset) {
				// Empty line
				offset++;
				continue;
			}

			// Strip prepending ':'
			// This is for IRC only!
			if (data[offset] == ':')
				offset++;

			int length = index - offset;
			bool has_cr = (data[index - 1] == '\r');

			{
				MutexLock _(con->m_recv_queue_lock);
				con->m_recv_queue.push(data.substr(offset, length - has_cr));
			}

			offset += length + 1; // + '\n'
		}

		if (offset > 0) {
			// Keep leftover text
			if (offset < data.size())
				data = data.substr(offset);
			else
				data.clear();
		}
	}

	LOG("Stop!");
	return nullptr;
}

size_t Connection::recvAsyncHTTP(void *buffer, size_t size, size_t nitems, void *con_p)
{
	Connection *con = (Connection *)con_p;

	MutexLock _(con->m_recv_queue_lock);
	con->m_recv_queue.push(std::string());
	auto &pkt = con->m_recv_queue.back();
	pkt.resize(nitems * size);
	memcpy(&pkt[0], buffer, pkt.size());

	return pkt.size();
}

size_t Connection::sendAsyncHTTP(void *buffer, size_t size, size_t nitems, void *con_p)
{
	Connection *con = (Connection *)con_p;

	MutexLock _(con->m_send_queue_lock);
	if (con->m_send_queue.empty())
		return 0;

	std::string &what = con->m_send_queue.front();

	size_t to_send = std::min(nitems * size, what.size() - con->m_send_index);
	memcpy(buffer, &what[con->m_send_index], to_send);

	con->m_send_index += to_send;
	if (con->m_send_index >= what.size()) {
		con->m_send_queue.pop();
		con->m_send_index = 0;
	}

	return to_send;
}

size_t Connection::recvAsyncWebsocketHeader(void *buffer, size_t size, size_t nitems, void *con_p)
{
	Connection *con = (Connection *)con_p;

	long response_code = 0;
	curl_easy_getinfo(con->m_curl, CURLINFO_RESPONSE_CODE, &response_code);
	if (response_code == 101) {
		// Redirect OK
	} else {
		// Error?
		//WARN("Got status code: " << response_code);
	}

	return nitems * size;
}
