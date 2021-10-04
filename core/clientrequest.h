#pragma once

#include <string>

class UserInstance;

struct ClientRequest {
	enum RequestType {
		RT_NONE,
		RT_RELOAD_MODULE,
		RT_STATUS_UPDATE,
	} type = RT_NONE;

	union {
		struct {
			std::string *path;
			bool keep_data;
		} reload_module;

		UserInstance *status_update;
	};
};
