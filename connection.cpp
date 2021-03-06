#include "inc/connection.h"
#include "inc/logger.h"
#include <curl/curl.h>
#include <sstream>


#define SLEEP_MS(ms) \
	std::this_thread::sleep_for(std::chrono::milliseconds((ms)));

	
Connection::Connection(const std::string &address, int port)
{
    // Open connection
	curl_global_init(CURL_GLOBAL_ALL);

	m_curl = curl_easy_init();
	ASSERT(m_curl, "CURL init failed");
	curl_easy_setopt(m_curl, CURLOPT_URL, address.c_str());
	curl_easy_setopt(m_curl, CURLOPT_PORT, (long)port);
	curl_easy_setopt(m_curl, CURLOPT_CONNECT_ONLY, 1L);
	curl_easy_setopt(m_curl, CURLOPT_TCP_KEEPALIVE, 1L);
	curl_easy_setopt(m_curl, CURLOPT_USE_SSL, (long)CURLUSESSL_ALL);
	curl_easy_setopt(m_curl, CURLOPT_TIMEOUT_MS, (long)CURL_TIMEOUT);
	curl_easy_setopt(m_curl, CURLOPT_VERBOSE, 1L);

	CURLcode res;
	res = curl_easy_perform(m_curl);

	ASSERT(res == CURLE_OK, "curl failed: " << curl_easy_strerror(res));
	//res = curl_easy_getinfo(m_curl, CURLINFO_ACTIVESOCKET, &m_socket);

	m_thread = std::thread(recvAsync, this);

	// Demo
	std::string what("GET / HTTP/1.0\r\nHost: example.com\r\n\r\n");
	send(what);

	SLEEP_MS(1000);
}

Connection::~Connection()
{
	m_is_alive = false;
	m_thread.join();

	// Disconnect
	curl_easy_cleanup(m_curl);
	curl_global_cleanup();
}

bool Connection::send(const std::string &data)
{
	LOG("<< Sending " << data.size() << " bytes");
	CURLcode res;
	int retries = 0;
	for (size_t sent_total = 0; sent_total < data.size();) {
		if (retries == MAX_RETRIES) {
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
	m_recv_queue_lock.lock();
	if (m_recv_queue.size() == 0) {
		m_recv_queue_lock.unlock();
		return nullptr;
	}

	// std::swap to avoid memory copy
	std::string *data = new std::string();
	std::swap(m_recv_queue.front(), *data);

	m_recv_queue_lock.unlock();
	return data;
}

size_t Connection::recv(std::string &data)
{
	// Re-use memory wherever possible
	thread_local char buf[RECEIVE_BUFSIZE];

	size_t nread = 0;
	CURLcode res = curl_easy_recv(m_curl, buf, sizeof(buf), &nread);

	if (res != CURLE_OK)
		WARN(curl_easy_strerror(res));

	if (nread == 0)
		return nread;

	data.append(buf, nread);

	return nread;
}

void Connection::recvAsync(Connection *con)
{
	// Receive thread function
	std::string data;

	while (con->m_is_alive) {
		size_t nread = con->recv(data);
		if (nread == 0) {
			SLEEP_MS(100);
			continue;
		}
		LOG(">> Received " << nread << " bytes");

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
			if (data[offset] == ':')
				offset++;

			int length = index - offset;

			{
				con->m_recv_queue_lock.lock();
				con->m_recv_queue.push(data.substr(offset, length));
				LOG("Got line: " << con->m_recv_queue.back());
				con->m_recv_queue_lock.unlock();
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
		SLEEP_MS(50);
	}
}
