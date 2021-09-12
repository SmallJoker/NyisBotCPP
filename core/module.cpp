
#include "channel.h"
#include "client.h"
#include "logger.h"
#include "module.h"
#include "utils.h"

#include <filesystem>
// Unix only
#include <dlfcn.h>

static const char *NAME_PREFIX = "libnbm_";


struct ModuleInternal {
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

bool ModuleMgr::reloadModule(std::string name, bool keep_data)
{
	name = NAME_PREFIX + name;

	ModuleInternal *mi = nullptr;
	for (ModuleInternal *it : m_modules) {
		if (it->path.find(name) == std::string::npos)
			continue;

		mi = it;
		break;
	}

	if (!mi)
		return false;

	Network *net = m_client ? m_client->getNetwork() : nullptr;
	if (!keep_data && net) {
		// Remove this module data from all locations
		for (Channel *c : net->getAllChannels()) {
			c->getContainers()->remove(mi->module);

			auto &users = c->getAllUsers();
			for (auto ui : users)
				ui->data->remove(mi->module);
		}
	}

	mi->unload();
	bool ok = mi->load();

	if (!ok)
		m_modules.erase(mi);
	return ok;
}

void ModuleMgr::unloadModules()
{
	LOG("Unloading modules...");
	for (ModuleInternal *mi : m_modules) {
		mi->unload();
		delete mi;
	}
	m_modules.clear();
}

bool ModuleMgr::loadSingleModule(const std::string &path)
{
	ModuleInternal *mi = new ModuleInternal();
	mi->path = path;
	bool ok = mi->load();

	if (ok)
		m_modules.insert(mi);
	else
		delete mi;

	return ok;
}

bool ModuleInternal::load()
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

void ModuleInternal::unload()
{
	delete module;
	dlclose(dll_handle);

	module = nullptr;
	dll_handle = nullptr;
}
