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

	void onUserJoin(Channel *c, UserInstance *ui)
	{
		LOG(ui->nickname);
	}
	virtual void onUserLeave(Channel *c, UserInstance *ui) {}
	virtual void onUserRename(Channel *c, UserInstance *ui, cstr_t &new_name) {}

	bool onUserSay(Channel *c, const ChatInfo &info)
	{
		if (info.ui) {
			c->say("Hello " + info.ui->nickname + "!");
			return true;
		}
		return false;
	}
};

extern "C" {
	DLL_EXPORT void *nbm_init()
	{
		return new nbm_builtin();
	}
}
