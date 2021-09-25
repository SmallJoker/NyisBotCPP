#include "connection.h"
#include "logger.h"
#include "utils.h" // strtrim
#include <curl/curl.h>
#include <pthread.h>
#include <sstream>
#include <string.h>

Connection *Connection::createStream(cstr_t &address, int port)
{
	Connection *con = new Connection(CT_STREAM);
	curl_easy_setopt(con->m_curl, CURLOPT_URL, address.c_str());
	curl_easy_setopt(con->m_curl, CURLOPT_PORT, (long)port);
	curl_easy_setopt(con->m_curl, CURLOPT_CONNECT_ONLY, 1L);
	curl_easy_setopt(con->m_curl, CURLOPT_TCP_KEEPALIVE, 1L);
	curl_easy_setopt(con->m_curl, CURLOPT_USE_SSL, (long)CURLUSESSL_ALL);
	curl_easy_setopt(con->m_curl, CURLOPT_TIMEOUT_MS, (long)CURL_TIMEOUT);

	curl_easy_setopt(con->m_curl, CURLOPT_VERBOSE, 1L);
	return con;
}

Connection *Connection::createHTTP_GET(cstr_t &url)
{
	Connection *con = new Connection(CT_HTTP_GET);
	curl_easy_setopt(con->m_curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_0);
	curl_easy_setopt(con->m_curl, CURLOPT_USERAGENT, "curl/" LIBCURL_VERSION);
	curl_easy_setopt(con->m_curl, CURLOPT_HTTPGET, 1L);
	curl_easy_setopt(con->m_curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(con->m_curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(con->m_curl, CURLOPT_WRITEFUNCTION, recvAsyncHTTP_GET);
	curl_easy_setopt(con->m_curl, CURLOPT_WRITEDATA, con);
	curl_easy_setopt(con->m_curl, CURLOPT_PIPEWAIT, 1L);
	return con;
}

Connection::Connection(ConnectionType ct) :
	m_type(ct)
{
	// Open connection
	curl_global_init(CURL_GLOBAL_ALL);

	m_curl = curl_easy_init();
	ASSERT(m_curl, "CURL init failed");
}

Connection::~Connection()
{
	m_is_alive = false;

	if (m_thread) {
		pthread_join(m_thread, nullptr);
		m_thread = 0;
	}

	// Disconnect
	curl_easy_cleanup(m_curl);
	curl_global_cleanup();
}

void Connection::connect()
{
	CURLcode res;
	res = curl_easy_perform(m_curl);

	ASSERT(res == CURLE_OK, "curl failed: " << curl_easy_strerror(res));

	m_is_alive = true;
	int status = 0;
	if (m_type == CT_STREAM)
		status = pthread_create(&m_thread, nullptr, &recvAsyncStream, this);

	if (status != 0) {
		m_is_alive = false;
		ERROR("pthread failed: " << strerror(status));
	}
	// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=67791
	//m_thread = new std::thread(recvAsync, this);
}

bool Connection::send(cstr_t &data) const
{
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
	while (con->m_is_alive) {
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

size_t Connection::recvAsyncHTTP_GET(void *contents, size_t size, size_t nmemb, void *con_p)
{
	Connection *con = (Connection *)con_p;

	MutexLock _(con->m_recv_queue_lock);
	con->m_recv_queue.push(std::string());
	auto &pkt = con->m_recv_queue.back();
	pkt.resize(nmemb * size);
	memcpy(&pkt[0], contents, pkt.size());

	return pkt.size();
}
