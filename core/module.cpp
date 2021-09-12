
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

		if (!loadSingleModule(entry.path().string()))
			exit(1);
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

	IModule *expired_ptr = mi->module;
	mi->unload();
	bool ok = mi->load();

	if (ok) {
		mi->module->setClient(m_client);
	} else {
		keep_data = false;
		m_modules.erase(mi);
	}

	Network *net = m_client ? m_client->getNetwork() : nullptr;
	if (net) {
		if (keep_data) {
			// Update and re-assign old references
			// let's hope the two classes were not modified between the module loads

			for (Channel *c : net->getAllChannels()) {
				c->getContainers()->move(expired_ptr, mi->module);

				auto &users = c->getAllUsers();
				for (auto ui : users)
					ui->data->move(expired_ptr, mi->module);
			}
		} else {
			// Remove this module data from all locations

			for (Channel *c : net->getAllChannels()) {
				c->getContainers()->remove(expired_ptr);

				auto &users = c->getAllUsers();
				for (auto ui : users)
					ui->data->remove(expired_ptr);
			}
		}
	}

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

	if (ok) {
		m_modules.insert(mi);
		mi->module->setClient(m_client);
	} else {
		delete mi;
	}

	return ok;
}

bool ModuleMgr::onUserSay(Channel *c, const ChatInfo &info)
{
	for (ModuleInternal *mi : m_modules) {
		if (mi->module->onUserSay(c, info))
			return true;
	}
	return false;
}

bool ModuleInternal::load()
{
	LOG("Loading module " << path);
	void *handle = dlopen(path.c_str(), RTLD_NOW);
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
