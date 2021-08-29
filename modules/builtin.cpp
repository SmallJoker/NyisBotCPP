#include "../core/module.h"
#include "../core/logger.h"

class nbm_builtin : public IModule {
public:
	nbm_builtin()
	{
		LOG("Contructed Builtin module!");
	}
	~nbm_builtin()
	{
		LOG("Destructed Builtin module!");
	}
	void onInitialize()
	{
		LOG("Loaded Builtin module!");
	}
};

DLL_EXPORT IModule *nbm_init()
{
	return new nbm_builtin();
}
