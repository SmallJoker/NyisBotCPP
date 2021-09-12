#include "../core/module.h"

extern "C" {
	DLL_EXPORT void *nbm_init()
	{
		return new IModule();
	}
}
