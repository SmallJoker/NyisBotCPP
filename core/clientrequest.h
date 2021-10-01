#pragma once

#include <string>

struct ClientRequest {
	enum RequestType {
		RT_NONE,
		RT_RELOAD_MODULE,
		RT_STATUS_UPDATE
	} type = RT_NONE;

	union {
		struct {
			std::string *path;
			bool keep_data;
		} reload_module;

		std::string *status_update_nick;
		/*struct {
			// For all requests
			std::string *nick;
			// Callback-specific only
			Channel *c;
			ChatCommand *cb_cmd;
			std::string *cb_msg;
		} status_update;*/
	};
};
