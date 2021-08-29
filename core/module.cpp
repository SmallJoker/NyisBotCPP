#include "module.h"
#include "logger.h"
#include <filesystem>
// Unix only
#include <dlfcn.h>

struct ModuleInternal {
	void *dll_handle;
	IModule *module;
};

void log(const std::string &what, bool is_error = false)
{
	WARN("stub");
}

BotManager::~BotManager()
{
	clearModules();
}

void BotManager::reloadModules()
{
	clearModules();

	// Find modules in path, named "libnbm_?????.so"
	for (const auto &entry : std::filesystem::directory_iterator("modules")) {
		const std::string &name = entry.path().filename();
		if (name.rfind("libnbm_") == std::string::npos)
			continue;
		if (entry.path().extension() == "so")
			continue;

		LOG("Loading module " << entry.path().string());
		void *handle = dlopen(entry.path().c_str(), RTLD_LAZY);
		if (!handle) {
			WARN("Failed to load module");
			continue;
		}

		using InitType = IModule*(*)();
		// "_Z8nbm_initv" for C++, or "nbm_init" for extern "C"
		auto func = reinterpret_cast<InitType>(dlsym(handle, "_Z8nbm_initv"));
		if (!func) {
			WARN("Missing init function");
			dlclose(handle);
			continue;
		}

		ModuleInternal *mi = new ModuleInternal();
		mi->dll_handle = handle;
		mi->module = func();
		m_modules.insert(mi);
	}
}

void BotManager::clearModules()
{
	for (ModuleInternal *mi : m_modules) {
		dlclose(mi->dll_handle);
		delete mi->module;
		delete mi;
	}
	m_modules.clear();
}
