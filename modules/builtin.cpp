#include "../core/module.h"
#include "../core/logger.h"

class nbm_builtin : public IModule {
public:
	nbm_builtin()
	{
		LOG("Load");
	}
	~nbm_builtin()
	{
		LOG("Unload");
	}

	void onUserJoin(const std::string &name)
	{
		LOG(name);
	}
	virtual void onUserLeave(const std::string &name) {}
	virtual void onUserRename(const std::string &old_name, const std::string &new_name) {}
	virtual void onUserSay(const ChatInfo &info) {}
};

extern "C" {
	DLL_EXPORT void *nbm_init()
	{
		return new nbm_builtin();
	}
}
