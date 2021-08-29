#include "module.h"
#include "logger.h"
#include <filesystem>
// Unix only
#include <dlfcn.h>

static const char *NAME_PREFIX = "libnbm_";

struct ModuleData {
	bool load();
	void unload();

	void *dll_handle;
	IModule *module;
	std::string path;
};


ModuleMgr::~ModuleMgr()
{
	unloadModules();
}

void ModuleMgr::loadModules()
{
	if (m_modules.size() > 0)
		return;

	// Find modules in path, named "libnbm_?????.so"
	for (const auto &entry : std::filesystem::directory_iterator("modules")) {
		const std::string &name = entry.path().filename();
		size_t cut_a = name.rfind(NAME_PREFIX);
		size_t cut_b = name.find('.');
		if (cut_a == std::string::npos || cut_b == std::string::npos)
			continue;
		if (entry.path().extension() == "so")
			continue;

		loadSingleModule(entry.path().string());
	}
}

bool ModuleMgr::reloadModule(std::string name)
{
	name = NAME_PREFIX + name;

	for (ModuleData *md : m_modules) {
		if (md->path.find(name) == std::string::npos)
			continue;

		md->unload();
		bool ok = md->load();

		if (!ok)
			m_modules.erase(md);
		return ok;
	}

	return false;
}

void ModuleMgr::unloadModules()
{
	LOG("Unloading modules...");
	for (ModuleData *md : m_modules) {
		md->unload();
		delete md;
	}
	m_modules.clear();
}

bool ModuleMgr::loadSingleModule(const std::string &path)
{
	ModuleData *md = new ModuleData();
	md->path = path;
	bool ok = md->load();

	if (ok)
		m_modules.insert(md);
	else
		delete md;

	return ok;
}

bool ModuleData::load()
{
	LOG("Loading module " << path);
	void *handle = dlopen(path.c_str(), RTLD_LAZY);
	if (!handle) {
		ERROR("Failed to load module: " << dlerror());
		return false;
	}

	using InitType = IModule*(*)();
	// "_Z8nbm_initv" for C++, or "nbm_init" for extern "C"
	auto func = reinterpret_cast<InitType>(dlsym(handle, "nbm_init"));
	if (!func) {
		ERROR("Missing init function");
		dlclose(handle);
		return false;
	}

	dll_handle = handle;
	module = func();
	return true;
}

void ModuleData::unload()
{
	delete module;
	dlclose(dll_handle);

	module = nullptr;
	dll_handle = nullptr;
}
