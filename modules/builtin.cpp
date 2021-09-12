#include "../core/channel.h"
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

	void onUserJoin(UserInstance *ui)
	{
		LOG(ui->nickname);
	}
	virtual void onUserLeave(UserInstance *ui) {}
	virtual void onUserRename(UserInstance *ui, cstr_t &new_name) {}
	virtual void onUserSay(const ChatInfo &info) {}
};

extern "C" {
	DLL_EXPORT void *nbm_init()
	{
		return new nbm_builtin();
	}
}
